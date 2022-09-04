/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            LED.h
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Class definition for the control of a single LED. The LED is used to indicate to a
//                  user a desired status. This class is normally instantiated as a singleton and
//                  manipulated by public methods.
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

#ifndef LED_H
#define LED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <map>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "driver/timer.h"
#include "PS2KeyAdvanced.h"
#include "PS2Mouse.h"
#include "NVS.h"

// NB: Macros definitions put inside class for clarity, they are still global scope.

// Define a class to encapsulate a LED and required control mechanisms,
class LED  {

    // Macros.
    //
    #define NUMELEM(a)                  (sizeof(a)/sizeof(a[0]))

    // Constants.
    #define LED_VERSION                 1.01
    
    public:
        // Interface LED activity modes.
        enum LED_MODE {
            LED_MODE_OFF              = 0x00,
            LED_MODE_ON               = 0x01,
            LED_MODE_BLINK_ONESHOT    = 0x02,
            LED_MODE_BLINK            = 0x03,
        };

        // Interface LED duty cycle.
        enum LED_DUTY_CYCLE {
            LED_DUTY_CYCLE_OFF        = 0x00,
            LED_DUTY_CYCLE_10         = 0x01,
            LED_DUTY_CYCLE_20         = 0x02,
            LED_DUTY_CYCLE_30         = 0x03,
            LED_DUTY_CYCLE_40         = 0x04,
            LED_DUTY_CYCLE_50         = 0x05,
            LED_DUTY_CYCLE_60         = 0x06,
            LED_DUTY_CYCLE_70         = 0x07,
            LED_DUTY_CYCLE_80         = 0x08,
            LED_DUTY_CYCLE_90         = 0x09,
        };

        // Prototypes.
                                        LED(uint32_t hwPin);
                                        LED(void);
        virtual                        ~LED(void) {};
        void                            identify(void) { };

        // LED Control.
        bool                            setLEDMode(enum LED_MODE mode, enum LED_DUTY_CYCLE dutyCycle, uint32_t maxBlinks, uint64_t usDutyPeriod, uint64_t msInterPeriod);
        IRAM_ATTR static void           ledInterface(void *pvParameters);
        void                            ledInit(uint8_t ledPin);

        // Helper method to identify the sub class, this is used in non volatile key management.
        // Warning: This method wont work if optimisation for size is enabled on the compiler.
        const char *getClassName(const std::string& prettyFunction)
        {
            // First find the CLASS :: METHOD seperation.
            size_t colons = prettyFunction.find("::");
           
            // None, then this is not a class.
            if (colons == std::string::npos)
                return "::";
            
            // Split out the class name.
            size_t begin = prettyFunction.substr(0,colons).rfind(" ") + 1;
            size_t end = colons - begin;
          
            // Return the name.
            return(prettyFunction.substr(begin,end).c_str());
        }

        // Template to aid in conversion of an enum to integer.
        template <typename E> constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept
        {
            return static_cast<typename std::underlying_type<E>::type>(e);
        }

        // Method to return the class version number.
        virtual float version(void)
        {
            return(LED_VERSION);
        }

        // Method to return the name of the inferface class.
        virtual std::string ifName(void)
        {
            return(className);
        }

    protected:

    private:
        // Prototypes.

        // Structure to maintain configuration for the LED.
        // 
        typedef struct {
            bool                        valid;              // The configuration is valid and should be processed.
            bool                        updated;            // This configuration is an update to override current configuration.

            enum LED_MODE               mode;               // Mode of LED activity.
            enum LED_DUTY_CYCLE         dutyCycle;          // Duty cycle of the BLINK LED period.
            uint32_t                    maxBlinks;          // Maximum number of blinks before switching to LED off mode.
            uint64_t                    dutyPeriod;         // Period, is micro-seconds of the full duty cycle.
            uint64_t                    interPeriod;        // Period, is milli-seconds between LED activity.
        } t_ledConfig;

        // Structure to maintain an active setting for the LED. The LED control thread uses these values to effect the required lighting of the LED.
        typedef struct {
            // Current, ie. working LED config acted upon by the LED thread.
            t_ledConfig                 currentConfig;
            // New config to replace current on next state.
            t_ledConfig                 newConfig;

            // Led GPIO pin.
            uint8_t                     ledPin;

            // Runtime parameters for state machine and control.
            uint32_t                    blinkCnt;           // count of blink on periods.

            // Mutex to block access to limit one thread at a time.
            SemaphoreHandle_t           mutexInternal;
        } t_ledControl;

        // Variables to control the LED.
        t_ledControl                    ledCtrl;

        // Name of the class for this instantiation.
        std::string                     className;

        // Thread handle for the LED control thread.
        TaskHandle_t                    TaskLEDIF  = NULL;
};
#endif // LED_H
