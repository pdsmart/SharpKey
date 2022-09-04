/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            HID.h
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     A HID Class definition, used to instantiate differing input device classes and
//                  present a standard API.
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

#ifndef HID_H
#define HID_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <map>
#include <functional>
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
#include "PS2KeyAdvanced.h"
#include "PS2Mouse.h"
#include "BTHID.h"
#include "LED.h"
#include "SWITCH.h"

// NB: Macros definitions put inside class for clarity, they are still global scope.

// Define a class which acts as the encapsulation object of many base classes which provide input device functionality.
class HID  {

    // Macros.
    //
    #define NUMELEM(a)                     (sizeof(a)/sizeof(a[0]))

    // Constants.
    #define HID_VERSION                    1.02
    #define HID_MOUSE_DATA_POLL_DELAY      10
    #define MAX_MOUSE_INACTIVITY_TIME      500 * HID_MOUSE_DATA_POLL_DELAY
    
    // Categories of configuration possible with the mouse. These are used primarily with the web based UI for rendering selection choices.
    #define HID_MOUSE_HOST_SCALING_TYPE    "host_scaling"
    #define HID_MOUSE_SCALING_TYPE         "mouse_scaling"
    #define HID_MOUSE_RESOLUTION_TYPE      "mouse_resolution"
    #define HID_MOUSE_SAMPLING_TYPE        "mouse_sampling"

    #define HID_MOUSE_HOST_SCALING_1_1_NAME "1:1"
    #define HID_MOUSE_HOST_SCALING_1_2_NAME "1:2"
    #define HID_MOUSE_HOST_SCALING_1_3_NAME "1:3"
    #define HID_MOUSE_HOST_SCALING_1_4_NAME "1:4"
    #define HID_MOUSE_HOST_SCALING_1_5_NAME "1:5"    

    // Names for the configuration value settings.
    #define HID_MOUSE_RESOLUTION_1_1_NAME  "1 c/mm"
    #define HID_MOUSE_RESOLUTION_1_2_NAME  "2 c/mm"
    #define HID_MOUSE_RESOLUTION_1_4_NAME  "4 c/mm"
    #define HID_MOUSE_RESOLUTION_1_8_NAME  "8 c/mm"
    #define HID_MOUSE_SCALING_1_1_NAME     "1:1"
    #define HID_MOUSE_SCALING_2_1_NAME     "2:1"
    #define HID_MOUSE_SAMPLE_RATE_10_NAME  "10 S/s"
    #define HID_MOUSE_SAMPLE_RATE_20_NAME  "20 S/s"
    #define HID_MOUSE_SAMPLE_RATE_40_NAME  "40 S/s"
    #define HID_MOUSE_SAMPLE_RATE_60_NAME  "60 S/s"
    #define HID_MOUSE_SAMPLE_RATE_80_NAME  "80 S/s"
    #define HID_MOUSE_SAMPLE_RATE_100_NAME "100 S/s"
    #define HID_MOUSE_SAMPLE_RATE_200_NAME "200 S/s"
    
    public:
        // Types of devices the HID class can support.
        enum HID_DEVICE_TYPES {
            HID_DEVICE_TYPE_KEYBOARD     = 0x00,
            HID_DEVICE_TYPE_MOUSE        = 0x01,
            HID_DEVICE_TYPE_BLUETOOTH    = 0x02,
        };

        // HID class can encapsulate many input device objects, only one at a time though. On startup the device is enumerated and then all
        // functionality serves the device object.
        enum HID_INPUT_DEVICE {
            HID_DEVICE_PS2_KEYBOARD      = 0x00,
            HID_DEVICE_PS2_MOUSE         = 0x01,
            HID_DEVICE_BLUETOOTH         = 0x02,
            HID_DEVICE_BT_KEYBOARD       = 0x03,
            HID_DEVICE_BT_MOUSE          = 0x04
        };

        // Scaling - The host receiving mouse data may have a different resolution to that of the mouse, so we use configurable host side scaling to compensate. The mouse data received
        // is scaled according to the enum setting.
        enum HID_MOUSE_HOST_SCALING {
            HID_MOUSE_HOST_SCALING_1_1   = 0x00,
            HID_MOUSE_HOST_SCALING_1_2   = 0x01,
            HID_MOUSE_HOST_SCALING_1_3   = 0x02,
            HID_MOUSE_HOST_SCALING_1_4   = 0x03,
            HID_MOUSE_HOST_SCALING_1_5   = 0x04,
        };

        // Resolution - the mouse can digitize movement from 1mm to 1/8mm, the default being 1/4 (ie. 1mm = 4 counts). This allows configuration for a finer or rougher
        // tracking digitisation.
        enum HID_MOUSE_RESOLUTION {
            HID_MOUSE_RESOLUTION_1_1     = PS2Mouse::PS2_MOUSE_RESOLUTION_1_1,
            HID_MOUSE_RESOLUTION_1_2     = PS2Mouse::PS2_MOUSE_RESOLUTION_1_2,
            HID_MOUSE_RESOLUTION_1_4     = PS2Mouse::PS2_MOUSE_RESOLUTION_1_4,
            HID_MOUSE_RESOLUTION_1_8     = PS2Mouse::PS2_MOUSE_RESOLUTION_1_8,
        };

        // Scaling - the mouse can provide linear (1:1 no scaling) or non liner (2:1 scaling) adaptation of the digitised data. This allows configuration for amplification of movements.
        enum HID_MOUSE_SCALING {
            HID_MOUSE_SCALING_1_1        = PS2Mouse::PS2_MOUSE_SCALING_1_1,
            HID_MOUSE_SCALING_2_1        = PS2Mouse::PS2_MOUSE_SCALING_2_1,
        };

        // Sampling rate - the mouse, in streaming mode, the mouse sends with movement updates. This allows for finer or rougher digitisation of movements. The default is 100 samples per
        // second and the X68000 is fixed at 100 samples per second. 
        enum HID_MOUSE_SAMPLING {
            HID_MOUSE_SAMPLE_RATE_10     = PS2Mouse::PS2_MOUSE_SAMPLE_RATE_10,
            HID_MOUSE_SAMPLE_RATE_20     = PS2Mouse::PS2_MOUSE_SAMPLE_RATE_20,
            HID_MOUSE_SAMPLE_RATE_40     = PS2Mouse::PS2_MOUSE_SAMPLE_RATE_40,
            HID_MOUSE_SAMPLE_RATE_60     = PS2Mouse::PS2_MOUSE_SAMPLE_RATE_60,
            HID_MOUSE_SAMPLE_RATE_80     = PS2Mouse::PS2_MOUSE_SAMPLE_RATE_80,
            HID_MOUSE_SAMPLE_RATE_100    = PS2Mouse::PS2_MOUSE_SAMPLE_RATE_100,
            HID_MOUSE_SAMPLE_RATE_200    = PS2Mouse::PS2_MOUSE_SAMPLE_RATE_200,
        };

        // Suspend flag. When active, the interface components enter an idle state after completing there latest cycle.
        bool                               suspend = false;
        bool                               suspended = true;
       
        // Element to store mouse data in a queue. The data is actual mouse movements, any control data and private data for the actual mouse is stripped.
        typedef struct {
            int16_t                        xPos;
            int16_t                        yPos;
            uint8_t                        status;
            uint8_t                        wheel;
        } t_mouseMessageElement;

        // Prototypes.
                                           HID(enum HID_DEVICE_TYPES, NVS *hdlNVS, LED *hdlLED, SWITCH *hdlSWITCH);
                                           HID(NVS *hdlNVS);
                                           HID(void);
        virtual                            ~HID(void);
        bool                               isBluetooth(void);
        void                               enableBluetooth(void);
        void                               disableBluetooth(void);
        bool                               isSuspended(bool waitForSuspend);
        void                               suspendInterface(bool suspendIf);
        bool                               persistConfig(void);
        uint16_t                           read(void);
        void                               setMouseResolution(enum HID_MOUSE_RESOLUTION resolution);
        void                               setMouseHostScaling(enum HID_MOUSE_HOST_SCALING scaling);
        void                               setMouseScaling(enum HID_MOUSE_SCALING scaling);
        void                               setMouseSampleRate(enum HID_MOUSE_SAMPLING sampleRate);
        void                               btStartPairing(void);
        void                               btCancelPairing(void);    


        // Method to register an object method for callback with context.
        template<typename A, typename B>
        void setDataCallback(A func_ptr, B obj_ptr)
        {
            hidCtrl.dataCallback = bind(func_ptr, obj_ptr, std::placeholders::_1);
        }

        // Method to suspend input device activity, yielding to the OS until suspend is cleared.
        inline virtual void yield(uint32_t delay)
        {
            // If suspended, go into a permanent loop until the suspend flag is reset.
            if(this->suspend)
            {
                // Suspend the keyboard interface.
                if(hidCtrl.deviceType == HID_DEVICE_TYPE_KEYBOARD) { printf("SUSPEND\n"); ps2Keyboard->suspend(true); }
                this->suspended = true;

                // Sleep while suspended.
                while(this->suspend)
                {
                    vTaskDelay(100);
                }
                
                // Release the keyboard interface.
                if(hidCtrl.deviceType == HID_DEVICE_TYPE_KEYBOARD) ps2Keyboard->suspend(false);
                this->suspended = false;
            } else
            // Otherwise just delay by the required amount for timing and to give other threads a time slice.
            {
                vTaskDelay(delay);
            }
            return;
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

        // Template to aid in conversion of an enum to integer.
        template <typename E> constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept
        {
            return static_cast<typename std::underlying_type<E>::type>(e);
        }

        // Method to return the class version number.
        virtual float version(void)
        {
            return(HID_VERSION);
        }

        // Method to return the name of this class.
        virtual std::string ifName(void)
        {
            return(className);
        }

    protected:

    private:
        // Prototypes.
                  void                     init(const char *className, enum HID_DEVICE_TYPES deviceTypes);
                  bool                     nvsPersistData(const char *key, void *pData, uint32_t size);
                  bool                     nvsRetrieveData(const char *key, void *pData, uint32_t size);
                  bool                     nvsCommitData(void);
                  void                     checkKeyboard( void );
                  bool                     checkPS2Keyboard( void );
                  bool                     checkPS2Mouse( void );
                  void                     checkMouse( void );
                  void                     processPS2Mouse( void );
                  void                     checkBTMouse( void );
                  void                     mouseReceiveData(uint8_t src, PS2Mouse::MouseData mouseData);
        IRAM_ATTR static void              hidControl( void * pvParameters );
                  static void              btPairingHandler(uint32_t pid, uint8_t trigger);
        inline uint32_t milliSeconds(void)
        {
            return( (uint32_t) (clock() ) );
        }


        enum HOST_CONFIG_MODES {
            HOST_CONFIG_OFF              = 0x00,
            HOST_CONFIG_SCALING          = 0x01,
            HOST_CONFIG_RESOLUTION       = 0x02,
        };

        // Structure to maintain configuration for the HID.
        // 
        typedef struct {

            struct {
                // Mouse data Adjustment and filtering options.
                //
                enum HID_MOUSE_RESOLUTION  resolution;
                enum HID_MOUSE_SCALING     scaling;
                enum HID_MOUSE_SAMPLING    sampleRate;
            } mouse;

            struct {
                // Host data for adjustment and configuration.
                enum HID_MOUSE_HOST_SCALING scaling;
            } host;

            struct {
                // Configuration mode time period used to select configuration option. Once the middle key is held, the configuration option starts at 1, after this number of seconds 
                // the configuration option advances to the next configuration item, and so on...
                uint16_t                   optionAdvanceDelay;
            } params;

        } t_hidConfig;

        // Structure to maintain an active settings for HID devices.
        typedef struct {
            enum HID_INPUT_DEVICE          hidDevice;          // Active HID device, only one can be active.
            enum HID_DEVICE_TYPES          deviceType;         // Type of device which is active.
            bool                           ps2Active;          // Flag to indicate PS/2 device is online and active.
            uint32_t                       noEchoCount   = 0L; // Echo back counter, used for testing if a keyboard is online.
            TickType_t                     ps2CheckTimer = 0;  // Check timer, used for timing periodic keyboard checks.

            // Mouse control variables.
            uint32_t                       noValidMouseMessage = 0;
            int                            wheelCnt            = 0;
            uint32_t                       loopTimer           = 0;
            bool                           middleKeyPressed    = false;
            PS2Mouse::MouseData            mouseData;

            // Flag to indicate the mouse is active and online.
            bool                           active;

            // Flag to indicate the configuration data has been updated.
            bool                           updated;

            // Configuration mode selected when middle button pressed.
            enum HOST_CONFIG_MODES         configMode;

            // Mutex to block access during maintenance tasks.
            SemaphoreHandle_t              mutexInternal;

            // Callback for streaming input devices with data to be processed.
            std::function<void(t_mouseMessageElement)> dataCallback;
        } t_hidControl;

        // Current configuration of the HID.
        t_hidConfig                        hidConfig;

        // Variables to control the HID.
        t_hidControl                       hidCtrl;

        // Handle to the persistent storage api.
        nvs_handle_t                       nvsHandle;

        // Name of this class, used for NVS access. 
        std::string                        className;
      
        // NVS persistence object.
        NVS                                *nvs;

        // LED activity object handle.
        LED                                *led;
       
        // SWITCH object handle.
        SWITCH                             *sw;
       
        // Keyboard object for PS/2 data retrieval and management.
        PS2KeyAdvanced                     *ps2Keyboard;
      
        // Keyboard object for Bluetooth data retrieval and management.
        BTHID                              *btHID;

        // Mouse object for PS/2 data retrieval and management.
        PS2Mouse                           *ps2Mouse;

        // Thread handle for the HID control thread.
        TaskHandle_t                       TaskHID  = NULL;
};
#endif // HID_H
