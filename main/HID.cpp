/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            HID.cpp
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Final class for the encapsulation and presentation of differing input devices to
//                  an instantiating object for the provision of HID input services. This class 
//                  provides a public API which a caller uses to receive keyboard and mouse data.
//                  No other HID devices are planned at this time but given Bluetooth is being used,
//                  the potential exists for other devices to be used.
// Credits:         
// Copyright:       (c) 2019-2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Mar 2022 - Initial write.
//            v1.01 May 2022 - Initial release version.
//            v1.02 Jun 2022 - Updates to support Bluetooth keyboard and mouse. The mouse can be
//                             a primary device or a secondary device for hosts which support
//                             keyboard and mouse over one physical port.
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "driver/timer.h"
#include "PS2KeyAdvanced.h"
#include "PS2Mouse.h"
#include "sdkconfig.h"
#include "HID.h"

// Tag for ESP HID logging.
#define  HIDTAG  "HID"

// Out of object pointer needed in the ESP API callbacks.
HID      *pHIDThis = NULL;

// Method to start Bluetooth pairing.
// The SharpKey doesnt have an output device so it is not possible to select a device for pairing or allow pairing key input.
// This limits us to mainly BLE devices and some BT devices which dont require a pairing key.
// The method used is to scan and select the first device found which is in pairing mode. It will be the users responsibility
// to ensure no other Bluetooth devices are close by and pairing.
//
void HID::btStartPairing(void)
{
    // Locals.
    //
    int                             scanCnt = 0;
    std::vector<BT::t_scanListItem> scanList;

    // Only pair if bluetooth is enabled.
    if(hidCtrl.hidDevice == HID_DEVICE_BT_KEYBOARD || hidCtrl.hidDevice == HID_DEVICE_BT_MOUSE || hidCtrl.hidDevice == HID_DEVICE_BLUETOOTH)
    {
        ESP_LOGW(HIDTAG, "Bluetooth Pairing Requested\n");

        // Scan for a device in 5 second chunks, maximum 60 seconds before giving up.
        do {
            vTaskDelay(1);
            btHID->getDeviceList(scanList, 5);              // Required to discover new keyboards and for pairing
            scanCnt++;

            // For debug purposes, print out any device found.
            for(std::size_t idx = 0; idx < scanList.size(); idx++)
            {
                ESP_LOGI(HIDTAG, "We have device:%s, %s, %d, %s", scanList[idx].deviceAddr.c_str(), scanList[idx].name.c_str(), scanList[idx].rssi, scanList[idx].deviceType.c_str());
            }
            // If devices were found, try and open them until success or end of list.
            for(std::size_t idx = 0; idx < scanList.size(); idx++)
            {
                // Try and open the device, on failure move onto next device.
                if(btHID->openDevice(scanList[idx].bda, scanList[idx].transport, scanList[idx].ble.addr_type) == true)
                {
                    ESP_LOGI(HIDTAG, "BT enabled on device:%s, %s, %d, %s", scanList[idx].deviceAddr.c_str(), scanList[idx].name.c_str(), scanList[idx].rssi, scanList[idx].deviceType.c_str());
                }
            }
        } while(scanCnt < 11);
    } else
    {
        ESP_LOGW(HIDTAG, "Bluetooth Pairing disabled\n");
    }
    return;
}

// Method to set the suspend flag. This is needed as input functionality may clash with WiFi, especially Bluetooth.
//
void HID::suspendInterface(bool suspendIf)
{
    this->suspend = suspendIf;
// WAIT FOR MUTEX?
}

// Method to test to see if the interface has been suspended. 
// Two modes, one just tests and returns the state, the second waits in a loop until the interface suspends.
//
bool HID::isSuspended(bool waitForSuspend)
{
    // If flag set, loop waiting for the suspended flag to be set.
    while(waitForSuspend == true && this->suspended == false)
    {
        vTaskDelay(1);
    }

    // Return the suspended status.
    return(this->suspended);
}

// Method to read data from the underlying input device (keyboard).
//
uint16_t HID::read(void)
{
    // Locals.
    //
    uint16_t   result = 0;

    // Ensure we have exclusive access before reading the input device.
    if(hidCtrl.mutexInternal != NULL)
    {
        // Take controol of the semaphore to block all actions whilst reading key data. Other processes such as keyboard check and validation may
        // require access hence waiting for exclusive access.
        if(xSemaphoreTake(hidCtrl.mutexInternal, (TickType_t)100) == pdTRUE)
        {
            // Call the device method according to type.
            //
            switch(hidCtrl.hidDevice)
            {
                case HID_DEVICE_PS2_KEYBOARD:
                    // Get a 16bit code from the keyboard: [15:8] = Control bits, [7:0] = Data bits.
                    if((result = ps2Keyboard->read()) != 0)
                    { 
                        hidCtrl.ps2CheckTimer = xTaskGetTickCount();
                    } 
                    break;

                case HID_DEVICE_BLUETOOTH:
                case HID_DEVICE_BT_KEYBOARD:
                    // Get a 16bit code from the keyboard: [15:8] = Control bits, [7:0] = Data bits.
                    if((result = btHID->getKey(0)) != 0)
                    { 
                        hidCtrl.ps2CheckTimer = xTaskGetTickCount();
                    }
                    break;
        
                // Mouse processing is different, based on callbacks.
                case HID_DEVICE_TYPE_MOUSE:
                    break;
        
                default:
                    break;
            }
            
            // Release mutex, internal or external methods can now access the HID devices.
            xSemaphoreGive(hidCtrl.mutexInternal);
        }
    }

    // Return 0 if no data available otherwise the key or mouse code.
    return(result);
}

// Method to allow update of the mouse resolution. The config is updated and the device configured but the change is not persisted.
//
void HID::setMouseResolution(enum HID_MOUSE_RESOLUTION resolution)
{
    // Update the resolution in the config.
    hidConfig.mouse.resolution = resolution;

    // Set the updated flag to trigger an update.
    hidCtrl.updated = false;
}

// Method to allow update of the host side scaling. The config is updated and is actioned realtime. The change is not persisted.
//
void HID::setMouseHostScaling(enum HID_MOUSE_HOST_SCALING scaling)
{
    // Update the mouse scaling in the config.
    hidConfig.host.scaling = scaling;
}

// Method to allow update of the mouse scaling. The config is updated and the device configured but the change is not persisted.
//
void HID::setMouseScaling(enum HID_MOUSE_SCALING scaling)
{
    // Update the mouse scaling in the config.
    hidConfig.mouse.scaling = scaling;

    // Set the updated flag to trigger an update.
    hidCtrl.updated = false;
}

// Method to allow update of the mouse sample rate. The config is updated and the device configured but the change is not persisted.
//
void HID::setMouseSampleRate(enum HID_MOUSE_SAMPLING sampleRate)
{
    // Update the mouse sample rate in the config.
    hidConfig.mouse.sampleRate = sampleRate;

    // Set the updated flag to trigger an update.
    hidCtrl.updated = false;
}

// Method to detect if the PS2 Mouse is connected and/or re-initialise it.
// This method is called on startup to detect if a PS2 device is connected, if it isnt then Bluetooth is started.
// If a PS2 mouse is detected on startup then this method is called periodically to check for it being unplugged and reconnected
// initialising it as required.
// Returns: true - mouse connected, false - not detected.
//
bool HID::checkPS2Mouse(void)
{
    // Locals.
    //
    bool    result = false;
 
    // Ask the mouse for it's ID. No mouse connected or error = 0xFF.
    result = ps2Mouse->getDeviceId() == 0xFF ? false : true;

    // Return current status.
    return(result);
}

// Method to check the mouse, if it goes offline then perform reset and configuration once online. Also allow third wheel configuration
// of mouse parameters.
void HID::processPS2Mouse( void )
{
    // Locals.

    // PS/2 mouse check involves periodically requesting the device Id. If no device Id is returned, then reset the mouse until it responds and re-initialise.
    //
    // Ensure we have exclusive access before checking mouse.
    if(xSemaphoreTake(hidCtrl.mutexInternal, (TickType_t)10) == pdTRUE)
    {
        // If this mouse has gone offline (ie. unplugged), keep sending the RESET command until it becomes available.
        if(hidCtrl.active == false)
        {
            // Issue a reset, if we dont get an acknowledgement back then the mouse is offline.
            if(ps2Mouse->reset() == false)
            {
                vTaskDelay(100);
            } else
            {
                hidCtrl.active = true;   // Set active.
                hidCtrl.updated = true;  // Configure the mouse with latest settings.
    
                // As the mouse has been reset, update the Intelli Mouse configuration as a different mouse could have been plugged in.
                ps2Mouse->checkIntelliMouseExtensions();
                
                // Mouse is online now so reset the check counter.
                hidCtrl.noValidMouseMessage   = 0;
            }
        } else
        {
            // If the mouse configuration has changed, send the updated values.
            if(hidCtrl.updated)
            {
                hidCtrl.updated = false;
                ps2Mouse->setResolution((enum PS2Mouse::PS2_RESOLUTION)hidConfig.mouse.resolution);
                ps2Mouse->setScaling((enum PS2Mouse::PS2_SCALING)hidConfig.mouse.scaling);
                ps2Mouse->setSampleRate((enum PS2Mouse::PS2_SAMPLING)hidConfig.mouse.sampleRate);
            }

            // Read mouse data. This triggers a callback if data is available.
            ps2Mouse->readData();
            
            // Keep a count of the number of times no valid message is received, reset when a valid message is received. This is used to determine if the mouse has gone
            // offline. If the counter goes above a threshold then request the mouse ID, if it is not sent, mouse is offline.
            if(hidCtrl.noValidMouseMessage++ > MAX_MOUSE_INACTIVITY_TIME)
            {
                // Check to see if the mouse is online.
                if(checkPS2Mouse())
                {
                    // Mouse is online just not being used.
                    hidCtrl.noValidMouseMessage   = 0;
                } else
                {
                    hidCtrl.active = false;
                }
            }
        }
      
        // Release mutex, external access now possible to the input devices.
        xSemaphoreGive(hidCtrl.mutexInternal);
    }

    // Done!
    return;
}

// Callback to process mouse data originating from a PS/2 or Bluetooth mouse. The data is encapsulated in a PS/2
// message and processed into a host message.
//
void HID::mouseReceiveData(uint8_t src, PS2Mouse::MouseData mouseData)
{
    // Locals.
    //
    uint32_t              loopTime = (milliSeconds() - hidCtrl.loopTimer)/1000;
    t_mouseMessageElement mouseMsg;

    ESP_LOGD(HIDTAG, "Valid:%d, Overrun:%d, Status:%d, X:%d, Y:%d, Wheel:%d", mouseData.valid, mouseData.overrun, mouseData.status, mouseData.position.x, mouseData.position.y, mouseData.wheel);

    // Check the loop timer and set the blink rate according to the mode which is determined by the range of the loop timer.
    if((hidCtrl.mouseData.status & 0x04) == 0 && hidCtrl.middleKeyPressed == true && hidCtrl.configMode == HOST_CONFIG_OFF)
    {
        // Do nothing, time exceeded to configuration cancelled.
        if(loopTime >= 4 * hidConfig.params.optionAdvanceDelay)
        {
            led->setLEDMode(LED::LED_MODE_ON, LED::LED_DUTY_CYCLE_OFF, 0, 0L, 0L);
        } else
        // Approx 2 times the delay setting stored in the config.
        if(loopTime >= 2 * hidConfig.params.optionAdvanceDelay)
        {
            led->setLEDMode(LED::LED_MODE_BLINK, LED::LED_DUTY_CYCLE_30, (uint32_t)(hidConfig.mouse.resolution)+1, 250000L, 1000L);
        } else
        // First configuration to be selected in the first optionAdvanceDelay/HID_MOUSE_DATA_POLL_DELAY seconds.
        if(loopTime >= 1)
        {
            led->setLEDMode(LED::LED_MODE_BLINK, LED::LED_DUTY_CYCLE_20, (uint32_t)(hidConfig.host.scaling)+1, 150000L, 1000L);
        } 
    }

    // Copy data into control structure as it is needed by the update process above.
    memcpy((void *)&hidCtrl.mouseData, (void *)&mouseData, sizeof(PS2Mouse::MouseData));

    // Process data if valid - normally the case on a callback but could be an overrun occurred invalidating the data.
    if(hidCtrl.mouseData.valid)
    {
        // If configuration mode is enabled then the wheel value is used to increment/decrement an option value.
        //
        int16_t wheel = -(((hidCtrl.mouseData.wheel & 0x80) ? 0xFF80 : 0x0000) | (hidCtrl.mouseData.wheel & 0x7F));
        if(hidCtrl.configMode != HOST_CONFIG_OFF)
        {
            hidCtrl.wheelCnt += wheel;
            if(hidCtrl.wheelCnt > 4)
            {
                if(hidCtrl.configMode == HOST_CONFIG_SCALING)
                {
                    hidConfig.host.scaling = static_cast<HID_MOUSE_HOST_SCALING>(static_cast<int>(hidConfig.host.scaling) + 1);
                } else
                if(hidCtrl.configMode == HOST_CONFIG_RESOLUTION)
                {
                    hidConfig.mouse.resolution = static_cast<HID::HID_MOUSE_RESOLUTION>(static_cast<int>(hidConfig.mouse.resolution) + 1);
                }
            }
            if(hidCtrl.wheelCnt < -4)
            {
                if(hidCtrl.configMode == HOST_CONFIG_SCALING)
                {
                    hidConfig.host.scaling = static_cast<HID_MOUSE_HOST_SCALING>(static_cast<int>(hidConfig.host.scaling) - 1);
                } else
                if(hidCtrl.configMode == HOST_CONFIG_RESOLUTION)
                {
                    hidConfig.mouse.resolution = static_cast<HID::HID_MOUSE_RESOLUTION>(static_cast<int>(hidConfig.mouse.resolution) - 1);
                }
            }
            if(hidCtrl.wheelCnt < -4 || hidCtrl.wheelCnt > 4)
            {
                if(hidCtrl.configMode == HOST_CONFIG_SCALING)
                {
                    if(hidConfig.host.scaling > HID::HID_MOUSE_HOST_SCALING_1_5) hidConfig.host.scaling = HID::HID_MOUSE_HOST_SCALING_1_5;
                    if(hidConfig.host.scaling < HID::HID_MOUSE_HOST_SCALING_1_1) hidConfig.host.scaling = HID::HID_MOUSE_HOST_SCALING_1_1;
                    led->setLEDMode(LED::LED_MODE_BLINK, LED::LED_DUTY_CYCLE_20, static_cast<int>(hidConfig.host.scaling)+1, 150000L, 1000L);
                } else
                if(hidCtrl.configMode == HOST_CONFIG_RESOLUTION)
                {
                    if(hidConfig.mouse.resolution > HID::HID_MOUSE_RESOLUTION_1_8) hidConfig.mouse.resolution = HID::HID_MOUSE_RESOLUTION_1_8;
                    if(hidConfig.mouse.resolution < HID::HID_MOUSE_RESOLUTION_1_1) hidConfig.mouse.resolution = HID::HID_MOUSE_RESOLUTION_1_1;
                    led->setLEDMode(LED::LED_MODE_BLINK, LED::LED_DUTY_CYCLE_30, static_cast<int>(hidConfig.mouse.resolution)+1, 250000L, 1000L);
                    hidCtrl.updated = true;
                }
                hidCtrl.wheelCnt = 0;
            }
        }
    
        // If the middle key has been pressed, reset the timer and set the flag.
        if((hidCtrl.mouseData.status & 0x04) && hidCtrl.middleKeyPressed == false)
        {
            hidCtrl.loopTimer = milliSeconds();
            hidCtrl.middleKeyPressed = true;
            led->setLEDMode(LED::LED_MODE_OFF, LED::LED_DUTY_CYCLE_OFF, 0, 0L, 0L);
        }
        // When the key has been released the timer can be used to decide on function required.
        if((hidCtrl.mouseData.status & 0x04) == 0 && hidCtrl.middleKeyPressed == true && loopTime >= 1)
        {
            // If the middle button is set we start configuration mode. This entails the wheel value being used to select the scaling required and the LED blink rate indicates
            // the mode to the user. When the middle button is clicked a second time the configuration is disabled.
            if(hidCtrl.configMode == HOST_CONFIG_OFF)
            {
                if(loopTime >= 1 && loopTime < 2 * hidConfig.params.optionAdvanceDelay)
                {
                    hidCtrl.configMode = HOST_CONFIG_SCALING;
                    led->setLEDMode(LED::LED_MODE_BLINK, LED::LED_DUTY_CYCLE_20, (uint32_t)(hidConfig.host.scaling)+1, 150000L, 1000L);
                } else
                if(loopTime >= 2 * hidConfig.params.optionAdvanceDelay && loopTime < 4 * hidConfig.params.optionAdvanceDelay)
                {
                    hidCtrl.configMode = HOST_CONFIG_RESOLUTION;
                    led->setLEDMode(LED::LED_MODE_BLINK, LED::LED_DUTY_CYCLE_30, (uint32_t)(hidConfig.mouse.resolution)+1, 250000L, 1000L);
                } else
                // If the button is held too long, do nothing, configuration mode cancelled.
                if(loopTime >= 4 * hidConfig.params.optionAdvanceDelay)
                {
                }
            } else
            if(hidCtrl.configMode != HOST_CONFIG_OFF)
            {
                hidCtrl.configMode = HOST_CONFIG_OFF;
    
                // Persist the changes.
                persistConfig();
    
                // Turn off LED as we have exitted configuration mode.
                led->setLEDMode(LED::LED_MODE_ON, LED::LED_DUTY_CYCLE_OFF, 0, 0L, 0L);
            }
            hidCtrl.loopTimer = milliSeconds();
            hidCtrl.middleKeyPressed = false;
        }
      
        // Build the next message with all data, scaled and filtered as necessary.
        // Firstly, for PS/2 extend the X,Y 9bit movement values into 16bit for easier manipulation.
        if(src == 0)
        {
            mouseMsg.xPos = (((hidCtrl.mouseData.status & 0x10) ? 0xFF00 : 0x0000) | hidCtrl.mouseData.position.x);
            mouseMsg.yPos = (((hidCtrl.mouseData.status & 0x20) ? 0xFF00 : 0x0000) | hidCtrl.mouseData.position.y);
        } else
        {
            mouseMsg.xPos = hidCtrl.mouseData.position.x / 16;
            mouseMsg.yPos = hidCtrl.mouseData.position.y / 16;
        }

        switch(hidConfig.mouse.resolution)
        {
            case HID_MOUSE_RESOLUTION_1_1:
                mouseMsg.xPos = mouseMsg.xPos / 8;
                mouseMsg.yPos = mouseMsg.yPos / 8;
                break;

            case HID_MOUSE_RESOLUTION_1_2:
                mouseMsg.xPos = mouseMsg.xPos / 4;
                mouseMsg.yPos = mouseMsg.yPos / 4;
                break;

            case HID_MOUSE_RESOLUTION_1_4:
                mouseMsg.xPos = mouseMsg.xPos / 2;
                mouseMsg.yPos = mouseMsg.yPos / 2;
                break;

            case HID_MOUSE_RESOLUTION_1_8:
            default:
                mouseMsg.xPos = mouseMsg.xPos / 1;
                mouseMsg.yPos = mouseMsg.yPos / 1;
                break;
        }

        // Perform any in-situ scaling and adjustments.
        switch(hidConfig.host.scaling)
        {
            case HID::HID_MOUSE_HOST_SCALING_1_2:
                mouseMsg.xPos = mouseMsg.xPos / 2;
                mouseMsg.yPos = mouseMsg.yPos / 2;
                break;
    
            case HID::HID_MOUSE_HOST_SCALING_1_3:
                mouseMsg.xPos = mouseMsg.xPos / 3;
                mouseMsg.yPos = mouseMsg.yPos / 3;
                break;
    
            case HID::HID_MOUSE_HOST_SCALING_1_4:
                mouseMsg.xPos = mouseMsg.xPos / 4;
                mouseMsg.yPos = mouseMsg.yPos / 4;
                break;
    
            case HID::HID_MOUSE_HOST_SCALING_1_5:
                mouseMsg.xPos = mouseMsg.xPos / 5;
                mouseMsg.yPos = mouseMsg.yPos / 5;
                break;
    
            // No scaling needed for 1:1, the data is clipped at 8bit 2's compliment threshold and overflow/underflow set accordingly.
            case HID::HID_MOUSE_HOST_SCALING_1_1:
            default:
                break;
        }

        // Add in status and wheel data to complete message.
        //
        mouseMsg.status = hidCtrl.mouseData.status;
        mouseMsg.wheel  = hidCtrl.mouseData.wheel;
     
        // If a data callback has been setup, invoke otherwise data is wasted.
        //
        if(hidCtrl.dataCallback != NULL)
            hidCtrl.dataCallback(mouseMsg);

        // Reset the mouse activity check counter.
        hidCtrl.noValidMouseMessage   = 0;
    }
}

// Method to check and process the Bluetooth mouse which operates slightly differently to the PS/2 Mouse.
// Data arriving over a BT connection is queued and we read and process it, invoking the application callback for sending the mouse data
// to the host.
// The Bluetooth HAL is responsible for maintaining a connection and if it goes offline, it will be closed. We detect this and invoke an open
// until it comes back online.
//
void HID::checkBTMouse( void )
{
    // Locals.
    
    // One common function for BT. The protocol manages checks and reconnections but should a device go out of range we initiate 
    // a new scan and connect.
    btHID->checkBTDevices();
    
    // If the mouse configuration has changed, send the updated values to the BTHID.
    if(hidCtrl.updated)
    {
        hidCtrl.updated = false;
        btHID->setResolution((enum PS2Mouse::PS2_RESOLUTION)hidConfig.mouse.resolution);
        btHID->setScaling((enum PS2Mouse::PS2_SCALING)hidConfig.mouse.scaling);
        btHID->setSampleRate((enum PS2Mouse::PS2_SAMPLING)hidConfig.mouse.sampleRate);
    }
       
    // Done!
    return;
}

// Method to detect if the PS2 Keyboard is connected and/or re-initialise it.
// This method is called on startup to detect if a PS2 device is connected if it isnt then Bluetooth is started.
// If a PS2 keyboard is detected on startup then this method is called periodically to check for it being unplugged and reconnected
// initialising it as required.
// Returns: true - keyboard connected, false - not detected.
//
bool HID::checkPS2Keyboard(void)
{
    // Locals.
    //
    uint16_t            scanCode      = 0x0000;

    // Check to see if the keyboard is still available, no keyboard = no point!!
    // Firstly, ping keyboard to see if it is there.
    ps2Keyboard->echo();              
    vTaskDelay(6);
    scanCode = ps2Keyboard->read();
    
    // If the keyboard doesnt answer back, then it has been disconnected.
    if( (scanCode & 0xFF) != PS2_KEY_ECHO && (scanCode & 0xFF) != PS2_KEY_BAT)
    {
        hidCtrl.noEchoCount++;
    
        // Re-initialise the subsystem, if the keyboard is plugged in then it will be detected on next loop.
        if(hidCtrl.noEchoCount > 5) ps2Keyboard->begin(CONFIG_PS2_HW_DATAPIN, CONFIG_PS2_HW_CLKPIN);
    
        // First entry print out message that the keyboard has disconnected.
        if(hidCtrl.noEchoCount == 10 && (hidCtrl.ps2Active == 1 || hidCtrl.ps2CheckTimer == 0))
        {
            // Turn on LED when keyboard is detached.
            led->setLEDMode(LED::LED_MODE_ON, LED::LED_DUTY_CYCLE_OFF, 0, 0L, 0L);

            ESP_LOGE(HIDTAG, "No PS2 keyboard detected, please connect.");
        }
        hidCtrl.ps2Active = 0;

        hidCtrl.ps2CheckTimer = xTaskGetTickCount(); // Check every second when offline.
    } else
    {
        // First entry after keyboard starts responding, print out message.
        if(hidCtrl.ps2Active == 0)
        {
            ESP_LOGW(HIDTAG, "PS2 keyboard detected and online.");
            hidCtrl.ps2Active = 1;
    
            // If indication was given that the keyboard has gone offline, issue a new message to show it is back online.
            // This coding is necessary due to KVM devices which can idle the PS/2 connection randomly or when another device such as the mouse is in use.
            if(hidCtrl.noEchoCount > 10)
            {
                ESP_LOGW(HIDTAG, "PS2 keyboard detected and online.");
               
                // Flash LED to indicate Keyboard recognised.
                led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_50, 5, 100000L, 0L);
            }
        }
        hidCtrl.noEchoCount = 0L;
        hidCtrl.ps2CheckTimer = xTaskGetTickCount();
    }

    // Return current status.
    return(hidCtrl.ps2Active);
}

// Method to verify keyboard connectivity. If the keyboard goes offline, once detected, it is re-initialised.
//
void HID::checkKeyboard( void )
{
    // Locals.

    switch(hidCtrl.hidDevice)
    {
        case HID_DEVICE_PS2_KEYBOARD:
            // PS/2 keyboard checks involve sending an echo and reading back the response. If no response is received then the keyboard is unplugged, once an echo returns then
            // re-initialise the keyboard so it continues to function.
            //
            // Check the keyboard is online, this is done at startup and periodically to cater for user disconnect.
            if((xTaskGetTickCount() - hidCtrl.ps2CheckTimer) > 1000 && (ps2Keyboard->keyAvailable() == 0 || hidCtrl.ps2Active == 0))
            {
                // Ensure we have exclusive access before checking keyboard.
                if(xSemaphoreTake(hidCtrl.mutexInternal, (TickType_t)10) == pdTRUE)
                {
                    // Check if the PS/2 keyboard is available.
                    checkPS2Keyboard();
               
                    // Release mutex, external access now possible to the input devices.
                    xSemaphoreGive(hidCtrl.mutexInternal);
                }
            }
            break;

        case HID_DEVICE_BLUETOOTH:
        case HID_DEVICE_BT_KEYBOARD:
            // Bluetooth checks involve reconnection on closed handles, ie. when keyboard goes out of range or is switched off, keep retrying to connect.
            if(hidCtrl.hidDevice == HID_DEVICE_BT_KEYBOARD)
            {
                btHID->checkBTDevices();
            }
            break;

        default:
            break;
    }

    // Done.
    return;
}

// Method to verify mouse connectivity and process any pending updates/changes.
//
void HID::checkMouse(void)
{
    // Locals.

    switch(hidCtrl.hidDevice)
    {
        case HID_DEVICE_PS2_MOUSE:
            // Process any data and check PS/2 mouse status.
            processPS2Mouse();
            break;

        case HID_DEVICE_BLUETOOTH:
        case HID_DEVICE_BT_MOUSE:
            // Process any pending setting updates and rescan for new device if current device goes out of range.
            checkBTMouse();
            break;

        default:
            break;
    }

    // Done.
    return;
}

// Method to manage and maintain input device connectivity.
//
IRAM_ATTR void HID::hidControl( void * pvParameters )
{
    // Locals.
    //
    int         checkCnt = 0;
    #define     HIDCTRLTAG  "hidControl"

    // Map the instantiating object so we can access its methods and data.
    HID* pThis = (HID*)pvParameters;
   
    // Infinite loop, performing maintenance and control checks.
    while(true)
    {
        // Run through the checks, first keyboard. Assumes PS/2 keyboard or singular Bluetooth keyboard.
        if(pThis->hidCtrl.deviceType == HID_DEVICE_TYPE_KEYBOARD)
        {
            // Check keyboard.
            pThis->checkKeyboard();
           
            // Relinquish CPU for other tasks.
            vTaskDelay(100);
        }
        // Scan mouse if enabled. Assumes PS/2 mouse or singular Bluetooth mouse.
        else if(pThis->hidCtrl.deviceType == HID_DEVICE_TYPE_MOUSE)
        {
            // Check mouse.
            pThis->checkMouse();

            // Yield if the suspend flag is set.
            vTaskDelay(HID_MOUSE_DATA_POLL_DELAY);
        }
        // Scan bluetooth mouse and keyboard as one or both can be active.
        else if(pThis->hidCtrl.deviceType == HID_DEVICE_TYPE_BLUETOOTH)
        {
            // Check keyboard. Mouse needs more frequent checking so we base delay on mouse period.
            if(checkCnt-- == 0)
            {
                pThis->checkKeyboard();
                checkCnt = 100/HID_MOUSE_DATA_POLL_DELAY;
            }
        
            // Check mouse.
            pThis->checkMouse();

            // Yield if the suspend flag is set.
            vTaskDelay(HID_MOUSE_DATA_POLL_DELAY);
        }

        // Check stack space, report if it is getting low.
        if(uxTaskGetStackHighWaterMark(NULL) < 1024)
        {
            ESP_LOGW(HIDCTRLTAG, "THREAD STACK SPACE(%d)\n",uxTaskGetStackHighWaterMark(NULL));
        }
    }
    return;
}

// Testing pairing method. The security has been disabled so this method shouldnt be called.
//
void HID::btPairingHandler(uint32_t pid, uint8_t trigger)
{
    // Trigger indicates which part of the BT/BLE stack triggered the password request.
    // BT: 1
    // BT AUTH: 2
    // BLE: 3
    // BLE shouldnt require a pin as the stack has been setup to authorise without pin, BT may require a PIN and so a BT AUTH callback will be made. 
    // BT AUTH pid = status, if 0 then successful connection, no pin, if 9 then FAIL, so raise an alert via the LED that a PIN is required for this device.
    switch(trigger)
    {
        case 1:
            std::cout << "Please enter the following pairing code, "
                      << std::endl
                      << "followed with ENTER on your keyboard: "
                      << pid
                      << std::endl;           
            ESP_LOGE(HIDTAG, "Password request for BT pairing device, normally this should be AUTH, please log details.");
            break;
        case 2:
            if(pid == 0)
            {
                pHIDThis->led->setLEDMode(LED::LED_MODE_OFF, LED::LED_DUTY_CYCLE_OFF, 0, 0L, 0L);
            } 
            else if(pid == 9)
            {
                pHIDThis->led->setLEDMode(LED::LED_MODE_BLINK, LED::LED_DUTY_CYCLE_80, 3, 250000L, 1000L);
            }
            break;
        case 3:
        default:
            ESP_LOGE(HIDTAG, "Password request for pairing device. Auth disabled so this shouldnt occur, please log details.");
            break;
    }
}

// Method to see if the enabled underlying HID device is Bluetooth.
//
bool HID::isBluetooth(void)
{
    return(hidCtrl.hidDevice == HID_DEVICE_BT_KEYBOARD || hidCtrl.hidDevice == HID_DEVICE_BT_MOUSE || hidCtrl.hidDevice == HID_DEVICE_BLUETOOTH);
}

// Method to re-initialise the bluetooth subsystem after being disabled.
// At the moment this is a stub because WiFi is used for configuration and once complete a reboot takes place.
void HID::enableBluetooth(void)
{
    if(isBluetooth())
    {
    }
    return;
}

// Method to disable the bluetooth subsystem. This is necessary if WiFi is required as the two wireless devices share the same
// antenna and dont coexist very well. 
void HID::disableBluetooth(void)
{
    if(isBluetooth())
    {
        // Disable and de-initialse BT and BLE to free up the antenna.
        //
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
    }
    return;
}

// Method to persist the current configuration into NVS storage.
//
bool HID::persistConfig(void)
{
    // Locals
    bool     result = true;;

    // Update persistence with changed data.
    if(nvs->persistData(getClassName(__PRETTY_FUNCTION__), &hidConfig, sizeof(t_hidConfig)) == false)
    {
        ESP_LOGW(HIDTAG, "Persisting Mouse configuration data failed, updates will not persist in future power cycles.");
        led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_10, 200, 1000L, 0L);
        result = false;
    }
    // Few other updates so make a commit here to ensure data is flushed and written.
    else if(nvs->commitData() == false)
    {
        ESP_LOGW(HIDTAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
        led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_10, 200, 500L, 0L);
        result = false;
    }

    // Return result, true = success.
    return(result);
}

// Base initialisation for generic HID hardware used.
void HID::init(const char *className, enum HID_DEVICE_TYPES deviceType)
{
    // Locals
    #define   INITTAG "init"

    // Initialise variables.
    hidCtrl.mutexInternal = NULL;
    hidCtrl.dataCallback  = NULL;
    hidCtrl.configMode    = HOST_CONFIG_OFF;
    hidCtrl.loopTimer     = milliSeconds();

    // Retrieve configuration, if it doesnt exist, set defaults.
    //
    if(nvs->retrieveData(className, &this->hidConfig, sizeof(t_hidConfig)) == false)
    {
        ESP_LOGW(INITTAG, "HID configuration set to default, no valid config in NVS found.");
        hidConfig.mouse.resolution           = HID_MOUSE_RESOLUTION_1_8;
        hidConfig.mouse.scaling              = HID_MOUSE_SCALING_1_1;
        hidConfig.mouse.sampleRate           = HID_MOUSE_SAMPLE_RATE_60;
        hidConfig.host.scaling               = HID_MOUSE_HOST_SCALING_1_1;
        hidConfig.params.optionAdvanceDelay  = 1;

        // Persist the data for next time.
        if(nvs->persistData(className, &this->hidConfig, sizeof(t_hidConfig)) == false)
        {
            ESP_LOGW(INITTAG, "Persisting Default HID configuration data failed, check NVS setup.\n");
        }
        // Commit data, ensuring values are written to NVS and the mutex is released.
        else if(nvs->commitData() == false)
        {
            ESP_LOGW(INITTAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
        }
    }

    // Store the class name for later use, ie. NVS key access.
    this->className = className;

    // Initially I wanted PS/2 and Bluetooth to work in tandem. They do, sort of, but Bluetooth heavy resource usage and high priority
    // interferes with the PS/2 interrupt latency and consequently the PS/2 data can become corrupt. Also the Bluetooth stack is not that
    // stable especially with BLE (Logitech K780 keyboard when it connects will sometimes hang, if it disconnects it may hang reconnecting). Too many issues
    // so decided to seperate the logic, if a PS/2 device cannot be seen on startup it is disabled and bluetooth enabled.
    // This setup is for the primary device expected by the host. ie. If the host is detected as an X68000 then logically it needs a keyboard which is true for PS/2
    // as you can only have one PS/2 device connected at a time. For Bluetooth though, it is possible to have a keyboard and mouse connected and the X68000 interface
    // allows for both over one port, so if the host is detected as an X68000 it will check for a PS/2 keyboard, if not found it will enable Bluetooth and then the
    // logic can connect both a mouse and keyboard and channel them to the X68000.
    switch(deviceType)
    {
        case HID_DEVICE_TYPE_KEYBOARD:
        {
            // Instantiate the PS/2 Keyboard object and initialise.
            ESP_LOGW(INITTAG, "Initialise PS2 keyboard.");
            ps2Keyboard = new PS2KeyAdvanced();
            ps2Keyboard->begin(CONFIG_PS2_HW_DATAPIN, CONFIG_PS2_HW_CLKPIN);

            // If no PS/2 keyboard detected then default to Bluetooth.
            if(checkPS2Keyboard() == false)
            {
                // Remove the PS/2 keyboard object, free up memory and disable the interrupts.
                ESP_LOGW(INITTAG, "PS2 keyboard not available.");
                delete ps2Keyboard;
                hidCtrl.hidDevice = HID_DEVICE_BT_KEYBOARD;
              
                // Instantiate Bluetooth HID object.
                ESP_LOGW(INITTAG, "Initialise Bluetooth keyboard.");
                btHID = new BTHID();
                btHID->setup(btPairingHandler);
                sw->setBTPairingEventCallback(&HID::btStartPairing, this);

                // Setup a mouse callback as it is possible to receive mouse data when the primary input method is a keyboard. This data can be used by a registered
                // mouse interface to provide dual services to a host.
                btHID->setMouseDataCallback(&HID::mouseReceiveData, this);

                hidCtrl.deviceType = HID_DEVICE_TYPE_BLUETOOTH;
                hidCtrl.hidDevice = HID_DEVICE_BLUETOOTH;
            } else
            {
                hidCtrl.deviceType = HID_DEVICE_TYPE_KEYBOARD;
                hidCtrl.hidDevice = HID_DEVICE_PS2_KEYBOARD;
            }
            break;
        }

        case HID_DEVICE_TYPE_MOUSE:
        {
            // Instantiate the PS/2 Keyboard object and initialise.
            ESP_LOGW(INITTAG, "Initialise PS2 Mouse.");
            ps2Mouse = new PS2Mouse(CONFIG_PS2_HW_CLKPIN, CONFIG_PS2_HW_DATAPIN);
            ps2Mouse->initialize();

            // Test to see if a PS/2 Mouse is connected. IF it isnt, delete the PS/2 Mouse object and initiate a Bluetooth object.
            if(checkPS2Mouse() == false)
            {
                ESP_LOGW(INITTAG, "PS2 Mouse not available.");
                delete ps2Mouse;
                hidCtrl.deviceType = HID_DEVICE_TYPE_BLUETOOTH;
                hidCtrl.hidDevice = HID_DEVICE_BT_MOUSE;
                
                // Instantiate Bluetooth HID object.
                ESP_LOGW(INITTAG, "Initialise Bluetooth mouse.");
                btHID = new BTHID();
                btHID->setup(btPairingHandler);
                btHID->setMouseDataCallback(&HID::mouseReceiveData, this);
                sw->setBTPairingEventCallback(&HID::btStartPairing, this);
            } else
            {
                hidCtrl.deviceType = HID_DEVICE_TYPE_MOUSE;
                hidCtrl.hidDevice = HID_DEVICE_PS2_MOUSE;

                // Set the mouse to streaming mode so all movements generate data.
                ps2Mouse->setMouseDataCallback(&HID::mouseReceiveData, this);
                ps2Mouse->setStreamMode();
                ps2Mouse->enableStreaming();
                hidCtrl.hidDevice = HID_DEVICE_PS2_MOUSE;
            }
            break;
        }

        default:
            break;
    }

    // Setup mutex's.
    hidCtrl.mutexInternal = xSemaphoreCreateMutex();
    xSemaphoreGive(hidCtrl.mutexInternal);

    // Core 0 - Application
    // HID control thread.
    ESP_LOGW(HIDTAG, "Starting HID thread...");
    ::xTaskCreatePinnedToCore(&this->hidControl, "HID", 4096, this, 0, &this->TaskHID, 0);

    // All done, no return code!
    return;
}

// Constructor, Initialise interface.
HID::HID(enum HID_DEVICE_TYPES deviceType, NVS *hdlNVS, LED *hdlLED, SWITCH *hdlSWITCH)
{
    // Check for multiple instantiations, only one instance allowed.
    if(pHIDThis != nullptr)
    {
        // If the constructor to create an object with the underlying hardware is called more than once, flag it and set to a basic object as only one object can access 
        // hardware at a time.
        ESP_LOGE(HIDTAG, "Constructor called more than once. Only one instance of HID with hardware allowed.");
        this->nvs = hdlNVS;
        return;
    }

    // Store current object, used in ESP API callbacks (C based).
    pHIDThis = this;

    // Save the NVS object so we can persist and retrieve config data.
    this->nvs = hdlNVS;
   
    // Save the LED object so it can be used to warn the user.
    this->led = hdlLED;
   
    // Save the SWITCH object so it can be used to enable Bluetooth pairing.
    this->sw  = hdlSWITCH;

    init(getClassName(__PRETTY_FUNCTION__), deviceType);
}

// Basic constructor, no input device defined, just NVS for config retrieval and persistence.
HID::HID(NVS *hdlNVS)
{
    // Save the NVS object so we can persist and retrieve config data.
    this->nvs = hdlNVS;
}

// Basic constructor, do nothing! Used for probing versions etc.
HID::HID(void)
{
    //
}

// Basic destructor, do nothing! Only ever called for instantiation of uninitialsed class to prove version data.Used for probing versions etc.
HID::~HID(void)
{
    // Release object pointer if set.
    if(pHIDThis == this)
    {
        pHIDThis = NULL;
    }
}
