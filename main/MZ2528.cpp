/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            MZ2528.cpp
// Created:         Jan 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     MZ2500/2800 Key Matrix logic.
//                  This source file contains the application logic to obtain PS/2 scan codes, map them
//                  into a virtual keyboard matrix as per the Sharp MZ series key matrix and the
//                  logic to transmit the virtual key matrix to the MZ2500/2800.
//
//                  The application uses a modified version of the PS2KeyAdvanced 
//                  https://github.com/techpaul/PS2KeyAdvanced class from Paul Carpenter.
//
//                  The application uses the Espressif Development environment with Arduino components.
//                  This is necessary for the PS2KeyAdvanced class, which I may in future convert to
//                  use esp-idf library calls rather than Arduino.
//
//                  The Espressif environment is necessary in order to have more control over the build.
//                  It is important, for timing, that Core 1 is dedicated to MZ Interface 
//                  logic and Core 0 is used for all other RTOS/Interrupts tasks. 
//
// Credits:         
// Copyright:       (c) 2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Jan 2022 - Initial write.
//                  Feb 2022 - Updates and fixes. Added logic to detect PS/2 disconnect and reconnect,
//                             added 3 alternative maps selected by ALT+F1 (MZ2500), ALT+F2(MZ2000)
//                             ALT+F3(MZ80B) due to slight differences in the key layout.
//                             Added framework for wifi so that the interface can enter AP mode to
//                             acquire local net parameters then connect to local net. Needs the web
//                             interface writing.
//                  Mar 2022 - Copied from the mz25key and seperated logic into modules as the sharpkey
//                             supports multiple host types.
//            v1.01 May 2022 - Initial release version.
//            v1.02 Jun 2022 - Updated interface to yield Core 1 when no key has been pressed. This is
//                             necessary to allow Bluetooth and NVS to work (even though BT is pinned
//                             to core 0, the NVS seems to require both CPU's).
//
// Notes:           See Makefile to enable/disable conditional components
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// This source file is free software: you can redistribute it and#or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This source file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
/////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <map>
#include <filesystem>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Arduino.h"
#include "driver/gpio.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "sys/stat.h"
#include "esp_littlefs.h"
#include "PS2KeyAdvanced.h"
#include "sdkconfig.h"
#include "MZ2528.h"

// Tag for ESP main application logging.
#define  MAINTAG  "mz25key"


// Method to connect and interact with the MZ-2500 keyboard controller. This method is seperate from the MZ-2800
// as the scan is different and as it is time critical it needs to be per target machine.
//
// The basic requirement is to:
//   1. Detect a falling edge on the RTSN signal
//   2. Read the provided ROW number.
//   3. Lookup the matrix data for given ROW.
//   4. Output data to LS257 Mux.
//   5. Wait for RTSN to return inactive.
//   5. Loop
//
//   The hidInterface method is responsible for obtaining scancodes from either a PS/2 or Bluetooth Keyboard and
//   creating the corresponding virtual matrix.
//
//   NB: As this method holds Core 1 under spinlock, no FreeRTOS or Arduino access 
//   can be made except for basic I/O ports. The spinlock has to be released for non
//   I/O work.
//
// The MZ timing period is ~600ns RTSN Low to High (Latch Row data coming from MZ machine),
// MPX (connected direct to external Quad 2-1 Mux goes high as RTSN goes low, the MPX
// signal is held high for 300ns then goes low for 300ns. The period when RTSN is low is
// when the MZ machine reads the scan row data. The cycle is ~1.2uS repeating, 14 rows
// so ~16.8uS for one full key matrix, The MZ machine if stuck in a tight loop will take
// approx 100uS to scan the matrix so the Gate Arrays are over sampling 6 times.
//
// WARNING: The GPIO's are configurable via menuconfig BUT it is assumed all except RTSNi
//          are in the first GPIO bank and RTSNi is in the second GPIO bank. Modify the code
//          if RTSNi is set in the first bank or KDB[3:0], KDI4 are in the second bank.
//
IRAM_ATTR void MZ2528::mz25Interface( void * pvParameters )
{
    // Locals.
    bool              critical = false;
    volatile uint32_t gpioIN;
    volatile uint8_t  strobeRow = 1;

    // Mask values declared as variables, let the optimiser decide wether they are constants or placed in-memory.
    uint32_t          rowBitMask = (1 << CONFIG_HOST_KDB3) | (1 << CONFIG_HOST_KDB2) | (1 << CONFIG_HOST_KDB1) | (1 << CONFIG_HOST_KDB0);
    uint32_t          colBitMask = (1 << CONFIG_HOST_KDO7) | (1 << CONFIG_HOST_KDO6) | (1 << CONFIG_HOST_KDO5) | (1 << CONFIG_HOST_KDO4) | 
                                   (1 << CONFIG_HOST_KDO3) | (1 << CONFIG_HOST_KDO2) | (1 << CONFIG_HOST_KDO1) | (1 << CONFIG_HOST_KDO0);
    uint32_t          KDB3_MASK  = (1 << CONFIG_HOST_KDB3);
    uint32_t          KDB2_MASK  = (1 << CONFIG_HOST_KDB2);
    uint32_t          KDB1_MASK  = (1 << CONFIG_HOST_KDB1);
    uint32_t          KDB0_MASK  = (1 << CONFIG_HOST_KDB0);
    uint32_t          KDI4_MASK  = (1 << CONFIG_HOST_KDI4);
    uint32_t          RTSNI_MASK = (1 << (CONFIG_HOST_RTSNI - 32));
  //uint32_t          MPXI_MASK  = (1 << CONFIG_HOST_MPXI);

    // Retrieve pointer to object in order to access data.
    MZ2528* pThis = (MZ2528*)pvParameters;

    // Initialise the MUTEX which prevents this core from being released to other tasks.
    pThis->mzMutex = portMUX_INITIALIZER_UNLOCKED;

    // Setup starting state.
    GPIO.out_w1ts = colBitMask;
   
    // Sign on.
    ESP_LOGW(MAINTAG, "Starting mz25Interface thread, colBitMask=%08x, rowBitMask=%08x.", colBitMask, rowBitMask);

    // Permanent loop, just wait for an RTSN strobe, latch the row, lookup matrix and output.
    // Timings with Power LED = LED Off to On = 108ns, LED On to Off = 392ns
    for(;;)
    {
        // Suspend processing if there are no new key presses or a suspend request has been made, ie from WiFi interface.
        if(pThis->yieldHostInterface == true)
        {
            // Exit spinlock.
            if(critical) portEXIT_CRITICAL(&pThis->mzMutex);

            // Requested to suspend?
            if(pThis->suspendRequested())
            {
                // Setting the ADC2 ports to output mode is required due to the ESP32 Client Mode which has many issues, one is if the ports are set to input and receiving data
                // it fails to start!
                // ESP32 WiFi/ADC2 workaround. The ESP32 wont connect to a router in station mode if the ADC2 pins are set to input and have an alternating signal present. 
                pThis->reconfigADC2Ports(true);

                // All bits to 1, ie. inactive - this is necessary otherwise the host could see a key being held.
                GPIO.out_w1ts = colBitMask;
                //GPIO.out_w1ts = KDB3_MASK | KDB2_MASK | KDB1_MASK | KDB0_MASK | KDI4_MASK | MPXI_MASK;

                // Yield until the core is released.
                pThis->yield(0);

                // Restore the GPIO.
                pThis->reconfigADC2Ports(false);
            } else
            {
                // All bits to 1, ie. inactive - this is necessary otherwise the host could see a key being held. The normal state for inputs is all high
                // if no key has been pressed.
                GPIO.out_w1ts = colBitMask;

                // Yield to allow other tasks to run.
                while(pThis->yieldHostInterface == true) vTaskDelay(0);
            }

            // Enter spinlock.
            portENTER_CRITICAL(&pThis->mzMutex);
            critical = true;
        }

        // Detect RTSN going high, the MZ will send the required row during this cycle.
        if(REG_READ(GPIO_IN1_REG) & RTSNI_MASK)
        {
            // Read the GPIO ports to get latest Row and KDI4 states.
            gpioIN = REG_READ(GPIO_IN_REG);

            // Assemble the required matrix row from the configured bits.
            strobeRow = ((gpioIN&KDB3_MASK) >> (CONFIG_HOST_KDB3-3)) | ((gpioIN&KDB2_MASK) >> (CONFIG_HOST_KDB2-2)) | ((gpioIN&KDB1_MASK) >> (CONFIG_HOST_KDB1-1)) | ((gpioIN&KDB0_MASK) >> CONFIG_HOST_KDB0);
         
            // Clear all KDO bits - clear state = '1'
            GPIO.out_w1ts = colBitMask;                                       // Reset all scan data bits to '1', inactive.

            // KDI4 indicates if row data is needed or a single byte ANDing all the keys together, ie. to detect a key press without strobing all rows.
            if(gpioIN & KDI4_MASK)
            {
                // Set all required KDO bits according to keyMatrix, set state = '0'.
                GPIO.out_w1tc = pThis->mzControl.keyMatrixAsGPIO[strobeRow];  // Set to '0' active bits.
            } else
            {
                // Set all required KDO bits according to the strobe all value. set state = '0'.
                GPIO.out_w1tc = pThis->mzControl.strobeAllAsGPIO;             // Set to '0' active bits.
            }

            // Wait for RTSN to go low. No lockup guarding as timing is critical also the watchdog is disabled, if RTSN never goes low then the user has probably unplugged the interface!
            while((REG_READ(GPIO_IN1_REG) & RTSNI_MASK) && pThis->yieldHostInterface == false);
        }

        // Logic to feed the watchdog if needed. Watchdog disabled in menuconfig but if enabled this will need to be used.
        //TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
        //TIMERG0.wdt_feed=1;                       // feed dog
        //TIMERG0.wdt_wprotect=0;                   // write protect
        //TIMERG1.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
        //TIMERG1.wdt_feed=1;                       // feed dog
        //TIMERG1.wdt_wprotect=0;                   // write protect
    }
}

// Method to connect and interact with the MZ-2800 keyboard controller. This method is seperate from the MZ-2500
// as the scan is different and as it is time critical it needs to be per target machine.
//
// The basic requirement is to:
//   1. Detect a rising edge on the RTSN signal
//   2. Wait at least 200ns before sampling KD4
//   3. Wait at least 650ns before reading ROW.
//   4. Read the provided ROW number.
//   5. If KD4 = 0 then output logical AND of all columns to LS257 Mux.
//   6. If KD4 = 1 then lookup data for given row and output to LS257 Mux.
//   7. Wait for RTSN to return low.
//   7. Loop
//
//   The hidInterface method is responsible for obtaining scancodes from either a PS/2 or Bluetooth Keyboard and
//   creating the corresponding virtual matrix.
//
//   NB: As this method holds Core 1 under spinlock, no FreeRTOS or Arduino access 
//   can be made except for basic I/O ports. The spinlock has to be released for non
//   I/O work.
//
// The MZ 2800 timing period is 1.78uS RTSN going active high, KD4 changing state 150ns after RTSN goes active,
// ROW number being set 650ns after RTSN goes active. MPX directly controls the LS257 latch so only need to write out
// and 8 bit value prior to RTSN going inactive.
// Normally the keyboard is in STROBE ALL mode. When a key is pressed, it commences a scan and when it arrives at the pressed
// key, RTSN cycle is suspended for varying amounts of time (ie 500us or 19ms) as the controller is looking for debounce and repeat.
//
// WARNING: The GPIO's are configurable via menuconfig BUT it is assumed all except RTSNi
//          are in the first GPIO bank and RTSNi is in the second GPIO bank. Modify the code
//          if RTSNi is set in the first bank or KDB[3:0], KDI4 are in the second bank.
//
IRAM_ATTR void MZ2528::mz28Interface( void * pvParameters )
{
    // Locals.
    bool              critical = false;
    volatile uint32_t gpioIN;
    volatile uint8_t  strobeRow = 1;

    // Mask values declared as variables, let the optimiser decide wether they are constants or placed in-memory.
    uint32_t          rowBitMask = (1 << CONFIG_HOST_KDB3) | (1 << CONFIG_HOST_KDB2) | (1 << CONFIG_HOST_KDB1) | (1 << CONFIG_HOST_KDB0);
    uint32_t          colBitMask = (1 << CONFIG_HOST_KDO7) | (1 << CONFIG_HOST_KDO6) | (1 << CONFIG_HOST_KDO5) | (1 << CONFIG_HOST_KDO4) | 
                                   (1 << CONFIG_HOST_KDO3) | (1 << CONFIG_HOST_KDO2) | (1 << CONFIG_HOST_KDO1) | (1 << CONFIG_HOST_KDO0);
    uint32_t          KDB3_MASK  = (1 << CONFIG_HOST_KDB3);
    uint32_t          KDB2_MASK  = (1 << CONFIG_HOST_KDB2);
    uint32_t          KDB1_MASK  = (1 << CONFIG_HOST_KDB1);
    uint32_t          KDB0_MASK  = (1 << CONFIG_HOST_KDB0);
    uint32_t          KDI4_MASK  = (1 << CONFIG_HOST_KDI4);
    uint32_t          RTSNI_MASK = (1 << (CONFIG_HOST_RTSNI - 32));
  //uint32_t          MPXI_MASK  = (1 << CONFIG_HOST_MPXI);

    // Retrieve pointer to object in order to access data.
    MZ2528* pThis = (MZ2528*)pvParameters;

    // Initialise the MUTEX which prevents this core from being released to other tasks.
    pThis->mzMutex = portMUX_INITIALIZER_UNLOCKED;

    // Sign on.
    ESP_LOGW(MAINTAG, "Starting mz28Interface thread, colBitMask=%08x, rowBitMask=%08x.", colBitMask, rowBitMask);

    // Permanent loop, just wait for an RTSN strobe, latch the row, lookup matrix and output.
    for(;;)
    {
        // Suspend processing if there are no new key presses or a suspend request has been made, ie from WiFi interface.
        if(pThis->yieldHostInterface == true)
        {
            // Exit spinlock.
            if(critical) portEXIT_CRITICAL(&pThis->mzMutex);

            // Requested to suspend?
            if(pThis->suspendRequested())
            {
                // Setting the ADC2 ports to output mode is required due to the ESP32 Client Mode which has many issues, one is if the ports are set to input and receiving data
                // it fails to start!
                // ESP32 WiFi/ADC2 workaround. The ESP32 wont connect to a router in station mode if the ADC2 pins are set to input and have an alternating signal present. 
                pThis->reconfigADC2Ports(true);

                // All bits to 1, ie. inactive - this is necessary otherwise the host could see a key being held.
                GPIO.out_w1ts = colBitMask;
                //GPIO.out_w1ts = KDB3_MASK | KDB2_MASK | KDB1_MASK | KDB0_MASK | KDI4_MASK | MPXI_MASK;
              
                // Yield until the core is released.
                pThis->yield(0);

                // Restore the GPIO.
                pThis->reconfigADC2Ports(false);
            } else
            {
                // All bits to 1, ie. inactive - this is necessary otherwise the host could see a key being held. The normal state for inputs is all high
                // if no key has been pressed.
                GPIO.out_w1ts = colBitMask;

                // Yield to allow other tasks to run.
                while(pThis->yieldHostInterface == true) vTaskDelay(0);
            }

            // Enter spinlock.
            portENTER_CRITICAL(&pThis->mzMutex);
            critical = true;
        }

        // Detect RTSN going high, the MZ will send the required row during this cycle.
        if(REG_READ(GPIO_IN1_REG) & RTSNI_MASK)
        {
            // Slight delay needed as KD4 lags behind RTSN by approx 200ns and ROW number lags 850ns behind RTSN.
            for(volatile uint32_t delay=0; delay < 8; delay++);

            // Read the GPIO ports to get latest Row and KDI4 states.
            gpioIN = REG_READ(GPIO_IN_REG);

            // Assemble the required matrix row from the configured bits.
            strobeRow = ((gpioIN&KDB3_MASK) >> (CONFIG_HOST_KDB3-3)) | ((gpioIN&KDB2_MASK) >> (CONFIG_HOST_KDB2-2)) | ((gpioIN&KDB1_MASK) >> (CONFIG_HOST_KDB1-1)) | ((gpioIN&KDB0_MASK) >> CONFIG_HOST_KDB0);
         
            // Clear all KDO bits - clear state = '1'
            GPIO.out_w1ts = colBitMask;                                // Reset all scan data bits to '1', inactive.

            // Another short delay once the row has been assembled as we dont want to change the latch setting too soon, changing to soon leads to ghosting on previous row.
            for(volatile uint32_t delay=0; delay < 5; delay++);

            // KDI4 indicates if row data is needed or a single byte ANDing all the keys together, ie. to detect a key press without strobing all rows.
            if(gpioIN & KDI4_MASK)
            {
                // Set all required KDO bits according to keyMatrix, set state = '0'.
                GPIO.out_w1tc = pThis->mzControl.keyMatrixAsGPIO[strobeRow];  // Set to '0' active bits.
            } else
            {
                // Set all required KDO bits according to the strobe all value. set state = '0'.
                GPIO.out_w1tc = pThis->mzControl.strobeAllAsGPIO;             // Set to '0' active bits.
            }

            // Wait for RTSN to go low. No lockup guarding as timing is critical also the watchdog is disabled, if RTSN never goes low then the user has probably unplugged the interface!
            while((REG_READ(GPIO_IN1_REG) & RTSNI_MASK) && pThis->yieldHostInterface == false);
        }

        // Logic to feed the watchdog if needed. Watchdog disabled in menuconfig but if enabled this will need to be used.
        //TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
        //TIMERG0.wdt_feed=1;                       // feed dog
        //TIMERG0.wdt_wprotect=0;                   // write protect
        //TIMERG1.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
        //TIMERG1.wdt_feed=1;                       // feed dog
        //TIMERG1.wdt_wprotect=0;                   // write protect
    }
}

// Method to select keyboard configuration options. When a key sequence is pressed, ie. SHIFT+CTRL+ESC then the fourth simultaneous key is the required option and given to this 
// method to act on. Options can be machine model, keyboard map etc.
//
void MZ2528::selectOption(uint8_t optionCode)
{
    // Locals.
    //
    bool                updated = true;
    #define             SELOPTTAG "selectOption"

    // Simple switch to decode the required option and act on it.
    switch(optionCode)
    {
        // Select a keymap using 1..8 or default (STANDARD) using 0.
        case PS2_KEY_1:
            this->mzConfig.params.activeKeyboardMap = KEYMAP_UK_WYSE_KB3926;
            break;
        case PS2_KEY_2:
            this->mzConfig.params.activeKeyboardMap = KEYMAP_JAPAN_OADG109;
            break;
        case PS2_KEY_3:
            this->mzConfig.params.activeKeyboardMap = KEYMAP_JAPAN_SANWA_SKBL1;
            break;
        case PS2_KEY_4:
            this->mzConfig.params.activeKeyboardMap = KEYMAP_NOT_ASSIGNED_4;
            break;
        case PS2_KEY_5:
            this->mzConfig.params.activeKeyboardMap = KEYMAP_NOT_ASSIGNED_5;
            break;
        case PS2_KEY_6:
            this->mzConfig.params.activeKeyboardMap = KEYMAP_NOT_ASSIGNED_6;
            break;
        case PS2_KEY_7:
            this->mzConfig.params.activeKeyboardMap = KEYMAP_UK_PERIBOARD_810;
            break;
        case PS2_KEY_8:
            this->mzConfig.params.activeKeyboardMap = KEYMAP_UK_OMOTON_K8508;
            break;
        case PS2_KEY_0:
            this->mzConfig.params.activeKeyboardMap = KEYMAP_STANDARD;
            break;

        // Select the active machine model. If we are connected to an MZ-2500 host then it is possible to enable an MZ-2000/MZ-80B mapping.
        case PS2_KEY_END:
            this->mzConfig.params.activeMachineModel = (this->mzControl.mode2500 ? MZ_2500 : MZ_2800);
            break;
        case PS2_KEY_DN_ARROW:
            if(this->mzControl.mode2500)
            {
                this->mzConfig.params.activeMachineModel = MZ_2000;
            }
            break;
        case PS2_KEY_PGDN:
            if(this->mzControl.mode2500)
            {
                this->mzConfig.params.activeMachineModel = MZ_80B;
            }
            break;

        // Unknown option so ignore.
        default:
            updated = false;
            break;
    }

    // If an update was made, persist it for power cycles.
    //
    if(updated)
    {
        this->mzControl.persistConfig = true;
    }

    return;
}

// Method to refresh the transposed matrix used in the MZ interface. The normal key matrix is transposed to save valuable time
// because even though a core is dedicated to the MZ interface the timing is critical and the ESP-32 doesnt have spare horse power!
//
void MZ2528::updateMirrorMatrix(void)
{
    // Locals.

    // To save time in the MZ Interface, a mirror keyMatrix is built up, 32bit (GPIO Bank 0) wide, with the keyMatrix 8 bit data
    // mapped onto the configured pins in the 32bit register. This saves precious time in order to meet the tight 1.2uS cycle.
    //
    mzControl.noKeyPressed = true;
    for(int idx=0; idx < 15; idx++)
    {
        mzControl.keyMatrixAsGPIO[idx] =  (((mzControl.keyMatrix[idx] >> 7) & 0x01) ^ 0x01) << CONFIG_HOST_KDO7 |
                                          (((mzControl.keyMatrix[idx] >> 6) & 0x01) ^ 0x01) << CONFIG_HOST_KDO6 |
                                          (((mzControl.keyMatrix[idx] >> 5) & 0x01) ^ 0x01) << CONFIG_HOST_KDO5 |
                                          (((mzControl.keyMatrix[idx] >> 4) & 0x01) ^ 0x01) << CONFIG_HOST_KDO4 |
                                          (((mzControl.keyMatrix[idx] >> 3) & 0x01) ^ 0x01) << CONFIG_HOST_KDO3 |
                                          (((mzControl.keyMatrix[idx] >> 2) & 0x01) ^ 0x01) << CONFIG_HOST_KDO2 |
                                          (((mzControl.keyMatrix[idx] >> 1) & 0x01) ^ 0x01) << CONFIG_HOST_KDO1 |
                                          (((mzControl.keyMatrix[idx]     ) & 0x01) ^ 0x01) << CONFIG_HOST_KDO0 ;
        if(mzControl.keyMatrixAsGPIO[idx] != 0x00) mzControl.noKeyPressed = false;
    }

    // Re-calculate the Strobe All (KD4 = 1) signal, this indicates if any bit (key) in the matrix is active.
    mzControl.strobeAll = 0xFF;
    mzControl.strobeAllAsGPIO = 0x00000000;
    for(int idx2=0; idx2 < 15; idx2++)
    {
        mzControl.strobeAll &= mzControl.keyMatrix[idx2];
    }

    // To speed up the mzInterface logic, pre-calculate the strobeAll value as a 32bit GPIO output value.
    mzControl.strobeAllAsGPIO |= (((mzControl.strobeAll >> 7) & 0x01) ^ 0x01) << CONFIG_HOST_KDO7 |
                                 (((mzControl.strobeAll >> 6) & 0x01) ^ 0x01) << CONFIG_HOST_KDO6 |
                                 (((mzControl.strobeAll >> 5) & 0x01) ^ 0x01) << CONFIG_HOST_KDO5 |
                                 (((mzControl.strobeAll >> 4) & 0x01) ^ 0x01) << CONFIG_HOST_KDO4 |
                                 (((mzControl.strobeAll >> 3) & 0x01) ^ 0x01) << CONFIG_HOST_KDO3 |
                                 (((mzControl.strobeAll >> 2) & 0x01) ^ 0x01) << CONFIG_HOST_KDO2 |
                                 (((mzControl.strobeAll >> 1) & 0x01) ^ 0x01) << CONFIG_HOST_KDO1 |
                                 (((mzControl.strobeAll     ) & 0x01) ^ 0x01) << CONFIG_HOST_KDO0 ;
    if(mzControl.strobeAllAsGPIO != 0x00) mzControl.noKeyPressed = false;
    return;
}

// Method to map the PS2 scan code into a key matrix representation which the MZ-2500/2800 is expecting.
//
uint32_t MZ2528::mapKey(uint16_t scanCode)
{
    // Locals.
    uint8_t   idx;
    bool      changed = false;
    bool      matchExact = false;
    uint8_t   keyCode = (scanCode & 0xFF);
    bool      mapped = false;
    #define   MAPKEYTAG "mapKey"

    // Intercept control keys and set state variables.
    //
    //
    if(scanCode & PS2_BREAK)
    {
        // Any break key clears the option select flag.
        this->mzControl.optionSelect = false;

        // Clear any feature LED blinking.
        led->setLEDMode(LED::LED_MODE_OFF, LED::LED_DUTY_CYCLE_OFF, 0, 0L, 0L);
    } else
    {
        // Special mapping to allow selection of keyboard options. If the user presses CTRL+SHIFT+ESC then a flag becomes active and should a fourth key be pressed before a BREAK then the fourth key is taken as an option key and processed accordingly.
        if(this->mzControl.optionSelect == true && keyCode != PS2_KEY_ESC) { mapped = true; this->mzControl.optionSelect = false; selectOption(keyCode); }
        if(keyCode == PS2_KEY_ESC && (scanCode & PS2_CTRL) && (scanCode & PS2_SHIFT)) { mapped = true; this->mzControl.optionSelect = true; }

        // Special mapping to allow selection of keyboard options. If the user presses CTRL+SHIFT+ESC then a flag becomes active and should a fourth key be pressed before a BREAK then the fourth key is taken as an option key and processed accordingly.
        if(this->mzControl.optionSelect == true && keyCode != PS2_KEY_ESC)
        {
            mapped = true;
            this->mzControl.optionSelect = false;
            selectOption(keyCode);
        }
        if(keyCode == PS2_KEY_ESC && (scanCode & PS2_CTRL) && (scanCode & PS2_SHIFT) && this->mzControl.optionSelect == false)
        {
            // Prime flag ready for fourth option key and start LED blinking periodically.
            mapped = true;
            this->mzControl.optionSelect = true;
            led->setLEDMode(LED::LED_MODE_BLINK, LED::LED_DUTY_CYCLE_50, 1, 500L, 500L);
        }
    }

    // If the key has been mapped as a special key, no further processing.
    if(mapped == true)
    {
        ESP_LOGW(MAPKEYTAG, "Mapped special key\n");
    } else
    {
        // Loop through the entire conversion table to find a match on this key, if found appy the conversion to the virtual
        // switch matrix.
        //
        for(idx=0, changed=false, matchExact=false; idx < mzControl.kmeRows && (changed == false || (changed == true && matchExact == false)); idx++)
        {
            // Match key code? Make sure the current machine and keymap match as well.
            if(mzControl.kme[idx].ps2KeyCode == (uint8_t)(scanCode&0xFF) && ((mzControl.kme[idx].machine == MZ_ALL) || (mzControl.kme[idx].machine & mzConfig.params.activeMachineModel) != 0) && ((mzControl.kme[idx].keyboardModel & mzConfig.params.activeKeyboardMap) != 0))
            {
                // Match Raw, Shift, Function, Control, ALT or ALT-Gr?
                if( (((mzControl.kme[idx].ps2Ctrl & PS2CTRL_SHIFT) == 0) && ((mzControl.kme[idx].ps2Ctrl & PS2CTRL_FUNC) == 0) && ((mzControl.kme[idx].ps2Ctrl & PS2CTRL_CTRL) == 0) && ((mzControl.kme[idx].ps2Ctrl & PS2CTRL_ALT) == 0) && ((mzControl.kme[idx].ps2Ctrl & PS2CTRL_ALTGR) == 0)) ||
                    ((scanCode & PS2_SHIFT)    && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_SHIFT) != 0) || 
                    ((scanCode & PS2_CTRL)     && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_CTRL)  != 0) ||
                    ((scanCode & PS2_ALT)      && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_ALT)   != 0) ||
                    ((scanCode & PS2_ALT_GR)   && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_ALTGR) != 0) ||
                    ((scanCode & PS2_GUI)      && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_GUI)   != 0) || 
                    ((scanCode & PS2_FUNCTION) && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_FUNC)  != 0) )
                {
                    // Exact entry match, data + control key? On an exact match we only process the first key. On a data only match we fall through to include additional data and control key matches to allow for un-mapped key combinations, ie. Japanese characters.
                    matchExact = (((scanCode & PS2_SHIFT)    && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_SHIFT) != 0) || ((scanCode & PS2_SHIFT) == 0    && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_SHIFT) == 0)) &&
                                 (((scanCode & PS2_CTRL)     && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_CTRL)  != 0) || ((scanCode & PS2_CTRL) == 0     && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_CTRL)  == 0)) &&
                                 (((scanCode & PS2_ALT)      && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_ALT)   != 0) || ((scanCode & PS2_ALT) == 0      && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_ALT)   == 0)) &&
                                 (((scanCode & PS2_ALT_GR)   && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_ALTGR) != 0) || ((scanCode & PS2_ALT_GR) == 0   && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_ALTGR) == 0)) &&
                                 (((scanCode & PS2_GUI)      && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_GUI)   != 0) || ((scanCode & PS2_GUI) == 0      && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_GUI)   == 0)) &&
                                 (((scanCode & PS2_FUNCTION) && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_FUNC)  != 0) || ((scanCode & PS2_FUNCTION) == 0 && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_FUNC)  == 0)) ? true : false;

                    // If the exact flag is set, skip as we cannot process this entry when we dont have an exact match.
                    if(matchExact == false && (mzControl.kme[idx].ps2Ctrl & PS2CTRL_EXACT) != 0)
                        continue;
                  
                    // RELEASE (PS2_BREAK == 1) or PRESS?
                    if((scanCode & PS2_BREAK))
                    {
                        // Special case for the PAUSE / BREAK key. The underlying logic has been modified to send a BREAK key event immediately 
                        // after a PAUSE make, this is necessary as the Sharp MZ machines require SHIFT (pause) BREAK so the PS/2 CTRL+BREAK wont
                        // work (unless logic is added to insert a SHIFT, pause, add BREAK). The solution was to generate a BREAK event
                        // and add a slight delay for the key matrix to register it.
                        if((scanCode&0x00FF) == PS2_KEY_PAUSE)
                        {
                            vTaskDelay(100);
                        }

                        // Loop through all the row/column combinations and if valid, apply to the matrix.
                        for(int row=0; row < PS2TBL_MZ_MAX_MKROW; row++)
                        {
                            // Reset the matrix bit according to the lookup table. 1 = No key, 0 = key in the matrix.
                            if(mzControl.kme[idx].mkRow[row] != 0xFF)
                            {
                                mzControl.keyMatrix[mzControl.kme[idx].mkRow[row]] |= mzControl.kme[idx].mkKey[row];
                                changed = true;
                            }
                        }
                       
                        // Loop through all the key releases associated with this key and reset the relevant matrix bit which was cleared on 
                        // initial keydown.
                        //
                        for(int row=0; row < PS2TBL_MZ_MAX_BRKROW; row++)
                        {
                            if(mzControl.kme[idx].brkRow[row] != 0xFF)
                            {
                                mzControl.keyMatrix[mzControl.kme[idx].brkRow[row]] &= ~mzControl.kme[idx].brkKey[row];
                                changed = true;
                            }
                        }
                    } else
                    {
                        // Loop through all the key releases associated with this key and clear the relevant matrix bit.
                        // This is done first so as to avoid false key detection in the MZ logic.
                        //
                        for(int row=0; row < PS2TBL_MZ_MAX_BRKROW; row++)
                        {
                            if(mzControl.kme[idx].brkRow[row] != 0xFF)
                            {
                                mzControl.keyMatrix[mzControl.kme[idx].brkRow[row]] |= mzControl.kme[idx].brkKey[row];
                                changed = true;
                            }
                        }
                       
                        // If a release key has been actioned, update the matrix and insert a slight pause to avoid
                        // the MZ logic seeing the released keys in combination with the newly pressed keys.
                        if(changed)
                        {
                            updateMirrorMatrix();
                            changed = false;
                            vTaskDelay(10);
                        }

                        // Loop through all the row/column combinations and if valid, apply to the matrix.
                        for(int row=0; row < PS2TBL_MZ_MAX_MKROW; row++)
                        {
                            // Set the matrix bit according to the lookup table. 1 = No key, 0 = key in the matrix.
                            if(mzControl.kme[idx].mkRow[row] != 0xFF)
                            {
                                mzControl.keyMatrix[mzControl.kme[idx].mkRow[row]] &= ~mzControl.kme[idx].mkKey[row];
                                changed = true;
                            }
                        }
                    }

                    // Only spend time updating signals if an actual change occurred. Some keys arent valid so no change will be effected.
                    if(changed)
                    {
                        updateMirrorMatrix();
                    }
                } // match key or a special function
            } // match key code
        } // for loop
    } // mapped

    // Return flag to indicate if a match occurred and the matrix updated.
    return((uint32_t)changed);
}

// Primary HID thread, running on Core 0.
// This thread is responsible for receiving PS/2 or BT scan codes and mapping them to an MZ-2500/2800 keyboard matrix.
//
IRAM_ATTR void MZ2528::hidInterface( void * pvParameters )
{
    // Locals.
    uint16_t            scanCode      = 0x0000;

    // Map the instantiating object so we can access its methods and data.
    MZ2528* pThis = (MZ2528*)pvParameters;

    // Thread never exits, just polls the keyboard and updates the matrix.
    while(1)
    { 
        // Check stack space, report if it is getting low.
        if(uxTaskGetStackHighWaterMark(NULL) < 1024)
        {
            ESP_LOGW(MAPKEYTAG, "THREAD STACK SPACE(%d)",uxTaskGetStackHighWaterMark(NULL));
        }

        // Check for PS/2 keyboard scan codes.
        while((scanCode = pThis->hid->read()) != 0)
        {
            // Scan Code Breakdown:
            // Define name bit     description
            // PS2_BREAK   15      1 = Break key code
            //            (MSB)    0 = Make Key code
            // PS2_SHIFT   14      1 = Shift key pressed as well (either side)
            //                     0 = No shift key
            // PS2_CTRL    13      1 = Ctrl key pressed as well (either side)
            //                     0 = No Ctrl key
            // PS2_CAPS    12      1 = Caps Lock ON
            //                     0 = Caps lock OFF
            // PS2_ALT     11      1 = Left Alt key pressed as well
            //                     0 = No Left Alt key
            // PS2_ALT_GR  10      1 = Right Alt (Alt GR) key pressed as well
            //                     0 = No Right Alt key
            // PS2_GUI      9      1 = GUI key pressed as well (either)
            //                     0 = No GUI key
            // PS2_FUNCTION 8      1 = FUNCTION key non-printable character (plus space, tab, enter)
            //                     0 = standard character key
            //              7-0    PS/2 Key code.
            //
            // BREAK code means all keys released so clear out flags and send update.
            ESP_LOGW(MAPKEYTAG, "SCANCODE:%04x",scanCode);

            // Update the virtual matrix with the new key value.
            pThis->mapKey(scanCode);

            // Toggle LED to indicate data flow.
            if((scanCode & PS2_BREAK) == 0)
                pThis->led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_10, 1, 100L, 0L);
        }

        // If all keys have been releaased or a suspend is requested, set the yieldInterface flag. This flag is intentional due to time
        // being critical in the host interface thread. then after a short count, 
        if(pThis->mzControl.noKeyPressed == true || pThis->suspendRequested())
        {
            vTaskDelay(10);
            pThis->yieldHostInterface = true;
        }
        else {
            pThis->yieldHostInterface = false;
        }

        // NVS writes require both CPU cores to be free, so we any config write must wait until the host interface is idle.
        if(pThis->yieldHostInterface == true && pThis->mzControl.persistConfig == true)
        {
            if(pThis->nvs->persistData(pThis->getClassName(__PRETTY_FUNCTION__), &pThis->mzConfig, sizeof(t_mzConfig)) == false)
            {
                ESP_LOGW(SELOPTTAG, "Persisting MZ-2500/MZ-2800 configuration data failed, updates will not persist in future power cycles.");
                pThis->led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_10, 200, 1000L, 0L);
            } else
            // Few other updates so make a commit here to ensure data is flushed and written.
            if(pThis->nvs->commitData() == false)
            {
                ESP_LOGW(SELOPTTAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
                pThis->led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_10, 200, 500L, 0L);
            }

            // Clear flag so we dont persist in a loop.
            pThis->mzControl.persistConfig = false;
        }
       
        // Yield if the suspend flag is set.
        pThis->yield(10);
   }
}

// A method to load the keyboard mapping table into memory for use in the interface mapping logic. If no persistence file exists or an error reading persistence occurs, the keymap 
// uses the internal static default. If no persistence file exists and attempt is made to create it with a copy of the inbuilt static map so that future operations all
// work with persistence such that modifications can be made.
//
bool MZ2528::loadKeyMap(void)
{
    // Locals.
    //
    bool        result = false;
    int         fileRows = 0;
    struct stat keyMapFileNameStat;

    // See if the file exists, if it does, get size so we can compute number of mapping rows.
    if(stat(mzControl.keyMapFileName.c_str(), &keyMapFileNameStat) == -1)
    {
        ESP_LOGW(MAINTAG, "No keymap file, using inbuilt definitions.");
    } else
    {
        // Get number of rows in the file.
        fileRows = keyMapFileNameStat.st_size/sizeof(t_keyMapEntry);

        // Subsequent reloads, delete memory prior to building new map, primarily to conserve precious resources rather than trying the memory allocation trying to realloc and then having to copy.
        if(mzControl.kme != NULL && mzControl.kme != PS2toMZ.kme)
        {
            delete mzControl.kme;
            mzControl.kme = NULL;
        }

        // Allocate memory for the new keymap table.
        mzControl.kme = new t_keyMapEntry[fileRows];
        if(mzControl.kme == NULL)
        {
            ESP_LOGW(MAINTAG, "Failed to allocate memory for keyboard map, fallback to inbuilt!");
        } else
        {
            // Open the keymap extension file for binary reading to add data to our map table.
            std::fstream keyFileIn(mzControl.keyMapFileName.c_str(), std::ios::in | std::ios::binary);

            int idx=0;
            while(keyFileIn.good())
            {
                keyFileIn.read((char *)&mzControl.kme[idx], sizeof(t_keyMapEntry));
                if(keyFileIn.good())
                {
                    idx++;
                }
            }
            // Any errors, we wind back and use the inbuilt mapping table.
            if(keyFileIn.bad())
            {
                keyFileIn.close();
                ESP_LOGW(MAINTAG, "Failed to read data from keymap extension file:%s, fallback to inbuilt!", mzControl.keyMapFileName.c_str());
            } else
            {
                // No longer need the file.
                keyFileIn.close();
               
                // Max rows in the KME table.
                mzControl.kmeRows = fileRows;

                // Good to go, map ready for use with the interface.
                result = true;
            }
        }
    }

    // Any failures, free up memory and use the inbuilt mapping table.
    if(result == false)
    {
        if(mzControl.kme != NULL && mzControl.kme != PS2toMZ.kme)
        {
            delete mzControl.kme;
            mzControl.kme = NULL;
        }
     
        // No point allocating memory if no extensions exist or an error occurs, just point to the static table.
        mzControl.kme = PS2toMZ.kme;
        mzControl.kmeRows = PS2TBL_MZ_MAXROWS;
     
        // Persist the data so that next load comes from file.
        saveKeyMap();
    }

    // Return code. Either memory map was successfully loaded, true or failed, false.
    return(result);
}

// Method to save the current keymap out to an extension file.
//
bool MZ2528::saveKeyMap(void)
{
    // Locals.
    //
    bool        result = false;
    int         idx = 0;

    // Has a map been defined? Cannot save unless loadKeyMap has been called which sets mzControl.kme to point to the internal keymap or a new memory resident map.
    //
    if(mzControl.kme == NULL)
    {
        ESP_LOGW(MAINTAG, "KeyMap hasnt yet been defined, need to call loadKeyMap.");
    } else
    {
        // Request mutex from NVS to prevent it from accessing the NVS - LittleFS is based on NVS.
        if(nvs->takeMutex() == true)
        {
            // Open file for binary writing, trunc specified to clear out the file, we arent appending.
            std::fstream keyFileOut(mzControl.keyMapFileName.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);

            // Loop whilst no errors and data rows still not written.
            while(keyFileOut.good() && idx < mzControl.kmeRows)
            {
                keyFileOut.write((char *)&mzControl.kme[idx], sizeof(t_keyMapEntry));
                idx++;
            }
            if(keyFileOut.bad())
            {
                ESP_LOGW(MAINTAG, "Failed to write data from the keymap to file:%s, deleting as state is unknown!", mzControl.keyMapFileName.c_str());
                keyFileOut.close();
                std::remove(mzControl.keyMapFileName.c_str());
            } else
            {
                // Success.
                keyFileOut.close();
                result = true;
            }

            // Relinquish mutex now write is complete.
            nvs->giveMutex();
        }
    }
   
    // Return code. Either memory map was successfully saved, true or failed, false.
    return(result);
}

// Public method to open a keymap file for data upload.
// This method opens the file and makes any validation checks as necessary.
//
bool MZ2528::createKeyMapFile(std::fstream &outFile)
{
    // Locals.
    //
    bool           result = true;
    std::string    fileName;

    // Attempt to open a temporary keymap file for writing.
    //
    fileName = mzControl.keyMapFileName;
    replaceExt(fileName, "tmp");
    outFile.open(fileName.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if(outFile.bad())
    {
        result = false;
    }

    // Send result.
    return(result);
}

// Public method to validate and store data provided by caller into an open file created by 'createKeyMapFile'.
//
bool MZ2528::storeDataToKeyMapFile(std::fstream &outFile, char *data, int size)
{
    // Locals.
    //
    bool         result = true;

    // Check that the file is still writeable then add data.
    if(outFile.good())
    {
        outFile.write(data, size);
    }
    if(outFile.bad())
    {
        result = false;
    }

    // Send result.
    return(result);
}

// Polymorphic alternative to take a vector of bytes for writing to the output file.
//
bool MZ2528::storeDataToKeyMapFile(std::fstream & outFile, std::vector<uint32_t>& dataArray)
{
    // Locals.
    //
    bool         result = true;
    char         data[1];

    // Check that the file is still writeable then add data. Not best for performace but ease of use and minimum memory.
    if(outFile.good())
    {
        for(std::size_t idx = 0; idx < dataArray.size(); idx++)
        {
            data[0] = (char)dataArray[idx];
            outFile.write((char *)&data, 1);
        }
    }
    if(outFile.bad())
    {
        result = false;
    }

    // Send result.
    return(result);
}

// Public method to close and commit a data file, created by 'createKeyMapFile' and populated by 'storeDataToKeyMapFile'.
// This involves renaming the original keymap file, closing the new file and renaming it to the original keymap filename.
//
bool MZ2528::closeAndCommitKeyMapFile(std::fstream &outFile, bool cleanupOnly)
{
    // Locals.
    //
    bool           result = true;
    std::string    fileName;

    // Check the file is still accessible and close.
    //
    outFile.close();
    if(!cleanupOnly)
    {
        if(outFile.good())
        {
            // Rename the original file.
            fileName = mzControl.keyMapFileName;
            replaceExt(fileName, "bak");
            // Remove old backup file. Dont worry if it is not there!
            std::remove(fileName.c_str());
            replaceExt(fileName, "tmp");
            // Rename new file to active.
            if(std::rename(fileName.c_str(), mzControl.keyMapFileName.c_str()) != 0)
            {
                result = false;
            }
        } else
        {
            result = false;
        }
    }

    // Send result.
    return(result);
}

// Method to return the keymap column names as header strings.
//
void MZ2528::getKeyMapHeaders(std::vector<std::string>& headerList)
{
    // Add the names.
    //
    headerList.push_back(PS2TBL_PS2KEYCODE_NAME);
    headerList.push_back(PS2TBL_PS2CTRL_NAME);
    headerList.push_back(PS2TBL_KEYBOARDMODEL_NAME);
    headerList.push_back(PS2TBL_MACHINE_NAME);
    headerList.push_back(PS2TBL_MZ_MK_ROW1_NAME);
    headerList.push_back(PS2TBL_MZ_MK_KEY1_NAME);
    headerList.push_back(PS2TBL_MZ_MK_ROW2_NAME);
    headerList.push_back(PS2TBL_MZ_MK_KEY2_NAME);
    headerList.push_back(PS2TBL_MZ_MK_ROW3_NAME);
    headerList.push_back(PS2TBL_MZ_MK_KEY3_NAME);
    headerList.push_back(PS2TBL_MZ_BRK_ROW1_NAME);
    headerList.push_back(PS2TBL_MZ_BRK_KEY1_NAME);
    headerList.push_back(PS2TBL_MZ_BRK_ROW2_NAME);
    headerList.push_back(PS2TBL_MZ_BRK_KEY2_NAME);

    return;
}

// A method to return the Type of data for a given column in the KeyMap table.
//
void MZ2528::getKeyMapTypes(std::vector<std::string>& typeList)
{
    // Add the types.
    //
    typeList.push_back(PS2TBL_PS2KEYCODE_TYPE);
    typeList.push_back(PS2TBL_PS2CTRL_TYPE);
    typeList.push_back(PS2TBL_KEYBOARDMODEL_TYPE);
    typeList.push_back(PS2TBL_MACHINE_TYPE);
    typeList.push_back(PS2TBL_MZ_MK_ROW1_TYPE);
    typeList.push_back(PS2TBL_MZ_MK_KEY1_TYPE);
    typeList.push_back(PS2TBL_MZ_MK_ROW2_TYPE);
    typeList.push_back(PS2TBL_MZ_MK_KEY2_TYPE);
    typeList.push_back(PS2TBL_MZ_MK_ROW3_TYPE);
    typeList.push_back(PS2TBL_MZ_MK_KEY3_TYPE);
    typeList.push_back(PS2TBL_MZ_BRK_ROW1_TYPE);
    typeList.push_back(PS2TBL_MZ_BRK_KEY1_TYPE);
    typeList.push_back(PS2TBL_MZ_BRK_ROW2_TYPE);
    typeList.push_back(PS2TBL_MZ_BRK_KEY2_TYPE);

    return;
}

// Method to return a list of key:value entries for a given keymap column. This represents the
// feature which can be selected and the value it uses. Features can be combined by ORing the values
// together.
bool MZ2528::getKeyMapSelectList(std::vector<std::pair<std::string, int>>& selectList, std::string option)
{
    // Locals.
    //
    bool result = true;

    // Build up a map, depending on the list required, of name to value. This list can then be used
    // by a user front end to select an option based on a name and return its value.
    if(option.compare(PS2TBL_PS2CTRL_TYPE) == 0)
    {
        selectList.push_back(std::make_pair(PS2TBL_PS2CTRL_SEL_SHIFT,     PS2CTRL_SHIFT));
        selectList.push_back(std::make_pair(PS2TBL_PS2CTRL_SEL_CTRL,      PS2CTRL_CTRL));
        selectList.push_back(std::make_pair(PS2TBL_PS2CTRL_SEL_CAPS,      PS2CTRL_CAPS));
        selectList.push_back(std::make_pair(PS2TBL_PS2CTRL_SEL_ALT,       PS2CTRL_ALT));
        selectList.push_back(std::make_pair(PS2TBL_PS2CTRL_SEL_ALTGR,     PS2CTRL_ALTGR));
        selectList.push_back(std::make_pair(PS2TBL_PS2CTRL_SEL_GUI,       PS2CTRL_GUI));
        selectList.push_back(std::make_pair(PS2TBL_PS2CTRL_SEL_FUNC,      PS2CTRL_FUNC));
        selectList.push_back(std::make_pair(PS2TBL_PS2CTRL_SEL_EXACT,     PS2CTRL_EXACT));
    } 
    else if(option.compare(PS2TBL_KEYBOARDMODEL_TYPE) == 0)
    {
        selectList.push_back(std::make_pair(KEYMAP_SEL_STANDARD,          KEYMAP_STANDARD));
        selectList.push_back(std::make_pair(KEYMAP_SEL_UK_WYSE_KB3926,    KEYMAP_UK_WYSE_KB3926));
        selectList.push_back(std::make_pair(KEYMAP_SEL_JAPAN_OADG109,     KEYMAP_JAPAN_OADG109));
        selectList.push_back(std::make_pair(KEYMAP_SEL_JAPAN_SANWA_SKBL1, KEYMAP_JAPAN_SANWA_SKBL1));
        selectList.push_back(std::make_pair(KEYMAP_SEL_NOT_ASSIGNED_4,    KEYMAP_NOT_ASSIGNED_4));
        selectList.push_back(std::make_pair(KEYMAP_SEL_NOT_ASSIGNED_5,    KEYMAP_NOT_ASSIGNED_5));
        selectList.push_back(std::make_pair(KEYMAP_SEL_NOT_ASSIGNED_6,    KEYMAP_NOT_ASSIGNED_6));
        selectList.push_back(std::make_pair(KEYMAP_SEL_UK_PERIBOARD_810,  KEYMAP_UK_PERIBOARD_810));
        selectList.push_back(std::make_pair(KEYMAP_SEL_UK_OMOTON_K8508,   KEYMAP_UK_OMOTON_K8508));
    } 
    else if(option.compare(PS2TBL_MACHINE_TYPE) == 0)
    {
        selectList.push_back(std::make_pair(MZ2528_SEL_ALL,               MZ_ALL));
        selectList.push_back(std::make_pair(MZ2528_SEL_MZ_80B,            MZ_80B));
        selectList.push_back(std::make_pair(MZ2528_SEL_MZ_2000,           MZ_2000));
        selectList.push_back(std::make_pair(MZ2528_SEL_MZ_2500,           MZ_2500));
        selectList.push_back(std::make_pair(MZ2528_SEL_MZ_2800,           MZ_2800));
    }
    else if(option.compare(PS2TBL_MZ_MK_ROW1_TYPE) == 0 || option.compare(PS2TBL_MZ_MK_ROW2_TYPE) == 0 || option.compare(PS2TBL_MZ_MK_ROW3_TYPE) == 0 || option.compare(PS2TBL_MZ_BRK_ROW1_TYPE) == 0 || option.compare(PS2TBL_MZ_BRK_ROW2_TYPE) == 0)
    {
        for(int idx=0; idx < 15; idx++)
        {
            std::string rowStr = "Strobe_Row_" + std::to_string(idx);
            selectList.push_back(std::make_pair(rowStr.c_str(), idx));
        }
        selectList.push_back(std::make_pair("Disabled", 255));
    } else
    {
        // Not found!
        result = false;
    }

    // Return result, false if the option not found, true otherwise.
    //
    return(result);
}


// Method to read the Keymap array, 1 row at a time and return it to the caller.
//
bool MZ2528::getKeyMapData(std::vector<uint32_t>& dataArray, int *row, bool start)
{
    // Locals.
    //
    bool      result = false;

    // If start flag is set, set row to 0.
    if(start == true)
    {
        (*row) = 0;
    }

    // Bound check and if still valid, push data onto the vector.
    if((*row) >= mzControl.kmeRows)
    {
        result = true;
    } else
    {
        dataArray.push_back(mzControl.kme[*row].ps2KeyCode);
        dataArray.push_back(mzControl.kme[*row].ps2Ctrl);
        dataArray.push_back(mzControl.kme[*row].keyboardModel);
        dataArray.push_back(mzControl.kme[*row].machine);
        for(int idx=0; idx < PS2TBL_MZ_MAX_MKROW; idx++)
        {
            dataArray.push_back(mzControl.kme[*row].mkRow[idx]);
            dataArray.push_back(mzControl.kme[*row].mkKey[idx]);
        }
        for(int idx=0; idx < PS2TBL_MZ_MAX_BRKROW; idx++)
        {
            dataArray.push_back(mzControl.kme[*row].brkRow[idx]);
            dataArray.push_back(mzControl.kme[*row].brkKey[idx]);
        }
        (*row) = (*row) + 1;
    }

    // True if no more rows, false if additional rows can be read.
    return(result);
}


// Initialisation routine. Start two threads, one to handle the incoming PS/2 keyboard data and map it, the second to handle the host interface.
void MZ2528::init(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID)
{
    // Initialise the basic components.
    init(hdlNVS, hdlHID);

    // Mode is important for configuring hardware and launching correct interface.
    mzControl.mode2500           = (ifMode == 2500 ? true : false);
  
    // Invoke the prototype init which initialises common variables and devices shared by all subclass. 
    KeyInterface::init(getClassName(__PRETTY_FUNCTION__), hdlNVS, hdlLED, hdlHID, ifMode);

    // Create a task pinned to core 1 which will fulfill the MZ-2500/2800 interface. This task has the highest priority
    // and it will also hold spinlock and manipulate the watchdog to ensure a scan cycle timing can be met. This means 
    // all other tasks running on Core 1 will suspend. The PS/2 controller will be serviced with core 0.
    //
    // Core 1 - MZ Interface
    if(mzControl.mode2500)
    {
        ESP_LOGW(MAINTAG, "Starting mz25if thread...");
        ::xTaskCreatePinnedToCore(&this->mz25Interface, "mz25if", 4096, this, 25, &this->TaskHostIF, 1);
    } else
    {
        ESP_LOGW(MAINTAG, "Starting mz28if thread...");
        ::xTaskCreatePinnedToCore(&this->mz28Interface, "mz28if", 2048, this, (configMAX_PRIORITIES - 1), &this->TaskHostIF, 1);
    }
    vTaskDelay(1500);

    // Core 0 - Application
    // HID Interface handler thread.
    ESP_LOGW(MAINTAG, "Starting hidInterface thread...");
    ::xTaskCreatePinnedToCore(&this->hidInterface, "hidIf", 4096, this, 0, &this->TaskHIDIF, 0);
    vTaskDelay(1500);
}

// Initialisation routine without hardware.
void MZ2528::init(NVS *hdlNVS, HID *hdlHID)
{
    // Initialise control variables.
    mzControl.strobeAll          = 0xFF;
    mzControl.strobeAllAsGPIO    = 0x00000000;
    for(int idx=0; idx < NUMELEM(mzControl.keyMatrix); idx++) { mzControl.keyMatrix[idx] = 0xFF; }
    for(int idx=0; idx < NUMELEM(mzControl.keyMatrixAsGPIO); idx++) { mzControl.keyMatrixAsGPIO[idx] = 0x00000000; }
    mzControl.mode2500           = true;
    mzControl.optionSelect       = false;
    mzControl.keyMapFileName     = mzControl.fsPath.append("/").append(MZ2528IF_KEYMAP_FILE);
    mzControl.kmeRows            = 0;
    mzControl.kme                = NULL;
    mzControl.noKeyPressed       = true;
    mzControl.persistConfig      = false;
    yieldHostInterface           = true;
  
    // Invoke the prototype init which initialises common variables and devices shared by all subclass. 
    KeyInterface::init(getClassName(__PRETTY_FUNCTION__), hdlNVS, hdlHID);

    // Load the keyboard mapping table into memory. If the file doesnt exist, create it.
    loadKeyMap();

    // Retrieve configuration, if it doesnt exist, set defaults.
    //
    if(nvs->retrieveData(getClassName(__PRETTY_FUNCTION__), &this->mzConfig, sizeof(t_mzConfig)) == false)
    {
        ESP_LOGW(MAINTAG, "MZ-2500/MZ-2800 configuration set to default, no valid config in NVS found.");
        mzConfig.params.activeKeyboardMap  = KEYMAP_STANDARD;
        mzConfig.params.activeMachineModel = (mzControl.mode2500 ? MZ_2500 : MZ_2800);

        // Persist the data for next time.
        if(nvs->persistData(getClassName(__PRETTY_FUNCTION__), &this->mzConfig, sizeof(t_mzConfig)) == false)
        {
            ESP_LOGW(MAINTAG, "Persisting Default MZ-2500/MZ-2800 configuration data failed, check NVS setup.\n");
        }
        // Commit data, ensuring values are written to NVS and the mutex is released.
        else if(nvs->commitData() == false)
        {
            ESP_LOGW(MAINTAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
        }
    }
}

// Constructor, basically initialise the Singleton interface and let the threads loose.
MZ2528::MZ2528(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID, const char* fsPath)
{
    // Setup the default path on the underlying filesystem.
    this->mzControl.fsPath = fsPath;

    // Initialise the interface.
    init(ifMode, hdlNVS, hdlLED, hdlHID);
}

// Constructor, initialise the Singleton interface without hardware.
MZ2528::MZ2528(NVS *hdlNVS, HID *hdlHID, const char* fsPath)
{
    // Setup the default path on the underlying filesystem.
    this->mzControl.fsPath = fsPath;

    // Initialise the interface.
    init(hdlNVS, hdlHID);
}

// Constructor, used for version reporting so no hardware is initialised.
MZ2528::MZ2528(void)
{
    return;
}

// Destructor - only ever called when the class is used for version reporting.
MZ2528::~MZ2528(void)
{
    return;
}
