/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            LED.cpp
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Base class for the encapsulation and control methods of an LED used primarily to 
//                  indicate to users the status of the application.
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
#include "LED.h"

// Method to set the LED mode, duty cycle and duty period. Once the current LED cycle has come to an end, the control
// thread will replace the working configuration with the new configuration set here.
//
bool LED::setLEDMode(enum LED_MODE mode, enum LED_DUTY_CYCLE dutyCycle, uint32_t maxBlinks, uint64_t usDutyPeriod, uint64_t msInterPeriod)
{
    // Locals.
    //
    bool result = true;

    // If a setup is already waiting to be processed, exit with fail. This could be stacked into a vector but not really beneficial.
    if(ledCtrl.newConfig.updated == false)
    {
        // Ensure we have exclusive access, the LED can be controlled by numerous threads, so ensure only one can access and setup at a time.
        if(xSemaphoreTake(ledCtrl.mutexInternal, (TickType_t)1000) == pdTRUE)
        {
            ledCtrl.newConfig.mode        = mode;
            ledCtrl.newConfig.dutyCycle   = dutyCycle;
            ledCtrl.newConfig.maxBlinks   = maxBlinks;
            ledCtrl.newConfig.dutyPeriod  = usDutyPeriod;
            ledCtrl.newConfig.interPeriod = msInterPeriod;
            ledCtrl.newConfig.updated     = true;
           
            // Release mutex so other threads can set the LED.
            xSemaphoreGive(ledCtrl.mutexInternal);
        } else
        {
            result = false;
        }
    } else
    {
        result = false;
    }
    return(result);
}            

// Thread method to provide LED control.
IRAM_ATTR void LED::ledInterface(void *pvParameters)
{
    // Locals.
    //
    uint32_t            LED_MASK;
    uint64_t            delayTimer = 0LL;
    uint64_t            curTime    = 0LL;
    enum                LEDSTATE {
                            LEDSTATE_IDLE        = 0,
                            LEDSTATE_BLINK_MARK  = 1,
                            LEDSTATE_BLINK_SPACE = 2,
                            LEDSTATE_BLINK_INTER = 3,
                        } fsmState = LEDSTATE_IDLE;
    #define             LEDIFTAG "ledInterface"
    
    // Map the instantiating object so we can access its methods and data.
    LED* pThis = (LED*)pvParameters;

    // Set the LED GPIO mask according to the defined pin. This code needs updating if GPIO1 pins are used.
    LED_MASK = (1 << pThis->ledCtrl.ledPin);

    // Sign on.
    ESP_LOGW("ledInterface", "Starting LED control thread.");

    // Turn off LED.
    GPIO.out_w1tc = LED_MASK;

    // Loops forever.
    for(;;)
    {
        // Check stack space, report if it is getting low.
        if(uxTaskGetStackHighWaterMark(NULL) < 1024)
        {
            ESP_LOGW(LEDIFTAG, "THREAD STACK SPACE(%d)\n",uxTaskGetStackHighWaterMark(NULL));
        }

        // If a new configuration is set, then once the running FSM has returned to idle, update the configuration prior to the next FSM run.
        //
        if(pThis->ledCtrl.newConfig.updated)
        {
            // Take control of the Mutex so we are able to take on the data without a new setup clashing. If the Mutex is taken then continue on with the state machine logic till next loop.
            if(xSemaphoreTake(pThis->ledCtrl.mutexInternal, (TickType_t)1) == pdTRUE)
            {
                pThis->ledCtrl.currentConfig = pThis->ledCtrl.newConfig;
                pThis->ledCtrl.currentConfig.valid = true;
                pThis->ledCtrl.newConfig.updated = false;
                pThis->ledCtrl.blinkCnt = 0;

                // Got new setup so release mutex.
                xSemaphoreGive(pThis->ledCtrl.mutexInternal);
            }
        }

        // Only run if we have a valid configuration.
        if(pThis->ledCtrl.currentConfig.valid)
        {
            do {
                // Get the current timer value, only run the FSM when the timer is idle.
                timer_get_counter_value(TIMER_GROUP_0, TIMER_1, &curTime);
                if(curTime >= delayTimer)
                {
                    // Ensure the timer is stopped.
                    timer_pause(TIMER_GROUP_0, TIMER_1);
                    delayTimer = 0LL;

                    // Mini finite state machine for LED control.
                    switch(fsmState)
                    {
                        case LEDSTATE_IDLE:
                            // For on/off, no need for the FSM, just apply setting to LED and loop.
                            switch(pThis->ledCtrl.currentConfig.mode)
                            {
                                case LED_MODE_ON:
                                    // Turn on LED.
                                    GPIO.out_w1ts = LED_MASK;
                                    delayTimer = 1000UL;
                                    break;

                                case LED_MODE_BLINK_ONESHOT:
                                    // If the number of blinks is not 0 then on reaching the count, switch to LED off mode.
                                    if(pThis->ledCtrl.currentConfig.maxBlinks > 0 && pThis->ledCtrl.blinkCnt++ >= pThis->ledCtrl.currentConfig.maxBlinks)
                                    {
                                        pThis->ledCtrl.currentConfig.mode = LED_MODE_OFF;
                                    } else
                                    {
                                        fsmState = LEDSTATE_BLINK_MARK;
                                    }
                                    break;

                                case LED_MODE_BLINK:
                                    // Normal blink mode increments the count which is used for determining inter blink period.
                                    pThis->ledCtrl.blinkCnt++;
                                    fsmState = LEDSTATE_BLINK_MARK;
                                    break;

                                case LED_MODE_OFF:
                                default:
                                    // Turn off LED.
                                    GPIO.out_w1tc = LED_MASK;
                                    delayTimer = 1000UL;
                                    break;

                            }
                            break;

                        case LEDSTATE_BLINK_MARK:
                            // Turn on LED.
                            GPIO.out_w1ts = LED_MASK;
                            
                            // Next state, SPACE.
                            fsmState = LEDSTATE_BLINK_SPACE;

                            // Calculate time to SPACE.
                            switch(pThis->ledCtrl.currentConfig.dutyCycle)
                            {
                                case LED_DUTY_CYCLE_10:
                                    delayTimer = (pThis->ledCtrl.currentConfig.dutyPeriod / 10);
                                    break;
                                case LED_DUTY_CYCLE_20:
                                    delayTimer = ((pThis->ledCtrl.currentConfig.dutyPeriod / 10) * 2);
                                    break;
                                case LED_DUTY_CYCLE_30:
                                    delayTimer = ((pThis->ledCtrl.currentConfig.dutyPeriod / 10) * 3);
                                    break;
                                case LED_DUTY_CYCLE_40:
                                    delayTimer = ((pThis->ledCtrl.currentConfig.dutyPeriod / 10) * 4);
                                    break;
                                case LED_DUTY_CYCLE_50:
                                    delayTimer = ((pThis->ledCtrl.currentConfig.dutyPeriod / 10) * 5);
                                    break;
                                case LED_DUTY_CYCLE_60:
                                    delayTimer = ((pThis->ledCtrl.currentConfig.dutyPeriod / 10) * 6);
                                    break;
                                case LED_DUTY_CYCLE_70:
                                    delayTimer = ((pThis->ledCtrl.currentConfig.dutyPeriod / 10) * 7);
                                    break;
                                case LED_DUTY_CYCLE_80:
                                    delayTimer = ((pThis->ledCtrl.currentConfig.dutyPeriod / 10) * 8);
                                    break;
                                case LED_DUTY_CYCLE_90:
                                    delayTimer = ((pThis->ledCtrl.currentConfig.dutyPeriod / 10) * 9);
                                    break;
                                // We shouldnt be here if duty cycle is off, so back to idle.
                                case LED_DUTY_CYCLE_OFF:
                                default:
                                    GPIO.out_w1tc = LED_MASK;
                                    delayTimer = 0;
                                    fsmState = LEDSTATE_IDLE;
                                    break;
                            }
                            break;

                        case LEDSTATE_BLINK_SPACE:
                            // Turn off LED.
                            GPIO.out_w1tc = LED_MASK;

                            // Calculate time to next MARK.
                            delayTimer = pThis->ledCtrl.currentConfig.dutyPeriod - delayTimer;

                            // Now add an interblink delay prior to next blink.
                            fsmState = LEDSTATE_BLINK_INTER;
                            break;

                        case LEDSTATE_BLINK_INTER:
                            // If we are in normal mode with a blink limit set and limit reached or in limited mode, then add an interblink delay as configured.
                            if((pThis->ledCtrl.currentConfig.mode == LED_MODE_BLINK && pThis->ledCtrl.currentConfig.maxBlinks > 0 && pThis->ledCtrl.blinkCnt >= pThis->ledCtrl.currentConfig.maxBlinks) ||
                               (pThis->ledCtrl.currentConfig.mode == LED_MODE_BLINK_ONESHOT))
                            {
                                // Interblink delay is given in milli-seconds, so multiply up and set delay.
                                delayTimer = pThis->ledCtrl.currentConfig.interPeriod * 1000;

                                // Reset blink counter to trigger next interperiod delay.
                                if(pThis->ledCtrl.currentConfig.mode == LED_MODE_BLINK)
                                    pThis->ledCtrl.blinkCnt = 0;
                            }
                            
                            // We return to IDLE to allow time for reconfiguration if requested.
                            fsmState = LEDSTATE_IDLE;
                            break;

                        // Unknown or not programmed state, return to IDLE.
                        default:
                            fsmState = LEDSTATE_IDLE;
                            break;
                    }

                    // If a new delay is requested, reset the value in the timer and start.
                    if(delayTimer > 0LL)
                    {
                        timer_set_counter_value(TIMER_GROUP_0, TIMER_1, 0LL);
                        timer_start(TIMER_GROUP_0, TIMER_1);
                    }
                }

                // Give the OS some time...
                taskYIELD();
            } while(fsmState != LEDSTATE_IDLE);

        }
    }
}

// Method to set the GPIO pin to be used for LED output.
void LED::ledInit(uint8_t ledPin)
{
    // Initialise variables.
    this->ledCtrl.currentConfig.valid = false;
    this->ledCtrl.currentConfig.updated = false;
    this->ledCtrl.currentConfig.mode = LED_MODE_OFF;
    this->ledCtrl.currentConfig.dutyCycle = LED_DUTY_CYCLE_OFF;
    this->ledCtrl.currentConfig.dutyPeriod = 0LL;
    this->ledCtrl.currentConfig.interPeriod = 0LL;
    this->ledCtrl.newConfig = this->ledCtrl.currentConfig;

    // Store GPIO pin to which LED is connected.
    this->ledCtrl.ledPin = ledPin;

    // Configure a timer to be used for the LED blink rate.
    timer_config_t timerConfig = {
        .alarm_en    = TIMER_ALARM_DIS,            // No alarm, were not using interrupts as we are in a dedicated thread.
        .counter_en  = TIMER_PAUSE,                // Timer paused until required.
        .intr_type   = TIMER_INTR_LEVEL,           // No interrupts used.
        .counter_dir = TIMER_COUNT_UP,             // Timing a fixed period.
        .auto_reload = TIMER_AUTORELOAD_DIS,       // No need for auto reload, fixed time period.
        .divider     = 80                          // 1Mhz operation giving 1uS resolution.
    };
    ESP_ERROR_CHECK(timer_init(TIMER_GROUP_0, TIMER_1, &timerConfig));
    ESP_ERROR_CHECK(timer_set_counter_value(TIMER_GROUP_0, TIMER_1, 0));            

    // Setup mutex's.
    ledCtrl.mutexInternal = xSemaphoreCreateMutex();

    // Core 0 - Application
    // LED control thread - dedicated thread to control the LED according to set mode.
    ESP_LOGW("ledInit", "Starting LEDif thread...");
    ::xTaskCreatePinnedToCore(&this->ledInterface, "ledif", 4096, this, 0, &this->TaskLEDIF, 0);            
}

// Constructor, basically initialise the Singleton interface and let the control thread loose.
LED::LED(uint32_t hwPin)
{
    // Store the class name for later use, ie. NVS key access.
    this->className = getClassName(__PRETTY_FUNCTION__);

    // Configure the Power LED used for activity and user interaction. Initial state is ON until a keyboard is detected when it turns off and only blinks on keyboard activity.
    ledInit(hwPin);

    // Initial state, turn on LED to indicate LED control is working.
    setLEDMode(LED::LED_MODE_ON, LED::LED_DUTY_CYCLE_OFF, 0, 0L, 0L);
}

// Basic constructor, do nothing!
LED::LED(void)
{
    // Store the class name for later use, ie. NVS key access.
    this->className = getClassName(__PRETTY_FUNCTION__);
}
