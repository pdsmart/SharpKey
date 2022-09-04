/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            Mouse.h
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Header for the PS/2 Mouse to Sharp Host interface logic.
// Credits:         
// Copyright:       (c) 2019-2022 Philip Smart <philip.smart@net2net.org>
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

#ifndef MOUSE_H
#define MOUSE_H

// Include the specification class.
#include "KeyInterface.h"
#include "NVS.h"
#include "HID.h"

// NB: Macros definitions put inside class for clarity, they are still global scope.

// Encapsulate the Mouse interface.
class Mouse : public KeyInterface  {

    // Macros.
    //
    #define NUMELEM(a)                  (sizeof(a)/sizeof(a[0]))

    // Constants.
    #define MOUSEIF_VERSION             1.02      
    #define MAX_MOUSE_XMIT_KEY_BUF      128
    #define BITBANG_UART_BIT_TIME       208UL

    public:

        // Prototypes.
                                        Mouse(void);
                                        Mouse(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID);
                                        Mouse(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID, bool secondaryIf);
                                        Mouse(NVS *hdlNVShdlHID, HID *hdlHID);
                                       ~Mouse(void);
        void                            getMouseConfigTypes(std::vector<std::string>& typeList);
        bool                            getMouseSelectList(std::vector<std::pair<std::string, int>>& selectList, std::string option);
        bool                            setMouseConfigValue(std::string paramName, std::string paramValue);
        void                            mouseReceiveData(HID::t_mouseMessageElement mouseMessage);
        bool                            persistConfig(void);

        // Method to return the class version number.
        float version(void)
        {
            return(MOUSEIF_VERSION);
        }

    protected:

    private:
        // Prototypes.
        IRAM_ATTR static void           hostInterface( void * pvParameters );
        void                            init(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID);
        void                            init(NVS *hdlNVS, HID *hdlHID);

        // Structure to maintain mouse interface configuration data. This data is persisted through powercycles as needed.
        typedef struct {
            struct {
                // PS/2 Mouse data Adjustment and filtering options.
                //
                enum HID::HID_MOUSE_RESOLUTION resolution;
                enum HID::HID_MOUSE_SCALING    scaling;
                enum HID::HID_MOUSE_SAMPLING   sampleRate;
            } mouse;

            struct {
                // Host data for adjustment and configuration.
                enum HID::HID_MOUSE_HOST_SCALING scaling;
            } host;

            struct {
            } params;
        } t_mouseConfig;
       
        // Configuration data.
        t_mouseConfig                   mouseConfig;
        
        // Structure to manage the Mouse control variables signifying the state of the Mouse.
        typedef struct {
        } t_msControl;
      
        // Mouse Control variables.
        volatile t_msControl            msCtrl;

        // Structure to manage the Sharp host control variables which define control and data mapping of the host interface and data sent.
        //
        typedef struct {
          #ifdef CONFIG_HOST_HW_UART
            int                         uartNum;
            int                         uartBufferSize;
            int                         uartQueueSize;
          #endif
            bool                        secondaryIf;                          // Mouse runs in tandem with a keyboard interface.

            // Data adjustment and processing options applied to the PS/2 data.
            bool                        updated;
        } t_hostControl;

        // Host Control variables.
        volatile t_hostControl          hostControl;

        // PS/2 to HOST serialiser buffer item.
        typedef struct {
            uint8_t                     xPos;
            uint8_t                     yPos;
            uint8_t                     status;
            uint8_t                     wheel;
            bool                        valid;
        } t_xmitMessage;

        // Create an object for storing the data to be sent to the Host. This data has already been converted and adjusted from the incoming PS/2 message.
        t_xmitMessage                   xmitMsg;
       
        // Thread handles - one per function, ie. ps/2 interface, host target interface, wifi interface.
        TaskHandle_t                    TaskHostIF = NULL;
        TaskHandle_t                    TaskHIDIF  = NULL;
       
        // Spin lock mutex to hold a coresied to an uninterruptable method. This only works on dual core ESP32's.
        portMUX_TYPE                    x1Mutex;
};

#endif // MOUSE_H
