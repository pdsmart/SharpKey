/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            X68K.cpp
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     HID (PS/2 or BT Keyboard) to Sharp X68000 Interface logic.
//                  This source file contains the singleton class containing logic to obtain
//                  PS/2 or BT scan codes, map them into Sharp X68000 keys and transmit the key to the X68000
//                  host.
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
// History:         Mar 2022 - Initial write.
//            v1.01 May 2022 - Initial release version.
//            v1.02 Jun 2022 - Updates to reflect changes realised in other modules due to addition of
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
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "driver/timer.h"
#include "sys/stat.h"
#include "esp_littlefs.h"
#include "PS2KeyAdvanced.h"
#include "sdkconfig.h"
#include "X68K.h"

// Tag for ESP main application logging.
#define                         MAINTAG  "x68kkey"

// FreeRTOS Queue handles to pass messages from the HID Keyboard Mapper into the X68000 transmission logic
// and from the X68000 reception logic for later processing.
static QueueHandle_t            xmitQueue;
static QueueHandle_t            rcvQueue;

// X68000 Protocol
// ---------------
//
// The X68000 uses an asynchronous serial protocol over two wires (RXD/TXD) with a READY state signal. 
//
// Protocol:
//     Idle State (RXD/TXD) = High.
//     <START BIT 0 (low)><DATABIT 0><DATABIT 1><DATABIT 2><DATABIT 3><DATABIT 4><DATABIT 5><DATABIT 6><DATaBIT 7><STOP BIT 1 (high)>
//
//     The READY signal is sent by the X68000 to the keyboard, 1 (High) = READY to accept key data, 0 (Low) = NOT READY to accept key data.
//
// DATA (KEYBOARD -> X68000):
//     Bit [7]   - Key MAKE (0), BREAK (1)
//     Bit [6:0] - Scan Code.
//
// DATA (X68000 -> KEYBOARD):
//     LED control: Set following LED's ON / OFF (0/1)
//       bit [7]   - Command specifier, set to "1"
//       bit [6]   - LED: full-width
//       bit [5]   - LED: Hiragana
//       bit [4]   - LED: INS
//       bit [3]   - LED: CAPS
//       bit [2]   - LED: Code input
//       bit [1]   - LED: Romaji
//       bit [0]   - LED: Kana
//
//     Brightness Control
//       bit [7:2] - Command specifier, set to "010101"
//       bit [1:0] - 00 = LED's are full brightness
//                   01 =           slightly bright.
//                   10 =           slightly dark.
//                   11 =                    dark.
//
//     Set Repeat delay
//       bit [7:4] - Command specifier, set to "0110"
//       bit [3:0] - Delay period in formula, REPEAT DELAY = 200 + (int(bit[3:0])) * 100(ms)). Default delay = 500ms
//     
//     Set Repeat time
//       bit [7:4] - Command specifier, set to "0111"
//       bit [3:0] - Repeat time period in formula, REPEAT TIME = 30 + (((int(bit[3:0]))^2) * 5(ms)). Default repeat time = 110ms
//
// Scan Codes
// ----------
// ,---. ,---.    ,-------------------,    ,-------------------.  ,-----------. ,---------------.
// | 61| | 62|    | 63| 64| 65| 66| 67|    | 68| 69| 6A| 6B| 6C|  | 5A| 5B| 5C| | 5D| 52| 53| 54|
// `---' `---'    `-------------------'    `-------------------'  `-----------' `---------------'
// ,-----------------------------------------------------------.  ,-----------. ,---------------.
// | 01| 02| 03| 04| 05| 06| 07| 08| 09| 0A| 0B| 0C| 0D| 0E| 0F|  | 36| 5E| 37| | 3F| 40| 41| 42|
// |-----------------------------------------------------------|  |------------ |---------------|
// |  10 | 11| 12| 13| 14| 15| 16| 17| 18| 19| 1A| 1B| 1C|     |  | 38| 39| 3A| | 43| 44| 45| 46|
// |------------------------------------------------------. 1D |  `---=====---' |---------------|
// |  71  | 1E| 1F| 20| 21| 2l| 23| 24| 25| 26| 27| 28| 29|    |   ___| 3C|___  | 47| 48| 49| 4A|
// |-----------------------------------------------------------|  | 3B|---| 3D| |-----------|---|
// |  70    | 2A| 2B| 2C| 2D| 2E| 2F| 30| 31| 32| 33| 34|   70 |  `---| 3E|---' | 4B| 4C| 4D|   |
// `-----------------------------------------------------------|  .---=====---. |-----------| 4E|
//        | 5F| 55 | 56 |     35     | 57 | 58 | 59 | 60|         |  72 |  73 | | 4F| 50| 51|   |
//        `---------------------------------------------'         `-----------' `---------------'
//
// ,---. ,---.    ,-------------------,    ,-------------------.  ,-----------. ,---------------.
// |BRK| |CPY|    | F1| F2| F3| F4| F5|    | F6| F7| F8| F9|F10|  |   |   |   | |CAP|   |   |HLP|
// `---' `---'    `-------------------'    `-------------------'  `-----------' `---------------'
// ,-----------------------------------------------------------.  ,-----------. ,---------------.
// |ESC| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 | - | ^ | Yn| BS|  |H  |   |   | |   |   |   |   |
// |-----------------------------------------------------------|  |------------ |---------------|
// |     |   |   |   |   |   |   |   |   |   |   |   |   |     |  |   |   |   | |   |   |   |   |
// |------------------------------------------------------.    |  `---=====---' |---------------|
// |      |   |   |   |   |  l|   |   |   |   |   |   |   |    |   ___|   |___  |   |   |   |   |
// |-----------------------------------------------------------|  |   |---|   | |-----------|---|
// |        |   |   |   |   |   |   |   |   |   |   |   |      |  `---|   |---' |   |   |   |   |
// `-----------------------------------------------------------|  .---=====---. |-----------|   |
//        |   |    |    |            |    |    |    |   |         |     |     | |   |   |   |   |
//        `---------------------------------------------'         `-----------' `---------------'


// Function to push a keycode onto the key queue ready for transmission.
//
IRAM_ATTR void X68K::pushKeyToQueue(uint32_t key)
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

// Function to push a host command onto the processing queue.
//
IRAM_ATTR void X68K::pushHostCmdToQueue(uint8_t cmd)
{
    // Locals.
    t_rcvQueueMessage   rcvMsg;
    #define             PUSHCMDTAG "pushHostCmdToQueue"

    rcvMsg.hostCmd = cmd;
    if( xQueueSend(rcvQueue, (void *)&rcvMsg, 10) != pdPASS)
    {
        ESP_LOGW(PUSHCMDTAG, "Failed to put host command:%02x onto rcvQueue", cmd);
    }
    return;
}

// Method to interface with the X68000.
// The X68000 uses a standard 2400 baud Asynchronous Serial protocol with READY state signal. 
// Data to be sent is received on the event queue filled by the PS/2 interface. Data received is pushed
// to an event queue for processing by the relevant processor.
IRAM_ATTR void X68K::x68kInterface( void * pvParameters )
{
    // Locals.
    t_xmitQueueMessage  rcvMsg;
    uint8_t uartData[128];
    int     uartXmitCnt;
    size_t  uartRcvCnt;

    // Retrieve pointer to object in order to access data.
    X68K* pThis = (X68K*)pvParameters;

    // Initialise the MUTEX which prevents this core from being released to other tasks.
    //pThis->x68kMutex = portMUX_INITIALIZER_UNLOCKED;

    // Initial delay needed because the xQueue will assert probably on a suspended task ALL if delay not inserted!
    vTaskDelay(1000);

    // Sign on.
    ESP_LOGW(MAINTAG, "Starting X68000 thread.");

    // Permanent loop, wait for an incoming message on the key to send queue, read it then transmit to the X68K, repeat!
    for(;;)
    {
        // Check stack space, report if it is getting low.
        if(uxTaskGetStackHighWaterMark(NULL) < 1024)
        {
            ESP_LOGW(MAINTAG, "THREAD STACK SPACE(%d)\n",uxTaskGetStackHighWaterMark(NULL));
        }

        if(xQueueReceive(xmitQueue, (void *)&rcvMsg, 0) == pdTRUE)
        {
            //ESP_LOGW(MAINTAG, "Received:%08x\n", rcvMsg.keyCode);

            // Allow for multi byte transmissions, MSB sent first.
            if(rcvMsg.keyCode != 0x00000000)
            {
                uartXmitCnt = 0;
                while((rcvMsg.keyCode & 0xff000000) == 0x00) { rcvMsg.keyCode = rcvMsg.keyCode << 8; }
                for(int idx=0; idx < 4 && (rcvMsg.keyCode & 0xff000000) != 0x00; idx++)
                {
                    if((rcvMsg.keyCode & 0xff000000) != 0)
                    {
                        uartData[idx] = (uint8_t)((rcvMsg.keyCode & 0xFF000000) >> 24);
                        uartXmitCnt++;
                    }
                    rcvMsg.keyCode = rcvMsg.keyCode << 8;
                }
                if(uartXmitCnt > 0)
                {
                    uart_write_bytes(pThis->x68kControl.uartNum, (const char *)uartData, uartXmitCnt);
                }
            }
        }

        // Get data from X68000 - send any relevant commands for processing.
        uart_get_buffered_data_len(pThis->x68kControl.uartNum, &uartRcvCnt);
        if(uartRcvCnt > 0)
        {
            do {
                uartRcvCnt = uart_read_bytes(pThis->x68kControl.uartNum, uartData, (128 - 1), 20 / portTICK_PERIOD_MS);
                for(int idx=0; idx < uartRcvCnt; idx++)
                {
                    // Filter out polling commands and send valid commands to the rcvQueue.
                    if(uartData[idx] != 0x40 && uartData[idx] != 0x41)
                    {
                        pThis->pushHostCmdToQueue(uartData[idx]);
                    }
                }
            } while(uartRcvCnt > 0);
        }
       
        // Yield if the suspend flag is set.
        pThis->yield(50);

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
void X68K::selectOption(uint8_t optionCode)
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
            this->x68kConfig.params.activeKeyboardMap = KEYMAP_UK_WYSE_KB3926;
            break;
        case PS2_KEY_2:
            this->x68kConfig.params.activeKeyboardMap = KEYMAP_JAPAN_OADG109;
            break;
        case PS2_KEY_3:
            this->x68kConfig.params.activeKeyboardMap = KEYMAP_JAPAN_SANWA_SKBL1;
            break;
        case PS2_KEY_4:
            this->x68kConfig.params.activeKeyboardMap = KEYMAP_NOT_ASSIGNED_4;
            break;
        case PS2_KEY_5:
            this->x68kConfig.params.activeKeyboardMap = KEYMAP_NOT_ASSIGNED_5;
            break;
        case PS2_KEY_6:
            this->x68kConfig.params.activeKeyboardMap = KEYMAP_NOT_ASSIGNED_6;
            break;
        case PS2_KEY_7:
            this->x68kConfig.params.activeKeyboardMap = KEYMAP_UK_PERIBOARD_810;
            break;
        case PS2_KEY_8:
            this->x68kConfig.params.activeKeyboardMap = KEYMAP_UK_OMOTON_K8508;
            break;
        case PS2_KEY_0:
            this->x68kConfig.params.activeKeyboardMap = KEYMAP_STANDARD;
            break;
        case PS2_KEY_END:
            this->x68kConfig.params.activeMachineModel = X68K_ORIG;
            break;
        case PS2_KEY_DN_ARROW:
            this->x68kConfig.params.activeMachineModel = X68K_ACE;
            break;
        case PS2_KEY_PGDN:
            this->x68kConfig.params.activeMachineModel = X68K_EXPERT;
            break;
        case PS2_KEY_L_ARROW:
            this->x68kConfig.params.activeMachineModel = X68K_PRO;
            break;
        case PS2_KEY_KP5:
            this->x68kConfig.params.activeMachineModel = X68K_SUPER;
            break;
        case PS2_KEY_R_ARROW:
            this->x68kConfig.params.activeMachineModel = X68K_XVI;
            break;
        case PS2_KEY_HOME:
            this->x68kConfig.params.activeMachineModel = X68K_COMPACT;
            break;
        case PS2_KEY_UP_ARROW:
            this->x68kConfig.params.activeMachineModel = X68K_X68030;
            break;
        case PS2_KEY_INSERT:
            this->x68kConfig.params.activeMachineModel = X68K_ALL;
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
        this->x68kControl.persistConfig = true;
    }

    return;
}

// Method to take a PS/2 key and control data and map it into an X68000 key and control equivalent, updating state values accordingly (ie. CAPS).
// A mapping table is used which maps a key and state values into an X68000 key and control values, the emphasis being on readability and easy configuration
// as opposed to concatenated byte tables.
//
uint32_t X68K::mapKey(uint16_t scanCode)
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
  //      if((keyCode == PS2_KEY_L_SHIFT || keyCode == PS2_KEY_R_SHIFT)  && (scanCode & PS2_SHIFT) == 0) { mapped=true; this->x68kControl.keyCtrl |= X68KCTRL_SHIFT; }
        if(keyCode == PS2_KEY_R_CTRL && (scanCode & PS2_CTRL) == 0)  { mapped=true; this->x68kControl.keyCtrl &= ~X68K_CTRL_R_CTRL; }

        // Any break key clears the option select flag.
        this->x68kControl.optionSelect = false;

        // Clear any feature LED blinking.
        led->setLEDMode(LED::LED_MODE_OFF, LED::LED_DUTY_CYCLE_OFF, 0, 0L, 0L);
    } else
    {
  //      if((keyCode == PS2_KEY_L_SHIFT || keyCode == PS2_KEY_R_SHIFT)  && (scanCode & PS2_SHIFT)) { mapped=true; this->x68kControl.keyCtrl &= ~X68KCTRL_SHIFT; }
        if(keyCode == PS2_KEY_R_CTRL   && (scanCode & PS2_CTRL))  { mapped=true; this->x68kControl.keyCtrl |= X68K_CTRL_R_CTRL; }
        // Special mapping to allow selection of keyboard options. If the user presses CTRL+SHIFT+ESC then a flag becomes active and should a fourth key be pressed before a BREAK then the fourth key is taken as an option key and processed accordingly.
        if(this->x68kControl.optionSelect == true && keyCode != PS2_KEY_ESC)
        {
            mapped = true;
            this->x68kControl.optionSelect = false;
            selectOption(keyCode);
        }
        if(keyCode == PS2_KEY_ESC && (scanCode & PS2_CTRL) && (scanCode & PS2_SHIFT) && this->x68kControl.optionSelect == false)
        {
            // Prime flag ready for fourth option key and start LED blinking periodically.
            mapped = true;
            this->x68kControl.optionSelect = true;
            led->setLEDMode(LED::LED_MODE_BLINK, LED::LED_DUTY_CYCLE_50, 1, 500L, 500L);
        }
    }

    // If the key has been mapped as a special key, no further processing.
    if(mapped == true)
    {
        ESP_LOGW(MAPKEYTAG, "Mapped special key:%02x\n", this->x68kControl.keyCtrl);
       // mappedKey = (this->x68kControl.keyCtrl << 8) | 0x00;
    } else
    {
        // Loop through the entire conversion table to find a match on this key, if found map to X68000 equivalent.
        // switch matrix.
        //
        for(idx=0, mapped=false, matchExact=false; idx < x68kControl.kmeRows && (mapped == false || (mapped == true && matchExact == false)); idx++)
        {
            // Match key code? Make sure the current machine and keymap match as well.
            if(x68kControl.kme[idx].ps2KeyCode == (uint8_t)(scanCode&0xFF) && ((x68kControl.kme[idx].machine == X68K_ALL) || ((x68kControl.kme[idx].machine & x68kConfig.params.activeMachineModel) != 0)) && ((x68kControl.kme[idx].keyboardModel & x68kConfig.params.activeKeyboardMap) != 0))
            {
                // Match Raw, Shift, Function, Control, ALT or ALT-Gr?
                if( (((x68kControl.kme[idx].ps2Ctrl & PS2CTRL_SHIFT) == 0) && ((x68kControl.kme[idx].ps2Ctrl & PS2CTRL_CTRL) == 0) && ((x68kControl.kme[idx].ps2Ctrl & PS2CTRL_R_CTRL)  == 0) && ((x68kControl.kme[idx].ps2Ctrl & PS2CTRL_ALTGR) == 0) && ((x68kControl.kme[idx].ps2Ctrl & PS2CTRL_GUI)   == 0) && ((x68kControl.kme[idx].ps2Ctrl & PS2CTRL_FUNC)  == 0)) ||
                    ((scanCode & PS2_SHIFT)                                && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_SHIFT) != 0) ||
                    ((scanCode & PS2_CTRL)                                 && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_CTRL)  != 0) ||
                    ((scanCode & PS2_GUI)                                  && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_GUI)   != 0) || 
                    ((this->x68kControl.keyCtrl & X68K_CTRL_R_CTRL)        && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_R_CTRL)!= 0) ||
                //    ((scanCode & PS2_CAPS)                               && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_CAPS)  != 0) || 
                    ((scanCode & PS2_FUNCTION)                             && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_FUNC)  != 0) )
                {
                    
                    // Exact entry match, data + control key? On an exact match we only process the first key. On a data only match we fall through to include additional data and control key matches to allow for un-mapped key combinations, ie. Japanese characters.
                    matchExact = (((scanCode & PS2_SHIFT)                            && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_SHIFT) != 0) || ((scanCode & PS2_SHIFT) == 0                         && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_SHIFT) == 0)) &&
                                 (((scanCode & PS2_CTRL)                             && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_CTRL)  != 0) || ((scanCode & PS2_CTRL) == 0                          && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_CTRL)  == 0)) &&
                                 (((scanCode & PS2_GUI)                              && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_GUI)   != 0) || ((scanCode & PS2_GUI) == 0                           && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_GUI)   == 0)) &&
                                 (((this->x68kControl.keyCtrl & X68K_CTRL_R_CTRL)    && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_R_CTRL)!= 0) || ((this->x68kControl.keyCtrl & X68K_CTRL_R_CTRL) == 0 && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_R_CTRL)== 0)) &&
                 //              (((scanCode & PS2_CAPS)                             && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_CAPS)  != 0) || ((scanCode & PS2_GUI) == 0                           && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_CAPS)  == 0)) &&
                                 (((scanCode & PS2_FUNCTION)                         && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_FUNC)  != 0) || ((scanCode & PS2_FUNCTION) == 0                      && (x68kControl.kme[idx].ps2Ctrl & PS2CTRL_FUNC)  == 0))
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
                        mappedKey = 0x80 | (x68kControl.kme[idx].x68kKey & 0x7F);
                        mapped = true;
                    } else
                    {
                        // Map key actioning any control overrides.
                        if((x68kControl.kme[idx].x68kCtrl & X68K_CTRL_RELEASESHIFT) != 0)
                        {
                            // RELEASESHIFT infers that the X68000 must cancel the current shift status prior to receiving the key code. This is necessary when using foreign keyboards and a character appears
                            // on a shifted key whereas on the original X68000 keyboard the character is the primary key.
                            //
                            mappedKey = ((0x80 | X68K_KEY_SHIFT) << 16) | 0x00 | ((x68kControl.kme[idx].x68kKey & 0x7F) << 8) | (0x00 | X68K_KEY_SHIFT);
                        } else
                        if((x68kControl.kme[idx].x68kCtrl & X68K_CTRL_SHIFT) != 0)
                        {
                            // SHIFT infers that the X68000 must invoke shift status prior to receiving the key code. This is necessary when using foreign keyboards and a character appears
                            // as a primary key on the foreign keyboard but as a shifted key on the X68000 keyboard.
                            //
                            mappedKey = ((0x00 | X68K_KEY_SHIFT) << 16) | 0x00 | ((x68kControl.kme[idx].x68kKey & 0x7F) << 8) | (0x80 | X68K_KEY_SHIFT);
                        }
                        else
                        {
                            mappedKey = 0x00 | (x68kControl.kme[idx].x68kKey & 0x7F);
                        }
                        mapped = true;
                    }
                }
            }
        }
    }
    return(mappedKey);
}

// Primary HID thread, running on Core 0.
// This thread is responsible for receiving HID (PS/2 or BT) keyboard scan codes and mapping them to Sharp X68000 equivalent keys, updating state flags as needed.
// The HID data is received via interrupt. The data to be sent to the X68000 is pushed onto a FIFO queue.
//
IRAM_ATTR void X68K::hidInterface( void * pvParameters )
{
    // Locals.
    uint16_t            scanCode      = 0x0000;
    uint32_t            x68kKey       = 0x00000000;
    t_rcvQueueMessage   rcvMsg;

    // Map the instantiating object so we can access its methods and data.
    X68K* pThis = (X68K*)pvParameters;

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

            // Map the PS/2 key to an X68000 CTRL + KEY
            x68kKey = pThis->mapKey(scanCode);
            if(x68kKey != 0L)
            {
                pThis->pushKeyToQueue(x68kKey);
            }

            // Toggle LED to indicate data flow.
            if((scanCode & PS2_BREAK) == 0)
                pThis->led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_10, 1, 100L, 0L);
        }

        // Check for incoming host keyboard commands and execute them.
        if(xQueueReceive(rcvQueue, (void *)&rcvMsg, 0) == pdTRUE)
        {
            ESP_LOGD(MAINTAG, "Received Host Cmd:%02x\n", rcvMsg.hostCmd);
        }

        // NVS writes require both CPU cores to be free so write config out at a known junction.
        if(pThis->x68kControl.persistConfig == true)
        {
            // Request and wait for the interface to suspend. This ensures that the host cpu is not held in a spinlock when NVS update is requested avoiding deadlock.
            pThis->suspendInterface(true);
            pThis->isSuspended(true);

            if(pThis->nvs->persistData(pThis->getClassName(__PRETTY_FUNCTION__), &pThis->x68kConfig, sizeof(t_x68kConfig)) == false)
            {
                ESP_LOGW(SELOPTTAG, "Persisting X68000 configuration data failed, updates will not persist in future power cycles.");
                pThis->led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_10, 200, 1000L, 0L);
            } else
            // Few other updates so make a commit here to ensure data is flushed and written.
            if(pThis->nvs->commitData() == false)
            {
                ESP_LOGW(SELOPTTAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
                pThis->led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_10, 200, 500L, 0L);
            }

            // Release interface.
            pThis->suspendInterface(false);

            // Clear flag so we dont persist in a loop.
            pThis->x68kControl.persistConfig = false;
        }

        // Yield if the suspend flag is set.
        pThis->yield(25);
    }
}

// A method to load the keyboard mapping table into memory for use in the interface mapping logic. If no persistence file exists or an error reading persistence occurs, the keymap 
// uses the internal static default. If no persistence file exists and attempt is made to create it with a copy of the inbuilt static map so that future operations all
// work with persistence such that modifications can be made.
//
bool X68K::loadKeyMap(void)
{
    // Locals.
    //
    bool        result = false;
    int         fileRows = 0;
    struct stat keyMapFileNameStat;

    // See if the file exists, if it does, get size so we can compute number of mapping rows.
    if(stat(x68kControl.keyMapFileName.c_str(), &keyMapFileNameStat) == -1)
    {
        ESP_LOGW(MAINTAG, "No keymap file, using inbuilt definitions.");
    } else
    {
        // Get number of rows in the file.
        fileRows = keyMapFileNameStat.st_size/sizeof(t_keyMapEntry);

        // Subsequent reloads, delete memory prior to building new map, primarily to conserve precious resources rather than trying the memory allocation trying to realloc and then having to copy.
        if(x68kControl.kme != NULL && x68kControl.kme != PS2toX68K.kme)
        {
            delete x68kControl.kme;
            x68kControl.kme = NULL;
        }

        // Allocate memory for the new keymap table.
        x68kControl.kme = new t_keyMapEntry[fileRows];
        if(x68kControl.kme == NULL)
        {
            ESP_LOGW(MAINTAG, "Failed to allocate memory for keyboard map, fallback to inbuilt!");
        } else
        {
            // Open the keymap extension file for binary reading to add data to our map table.
            std::fstream keyFileIn(x68kControl.keyMapFileName.c_str(), std::ios::in | std::ios::binary);

            int idx=0;
            while(keyFileIn.good())
            {
                keyFileIn.read((char *)&x68kControl.kme[idx], sizeof(t_keyMapEntry));
                if(keyFileIn.good())
                {
                    idx++;
                }
            }
            // Any errors, we wind back and use the inbuilt mapping table.
            if(keyFileIn.bad())
            {
                keyFileIn.close();
                ESP_LOGW(MAINTAG, "Failed to read data from keymap extension file:%s, fallback to inbuilt!", x68kControl.keyMapFileName.c_str());
            } else
            {
                // No longer need the file.
                keyFileIn.close();
               
                // Max rows in the KME table.
                x68kControl.kmeRows = fileRows;

                // Good to go, map ready for use with the interface.
                result = true;
            }
        }
    }

    // Any failures, free up memory and use the inbuilt mapping table.
    if(result == false)
    {
        if(x68kControl.kme != NULL && x68kControl.kme != PS2toX68K.kme)
        {
            delete x68kControl.kme;
            x68kControl.kme = NULL;
        }
     
        // No point allocating memory if no extensions exist or an error occurs, just point to the static table.
        x68kControl.kme = PS2toX68K.kme;
        x68kControl.kmeRows = PS2TBL_X68K_MAXROWS;

        // Persist the data so that next load comes from file.
        saveKeyMap();
    }

    // Return code. Either memory map was successfully loaded, true or failed, false.
    return(result);
}

// Method to save the current keymap out to an extension file.
//
bool X68K::saveKeyMap(void)
{
    // Locals.
    //
    bool        result = false;
    int         idx = 0;

    // Has a map been defined? Cannot save unless loadKeyMap has been called which sets x68kControl.kme to point to the internal keymap or a new memory resident map.
    //
    if(x68kControl.kme == NULL)
    {
        ESP_LOGW(MAINTAG, "KeyMap hasnt yet been defined, need to call loadKeyMap.");
    } else
    {
        // Open file for binary writing, trunc specified to clear out the file, we arent appending.
        std::fstream keyFileOut(x68kControl.keyMapFileName.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);

        // Loop whilst no errors and data rows still not written.
        while(keyFileOut.good() && idx < x68kControl.kmeRows)
        {
            keyFileOut.write((char *)&x68kControl.kme[idx], sizeof(t_keyMapEntry));
            idx++;
        }
        if(keyFileOut.bad())
        {
            ESP_LOGW(MAINTAG, "Failed to write data from the keymap to file:%s, deleting as state is unknown!", x68kControl.keyMapFileName.c_str());
            keyFileOut.close();
            std::remove(x68kControl.keyMapFileName.c_str());
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
bool X68K::createKeyMapFile(std::fstream &outFile)
{
    // Locals.
    //
    bool           result = true;
    std::string    fileName;

    // Attempt to open a temporary keymap file for writing.
    //
    fileName = x68kControl.keyMapFileName;
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
bool X68K::storeDataToKeyMapFile(std::fstream &outFile, char *data, int size)
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
bool X68K::storeDataToKeyMapFile(std::fstream & outFile, std::vector<uint32_t>& dataArray)
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
bool X68K::closeAndCommitKeyMapFile(std::fstream &outFile, bool cleanupOnly)
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
            fileName = x68kControl.keyMapFileName;
            replaceExt(fileName, "bak");
            // Remove old backup file. Dont worry if it is not there!
            std::remove(fileName.c_str());
            replaceExt(fileName, "tmp");
            // Rename new file to active.
            if(std::rename(fileName.c_str(), x68kControl.keyMapFileName.c_str()) != 0)
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
void X68K::getKeyMapHeaders(std::vector<std::string>& headerList)
{
    // Add the names.
    //
    headerList.push_back(PS2TBL_PS2KEYCODE_NAME);
    headerList.push_back(PS2TBL_PS2CTRL_NAME);
    headerList.push_back(PS2TBL_KEYBOARDMODEL_NAME);
    headerList.push_back(PS2TBL_MACHINE_NAME);
    headerList.push_back(PS2TBL_X68KKEYCODE_NAME);
    headerList.push_back(PS2TBL_X68KCTRL_NAME);

    return;
}

// A method to return the Type of data for a given column in the KeyMap table.
//
void X68K::getKeyMapTypes(std::vector<std::string>& typeList)
{
    // Add the types.
    //
    typeList.push_back(PS2TBL_PS2KEYCODE_TYPE);
    typeList.push_back(PS2TBL_PS2CTRL_TYPE);
    typeList.push_back(PS2TBL_KEYBOARDMODEL_TYPE);
    typeList.push_back(PS2TBL_MACHINE_TYPE);
    typeList.push_back(PS2TBL_X68KKEYCODE_TYPE);
    typeList.push_back(PS2TBL_X68KCTRL_TYPE);

    return;
}

// Method to return a list of key:value entries for a given keymap column. This represents the
// feature which can be selected and the value it uses. Features can be combined by ORing the values
// together.
bool X68K::getKeyMapSelectList(std::vector<std::pair<std::string, int>>& selectList, std::string option)
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
        selectList.push_back(std::make_pair(PS2TBL_PS2CTRL_SEL_R_CTRL,    PS2CTRL_R_CTRL));
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
        selectList.push_back(std::make_pair(X68K_SEL_ALL,                 X68K_ALL));
        selectList.push_back(std::make_pair(X68K_SEL_ORIG,                X68K_ORIG));
        selectList.push_back(std::make_pair(X68K_SEL_ACE,                 X68K_ACE));
        selectList.push_back(std::make_pair(X68K_SEL_EXPERT,              X68K_EXPERT));
        selectList.push_back(std::make_pair(X68K_SEL_PRO,                 X68K_PRO));
        selectList.push_back(std::make_pair(X68K_SEL_SUPER,               X68K_SUPER));
        selectList.push_back(std::make_pair(X68K_SEL_XVI,                 X68K_XVI));
        selectList.push_back(std::make_pair(X68K_SEL_COMPACT,             X68K_COMPACT));
        selectList.push_back(std::make_pair(X68K_SEL_X68030,              X68K_X68030));
    } 
    else if(option.compare(PS2TBL_X68KCTRL_TYPE) == 0)
    {
        selectList.push_back(std::make_pair(X68K_CTRL_SEL_SHIFT,          X68K_CTRL_SHIFT));
        selectList.push_back(std::make_pair(X68K_CTRL_SEL_RELEASESHIFT,   X68K_CTRL_RELEASESHIFT));
        selectList.push_back(std::make_pair(X68K_CTRL_SEL_R_CTRL,         X68K_CTRL_R_CTRL));
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
bool X68K::getKeyMapData(std::vector<uint32_t>& dataArray, int *row, bool start)
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
    if((*row) >= x68kControl.kmeRows)
    {
        result = true;
    } else
    {
        dataArray.push_back(x68kControl.kme[*row].ps2KeyCode);
        dataArray.push_back(x68kControl.kme[*row].ps2Ctrl);
        dataArray.push_back(x68kControl.kme[*row].keyboardModel);
        dataArray.push_back(x68kControl.kme[*row].machine);
        dataArray.push_back(x68kControl.kme[*row].x68kKey);
        dataArray.push_back(x68kControl.kme[*row].x68kCtrl);
        (*row) = (*row) + 1;
    }

    // True if no more rows, false if additional rows can be read.
    return(result);
}


// Initialisation routine. Start two threads, one to handle the incoming PS/2 keyboard data and map it, the second to handle the host interface.
void X68K::init(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID)
{
    // Basic initialisation.
    init(hdlNVS, hdlHID);

    // Invoke the prototype init which initialises common variables and devices shared by all subclass. 
    KeyInterface::init(getClassName(__PRETTY_FUNCTION__), hdlNVS, hdlLED, hdlHID, ifMode);

    // Prepare the UART to be used for communications with the X68000.
    // The X68000 uses Asynchronous protocol with 1 stop bit no parity 2400 baud.
    //
    uart_config_t uartConfig        = {
        .baud_rate                  = 2400,
        .data_bits                  = UART_DATA_8_BITS,
        .parity                     = UART_PARITY_DISABLE,
        .stop_bits                  = UART_STOP_BITS_1,
        .flow_ctrl                  = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh        = 122,
        .source_clk                 = UART_SCLK_APB,
    };

    // Install UART driver. Use RX/TX buffers without event queue. 
    ESP_ERROR_CHECK(uart_driver_install(x68kControl.uartNum, x68kControl.uartBufferSize, x68kControl.uartBufferSize, 0, NULL, 0));

    // Configure UART parameters and pin assignments, software flow control, not RTS/CTS.
    ESP_ERROR_CHECK(uart_param_config(x68kControl.uartNum, &uartConfig));
    ESP_ERROR_CHECK(uart_set_pin(x68kControl.uartNum, CONFIG_HOST_KDB0, CONFIG_HOST_KDB1, -1, -1));

    // Create queue for buffering incoming HID keys prior to transmitting to the X68000.
    xmitQueue = xQueueCreate(MAX_X68K_XMIT_KEY_BUF, sizeof(t_xmitQueueMessage));
    // Create queue for buffering incoming X68000 data for later processing.
    rcvQueue  = xQueueCreate(MAX_X68K_RCV_KEY_BUF, sizeof(t_rcvQueueMessage));

    // Create a task pinned to core 1 which will fulfill the Sharp X68000 interface. This task has the highest priority
    // and it will also hold spinlock and manipulate the watchdog to ensure a scan cycle timing can be met. This means 
    // all other tasks running on Core 1 will suspend as needed. The HID devices will be serviced with core 0.
    //
    // Core 1 - X68000 Interface
    ESP_LOGW(MAINTAG, "Starting x68kif thread...");
    ::xTaskCreatePinnedToCore(&this->x68kInterface, "x68kif", 4096, this, 25, &this->TaskHostIF, 1);
    vTaskDelay(500);

    // Core 0 - Application
    // HID Interface handler thread.
    ESP_LOGW(MAINTAG, "Starting hidIf thread...");
    ::xTaskCreatePinnedToCore(&this->hidInterface, "hidIf", 8192, this, 22, &this->TaskHIDIF, 0);
}

// Initialisation routine without hardware.
void X68K::init(NVS *hdlNVS, HID *hdlHID)
{
    // Initialise control variables.
    this->x68kControl.keyCtrl       = 0x00;
    x68kControl.optionSelect        = false;
    x68kControl.uartNum             = UART_NUM_2;
    x68kControl.uartBufferSize      = 256;
    x68kControl.uartQueueSize       = 10;
    x68kControl.keyMapFileName      = x68kControl.fsPath.append("/").append(X68KIF_KEYMAP_FILE);
    x68kControl.kmeRows             = 0;
    x68kControl.kme                 = NULL;
    x68kControl.persistConfig       = false;

    // Invoke the prototype init which initialises common variables and devices shared by all subclass. 
    KeyInterface::init(getClassName(__PRETTY_FUNCTION__), hdlNVS, hdlHID);

    // Load the keyboard mapping table into memory. If the file doesnt exist, create it.
    loadKeyMap();

    // Retrieve configuration, if it doesnt exist, set defaults.
    //
    if(nvs->retrieveData(getClassName(__PRETTY_FUNCTION__), &this->x68kConfig, sizeof(t_x68kConfig)) == false)
    {
        ESP_LOGW(MAINTAG, "X68000 configuration set to default, no valid config in NVS found.");
        x68kConfig.params.activeKeyboardMap   = KEYMAP_STANDARD;
        x68kConfig.params.activeMachineModel  = X68K_ALL;

        // Persist the data for next time.
        if(nvs->persistData(getClassName(__PRETTY_FUNCTION__), &this->x68kConfig, sizeof(t_x68kConfig)) == false)
        {
            ESP_LOGW(MAINTAG, "Persisting Default X68000 configuration data failed, check NVS setup.\n");
        }
        // Few other updates so make a commit here to ensure data is flushed and written.
        else if(this->nvs->commitData() == false)
        {
            ESP_LOGW(MAINTAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
        }
    }
}

// Constructor, basically initialise the Singleton interface and let the threads loose.
X68K::X68K(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID, const char* fsPath)
{
    // Setup the default path on the underlying filesystem.
    this->x68kControl.fsPath = fsPath;

    // Initialise the interface.
    init(ifMode, hdlNVS, hdlLED, hdlHID);
}

// Constructor, basic initialisation without hardware.
X68K::X68K(NVS *hdlNVS, HID *hdlHID, const char* fsPath)
{
    // Setup the default path on the underlying filesystem.
    this->x68kControl.fsPath = fsPath;

    // Initialise the interface.
    init(hdlNVS, hdlHID);
}

// Constructor, used for version reporting so no hardware is initialised.
X68K::X68K(void)
{
    return;
}

// Destructor - only ever called when the class is used for version reporting.
X68K::~X68K(void)
{
    return;
}
