/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            Mouse.cpp
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     PS/2 Mouse to Sharp Host Interface logic.
//                  This source file contains the singleton class containing logic to obtain
//                  PS/2 mouse data (position, keys etc), map them into Sharp compatible codes and 
//                  transmit the data to the connected host.
//
//                  The whole application of which this class is a member, uses the Espressif Development
//                  environment with Arduino components.
//
// Credits:         
// Copyright:       (c) 2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Mar 2022 - Initial write.
//            v1.01 May 2022 - Initial release version.
//            v1.02 Jun 2022 - Updates to reflect changes realised in other modules due to addition of
//                             bluetooth and suspend logic due to NVS issues using both cores.
//                             Updates to reflect moving functionality into the HID and to support
//                             Bluetooth as a primary mouse or secondary mouse.
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
#include <bitset>
#include <iostream>
#include <sstream>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "Arduino.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "driver/timer.h"
#include "sdkconfig.h"
#include "Mouse.h"

// Tag for ESP main application logging.
#define                         MAINTAG  "Mouse"

// Mouse Protocol
// --------------
//
//
// The Sharp (X68000/X1/MZ-2500/MZ-2800) mouse uses an asynchronous serial protocol over two wires (MSDATA/MSCTRL).
// The MSCTRL signal is an enable signal, idle state = HIGH, it goes low prior to transmission of data by at least 1mS and goes high after
// transmission of last bit by ~2.56mS.
// The MSDATA signal is a standard asynchronous signal, idle = HIGH, 1 start bit, 8 data bits, 2 stop bits @ 4800baud.
//
// Protocol:
//     Idle State (MSDATA/MSCTRL) = High.
//     Transmission: MSCTRL -> LOW
//                   1ms delay
//                   MSDATA -> low, first start bit.
//                   3 bytes transmitted in a <1xStart><8xdata><2xstop> format.
//                   MSDATA -> high 
//                   2.56ms delay.
//                   MSCTRL -> HIGH
//     Data bytes: <CTRL><POS X><POS Y>
//                 CTRL = [7]   - Mouse rolling forward when high, backward when low.
//                        [6]
//                        [5]   - Mouse rolling left, right when low.
//                        [4]
//                        [3]
//                        [2]
//                        [1]   - Right button pressed = HIGH.
//                        [0]   - Left button pressed = HIGH.
//                 POS X  [7:0] - X Position data.
//                 POS Y  [7:0] - Y Position data.


// Method to realise the Sharp host Mouse protocol.
// This method uses Core 1 and it will hold it in a spinlock as necessary to ensure accurate timing.
// Mouse data is passed into the method via a direct object, using the FreeRTOS Queue creates a time lag resulting in the mouse data being out of sync with hand movement.
IRAM_ATTR void Mouse::hostInterface( void * pvParameters )
{
    // Locals.
    //
    Mouse*              pThis = (Mouse*)pvParameters;                         // Retrieve pointer to object in order to access data.
    bool                msctrlEdge = false;
    uint8_t             txBuf[4];
    uint32_t            MSCTRL_MASK;
    uint32_t            MSDATA_MASK;
  #ifdef CONFIG_HOST_BITBANG_UART
    int                 txPos;
    int                 txCnt;
    uint32_t            shiftReg;
    uint64_t            delayTimer = 0LL;
    uint64_t            curTime    = 0LL;
    uint32_t            bitCount = 0;
    enum HOSTXMITSTATE {
                        FSM_IDLE        = 0,
                        FSM_STARTXMIT   = 1,
                        FSM_STARTBIT    = 2,
                        FSM_DATA        = 3,
                        FSM_STOP        = 4,
                        FSM_ENDXMIT     = 5
    }                   state = FSM_IDLE;
  #endif

    // Initialise the MUTEX which prevents this core from being released to other tasks.
    pThis->x1Mutex = portMUX_INITIALIZER_UNLOCKED;

    if(pThis->hostControl.secondaryIf == false)
    {
        MSCTRL_MASK     = (1 << CONFIG_HOST_KDB0);
        MSDATA_MASK     = (1 << CONFIG_HOST_KDB1);
    } else
    {
        MSCTRL_MASK     = (1 << CONFIG_HOST_KDB0);
        MSDATA_MASK     = (1 << CONFIG_HOST_KDI4);
    }

    gpio_config_t     ioConf;
    ioConf.intr_type    = GPIO_INTR_DISABLE;
    ioConf.mode         = GPIO_MODE_INPUT; 
    ioConf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ioConf.pull_up_en   = GPIO_PULLUP_ENABLE;
    // Both Hardware UART and bitbang need MSCTRL setting as an input.
    if(pThis->hostControl.secondaryIf == false)
    {
        ioConf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDB0); 
        gpio_config(&ioConf);
    }
    // Bitbang mode also needs MSDATA setting as an output.
  #ifdef CONFIG_HOST_BITBANG_UART
    ioConf.pull_up_en   = GPIO_PULLUP_DISABLE;
    ioConf.mode         = GPIO_MODE_OUTPUT; 
    if(pThis->hostControl.secondaryIf == false)
    {
        ioConf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDB1); 
    } else
    {
        ioConf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDI4); 
    }
    gpio_config(&ioConf);
  
    // Set MSDATA to default state which is high.
    GPIO.out_w1ts = MSDATA_MASK;
  #endif

    // Configure a timer to be used for the host mouse asynchronous protocol spacing with 1uS resolution. The default clock source is the APB running at 80MHz.
    timer_config_t timerConfig = {
        .alarm_en    = TIMER_ALARM_DIS,            // No alarm, were not using interrupts as we are in a dedicated thread.
        .counter_en  = TIMER_PAUSE,                // Timer paused until required.
        .intr_type   = TIMER_INTR_LEVEL,           // No interrupts used.
        .counter_dir = TIMER_COUNT_UP,             // Timing a fixed period.
        .auto_reload = TIMER_AUTORELOAD_DIS,       // No need for auto reload, fixed time period.
        .divider     = 80                          // 1Mhz operation giving 1uS resolution.
    };
    ESP_ERROR_CHECK(timer_init(TIMER_GROUP_0, TIMER_0, &timerConfig));
    ESP_ERROR_CHECK(timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0));

    // Sign on.
    ESP_LOGW(MAINTAG, "Starting Host side Mouse thread.");

    // Permanent loop, wait for an incoming message on the key to send queue, read it then transmit to the host, repeat!
    for(;;)
    {
      #ifdef CONFIG_HOST_BITBANG_UART
        // Get the current timer value, only run the FSM when the timer is idle.
        timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &curTime);
        if((state == FSM_IDLE && pThis->hostControl.secondaryIf == false) || curTime >= delayTimer)
        {
            // Ensure the timer is stopped.
            timer_pause(TIMER_GROUP_0, TIMER_0);
            delayTimer = 0LL;

            // Finite state machine to retrieve a key for transmission then serialise it according to the X1 protocol.
            switch(state)
            {
                case FSM_IDLE:
                    // Yield if the suspend flag is set.
                    pThis->yield(0);

                    // Check stack space, report if it is getting low.
                    if(uxTaskGetStackHighWaterMark(NULL) < 1024)
                    {
                        ESP_LOGW(MAINTAG, "THREAD STACK SPACE(%d)\n",uxTaskGetStackHighWaterMark(NULL));
                    }                    
                    
                    if(pThis->hostControl.secondaryIf == false)
                    {
                        // Detect high to low edge. On mouse primary mode the MSCTRL signal forces the tempo. On mouse secondary mode (operating in tandem to keyboard),
                        // the timer forces the tempo.
                        //
                        msctrlEdge = (REG_READ(GPIO_IN_REG) & MSCTRL_MASK) != 0 ? true : msctrlEdge;
                    }

                    // Wait for a window when MSCTRL goes low.
                    if(pThis->hostControl.secondaryIf == true || (msctrlEdge == true && (REG_READ(GPIO_IN_REG) & MSCTRL_MASK) == 0))
                    {
                        // Wait for incoming mouse movement message.
                        if(pThis->xmitMsg.valid)
                        {
                            txBuf[0] = (uint8_t)pThis->xmitMsg.status;
                            txBuf[1] = (uint8_t)pThis->xmitMsg.xPos;
                            txBuf[2] = (uint8_t)pThis->xmitMsg.yPos;
                            pThis->xmitMsg.valid = false;   // Shouldnt be a race state here but consider a mutex if mouse gets out of sync.
                            txBuf[3] = 0x00;
                            txPos = 0;
                            txCnt = 3;
                        } else
                        {
                            // Sharp host protocol requires us to send zero change messages on a regular period regardless of new data.
                            txBuf[0] = 0x00;
                            txBuf[1] = 0x00;
                            txBuf[2] = 0x00;
                            txBuf[3] = 0x00;
                            txPos = 0;
                            txCnt = 3;
                        }

                        // Advance to first start bit.
                        state = FSM_STARTXMIT; 

                        // Clear edge detect for next loop.
                        msctrlEdge = false;
                    }
                    break;

                case FSM_STARTXMIT:
                    // Ensure all variables and states correct before entering serialisation.
                    GPIO.out_w1ts = MSDATA_MASK;
                    state = FSM_STARTBIT;
                    bitCount = 8;
                    shiftReg = txBuf[txPos++];
                    txCnt--;
                   
                    // Create, initialise and hold a spinlock so the current core is bound to this one method.
                    portENTER_CRITICAL(&pThis->x1Mutex);

                    break;

                case FSM_STARTBIT:
                    // Send out the start bit by bringing MSDATA low for 208us (4800 baud 1bit time period).
                    GPIO.out_w1tc = MSDATA_MASK;
                    delayTimer    = BITBANG_UART_BIT_TIME;
                    state         = FSM_DATA;
                    break;

                case FSM_DATA:
                    if(bitCount > 0)
                    {
                        // Setup the bit on MSDATA
                        if(shiftReg & 0x00000001)
                        {
                            GPIO.out_w1ts = MSDATA_MASK;
                        } else
                        {
                            GPIO.out_w1tc = MSDATA_MASK;
                        }

                        // Shift the data to the next bit for transmission.
                        shiftReg = shiftReg >> 1;

                        // 1 bit period.
                        delayTimer = BITBANG_UART_BIT_TIME;

                        // 1 Less bit in frame.
                        bitCount--;
                    } else
                    {
                        state = FSM_STOP;
                    }
                    break;                    

                case FSM_STOP:
                    // Send out the stop bit, 2 are needed so just adjust the time delay.
                    GPIO.out_w1ts = MSDATA_MASK;
                    delayTimer = BITBANG_UART_BIT_TIME * 2;
                    state = FSM_ENDXMIT;
                    break;

                case FSM_ENDXMIT:
                    // End of critical timing loop, release the core so other tasks can run whilst we load up the next byte.
                    portEXIT_CRITICAL(&pThis->x1Mutex);

                    // Any more bytes to transmit, loop and send if there are.
                    if(txCnt > 0)
                    {
                        state = FSM_STARTXMIT;
                    } else
                    {
                        // Reset timer for next loop.
                        delayTimer = 20000UL;
                        state = FSM_IDLE;
                    }
                    break;
            }

            // If a new delay is requested, set the value into the timer and start.
            if(delayTimer > 0LL)
            {
                timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0LL);
                timer_start(TIMER_GROUP_0, TIMER_0);
            }
        }
      #endif

      #ifdef CONFIG_HOST_HW_UART
        // Get the current timer value, we need to wait 20ms between transmissions.
        timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &curTime);
        if(curTime >= delayTimer)
        {
            // Wait for a window when MSCTRL goes low.
            if(pThis->hostControl.secondaryIf == true || (REG_READ(GPIO_IN_REG) & MSCTRL_MASK) == 0)
            {
                // Ensure the timer is stopped, initialise to 0 and restart.
                timer_pause(TIMER_GROUP_0, TIMER_0);
                delayTimer = 20000LL;
                timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0LL);
                timer_start(TIMER_GROUP_0, TIMER_0);

                // Wait for incoming mouse movement message.
                if(pThis->xmitMsg.valid)
                {
                    txBuf[0] = (uint8_t)pThis->xmitMsg.status;
                    txBuf[1] = (uint8_t)pThis->xmitMsg.xPos;
                    txBuf[2] = (uint8_t)pThis->xmitMsg.yPos;
                    pThis->xmitMsg.valid = false;   // Shouldnt be a race state here but consider a mutex if mouse gets out of sync.
                    txBuf[3] = 0x00;
                    txPos = 0;
                    txCnt = 3;
                } else
                {
                    // Sharp host protocol requires us to send zero change messages on a regular period regardless of new data.
                    txBuf[0] = 0x00;
                    txBuf[1] = 0x00;
                    txBuf[2] = 0x00;
                    txBuf[3] = 0x00;
                    txPos = 0;
                    txCnt = 3;
                }

                // Send the bytes and wait.
                uart_write_bytes(pThis->hostControl.uartNum, (const char *)txBuf, 3);

                // This method doesnt actually return after the last byte is transmitted, it returns well before, so we tack on a 10ms delay which is the width for 3 bytes at 4800 baud.
                uart_wait_tx_done(pThis->hostControl.uartNum, 25000);
                vTaskDelay(10);
            }
        
            // Check stack space, report if it is getting low.
            if(uxTaskGetStackHighWaterMark(NULL) < 1024)
            {
                ESP_LOGW(MAPKEYTAG, "THREAD STACK SPACE(%d)\n",uxTaskGetStackHighWaterMark(NULL));
            }
            
            // Yield if the suspend flag is set.
            pThis->yield(0);
        }
      #endif

        // Logic to feed the watchdog if needed. Watchdog disabled in menuconfig but if enabled this will need to be used.
        //TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
        //TIMERG0.wdt_feed=1;                       // feed dog
        //TIMERG0.wdt_wprotect=0;                   // write protect
        //TIMERG1.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
        //TIMERG1.wdt_feed=1;                       // feed dog
        //TIMERG1.wdt_wprotect=0;                   // write protect
    }
}

// Primary HID routine.
// This method is responsible for receiving HID (PS/2 or BT) mouse scan data and mapping it into Sharp compatible mouse data.
// The HID mouse data once received is mapped and pushed onto a FIFO queue for transmission to the host.
//
void Mouse::mouseReceiveData(HID::t_mouseMessageElement mouseMessage)
{
    // Locals.
    uint8_t    status;

    // Invert Y as the Sharp host is inverted compared to a PS/2 on the Y axis.
    mouseMessage.yPos = -mouseMessage.yPos;

    // Initialise the status flag, on the Sharp host it is <Y Overflow><Y Underflow><X Overflow><X Underflow><1><0><Right Button><Left Button>
    status = (((mouseMessage.xPos >> 8) & 0x01) << 4) | (mouseMessage.status & 0x0F );

    // Check bounds and set flags accordingly.
    if(mouseMessage.xPos > 127)
    {
        mouseMessage.xPos = 127;            // Maximum resolution of Sharp host X movement.
        status |= (1UL << 4);               // Set overflow bit.
    }
    if(mouseMessage.xPos < -128)
    {
        mouseMessage.xPos = -128;           // Minimum resolution of Sharp host X movement.
        status |= (1UL << 5);               // Set underflow bit.
    }
    if(mouseMessage.yPos > 127)
    {
        mouseMessage.yPos = 127;            // Maximum resolution of Sharp host Y movement.
        status |= (1UL << 6);               // Set overflow bit.
    }
    if(mouseMessage.yPos < -128)
    {
        mouseMessage.yPos = -128;           // Minimum resolution of Sharp host Y movement.
        status |= (1UL << 7);               // Set underflow bit.
    }

    // Convert back to 8bit 2's compliment and store in the host message to the host thread.
    xmitMsg.xPos   = (int8_t)mouseMessage.xPos;
    xmitMsg.yPos   = (int8_t)mouseMessage.yPos;
    xmitMsg.status = status;
    xmitMsg.wheel  = mouseMessage.wheel;
    xmitMsg.valid  = true;

    return;
}

// A method to return the Type of data for a given column in the KeyMap table.
//
void Mouse::getMouseConfigTypes(std::vector<std::string>& typeList)
{
    // Add the types.
    //
    typeList.push_back(HID_MOUSE_HOST_SCALING_TYPE);
    typeList.push_back(HID_MOUSE_SCALING_TYPE);
    typeList.push_back(HID_MOUSE_RESOLUTION_TYPE);
    typeList.push_back(HID_MOUSE_SAMPLING_TYPE);
    return;
}

// Method to return a list of key:value entries for a given config category. This represents the
// feature which can be selected and the value it uses. Features can be combined by ORing the values
// together.
bool Mouse::getMouseSelectList(std::vector<std::pair<std::string, int>>& selectList, std::string option)
{
    // Locals.
    //
    bool result = true;

    // Build up a map, depending on the list required, of name to value. This list can then be used
    // by a user front end to select an option based on a name and return its value.
    if(option.compare(HID_MOUSE_HOST_SCALING_TYPE) == 0)
    {
        selectList.push_back(std::make_pair("ACTIVE",                         mouseConfig.host.scaling));
        selectList.push_back(std::make_pair(HID_MOUSE_HOST_SCALING_1_1_NAME,  HID::HID_MOUSE_HOST_SCALING_1_1));
        selectList.push_back(std::make_pair(HID_MOUSE_HOST_SCALING_1_2_NAME,  HID::HID_MOUSE_HOST_SCALING_1_2));
        selectList.push_back(std::make_pair(HID_MOUSE_HOST_SCALING_1_3_NAME,  HID::HID_MOUSE_HOST_SCALING_1_3));
        selectList.push_back(std::make_pair(HID_MOUSE_HOST_SCALING_1_4_NAME,  HID::HID_MOUSE_HOST_SCALING_1_4));
        selectList.push_back(std::make_pair(HID_MOUSE_HOST_SCALING_1_5_NAME,  HID::HID_MOUSE_HOST_SCALING_1_5));
    } 
    else if(option.compare(HID_MOUSE_SCALING_TYPE) == 0)
    {
        selectList.push_back(std::make_pair("ACTIVE",                         mouseConfig.mouse.scaling));
        selectList.push_back(std::make_pair(HID_MOUSE_SCALING_1_1_NAME,       HID::HID_MOUSE_SCALING_1_1));
        selectList.push_back(std::make_pair(HID_MOUSE_SCALING_2_1_NAME,       HID::HID_MOUSE_SCALING_2_1));
    } 
    else if(option.compare(HID_MOUSE_RESOLUTION_TYPE) == 0)
    {
        selectList.push_back(std::make_pair("ACTIVE",                         mouseConfig.mouse.resolution));
        selectList.push_back(std::make_pair(HID_MOUSE_RESOLUTION_1_1_NAME,    HID::HID_MOUSE_RESOLUTION_1_1));
        selectList.push_back(std::make_pair(HID_MOUSE_RESOLUTION_1_2_NAME,    HID::HID_MOUSE_RESOLUTION_1_2));
        selectList.push_back(std::make_pair(HID_MOUSE_RESOLUTION_1_4_NAME,    HID::HID_MOUSE_RESOLUTION_1_4));
        selectList.push_back(std::make_pair(HID_MOUSE_RESOLUTION_1_8_NAME,    HID::HID_MOUSE_RESOLUTION_1_8));
    } 
    else if(option.compare(HID_MOUSE_SAMPLING_TYPE) == 0)
    {
        selectList.push_back(std::make_pair("ACTIVE",                         mouseConfig.mouse.sampleRate));
        selectList.push_back(std::make_pair(HID_MOUSE_SAMPLE_RATE_10_NAME,    HID::HID_MOUSE_SAMPLE_RATE_10));
        selectList.push_back(std::make_pair(HID_MOUSE_SAMPLE_RATE_20_NAME,    HID::HID_MOUSE_SAMPLE_RATE_20));
        selectList.push_back(std::make_pair(HID_MOUSE_SAMPLE_RATE_40_NAME,    HID::HID_MOUSE_SAMPLE_RATE_40));
        selectList.push_back(std::make_pair(HID_MOUSE_SAMPLE_RATE_60_NAME,    HID::HID_MOUSE_SAMPLE_RATE_60));
        selectList.push_back(std::make_pair(HID_MOUSE_SAMPLE_RATE_80_NAME,    HID::HID_MOUSE_SAMPLE_RATE_80));
        selectList.push_back(std::make_pair(HID_MOUSE_SAMPLE_RATE_100_NAME,   HID::HID_MOUSE_SAMPLE_RATE_100));
        selectList.push_back(std::make_pair(HID_MOUSE_SAMPLE_RATE_200_NAME,   HID::HID_MOUSE_SAMPLE_RATE_200));
    } else
    {
        // Not found!
        result = false;
    }

    // Return result, false if the option not found, true otherwise.
    //
    return(result);
}

// Public method to set the mouse configuration parameters.
//
bool Mouse::setMouseConfigValue(std::string paramName, std::string paramValue)
{
    // Locals.
    //
    bool                  dataError = false;
    int                   value(0);
    std::stringstream     testVal(paramValue);

    // Match the parameter name to a known mouse parameter, type and data check the parameter value and assign to the config accordingly.
    if(paramName.compare(HID_MOUSE_HOST_SCALING_TYPE) == 0)
    {
        // Exception handling is disabled, stringstream is used to catch bad input.
        dataError = (static_cast<bool>(testVal >> value) ? false : true);
        if(dataError == false)
        {
            if(value >= to_underlying(HID::HID_MOUSE_HOST_SCALING_1_1) && value <= to_underlying(HID::HID_MOUSE_HOST_SCALING_1_5))
            {
                mouseConfig.host.scaling = static_cast<HID::HID_MOUSE_HOST_SCALING>(value);
                hid->setMouseHostScaling(mouseConfig.host.scaling);
            } else
            {
                dataError = true;
            }
        }
    }
    if(paramName.compare(HID_MOUSE_SCALING_TYPE) == 0)
    {
        dataError = (static_cast<bool>(testVal >> value) ? false : true);
        if(dataError == false)
        {
            if(value >= to_underlying(HID::HID_MOUSE_SCALING_1_1) && value <= to_underlying(HID::HID_MOUSE_SCALING_2_1))
            {
                mouseConfig.mouse.scaling = static_cast<HID::HID_MOUSE_SCALING>(value);
                hid->setMouseScaling(mouseConfig.mouse.scaling);
            } else
            {
                dataError = true;
            }
        }
    }
    if(paramName.compare(HID_MOUSE_RESOLUTION_TYPE) == 0)
    {
        dataError = (static_cast<bool>(testVal >> value) ? false : true);
        if(dataError == false)
        {
            if(value >= to_underlying(HID::HID_MOUSE_RESOLUTION_1_1) && value <= to_underlying(HID::HID_MOUSE_RESOLUTION_1_8))
            {
                mouseConfig.mouse.resolution = static_cast<HID::HID_MOUSE_RESOLUTION>(value);
                hid->setMouseResolution(mouseConfig.mouse.resolution);
            } else
            {
                dataError = true;
            }
        }
    }
    if(paramName.compare(HID_MOUSE_SAMPLING_TYPE) == 0)
    {
        dataError = (static_cast<bool>(testVal >> value) ? false : true);
        if(dataError == false)
        {
            if(value >= to_underlying(HID::HID_MOUSE_SAMPLE_RATE_10) && value <= to_underlying(HID::HID_MOUSE_SAMPLE_RATE_200))
            {
                mouseConfig.mouse.sampleRate = static_cast<HID::HID_MOUSE_SAMPLING>(value);
                hid->setMouseSampleRate(mouseConfig.mouse.sampleRate);
            } else
            {
                dataError = true;
            }
        }
    }

    // Error = true, success = false.
    return(dataError);
}

// Method to save (persist) the configuration into NVS RAM.
bool Mouse::persistConfig(void)
{
    // Locals.
    bool                  result = true;

    // Persist the data for next time.
    if(nvs->persistData(getClassName(__PRETTY_FUNCTION__), &this->mouseConfig, sizeof(t_mouseConfig)) == false)
    {
        ESP_LOGW(MAINTAG, "Persisting Mouse configuration data failed, check NVS setup.\n");
        result = false;
    }
    // Few other updates so make a commit here to ensure data is flushed and written.
    else if(nvs->commitData() == false)
    {
        ESP_LOGW(MAINTAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
    }

    // Request persistence in the HID module.
    result |= hid->persistConfig();
 
    // Error = false, success = true.
    return(result);
}

// Initialisation routine. Start two threads, one to handle the incoming PS/2 mouse data and map it, the second to handle the host interface.
void Mouse::init(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID)
{
    // Initialise control variables.
  #ifdef CONFIG_HOST_HW_UART
    hostControl.uartNum             = UART_NUM_2;
    hostControl.uartBufferSize      = 256;
    hostControl.uartQueueSize       = 10;
  #endif

    // Initialise the basic components.
    init(hdlNVS, hdlHID);

    // Invoke the prototype init which initialises common variables and devices shared by all subclass. 
    KeyInterface::init(getClassName(__PRETTY_FUNCTION__), hdlNVS, hdlLED, hdlHID, ifMode);

    // There are two build possibilities, hardware UART and BITBANG. I initially coded using hardware but whilst trying to find a bug, wrote a bitbang
    // technique and both are fit for purpose, so enabling either yields the same result.
  #ifdef CONFIG_HOST_HW_UART
    // Prepare the UART to be used for communications with the Sharp host.
    // The Sharp host Mouse uses an Asynchronous protocol with 2 stop bits no parity 4800 baud.
    //
    uart_config_t uartConfig        = {
        .baud_rate                  = 4800,
        .data_bits                  = UART_DATA_8_BITS,
        .parity                     = UART_PARITY_DISABLE,
        .stop_bits                  = UART_STOP_BITS_2,
        .flow_ctrl                  = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh        = 122,
        .source_clk                 = UART_SCLK_APB,
    };

    // Configure UART parameters and pin assignments, software flow control, not RTS/CTS.
    // The mouse only uses a Tx line, the MSCTRL line is used as a gate signal, so assign the Rx line to an unused pin.
    ESP_ERROR_CHECK(uart_param_config(hostControl.uartNum, &uartConfig));
    ESP_ERROR_CHECK(uart_set_pin(hostControl.uartNum, CONFIG_HOST_KDB1, CONFIG_HOST_KDB2, -1, -1));
    // Install UART driver. Use RX/TX buffers without event queue. 
    ESP_ERROR_CHECK(uart_driver_install(hostControl.uartNum, hostControl.uartBufferSize, hostControl.uartBufferSize, 0, NULL, 0));
  #endif
  
    // Register the streaming callback for the mouse, this will receive data, process it and send to the hostInterface for transmission to the host.
    hid->setDataCallback(&Mouse::mouseReceiveData, this);

    // Create a task pinned to core 1 which will fulfill the Sharp Mouse host interface. This task has the highest priority
    // and it will also hold spinlock and manipulate the watchdog to ensure a scan cycle timing can be met. This means 
    // all other tasks running on Core 1 will suspend as needed. The HID mouse controller will be serviced with core 0.
    //
    // Core 1 - Sharp Mouse Host Interface
    ESP_LOGW(MAINTAG, "Starting mouseIf thread...");
    ::xTaskCreatePinnedToCore(&this->hostInterface, "mouseIf", 4096, this, 25, &this->TaskHostIF, 1);
    vTaskDelay(500);
}

// Initialisation routine without hardware.
void Mouse::init(NVS *hdlNVS, HID *hdlHID)
{
    // Invoke the prototype init which initialises common variables and devices shared by all subclass. 
    KeyInterface::init(getClassName(__PRETTY_FUNCTION__), hdlNVS, hdlHID);

    // Retrieve configuration, if it doesnt exist, set defaults.
    //
    if(nvs->retrieveData(getClassName(__PRETTY_FUNCTION__), &this->mouseConfig, sizeof(t_mouseConfig)) == false)
    {
        ESP_LOGW(MAINTAG, "Mouse configuration set to default, no valid config in NVS found.");
        mouseConfig.mouse.resolution= HID::HID_MOUSE_RESOLUTION_1_8;
        mouseConfig.mouse.scaling   = HID::HID_MOUSE_SCALING_1_1;
        mouseConfig.mouse.sampleRate= HID::HID_MOUSE_SAMPLE_RATE_60;
        mouseConfig.host.scaling    = HID::HID_MOUSE_HOST_SCALING_1_2;

        // Persist the data for next time.
        if(nvs->persistData(getClassName(__PRETTY_FUNCTION__), &this->mouseConfig, sizeof(t_mouseConfig)) == false)
        {
            ESP_LOGW(MAINTAG, "Persisting Default Mouse configuration data failed, check NVS setup.\n");
        }
        // Few other updates so make a commit here to ensure data is flushed and written.
        else if(nvs->commitData() == false)
        {
            ESP_LOGW(MAINTAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
        }
    }
}

// Constructor, basically initialise the Singleton interface and let the threads loose.
Mouse::Mouse(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID)
{
    // Operating in uni-mode.
    hostControl.secondaryIf = false;

    // Initialise the interface
    init(ifMode, hdlNVS, hdlLED, hdlHID);
}

// Constructor, basic initialisation without hardware.
Mouse::Mouse(NVS *hdlNVS, HID *hdlHID)
{
    // Operating in uni-mode.
    hostControl.secondaryIf = false;

    // Initialise the interface
    init(hdlNVS, hdlHID);
}

// Constructor for use when mouse operates in tandem with a keyboard.
Mouse::Mouse(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID, bool secondaryIf)
{
    // The interface can act in primary mode, ie. sole interface or secondary mode where it acts in tandem to a keyboard host. Slight processing differences occur
    // in secondary mode, for example, the pin used to output mouse data differs.
    hostControl.secondaryIf = secondaryIf;

    // Initialise the interface
    init(ifMode, hdlNVS, hdlLED, hdlHID);
}

// Constructor, used for version reporting so no hardware is initialised.
Mouse::Mouse(void)
{
    return;
}

// Destructor - only ever called when the class is used for version reporting.
Mouse::~Mouse(void)
{
    return;
}
