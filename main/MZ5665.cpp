/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            MZ5665.cpp
// Created:         Apr 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     HID (PS/2 or BT keyboard) to Sharp MZ-5600/MZ-6500 series Interface logic.
//                  This source file contains the singleton class containing logic to obtain
//                  PS/2 or BT scan codes, map them into Sharp MZ-5600/MZ-6500 keys and transmit the key
//                  to the MZ-5600/MZ-6500 host.
//
//                  The class uses a modified version of the PS2KeyAdvanced 
//                  https://github.com/techpaul/PS2KeyAdvanced class from Paul Carpenter.
//
//                  The whole application of which this class is a member, uses the Espressif Development
//                  environment with Arduino components.  This is necessary for the PS2KeyAdvanced class, 
//                  which I may in future convert to use esp-idf library calls rather than Arduino.
//
// Credits:         
// Copyright:       (c) 2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Apr 2022 - Initial framework, waiting on arrival of real machine to progress further.
//            v1.01 Jun 2022 - Updates to reflect changes realised in other modules due to addition of
//                             bluetooth and suspend logic due to NVS issues using both cores.
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
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "driver/timer.h"
#include "sys/stat.h"
#include "esp_littlefs.h"
#include "PS2KeyAdvanced.h"
#include "sdkconfig.h"
#include "MZ5665.h"

// Tag for ESP main application logging.
#define                         MAINTAG  "mz5665key"

// FreeRTOS Queue handle to pass messages from the HID Keyboard Mapper into the MZ5665 transmission logic.
static QueueHandle_t            xmitQueue;

// MZ-5600/MZ-6500 Protocol
// ------------------------
//


// Function to push a keycode onto the key queue ready for transmission.
//
void MZ5665::pushKeyToQueue(uint32_t key)
{
    // Locals.
    t_xmitQueueMessage  xmitMsg;
    #define             PUSHKEYTAG "pushKeyToQueue"

    xmitMsg.keyCode = key;
    if( xQueueSend(xmitQueue, (void *)&xmitMsg, 10) != pdPASS)
    {
        ESP_LOGW(PUSHKEYTAG, "Failed to put scancode:%04x into xmitQueue", key);
    }
    return;
}

// Method to realise the MZ-5600/MZ-6500 4 wire serial protocol in order to transmit key presses to the
// MZ-5600/MZ-6500.
// This method uses Core 1 and it will hold it in a spinlock as necessary to ensure accurate timing.
// A key is passed into the method via the FreeRTOS Queue handle xmitQueue.
IRAM_ATTR void MZ5665::mzInterface( void * pvParameters )
{
    // Locals.
    //t_xmitQueueMessage  rcvMsg;

    // Mask values declared as variables, let the optimiser decide wether they are constants or placed in-memory.
//    uint32_t            X1DATA_MASK  = (1 << CONFIG_HOST_KDO0);
    //uint64_t            delayTimer = 0LL;
    //uint64_t            curTime    = 0LL;
    //bool                bitStart = true;
    //uint32_t            bitCount = 0;
    //enum XMITSTATE {
    //                    FSM_IDLE        = 0,
    //                    FSM_STARTXMIT   = 1,
    //                    FSM_HEADER      = 2,
    //                    FSM_START       = 3,
    //                    FSM_DATA        = 4,
    //                    FSM_STOP        = 5,
    //                    FSM_ENDXMIT     = 6
    //}                   state = FSM_IDLE;

    // Retrieve pointer to object in order to access data.
    MZ5665* pThis = (MZ5665*)pvParameters;

    // Initialise the MUTEX which prevents this core from being released to other tasks.
    pThis->mzMutex = portMUX_INITIALIZER_UNLOCKED;

    // Initial delay needed because the xQueue will assert probably on a suspended task ALL if delay not inserted!
    vTaskDelay(1000);

    // Sign on.
    ESP_LOGW(MAINTAG, "Starting MZ-6500 thread.");

//    // X1 data out default state is high.
//    GPIO.out_w1ts = X1DATA_MASK;
//
//    // Configure a timer to be used for X1 protocol spacing with 1uS resolution. The default clock source is the APB running at 80MHz.
//    timer_config_t timerConfig = {
//        .alarm_en    = TIMER_ALARM_DIS,            // No alarm, were not using interrupts as we are in a dedicated thread.
//        .counter_en  = TIMER_PAUSE,                // Timer paused until required.
//        .intr_type   = TIMER_INTR_LEVEL,           // No interrupts used.
//        .counter_dir = TIMER_COUNT_UP,             // Timing a fixed period.
//        .auto_reload = TIMER_AUTORELOAD_DIS,       // No need for auto reload, fixed time period.
//        .divider     = 80                          // 1Mhz operation giving 1uS resolution.
//    };
//    ESP_ERROR_CHECK(timer_init(TIMER_GROUP_0, TIMER_0, &timerConfig));
//    ESP_ERROR_CHECK(timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0));
//
//    // Permanent loop, wait for an incoming message on the key to send queue, read it then transmit to the X1, repeat!
//    for(;;)
//    {
//        // Get the current timer value, only run the FSM when the timer is idle.
//        timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &curTime);
//        if(curTime >= delayTimer)
//        {
//            // Ensure the timer is stopped.
//            timer_pause(TIMER_GROUP_0, TIMER_0);
//            delayTimer = 0LL;
//
//            // Finite state machine to retrieve a key for transmission then serialise it according to the X1 protocol.
//            switch(state)
//            {
//                case FSM_IDLE:
//                    // Yield if the suspend flag is set.
//                    pThis->yield(0);
//
//                    // Check stack space, report if it is getting low.
//                    if(uxTaskGetStackHighWaterMark(NULL) < 1024)
//                    {
//                        ESP_LOGW(MAINTAG, "THREAD STACK SPACE(%d)\n",uxTaskGetStackHighWaterMark(NULL));
//                    }
//
//                    // If a new message arrives, start the serialiser to send it to the X1.
//                    if(xQueueReceive(xmitQueue, (void *)&rcvMsg, 0) == pdTRUE)
//                    {
//                        ESP_LOGW(MAINTAG, "Received:%08x, %d", rcvMsg.keyCode, rcvMsg.modeB);
//                        state = FSM_STARTXMIT; 
//                   
//                        // Create, initialise and hold a spinlock so the current core is bound to this one method.
//                        portENTER_CRITICAL(&pThis->mzMutex);
//                    }
//                    break;
//
//                case FSM_STARTXMIT:
//                    // Ensure all variables and states correct before entering serialisation.
//                    bitStart = true;
//                    GPIO.out_w1ts = X1DATA_MASK;
//                    state = FSM_HEADER;
//                    if(rcvMsg.modeB)
//                        bitCount = 24;
//                    else
//                        bitCount = 16;
//                    break;
//
//                case FSM_HEADER:
//                    if(bitStart)
//                    {
//                        // Send out the header by bringing X1DATA low for 1000us then high for 700uS.
//                        GPIO.out_w1tc = X1DATA_MASK;
//                        delayTimer = pThis->mzCtrl.modeB ? 400LL : 1000LL;
//                    } else
//                    {
//                        // Bring high for 700us.
//                        GPIO.out_w1ts = X1DATA_MASK;
//                        delayTimer = pThis->mzCtrl.modeB ? 200LL : 700LL;
//                        state = FSM_DATA;  // Jump past the Start Bit, I think the header is the actual start bit as there is an error in the X1 Center specs.
//                    }
//                    bitStart = !bitStart;
//                    break;
//
//                // The original X1 Center specification shows a start bit but this doesnt seem necessary, in fact it is interpreted as a data bit, hence the
//                // FSM jumps this state.
//                case FSM_START:
//                    if(bitStart)
//                    {
//                        // Send out the start bit by bringing X1DATA low for 250us then high for 750uS.
//                        GPIO.out_w1tc = X1DATA_MASK;
//                        delayTimer = pThis->mzCtrl.modeB ? 250LL : 250LL;
//                    } else
//                    {
//                        // Bring high for 750us.
//                        GPIO.out_w1ts = X1DATA_MASK;
//                        delayTimer = pThis->mzCtrl.modeB ? 250LL : 750LL;
//                        state = FSM_DATA;
//                    }
//                    bitStart = !bitStart;
//                    break;
//
//                case FSM_DATA:
//                    if(bitCount > 0)
//                    {
//                        if(bitStart)
//                        {
//                            // Send out the data bit by bringing X1DATA low for 250us then high for 1750uS when bit = 1 else 750uS when bit = 0.
//                            GPIO.out_w1tc = X1DATA_MASK;
//                            delayTimer = 250LL;
//                            delayTimer = pThis->mzCtrl.modeB ? 250LL : 250LL;
//                        } else
//                        {
//                            // Bring X1DATA high...
//                            GPIO.out_w1ts = X1DATA_MASK;
//
//                            // ... Mode A 1750us as bit = 1, mode B 750uS.
//                            if((rcvMsg.modeB && rcvMsg.keyCode & 0x800000) || (!rcvMsg.modeB && rcvMsg.keyCode & 0x8000))
//                            {
//                                delayTimer = pThis->mzCtrl.modeB ? 750LL : 1750LL;
//                            } else
//                            // ... Mode A 750us as bit = 0, mode B 250uS.
//                            {
//                                delayTimer = pThis->mzCtrl.modeB ? 250LL : 750LL;
//                            }
//                            rcvMsg.keyCode = (rcvMsg.keyCode << 1);
//                            bitCount--;
//                        }
//                        bitStart = !bitStart;
//                    } else
//                    {
//                        state = FSM_STOP;
//                    }
//                    break;
//
//                case FSM_STOP:
//                    if(bitStart)
//                    {
//                        // Send out the stop bit, same in Mode A and B, by bringing X1DATA low for 250us then high for 250uS.
//                        GPIO.out_w1tc = X1DATA_MASK;
//                        delayTimer = 250LL;
//                        delayTimer = pThis->mzCtrl.modeB ? 250LL : 250LL;
//                    } else
//                    {
//                        // Bring high for 250us.
//                        GPIO.out_w1ts = X1DATA_MASK;
//                        delayTimer = pThis->mzCtrl.modeB ? 250LL : 250LL;
//                        state = FSM_ENDXMIT;
//                    }
//                    bitStart = !bitStart;
//                    break;
//
//                case FSM_ENDXMIT:
//                    // End of critical timing loop, release the core.
//                    portEXIT_CRITICAL(&pThis->mzMutex);
//                    state = FSM_IDLE;
//                    break;
//
//            }
//
//            // If a new delay is requested, set the value into the timer and start.
//            if(delayTimer > 0LL)
//            {
//                timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0LL);
//                timer_start(TIMER_GROUP_0, TIMER_0);
//            }
//        }
//
//        // Logic to feed the watchdog if needed. Watchdog disabled in menuconfig but if enabled this will need to be used.
//        //TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
//        //TIMERG0.wdt_feed=1;                       // feed dog
//        //TIMERG0.wdt_wprotect=0;                   // write protect
//        //TIMERG1.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
//        //TIMERG1.wdt_feed=1;                       // feed dog
//        //TIMERG1.wdt_wprotect=0;                   // write protect
//    }
}

// Method to select keyboard configuration options. When a key sequence is pressed, ie. SHIFT+CTRL+ESC then the fourth simultaneous key is the required option and given to this 
// method to act on. Options can be machine model, keyboard map etc.
//
void MZ5665::selectOption(uint8_t optionCode)
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

//        // Select the model of the host to enable specific mappings.
//        case PS2_KEY_END:
//            this->mzConfig.params.activeMachineModel = X1_ORIG;
//            break;
//        case PS2_KEY_DN_ARROW:
//            this->mzConfig.params.activeMachineModel = X1_TURBO;
//            break;
//        case PS2_KEY_PGDN:
//            this->mzConfig.params.activeMachineModel = X1_TURBOZ;
//            break;
//        case PS2_KEY_INSERT:
//            this->mzConfig.params.activeMachineModel = X1_ALL;
//            break;
//
//        // Switch to keyboard Mode A. This mode is not persisted.
//        case PS2_KEY_HOME:
//            updated = false;
//            this->mzCtrl.modeB = false;
//            break;
//        // Switch to keyboard Mode B. This mode is not persisted.
//        case PS2_KEY_PGUP:
//            updated = false;
//            this->mzCtrl.modeB = true;
//            break;
//
        // Unknown option so ignore.
        default:
            updated = false;
            break;
    }

    // If an update was made, persist it for power cycles.
    //
    if(updated)
    {
        if(this->nvs->persistData(this->getClassName(__PRETTY_FUNCTION__), &this->mzConfig, sizeof(t_mzConfig)) == false)
        {
            ESP_LOGW(SELOPTTAG, "Persisting MZ-6500 configuration data failed, updates will not persist in future power cycles.");
            led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_10, 200, 1000L, 0L);
        } else
        // Few other updates so make a commit here to ensure data is flushed and written.
        if(this->nvs->commitData() == false)
        {
            ESP_LOGW(SELOPTTAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
            led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_10, 200, 500L, 0L);
        }
    }

    return;
}

// Method to take a PS/2 key and control data and map it into an MZ-6500 key and control equivalent, updating state values accordingly (ie. CAPS).
// A mapping table is used which maps a key and state values into an MZ-6500 key and control values, the emphasis being on readability and easy configuration
// as opposed to concatenated byte tables.
//
uint32_t MZ5665::mapKey(uint16_t scanCode)
{
    // Locals.
    uint32_t  idx;
    uint8_t   keyCode = (scanCode & 0xFF);
    bool      mapped = false;
    bool      matchExact = false;
    uint32_t  mappedKey = 0x00000000;
    #define   MAPKEYTAG "mapKey"

    // Intercept control keys and set state variables.
    //
    //
    if(scanCode & PS2_BREAK)
    {
 //       if((keyCode == PS2_KEY_L_SHIFT || keyCode == PS2_KEY_R_SHIFT)  && (scanCode & PS2_SHIFT) == 0) { mapped=true; this->mzCtrl.keyCtrl |= X1_CTRL_SHIFT; }
 //       if((keyCode == PS2_KEY_L_CTRL  || keyCode == PS2_KEY_R_CTRL)   && (scanCode & PS2_CTRL) == 0)  { mapped=true; this->mzCtrl.keyCtrl |= X1_CTRL_CTRL; }

        // Any break key clears the option select flag.
        this->mzCtrl.optionSelect = false;

        // Clear any feature LED blinking.
        led->setLEDMode(LED::LED_MODE_OFF, LED::LED_DUTY_CYCLE_OFF, 0, 0L, 0L);
    } else
    {
 //       if((keyCode == PS2_KEY_L_SHIFT || keyCode == PS2_KEY_R_SHIFT)  && (scanCode & PS2_SHIFT)) { mapped=true; this->mzCtrl.keyCtrl &= ~X1_CTRL_SHIFT; }
 //       if((keyCode == PS2_KEY_L_CTRL  || keyCode == PS2_KEY_R_CTRL)   && (scanCode & PS2_CTRL))  { mapped=true; this->mzCtrl.keyCtrl &= ~X1_CTRL_CTRL; }
 //       if(keyCode == PS2_KEY_L_ALT)     { mapped = true; this->mzCtrl.keyCtrl ^= X1_CTRL_KANA; }
 //       if(keyCode == PS2_KEY_R_ALT)     { mapped = true; this->mzCtrl.keyCtrl ^= X1_CTRL_GRAPH; }
 //       if(keyCode == PS2_KEY_CAPS)      { mapped = true; this->mzCtrl.keyCtrl ^= X1_CTRL_CAPS; }
        // Special mapping to allow selection of keyboard options. If the user presses CTRL+SHIFT+ESC then a flag becomes active and should a fourth key be pressed before a BREAK then the fourth key is taken as an option key and processed accordingly.
        if(this->mzCtrl.optionSelect == true) { mapped = true; this->mzCtrl.optionSelect = false; selectOption(keyCode); }
        if(keyCode == PS2_KEY_ESC && (scanCode & PS2_CTRL) && (scanCode & PS2_SHIFT)) { mapped = true; this->mzCtrl.optionSelect = true; }
        if(this->mzCtrl.optionSelect == true && keyCode != PS2_KEY_ESC)
        {
            mapped = true;
            this->mzCtrl.optionSelect = false;
            selectOption(keyCode);
        }
        if(keyCode == PS2_KEY_ESC && (scanCode & PS2_CTRL) && (scanCode & PS2_SHIFT) && this->mzCtrl.optionSelect == false)
        {
            // Prime flag ready for fourth option key and start LED blinking periodically.
            mapped = true;
            this->mzCtrl.optionSelect = true;
            led->setLEDMode(LED::LED_MODE_BLINK, LED::LED_DUTY_CYCLE_50, 1, 500L, 500L);
        }        
    }

    // If the key already mapped, ie. due to control signals, send the update as <CTRL><0x00> so the MZ5665 knows the current control signal state.
    if(mapped == true)
    {
        ESP_LOGW(MAPKEYTAG, "Mapped special key:%02x\n", this->mzCtrl.keyCtrl);
        mappedKey = (this->mzCtrl.keyCtrl << 8) | 0x00;
    } else
    {
        // Loop through the entire conversion table to find a match on this key, if found map to MZ5665 equivalent.
        // switch matrix.
        //
        for(idx=0, mapped=false, matchExact=false; idx < mzCtrl.kmeRows && (mapped == false || (mapped == true && matchExact == false)); idx++)
        {
            // Match key code? Make sure the current machine and keymap match as well.
            if(mzCtrl.kme[idx].ps2KeyCode == (uint8_t)(scanCode&0xFF) && ((mzCtrl.kme[idx].machine == MZ5665_ALL) || ((mzCtrl.kme[idx].machine & mzConfig.params.activeMachineModel) != 0)) && ((mzCtrl.kme[idx].keyboardModel & mzConfig.params.activeKeyboardMap) != 0))
            {
                // If CAPS lock is set in the table and in the scanCode, invert SHIFT so we send the correct value.
                if((scanCode & PS2_CAPS) && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_CAPS) != 0)
                {
                    scanCode ^= PS2_SHIFT;
                }

                // Match Raw, Shift, Function, Control, ALT or ALT-Gr?
                if( (((mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_SHIFT) == 0) && ((mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_CTRL) == 0) && ((mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_KANA)  == 0) && ((mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_GRAPH) == 0) && ((mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_GUI)   == 0) && ((mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_FUNC)  == 0)) ||
                    ((scanCode & PS2_SHIFT)                         && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_SHIFT) != 0) ||
                    ((scanCode & PS2_CTRL)                          && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_CTRL)  != 0) ||
              //      ((this->mzCtrl.keyCtrl & X1_CTRL_KANA) == 0  && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_KANA)  != 0) ||
              //      ((this->mzCtrl.keyCtrl & X1_CTRL_GRAPH) == 0 && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_GRAPH) != 0) ||
                    ((scanCode & PS2_GUI)                           && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_GUI)   != 0) || 
                    ((scanCode & PS2_FUNCTION)                      && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_FUNC)  != 0) )
                {
                    
                    // Exact entry match, data + control key? On an exact match we only process the first key. On a data only match we fall through to include additional data and control key matches to allow for un-mapped key combinations, ie. Japanese characters.
                    matchExact = (((scanCode & PS2_SHIFT)                         && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_SHIFT) != 0) || ((scanCode & PS2_SHIFT) == 0               && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_SHIFT) == 0)) &&
                                 (((scanCode & PS2_CTRL)                          && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_CTRL)  != 0) || ((scanCode & PS2_CTRL) == 0                && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_CTRL)  == 0)) &&
                       //          (((this->mzCtrl.keyCtrl & X1_CTRL_KANA) == 0  && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_KANA)  != 0) || ((this->mzCtrl.keyCtrl & X1_CTRL_KANA)  && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_KANA)  == 0)) &&
                       //          (((this->mzCtrl.keyCtrl & X1_CTRL_GRAPH) == 0 && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_GRAPH) != 0) || ((this->mzCtrl.keyCtrl & X1_CTRL_GRAPH) && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_GRAPH) == 0)) &&
                                 (((scanCode & PS2_GUI)                           && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_GUI)   != 0) || ((scanCode & PS2_GUI) == 0                 && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_GUI)   == 0)) &&
                                 (((scanCode & PS2_FUNCTION)                      && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_FUNC)  != 0) || ((scanCode & PS2_FUNCTION) == 0            && (mzCtrl.kme[idx].ps2Ctrl & PS2CTRL_FUNC)  == 0))
                               ? true : false;
    
                    // RELEASE (PS2_BREAK == 1) or PRESS?
                    if((scanCode & PS2_BREAK))
                    {
                        // Special case for the PAUSE / BREAK key. The underlying logic has been modified to send a BREAK key event immediately 
                        // after a PAUSE make, this is necessary as the Sharp machines require SHIFT (pause) BREAK so the PS/2 CTRL+BREAK wont
                        // work (unless logic is added to insert a SHIFT, pause, add BREAK). The solution was to generate a BREAK event
                        // when SHIFT+PAUSE is pressed.
                        if(keyCode == PS2_KEY_PAUSE)
                        {
                            vTaskDelay(100);
                        }

                        // Mode A sends a release with 0x00.
                //        if(this->mzCtrl.modeB == false)
                //        {
                //            mappedKey = (0xFF << 8) | 0x00;
                //            mapped = true;
                //          //  vTaskDelay(300);
                //        } else
                //        if(this->mzCtrl.modeB == true)
                //        {
                            // Clear only the bits relevant to the released key.
               //             mappedKey &= ((mzCtrl.kme[idx].x1Ctrl << 16) | (mzCtrl.kme[idx].x1Key2 << 8) | mzCtrl.kme[idx].x1Key);
                //        }
                    } else
                    {
                        // Mode A return the key in the table, mode B OR the key to build up a final map.
              //          if(this->mzCtrl.modeB == false)
               //             mappedKey = ((mzCtrl.kme[idx].x1Ctrl & this->mzCtrl.keyCtrl) << 8) | mzCtrl.kme[idx].x1Key;
               //         else
               //             mappedKey |= ((mzCtrl.kme[idx].x1Ctrl << 16) | (mzCtrl.kme[idx].x1Key2 << 8) | mzCtrl.kme[idx].x1Key);
               //         mapped = true;
                    }
                }
            }
        }
    }
    return(mappedKey);
}

// Primary HID thread, running on Core 0.
// This thread is responsible for receiving HID (PS/2 or BT) keyboard scan codes and mapping them to Sharp MZ5665 equivalent keys, updating state flags as needed.
// The HID data is received via interrupt. The data to be sent to the MZ5665 is pushed onto a FIFO queue.
//
IRAM_ATTR void MZ5665::hidInterface( void * pvParameters )
{
    // Locals.
    uint16_t            scanCode      = 0x0000;
    uint32_t            mzKey         = 0x00000000;

    // Map the instantiating object so we can access its methods and data.
    MZ5665* pThis = (MZ5665*)pvParameters;

    // Thread never exits, just polls the keyboard and updates the matrix.
    while(1)
    { 
        // Check stack space, report if it is getting low.
        if(uxTaskGetStackHighWaterMark(NULL) < 1024)
        {
            ESP_LOGW(MAINTAG, "THREAD STACK SPACE(%d)\n",uxTaskGetStackHighWaterMark(NULL));
        }

        // Check for HID keyboard scan codes.
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

            // Map the PS/2 key to an MZ5665 CTRL + KEY
            mzKey = pThis->mapKey(scanCode);
            if(mzKey != 0L) { pThis->pushKeyToQueue(mzKey); }

            // Toggle LED to indicate data flow.
            if((scanCode & PS2_BREAK) == 0)
                pThis->led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_10, 1, 100L, 0L);
        }

        // Yield if the suspend flag is set.
        pThis->yield(10);
    }
}

// A method to load the keyboard mapping table into memory for use in the interface mapping logic. If no persistence file exists or an error reading persistence occurs, the keymap 
// uses the internal static default. If no persistence file exists and attempt is made to create it with a copy of the inbuilt static map so that future operations all
// work with persistence such that modifications can be made.
//
bool MZ5665::loadKeyMap(void)
{
    // Locals.
    //
    bool        result = false;
    int         fileRows = 0;
    struct stat keyMapFileNameStat;

    // See if the file exists, if it does, get size so we can compute number of mapping rows.
    if(stat(mzCtrl.keyMapFileName.c_str(), &keyMapFileNameStat) == -1)
    {
        ESP_LOGW(MAINTAG, "No keymap file, using inbuilt definitions.");
    } else
    {
        // Get number of rows in the file.
        fileRows = keyMapFileNameStat.st_size/sizeof(t_keyMapEntry);

        // Subsequent reloads, delete memory prior to building new map, primarily to conserve precious resources rather than trying the memory allocation trying to realloc and then having to copy.
        if(mzCtrl.kme != NULL && mzCtrl.kme != PS2toMZ5665.kme)
        {
            delete mzCtrl.kme;
            mzCtrl.kme = NULL;
        }

        // Allocate memory for the new keymap table.
        mzCtrl.kme = new t_keyMapEntry[fileRows];
        if(mzCtrl.kme == NULL)
        {
            ESP_LOGW(MAINTAG, "Failed to allocate memory for keyboard map, fallback to inbuilt!");
        } else
        {
            // Open the keymap extension file for binary reading to add data to our map table.
            std::fstream keyFileIn(mzCtrl.keyMapFileName.c_str(), std::ios::in | std::ios::binary);

            int idx=0;
            while(keyFileIn.good())
            {
                keyFileIn.read((char *)&mzCtrl.kme[idx], sizeof(t_keyMapEntry));
                if(keyFileIn.good())
                {
                    idx++;
                }
            }
            // Any errors, we wind back and use the inbuilt mapping table.
            if(keyFileIn.bad())
            {
                keyFileIn.close();
                ESP_LOGW(MAINTAG, "Failed to read data from keymap extension file:%s, fallback to inbuilt!", mzCtrl.keyMapFileName.c_str());
            } else
            {
                // No longer need the file.
                keyFileIn.close();
               
                // Max rows in the KME table.
                mzCtrl.kmeRows = fileRows;

                // Good to go, map ready for use with the interface.
                result = true;
            }
        }
    }

    // Any failures, free up memory and use the inbuilt mapping table.
    if(result == false)
    {
        if(mzCtrl.kme != NULL && mzCtrl.kme != PS2toMZ5665.kme)
        {
            delete mzCtrl.kme;
            mzCtrl.kme = NULL;
        }
     
        // No point allocating memory if no extensions exist or an error occurs, just point to the static table.
        mzCtrl.kme = PS2toMZ5665.kme;
        mzCtrl.kmeRows = PS2TBL_MZ5665_MAXROWS;

        // Persist the data so that next load comes from file.
        saveKeyMap();
    }

    // Return code. Either memory map was successfully loaded, true or failed, false.
    return(result);
}

// Method to save the current keymap out to an extension file.
//
bool MZ5665::saveKeyMap(void)
{
    // Locals.
    //
    bool        result = false;
    int         idx = 0;

    // Has a map been defined? Cannot save unless loadKeyMap has been called which sets mzCtrl.kme to point to the internal keymap or a new memory resident map.
    //
    if(mzCtrl.kme == NULL)
    {
        ESP_LOGW(MAINTAG, "KeyMap hasnt yet been defined, need to call loadKeyMap.");
    } else
    {
        // Open file for binary writing, trunc specified to clear out the file, we arent appending.
        std::fstream keyFileOut(mzCtrl.keyMapFileName.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);

        // Loop whilst no errors and data rows still not written.
        while(keyFileOut.good() && idx < mzCtrl.kmeRows)
        {
            keyFileOut.write((char *)&mzCtrl.kme[idx], sizeof(t_keyMapEntry));
            idx++;
        }
        if(keyFileOut.bad())
        {
            ESP_LOGW(MAINTAG, "Failed to write data from the keymap to file:%s, deleting as state is unknown!", mzCtrl.keyMapFileName.c_str());
            keyFileOut.close();
            std::remove(mzCtrl.keyMapFileName.c_str());
        } else
        {
            // Success.
            keyFileOut.close();
            result = true;
        }
    }
   
    // Return code. Either memory map was successfully saved, true or failed, false.
    return(result);
}

// Public method to open a keymap file for data upload.
// This method opens the file and makes any validation checks as necessary.
//
bool MZ5665::createKeyMapFile(std::fstream &outFile)
{
    // Locals.
    //
    bool           result = true;
    std::string    fileName;

    // Attempt to open a temporary keymap file for writing.
    //
    fileName = mzCtrl.keyMapFileName;
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
bool MZ5665::storeDataToKeyMapFile(std::fstream &outFile, char *data, int size)
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
bool MZ5665::storeDataToKeyMapFile(std::fstream & outFile, std::vector<uint32_t>& dataArray)
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
bool MZ5665::closeAndCommitKeyMapFile(std::fstream &outFile, bool cleanupOnly)
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
            fileName = mzCtrl.keyMapFileName;
            replaceExt(fileName, "bak");
            // Remove old backup file. Dont worry if it is not there!
            std::remove(fileName.c_str());
            replaceExt(fileName, "tmp");
            // Rename new file to active.
            if(std::rename(fileName.c_str(), mzCtrl.keyMapFileName.c_str()) != 0)
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
void MZ5665::getKeyMapHeaders(std::vector<std::string>& headerList)
{
    // Add the names.
    //
    headerList.push_back(PS2TBL_PS2KEYCODE_NAME);
    headerList.push_back(PS2TBL_PS2CTRL_NAME);
    headerList.push_back(PS2TBL_KEYBOARDMODEL_NAME);
    headerList.push_back(PS2TBL_MACHINE_NAME);
//    headerList.push_back(PS2TBL_X1MODE_NAME);
//    headerList.push_back(PS2TBL_X1KEYCODE_NAME);
//    headerList.push_back(PS2TBL_X1KEYCODE_BYTE2_NAME);
//    headerList.push_back(PS2TBL_X1_CTRL_NAME);

    return;
}

// A method to return the Type of data for a given column in the KeyMap table.
//
void MZ5665::getKeyMapTypes(std::vector<std::string>& typeList)
{
    // Add the types.
    //
    typeList.push_back(PS2TBL_PS2KEYCODE_TYPE);
    typeList.push_back(PS2TBL_PS2CTRL_TYPE);
    typeList.push_back(PS2TBL_KEYBOARDMODEL_TYPE);
    typeList.push_back(PS2TBL_MACHINE_TYPE);
//    typeList.push_back(PS2TBL_X1MODE_TYPE);
//    typeList.push_back(PS2TBL_X1KEYCODE_TYPE);
//    typeList.push_back(PS2TBL_X1KEYCODE_BYTE2_TYPE);
//    typeList.push_back(PS2TBL_X1CTRL_TYPE);

    return;
}

// Method to return a list of key:value entries for a given keymap column. This represents the
// feature which can be selected and the value it uses. Features can be combined by ORing the values
// together.
bool MZ5665::getKeyMapSelectList(std::vector<std::pair<std::string, int>>& selectList, std::string option)
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
        selectList.push_back(std::make_pair(PS2TBL_PS2CTRL_SEL_KANA,      PS2CTRL_KANA));
        selectList.push_back(std::make_pair(PS2TBL_PS2CTRL_SEL_GRAPH,     PS2CTRL_GRAPH));
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
        selectList.push_back(std::make_pair(MZ5665_SEL_ALL,               MZ5665_ALL));
    } 
    else if(option.compare(PS2TBL_MZ5665_CTRL_TYPE) == 0)
    {
        selectList.push_back(std::make_pair(MZ5665_CTRL_SEL_GRAPH,        MZ5665_CTRL_GRAPH));
        selectList.push_back(std::make_pair(MZ5665_CTRL_SEL_CAPS,         MZ5665_CTRL_CAPS));
        selectList.push_back(std::make_pair(MZ5665_CTRL_SEL_KANA,         MZ5665_CTRL_KANA));
        selectList.push_back(std::make_pair(MZ5665_CTRL_SEL_SHIFT,        MZ5665_CTRL_SHIFT));
        selectList.push_back(std::make_pair(MZ5665_CTRL_SEL_CTRL,         MZ5665_CTRL_CTRL));
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
bool MZ5665::getKeyMapData(std::vector<uint32_t>& dataArray, int *row, bool start)
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
    if((*row) >= mzCtrl.kmeRows)
    {
        result = true;
    } else
    {
        dataArray.push_back(mzCtrl.kme[*row].ps2KeyCode);
        dataArray.push_back(mzCtrl.kme[*row].ps2Ctrl);
        dataArray.push_back(mzCtrl.kme[*row].keyboardModel);
        dataArray.push_back(mzCtrl.kme[*row].machine);
   //     dataArray.push_back(mzCtrl.kme[*row].x1Mode);
   //     dataArray.push_back(mzCtrl.kme[*row].x1Key);
   //     dataArray.push_back(mzCtrl.kme[*row].x1Key2);
   //     dataArray.push_back(mzCtrl.kme[*row].x1Ctrl);
        (*row) = (*row) + 1;
    }

    // True if no more rows, false if additional rows can be read.
    return(result);
}


// Initialisation routine. Start two threads, one to handle the incoming PS/2 keyboard data and map it, the second to handle the host interface.
void MZ5665::init(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID)
{
    // Basic initialisation.
    init(hdlNVS, hdlHID);

    // Invoke the prototype init which initialises common variables and devices shared by all subclass. 
    KeyInterface::init(getClassName(__PRETTY_FUNCTION__), hdlNVS, hdlLED, hdlHID, ifMode);

    // Create a task pinned to core 1 which will fulfill the Sharp MZ-6500 interface. This task has the highest priority
    // and it will also hold spinlock and manipulate the watchdog to ensure a scan cycle timing can be met. This means 
    // all other tasks running on Core 1 will suspend as needed. The PS/2 controller will be serviced with core 0.
    //
    // Core 1 - MZ-6500 Interface
    ESP_LOGW(MAINTAG, "Starting mz5600/mz6500 if thread...");
    ::xTaskCreatePinnedToCore(&this->mzInterface, "mzif", 4096, this, 25, &this->TaskHostIF, 1);
    vTaskDelay(500);

    // Core 0 - Application
    // HID Interface handler thread.
    ESP_LOGW(MAINTAG, "Starting hidIf thread...");
    ::xTaskCreatePinnedToCore(&this->hidInterface, "hidIf", 8192, this, 22, &this->TaskHIDIF, 0);

    // Create queue for buffering incoming keys prior to transmitting to the MZ-6500.
    xmitQueue = xQueueCreate(MAX_MZ5665_XMIT_KEY_BUF, sizeof(t_xmitQueueMessage));
}  

// Initialisation routine without hardware.
void MZ5665::init(NVS *hdlNVS, HID *hdlHID)
{
    // Initialise control variables.
    this->mzCtrl.keyCtrl      = 0xFF;     // Negative logic, 0 - active, 1 = inactive.
    mzCtrl.optionSelect       = false;
    mzCtrl.keyMapFileName     = mzCtrl.fsPath.append("/").append(MZ5665IF_KEYMAP_FILE);
    mzCtrl.kmeRows            = 0;
    mzCtrl.kme                = NULL;

    // Invoke the prototype init which initialises common variables and devices shared by all subclass. 
    KeyInterface::init(getClassName(__PRETTY_FUNCTION__), hdlNVS, hdlHID);

    // Load the keyboard mapping table into memory. If the file doesnt exist, create it.
    loadKeyMap();

    // Retrieve configuration, if it doesnt exist, set defaults.
    //
    if(nvs->retrieveData(getClassName(__PRETTY_FUNCTION__), &this->mzConfig, sizeof(t_mzConfig)) == false)
    {
        ESP_LOGW(MAINTAG, "MZ5665 configuration set to default, no valid config in NVS found.");
        mzConfig.params.activeKeyboardMap   = KEYMAP_STANDARD;
        mzConfig.params.activeMachineModel  = MZ5665_ALL;

        // Persist the data for next time.
        if(nvs->persistData(getClassName(__PRETTY_FUNCTION__), &this->mzConfig, sizeof(t_mzConfig)) == false)
        {
            ESP_LOGW(MAINTAG, "Persisting Default MZ5665 configuration data failed, check NVS setup.\n");
        }
        // Few other updates so make a commit here to ensure data is flushed and written.
        else if(this->nvs->commitData() == false)
        {
            ESP_LOGW(SELOPTTAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
        }
    }
}  

// Constructor, basically initialise the Singleton interface and let the threads loose.
MZ5665::MZ5665(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID, const char* fsPath)
{
    // Setup the default path on the underlying filesystem.
    this->mzCtrl.fsPath = fsPath;

    // Initialise the interface.
    init(ifMode, hdlNVS, hdlLED, hdlHID);
}

// Constructor, basic initialisation without hardware.
MZ5665::MZ5665(NVS *hdlNVS, HID *hdlHID, const char* fsPath)
{
    // Setup the default path on the underlying filesystem.
    this->mzCtrl.fsPath = fsPath;

    // Initialise the interface.
    init(hdlNVS, hdlHID);
}

// Constructor, used for version reporting so no hardware is initialised.
MZ5665::MZ5665(void)
{
    return;
}

// Destructor - only ever called when the class is used for version reporting.
MZ5665::~MZ5665(void)
{
    return;
}
