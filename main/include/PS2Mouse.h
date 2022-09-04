/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            PS2Mouse.h
// Created:         Jan 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Header file for the PS/2 Mouse Class.
//
// Credits:         
// Copyright:       (c) 2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Mar 2022 - Initial write.
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
#ifndef MOUSE_H_
#define MOUSE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Arduino.h"
#include "driver/gpio.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

// PS/2 Mouse Class.
class PS2Mouse {

    // mode status flags
    #define _PS2_BUSY                      0x04
    #define _TX_MODE                       0x02
    #define _WAIT_RESPONSE                 0x01

    // Constants.
    #define MAX_PS2_XMIT_KEY_BUF           16
    #define MAX_PS2_RCV_KEY_BUF            16
    #define INTELLI_MOUSE                  3
    #define SCALING_1_TO_1                 0xE6
    #define DEFAULT_MOUSE_TIMEOUT          100

    public:
        // Public structures used for containment of mouse movement and control data. These are used by the insantiating
        // object to request and process mouse data.
        // Postional data - X/Y co-ordinates of mouse movement.
        typedef struct {
            int x, y;
        } Position;
    
        // Mouse data, containing positional data, status, wheel data and validity.
        typedef struct {
            bool     valid;
            bool     overrun;
            int      status;
            Position position;
            int      wheel;
        } MouseData;
    
        // Enumeration of all processed mouse responses.
        enum Responses {
            MOUSE_RESP_ACK                   = 0xFA,
        };
    
        // Enumeration of all processed mouse commands.
        enum Commands {
            MOUSE_CMD_SET_SCALING_1_1        = 0xE6,
            MOUSE_CMD_SET_SCALING_2_1        = 0xE7,
            MOUSE_CMD_SET_RESOLUTION         = 0xE8,
            MOUSE_CMD_GET_STATUS             = 0xE9,
            MOUSE_CMD_SET_STREAM_MODE        = 0xEA,
            MOUSE_CMD_REQUEST_DATA           = 0xEB,
            MOUSE_CMD_SET_REMOTE_MODE        = 0xF0,
            MOUSE_CMD_GET_DEVICE_ID          = 0xF2,
            MOUSE_CMD_SET_SAMPLE_RATE        = 0xF3,
            MOUSE_CMD_ENABLE_STREAMING       = 0xF4,
            MOUSE_CMD_DISABLE_STREAMING      = 0xF5,
            MOUSE_CMD_RESEND                 = 0xFE,
            MOUSE_CMD_RESET                  = 0xFF,
        };
    
        // Resolution - the PS/2 mouse can digitize movement from 1mm to 1/8mm, the default being 1/4 (ie. 1mm = 4 counts). This allows configuration for a finer or rougher
        // tracking digitisation.
        enum PS2_RESOLUTION {
            PS2_MOUSE_RESOLUTION_1_1         = 0x00,
            PS2_MOUSE_RESOLUTION_1_2         = 0x01,
            PS2_MOUSE_RESOLUTION_1_4         = 0x02,
            PS2_MOUSE_RESOLUTION_1_8         = 0x03,
        };
    
        // Scaling - the PS/2 mouse can provide linear (1:1 no scaling) or non liner (2:1 scaling) adaptation of the digitised data. This allows configuration for amplification of movements.
        enum PS2_SCALING {
            PS2_MOUSE_SCALING_1_1            = 0x00,
            PS2_MOUSE_SCALING_2_1            = 0x01,
        };
    
        // Sampling rate - the PS/2 mouse, in streaming mode, the mouse sends with movement updates. This allows for finer or rougher digitisation of movements. The default is 100 samples per
        // second and the X68000 is fixed at 100 samples per second. Adjusting the ps/2 sample rate will affect tracking granularity on the X68000.
        enum PS2_SAMPLING {
            PS2_MOUSE_SAMPLE_RATE_10         = 10,
            PS2_MOUSE_SAMPLE_RATE_20         = 20,
            PS2_MOUSE_SAMPLE_RATE_40         = 40,
            PS2_MOUSE_SAMPLE_RATE_60         = 60,
            PS2_MOUSE_SAMPLE_RATE_80         = 80,
            PS2_MOUSE_SAMPLE_RATE_100        = 100,
            PS2_MOUSE_SAMPLE_RATE_200        = 200,
        };
    
        // Public accessible prototypes.
                  PS2Mouse(int clockPin, int dataPin);
                  ~PS2Mouse();
        void      writeByte(uint8_t);
        bool      setResolution(enum PS2_RESOLUTION resolution);
        bool      setStreamMode(void);
        bool      setRemoteMode(void);
        bool      setScaling(enum PS2_SCALING scaling);
        char      getDeviceId(void);
        bool      checkIntelliMouseExtensions(void);
        bool      setSampleRate(enum PS2_SAMPLING rate);
        bool      enableStreaming(void);
        bool      disableStreaming(void);
        bool      getStatus(uint8_t *respBuf);
        bool      reset(void);
        MouseData readData(void);
        void      initialize(void);
    
        // Method to register an object method for callback with context.
        template<typename A, typename B>
        void setMouseDataCallback(A func_ptr, B obj_ptr)
        {
            ps2Ctrl.mouseDataCallback = bind(func_ptr, obj_ptr, 0, std::placeholders::_1);
        }
    
    private:
        // PS/2 Control structure - maintains all data and variables relevant to forming a connection with a PS/2 mouse, interaction and processing of its data.
        struct {
            int                  clkPin;                                     // Hardware clock pin - bidirectional.
            int                  dataPin;                                    // Hardware data pin - bidirectional.
            volatile uint8_t     mode;                                       // mode contains _PS2_BUSY      bit 2 = busy until all expected bytes RX/TX
                                                                             //               _TX_MODE       bit 1 = direction 1 = TX, 0 = RX (default)
                                                                             //               _WAIT_RESPONSE bit 0 = expecting data response
            bool                 supportsIntelliMouseExtensions;             // Intellimouse extensions supported.
            bool                 streamingEnabled;                           // Streaming mode has been enabled.
            volatile uint8_t     bitCount;                                   // Main state variable and bit count for interrupts
            volatile uint8_t     shiftReg;                                   // Incoming/Outgoing data shift register.
            volatile uint8_t     parity;                                     // Parity flag for data being sent/received.
            uint16_t             rxBuf[16];                                  // RX buffer - assembled bytes are stored in this buffer awaiting processing.
            int                  rxPos;                                      // Position in buffer to store next byte. 

            // Callback for streaming processed mouse data to HID handler.
            std::function<void(PS2Mouse::MouseData)> mouseDataCallback;
        } ps2Ctrl;
    
        // Structure to store incoming streamed mouse data along with validity flags.
        struct {
            MouseData            mouseData;
            bool                 newData;                                    // An update has occurred since the last query.
            bool                 overrun;                                    // A data overrun has occurred since the last query.
        } streaming;
    
        // Interrupt handler - needs to be declared static and assigned to internal RAM (within the ESP32) to function correctly.
        IRAM_ATTR static void ps2interrupt( void );
    
        // Prototypes.
        bool requestData(uint8_t expectedBytes, uint8_t *respBuf, uint32_t timeout);
        bool sendCmd(uint8_t cmd, uint8_t expectedBytes, uint8_t *respBuf, uint32_t timeout);

};

#endif // MOUSE_H_
