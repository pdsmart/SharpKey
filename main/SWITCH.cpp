/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            SWITCH.cpp
// Created:         May 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Base class for encapsulating the SharpKey WiFi/Config switch.
// Credits:         
// Copyright:       (c) 2019-2022 Philip Smart <philip.smart@net2net.org>
//
// History:         May 2022 - Initial write.
//            v1.00 Jun 2022 - Updates to add additional callbacks for RESET and CLEARNVS
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
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "driver/timer.h"
#include "sdkconfig.h"
#include "SWITCH.h"

// Primary SWITCH thread, running on Core 0.
// This thread is responsible for scanning the config/WiFi key on the SharpKey and generating callbacks according to state.
//
IRAM_ATTR void SWITCH::swInterface( void * pvParameters )
{
    // Locals.
    //
    uint32_t          keyDebCtr     = 0;
    uint32_t          WIFIEN_MASK   = (1 << (CONFIG_IF_WIFI_EN_KEY - 32));
    uint32_t          resetTimer    = 0;
    #define           WIFIIFTAG       "swInterface"

    // Map the instantiating object so we can access its methods and data.
    SWITCH* pThis = (SWITCH*)pvParameters;

    // Loop indefinitely.
    while(true)
    {
        // Check the switch, has it gone to zero, ie. pressed?
        //
        if((REG_READ(GPIO_IN1_REG) & WIFIEN_MASK) == 0)
        {
            // First press detection turn LED off.
            if(keyDebCtr == 0)
            {
                pThis->led->setLEDMode(LED::LED_MODE_OFF, LED::LED_DUTY_CYCLE_OFF, 0, 0L, 0L);
            }
            // Entering WiFi enable mode, blink LED 
            if(keyDebCtr == 10)
            {
                pThis->led->setLEDMode(LED::LED_MODE_BLINK, LED::LED_DUTY_CYCLE_50, 1, 50000L, 500L);
            }
            // Enter default AP mode.
            if(keyDebCtr == 50)
            {
                pThis->led->setLEDMode(LED::LED_MODE_BLINK, LED::LED_DUTY_CYCLE_30, 1, 25000L, 250L);
            }
            // Enter BT pairing mode.
            if(keyDebCtr == 100)
            {
                pThis->led->setLEDMode(LED::LED_MODE_BLINK, LED::LED_DUTY_CYCLE_10, 1, 10000L, 100L);
            }
            // Enter Clear NVS settings mode.
            if(keyDebCtr == 150)
            {
                pThis->led->setLEDMode(LED::LED_MODE_BLINK, LED::LED_DUTY_CYCLE_80, 5, 10000L, 1000L);
            }
            // Increment counter so we know how long it has been held.
            keyDebCtr++;
        } else
        if((REG_READ(GPIO_IN1_REG) & WIFIEN_MASK) != 0 && keyDebCtr > 1)
        {
            // On first 1/2 second press, if WiFi active, disable and reboot.
            if(keyDebCtr > 1 && keyDebCtr < 10)
            {
                // If a cancel callback has been setup, invoke it.
                //
                if(pThis->swCtrl.cancelEventCallback != NULL)
                    pThis->swCtrl.cancelEventCallback();

                // If the reset timer is running then a previous button press occurred. If it is less than 1 second then a RESET event
                // is required.
                if(resetTimer != 0 && (pThis->milliSeconds() - resetTimer) < 1000L)
                {
                    // If a handler is installed call it. If the return value is true then a restart is possible. No handler then we just restart.
                    if(pThis->swCtrl.resetEventCallback != NULL)
                    {
                        if(pThis->swCtrl.resetEventCallback())
                            esp_restart();
                    } else
                        esp_restart();
                } else
                {
                    resetTimer = pThis->milliSeconds();
                }
            }
            // If counter is in range 1 to 4 seconds then assume a WiFi on (so long as the client parameters have been configured).
            else if(keyDebCtr > 10 && keyDebCtr < 40)
            {
                // If a wifi enable callback has been setup, invoke it.
                //
                if(pThis->swCtrl.wifiEnEventCallback != NULL)
                    pThis->swCtrl.wifiEnEventCallback();
            }
            // If the key is held for 5 or more seconds, then enter Wifi Config Default AP mode.
            else if(keyDebCtr > 50 && keyDebCtr < 100)
            {
                // If a wifi default enable callback has been setup, invoke it.
                //
                if(pThis->swCtrl.wifiDefEventCallback != NULL)
                    pThis->swCtrl.wifiDefEventCallback();
            } 
            // If the key is held for 10 seconds or more, invoke Bluetooth pairing mode.
            else if(keyDebCtr >= 100 && keyDebCtr < 150)
            {
                // If a bluetooth start pairing callback has been setup, invoke it.
                //
                if(pThis->swCtrl.btPairingEventCallback != NULL)
                    pThis->swCtrl.btPairingEventCallback();
            }
            // If the key is held for 15 seconds or more, invoke the clear NVS settings (factory) mode.
            else if(keyDebCtr >= 150)
            {
                // If a clear NVS handler has been installed, call it.
                //
                if(pThis->swCtrl.clearNVSEventCallback != NULL)
                    pThis->swCtrl.clearNVSEventCallback();
            }
           
            // LED off, no longer needed.
            pThis->led->setLEDMode(LED::LED_MODE_OFF, LED::LED_DUTY_CYCLE_OFF, 0, 0L, 0L);

            // Re-init switch variables for next activation.
            keyDebCtr = 0;
        }
      
        // Reset the reset timer if not activated.
        if(resetTimer != 0 && (pThis->milliSeconds() - resetTimer) > 2000L) { resetTimer = 0; }

        // Let other tasks run. NB. This value affects the debounce counter, update as necessary.
        vTaskDelay(100);
    }
    return;
}

// Initialisation routine. Setup variables and spawn a task to monitor the config switch.
// 
void SWITCH::init(void)
{
    // Initialise control variables.
    #define   SWINITTAG "SWINIT"
  
    // Core 0 - Application
    // SWITCH handler thread.
    ESP_LOGW(SWINITTAG, "Starting SWITCH thread...");
    ::xTaskCreatePinnedToCore(&this->swInterface, "switch", 4096, this, 0, &this->swCtrl.TaskSWIF, 0);
    vTaskDelay(1500);
}

// Basic constructor, init variables!
SWITCH::SWITCH(LED *hdlLED)
{
    swCtrl.cancelEventCallback    = NULL;
    swCtrl.wifiEnEventCallback    = NULL;
    swCtrl.wifiDefEventCallback   = NULL;
    swCtrl.btPairingEventCallback = NULL;

    // Store the class name for later use.
    this->swCtrl.swClassName = getClassName(__PRETTY_FUNCTION__);

    // Save the LED object so it can be used to warn the user.
    this->led = hdlLED;

    // Initialse the SWITCH object.
    init();
}

// Basic consructor, do nothing!
SWITCH::SWITCH(void)
{
    // Store the class name for later use.
    this->swCtrl.swClassName = getClassName(__PRETTY_FUNCTION__);
}

// Basic destructor.
SWITCH::~SWITCH(void)
{
}
