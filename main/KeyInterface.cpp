/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            KeyInterface.cpp
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Base class with virtual abstraction of key methods on which all host interfaces, 
//                  instantiated as a singleton, are based. This module comprises all common interface
//                  code and the header contains the virtual abstraction methods overriden by the
//                  sub-class which forms an actual interface.
// Credits:         
// Copyright:       (c) 2019-2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Mar 2022 - Initial write.
//            v1.01 May 2022 - Initial release version.
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
#include "KeyInterface.h"


// Method to reconfigure the GPIO on ADC2. This is necessary due to an ESP32 issue regarding WiFi Client mode and ADC2. If the
// pins are input and wrong value the WiFi Client mode wont connect, if they are output it will connect!! A few issues with the
// ESP32 you have to work around, next design, try use ADC2 as outputs only!
void KeyInterface::reconfigADC2Ports(bool setAsOutput)
{
    // Locals.
    //
    gpio_config_t            io_conf;

    // Configure 4 inputs to be the Strobe Row Number which is used to index the virtual key matrix and the strobe data returned.
    #if !defined(CONFIG_DEBUG_DISABLE_KDB)
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = (setAsOutput == true ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDB0); 
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = (setAsOutput == true ? GPIO_PULLUP_DISABLE : GPIO_PULLUP_ENABLE);
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDB1); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDB2); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDB3); 
        gpio_config(&io_conf);
    #endif

    #if !defined(CONFIG_DEBUG_DISABLE_KDI)
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = (setAsOutput == true ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDI4); 
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = (setAsOutput == true ? GPIO_PULLUP_DISABLE : GPIO_PULLUP_ENABLE);
        gpio_config(&io_conf);
    #endif

    #if !defined(CONFIG_DEBUG_DISABLE_MPXI)
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = (setAsOutput == true ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_MPXI); 
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = (setAsOutput == true ? GPIO_PULLUP_DISABLE : GPIO_PULLUP_ENABLE);
        gpio_config(&io_conf);
    #endif        

    // In output mode set the drive capability to weak so as not to adversely affect the 74LS257.
    if(setAsOutput == true)
    {
      #if !defined(CONFIG_DEBUG_DISABLE_KDB)
        gpio_set_drive_capability((gpio_num_t)CONFIG_HOST_KDB0, GPIO_DRIVE_CAP_0);
        gpio_set_drive_capability((gpio_num_t)CONFIG_HOST_KDB1, GPIO_DRIVE_CAP_0);
        gpio_set_drive_capability((gpio_num_t)CONFIG_HOST_KDB2, GPIO_DRIVE_CAP_0);
        gpio_set_drive_capability((gpio_num_t)CONFIG_HOST_KDB3, GPIO_DRIVE_CAP_0);
    #endif        
    #if !defined(CONFIG_DEBUG_DISABLE_KDI)
        gpio_set_drive_capability((gpio_num_t)CONFIG_HOST_KDI4, GPIO_DRIVE_CAP_0);
    #endif        
    #if !defined(CONFIG_DEBUG_DISABLE_MPXI)
        gpio_set_drive_capability((gpio_num_t)CONFIG_HOST_MPXI, GPIO_DRIVE_CAP_0);
    #endif        
    }
    return;
}

// Method to set the suspend flag. This is needed as some sub-class logic (ie. the MZ sub-class) locks and dedicates a core to meet
// required timing. Unfortunately if using some Espressif/Arduino/FreeRTOS API modules (such as WiFi) and a core is held in a spinlock 
// it appears the API code has been written to use or attach to a fixed core thus the spinlock blocks its operation. Thus this method is provided
// so that external logic such as WiFi can disable the interface during these situations.
//
void KeyInterface::suspendInterface(bool suspendIf)
{
    this->suspend = suspendIf;
}

// Method to test to see if the interface has been suspended. 
// Two modes, one just tests and returns the state, the second waits in a loop until the interface suspends.
//
bool KeyInterface::isSuspended(bool waitForSuspend)
{
    // If flag set, loop waiting for the suspended flag to be set.
    while(waitForSuspend == true && this->suspended == false)
    {
        vTaskDelay(1);
    }

    // Return the suspended status.
    return(this->suspended);
}

// Method to test to see if the interface is running and not suspended.
// Two modes, one just tests and returns the state, the second waits in a loop until the interface runs.
bool KeyInterface::isRunning(bool waitForRelease)
{
    // If flag set, loop waiting for the suspended flag to be set.
    while(waitForRelease == true && this->suspended == true)
    {
        vTaskDelay(1);
    }

    // Return the suspended status.
    return(this->suspended);
}

// Base initialisation for generic hardware used by all sub-classes. The sub-class invokes the init
// method manually from within it's init method.
void KeyInterface::init(const char *subClassName, NVS *hdlNVS, LED *hdlLED, HID *hdlHID, uint32_t ifMode)
{
    // Locals
    #define   INITTAG "init"

    // Store the NVS object.
    this->nvs = hdlNVS;
  
    // Store the LED object.
    this->led = hdlLED;

    // Store the HID object.
    this->hid = hdlHID;

    // Store the sub-class name for later use, ie. NVS key access.
    this->subClassName = subClassName;

    // Set LED to on.
    led->setLEDMode(LED::LED_MODE_ON, LED::LED_DUTY_CYCLE_OFF, 0, 0L, 0L);

    // All done, no return code!
    return;
}

// Base initialisation for generic hardware used by all sub-classes. The sub-class invokes the init
// method manually from within it's init method.
// This method doesnt initialise hardware, used for objects probing this object data.
void KeyInterface::init(const char *subClassName, NVS *hdlNVS, HID *hdlHID)
{
    // Locals
    #define   INITTAG "init"

    // Store the NVS object.
    this->nvs = hdlNVS;
  
    // Store the HID object.
    this->hid = hdlHID;
  
    // Store the sub-class name for later use, ie. NVS key access.
    this->subClassName = subClassName;

    // All done, no return code!
    return;
}


// Constructor, basically initialise the Singleton interface and let the threads loose.
//KeyInterface::KeyInterface(uint32_t ifMode)
//{
//  //  init(className(__PRETTY_FUNCTION__), ifMode);
//}

// Basic constructor, do nothing!
//KeyInterface::KeyInterface(void)
//{
//    //
//}
