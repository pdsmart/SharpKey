/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            NVS.h
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Class definition to encapsulate the Espressif Non Volatile Storage into a thread safe
//                  object, The underlying API is supposed to be thread safe but experience has shown
//                  that two threads, each with there own handle can cause a lockup.
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

#ifndef NVS_H
#define NVS_H

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
#include "nvs_flash.h"
#include "nvs.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "driver/timer.h"


// NB: Macros definitions put inside class for clarity, they are still global scope.

// Define a virtual class which acts as the base and specification of all super classes forming host
// interface objects.
class NVS  {

    // Macros.
    //
    #define NUMELEM(a)                  (sizeof(a)/sizeof(a[0]))

    // Constants.
    #define NVS_VERSION                 1.01
    
    public:

        // Prototypes.
                                        NVS(void);
                                        NVS(std::string keyName);
        virtual                        ~NVS(void) {};
        void                            eraseAll(void);
        void                            init(void);
        bool                            takeMutex(void);
        void                            giveMutex(void);   
        // Persistence.
        bool                            open(std::string keyName);
        bool                            persistData(const char *key, void *pData, uint32_t size);
        bool                            retrieveData(const char *key, void *pData, uint32_t size);
        bool                            commitData(void);

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
            return(NVS_VERSION);
        }

        // Method to return the name of the class.
        virtual std::string ifName(void)
        {
            return(nvsCtrl.nvsClassName);
        }
       
        // Method to return the name of the nvs key.
        virtual std::string keyName(void)
        {
            return(nvsCtrl.nvsKeyName);
        }

    protected:

    private:

        // Structure to maintain an active setting for the LED. The LED control thread uses these values to effect the required lighting of the LED.
        typedef struct {
            // Handle to the persistent storage api.
            nvs_handle_t                nvsHandle;
            
            // Name of the class for this instantiation.
            std::string                 nvsClassName;

            // Name of the key under which NVS was opened.
            std::string                 nvsKeyName;

            // Mutex to block access to limit one thread at a time.
            SemaphoreHandle_t           mutexInternal;
        } t_nvsControl;

        // Var to store all NVS control variables.
        t_nvsControl                    nvsCtrl;

};
#endif // NVS_H
