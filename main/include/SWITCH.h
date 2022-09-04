/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            SWITCH.h
// Created:         May 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Class definition to encapsulate the SharpKey WiFi/Config Switch.
//
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

#ifndef SWITCH_H
#define SWITCH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "driver/timer.h"
#include "LED.h"


// NB: Macros definitions put inside class for clarity, they are still global scope.

// Switch class.
class SWITCH  {

    // Macros.
    //
    #define NUMELEM(a)                  (sizeof(a)/sizeof(a[0]))

    // Constants.
    #define SWITCH_VERSION              1.00
    
    public:

        // Prototypes.
                                        SWITCH(LED *led);
                                        SWITCH(void);
        virtual                        ~SWITCH(void);

        // Method to register an object method for callback with context.
        template<typename A, typename B>
        void setCancelEventCallback(A func_ptr, B obj_ptr)
        {
            swCtrl.cancelEventCallback = std::bind(func_ptr, obj_ptr);
        }
        template<typename A>
        void setCancelEventCallback(A func_ptr)
        {
            swCtrl.cancelEventCallback = std::bind(func_ptr);
        }
        // Wifi enable (configured mode).
        template<typename A, typename B>
        void setWifiEnEventCallback(A func_ptr, B obj_ptr)
        {
            swCtrl.wifiEnEventCallback = std::bind(func_ptr, obj_ptr);
        }
        template<typename A>
        void setWifiEnEventCallback(A func_ptr)
        {
            swCtrl.wifiEnEventCallback = std::bind(func_ptr);
        }
        // Wifi default mode enable.
        template<typename A, typename B>
        void setWifiDefEventCallback(A func_ptr, B obj_ptr)
        {
            swCtrl.wifiDefEventCallback = std::bind(func_ptr, obj_ptr);
        }
        template<typename A>
        void setWifiDefEventCallback(A func_ptr)
        {
            swCtrl.wifiDefEventCallback = std::bind(func_ptr);
        }
        // Bluetooth start pairing event.
        template<typename A, typename B>
        void setBTPairingEventCallback(A func_ptr, B obj_ptr)
        {
            swCtrl.btPairingEventCallback = std::bind(func_ptr, obj_ptr);
        }
        template<typename A>
        void setBTPairingEventCallback(A func_ptr)
        {
            swCtrl.btPairingEventCallback = std::bind(func_ptr);
        }
        // RESET event - callback is executed prior to issuing an esp_restart().
        template<typename A, typename B>
        void setResetEventCallback(A func_ptr, B obj_ptr)
        {
            swCtrl.resetEventCallback = std::bind(func_ptr, obj_ptr);
        }
        template<typename A>
        void setResetEventCallback(A func_ptr)
        {
            swCtrl.resetEventCallback = std::bind(func_ptr);
        }
        // CLEARNVS event - callback when user requests all NVS settings are erased.
        template<typename A, typename B>
        void setClearNVSEventCallback(A func_ptr, B obj_ptr)
        {
            swCtrl.clearNVSEventCallback = std::bind(func_ptr, obj_ptr);
        }
        template<typename A>
        void setClearNVSEventCallback(A func_ptr)
        {
            swCtrl.clearNVSEventCallback = std::bind(func_ptr);
        }

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

        // Helper method to change a file extension.
        void replaceExt(std::string& fileName, const std::string& newExt)
        {
            // Locals.
            std::string::size_type extPos = fileName.rfind('.', fileName.length());

            if(extPos != std::string::npos) 
            {
                fileName.replace(extPos+1, newExt.length(), newExt);
            }
            return;
        }

        // Template to aid in conversion of an enum to integer.
        template <typename E> constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept
        {
            return static_cast<typename std::underlying_type<E>::type>(e);
        }

        // Method to return the class version number.
        virtual float version(void)
        {
            return(SWITCH_VERSION);
        }

        // Method to return the name of the class.
        virtual std::string ifName(void)
        {
            return(swCtrl.swClassName);
        }
       
    protected:

    private:

        // Prototypes.
                         void               init(void);
        IRAM_ATTR static void               swInterface( void * pvParameters );
        inline uint32_t milliSeconds(void)
        {
            return( (uint32_t) clock() );
        }        

        // Structure to maintain an active setting for the LED. The LED control thread uses these values to effect the required lighting of the LED.
        typedef struct {
            // Name of the class for this instantiation.
            std::string                     swClassName;

            // Thread handles - Switch interface.
            TaskHandle_t                    TaskSWIF = NULL;

            // Callback for Cancel Event.
            std::function<void(void)>       cancelEventCallback;
           
            // Callback for WiFi Enable Event.
            std::function<void(void)>       wifiEnEventCallback;
           
            // Callback for WiFi Default Event.
            std::function<void(void)>       wifiDefEventCallback;
           
            // Callback for Bluetooth Pairing Event.
            std::function<void(void)>       btPairingEventCallback;

            // Callback is executed prior to issuing an esp_restart().
            std::function<bool(void)>       resetEventCallback;

            // Callback when user requests all NVS settings are erased.
            std::function<void(void)>       clearNVSEventCallback;
        } t_swControl;

        // Var to store all SWITCH control variables.
        t_swControl                         swCtrl;
     
        // LED activity object handle.
        LED                                *led;
};
#endif // SWITCH_H
