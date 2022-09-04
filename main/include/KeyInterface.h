/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            KeyInterface.h
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Virtual class definition on which all host interfaces, instantiated as a singleton,
//                  are based.
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

#ifndef KEYINTERFACE_H
#define KEYINTERFACE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <map>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "driver/timer.h"
#include "PS2KeyAdvanced.h"
#include "PS2Mouse.h"
#include "NVS.h"
#include "LED.h"
#include "HID.h"


// NB: Macros definitions put inside class for clarity, they are still global scope.

// Define a virtual class which acts as the base and specification of all super classes forming host
// interface objects.
class KeyInterface  {

    // Macros.
    //
    #define NUMELEM(a)                  (sizeof(a)/sizeof(a[0]))

    // Constants.
    #define KEYIF_VERSION               1.01
    
    public:
        // Suspend flag. When active, the interface components enter an idle state after completing there latest cycle.
        bool                            suspend = false;
        bool                            suspended = true;
      
        // NVS object.
        NVS                            *nvs;

        // LED object.
        LED                            *led;

        // HID object, used for keyboard input.
        HID                             *hid;

        // Prototypes.
                                        KeyInterface(void) {};
        virtual                        ~KeyInterface(void) {};
                                        KeyInterface(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID) { init(getClassName(__PRETTY_FUNCTION__), hdlNVS, hdlLED, hdlHID, ifMode); };
                                        KeyInterface(NVS *hdlNVS, HID *hdlHID) { init(getClassName(__PRETTY_FUNCTION__), hdlNVS, hdlHID); };
        void                            reconfigADC2Ports(bool setAsOutput);
        void                            suspendInterface(bool suspendIf);
        virtual bool                    isSuspended(bool waitForSuspend);
        virtual bool                    isRunning(bool waitForRelease);
        virtual void                    identify(void) { };
        virtual void                    init(const char * subClassName, NVS *hdlNVS, LED *hdlLED, HID *hdlHID, uint32_t ifMode);
        virtual void                    init(const char * subClassName, NVS *hdlNVS, HID *hdlHID);
        // Persistence.
        virtual bool                    persistConfig(void) { return(true); }

        // Key mapping.
        virtual IRAM_ATTR uint32_t      mapKey(uint16_t scanCode) { return(0); };
        virtual bool                    createKeyMapFile(std::fstream &outFile) { return(false); };
        virtual bool                    storeDataToKeyMapFile(std::fstream &outFile, char *data, int size) { return(false); };
        virtual bool                    storeDataToKeyMapFile(std::fstream & outFile, std::vector<uint32_t>& dataArray) { return(false); }
        virtual bool                    closeAndCommitKeyMapFile(std::fstream &outFile, bool cleanupOnly) { return(false); };
        virtual std::string             getKeyMapFileName(void) { return("nokeymap.bin"); };
        virtual void                    getKeyMapHeaders(std::vector<std::string>& headerList) { };
        virtual void                    getKeyMapTypes(std::vector<std::string>& typeList) { };
        virtual bool                    getKeyMapSelectList(std::vector<std::pair<std::string, int>>& selectList, std::string option) { return(true); }
        virtual bool                    getKeyMapData(std::vector<uint32_t>& dataArray, int *row, bool start) { return(true); };
        // Mouse config.
        virtual void                    getMouseConfigTypes(std::vector<std::string>& typeList) { };
        virtual bool                    getMouseSelectList(std::vector<std::pair<std::string, int>>& selectList, std::string option) { return(true); }
        virtual bool                    setMouseConfigValue(std::string paramName, std::string paramValue) { return(true); }
                                       
        // Method to suspend an active interface thread by holding in a tight loop, yielding to the OS. This method was chosen rather than the more conventional
        // vTaskSuspend as it allows multiple threads, without giving a handle, to yield if required for a fixed period or indefinitely until the suspend mode is de-activated.
        // The method is inline to avoid a call overhead as it is generally used in time sensitive interface timing.
        inline virtual void yield(uint32_t delay)
        {
            // If suspended, go into a permanent loop until the suspend flag is reset.
            if(this->suspend)
            {
                this->suspended = true;
                while(this->suspend)
                {
                    vTaskDelay(100);
                }
                this->suspended = false;
            } else
            // Otherwise just delay by the required amount for timing and to give other threads a time slice.
            {
                vTaskDelay(delay);
            }
        }

        // Method to see if the interface must enter suspend mode.
        //
        inline virtual bool suspendRequested(void)
        {
            return(this->suspend);
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
            return(KEYIF_VERSION);
        }

        // Method to return the name of the inferface class.
        virtual std::string ifName(void)
        {
            return(subClassName);
        }

    protected:

    private:
        // Prototypes.
        virtual IRAM_ATTR void          selectOption(uint8_t optionCode) {};

        // Name of the sub-class for this instantiation.
        std::string                     subClassName;

        // Thread handle for the LED control thread.
        TaskHandle_t                    TaskLEDIF  = NULL;
};
#endif // KEYINTERFACE_H
