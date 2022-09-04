/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            PS2Mouse.cpp
// Created:         Jan 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     PS/2 Mouse Class.
//                  This source file contains the class to encapsulate a PS/2 mouse. Given two GPIO
//                  pins, datapin and clkpin, it is able to communicate, configure and return mouse
//                  data via a rich set of methods.
//
//                  This class borrows ideas from the interrupt concept of the PS2KeyAdvanced class 
//                  for communicating via the PS/2 protocol.
//                  https://github.com/techpaul/PS2KeyAdvanced class from Paul Carpenter.
//
//                  The application uses the Espressif Development environment with Arduino components.
//                  This is necessary as the class uses the Arduino methods for GPIO manipulation. I
//                  was considering using pure Espressif IDF methods but considered the potential
//                  of also using this class on an Arduino project. 
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
#include "PS2Mouse.h"

// Global handle to allow the static interrupt routine to access the instantiated object. This does limit this class to being Singleton
// but it is unusual to have more than 1 PS/2 mouse on a project so less of a problem.
PS2Mouse *pThis;

// Constructor. Simple assign the hardware data and clock pins to internal variables and setup
// variables. Actual real initialisation is performed by a public method so re-initialisation can
// be made if required.
//
PS2Mouse::PS2Mouse(int clockPin, int dataPin)
{
    ps2Ctrl.clkPin  = clockPin;
    ps2Ctrl.dataPin = dataPin;
    ps2Ctrl.supportsIntelliMouseExtensions = false;
    ps2Ctrl.mouseDataCallback = NULL;
}

// Destructor - Detach interrupts and free resources.
//
PS2Mouse::~PS2Mouse()
{
    // Disable interrupts.
    detachInterrupt( digitalPinToInterrupt( ps2Ctrl.clkPin ) );
}

// The interrupt handler triggered on each falling edge of the clock pin.
//   Rx Mode: 11 bits - <start><8 data bits><ODD parity bit><stop bit>
//   Tx Mode: 11 bits - <start><8 data bits><ODD parity bit><stop bit>
IRAM_ATTR void PS2Mouse::ps2interrupt( void )
{
    // Locals.
    //
    static uint32_t    timeLast = 0;
    uint32_t           timeCurrent;
    uint8_t            dataBit;

    // Workaround for ESP32 SILICON error see extra/Porting.md
    #ifdef PS2_ONLY_CHANGE_IRQ
    if( digitalRead( ps2Ctrl.clkPin ) )
        return;
    #endif
 
    // TRANSMIT MODE.
    if( pThis->ps2Ctrl.mode & _TX_MODE )
    {
        // Received data not valid when transmitting.
        pThis->ps2Ctrl.rxPos = 0;

        // Now point to next bit
        pThis->ps2Ctrl.bitCount++;

        // BIT 1 - START BIT
        if(pThis->ps2Ctrl.bitCount == 1)
        {
            #if defined( PS2_CLEAR_PENDING_IRQ ) 
                // Start bit due to Arduino bug
                digitalWrite(pThis->ps2Ctrl.dataPin, LOW);
                break;
            #endif
        } else
        // BIT 2->9 - DATA BIT MSB->LSB
        if(pThis->ps2Ctrl.bitCount >= 2 && pThis->ps2Ctrl.bitCount <= 9)
        {
            // Data bits
            dataBit = pThis->ps2Ctrl.shiftReg & 0x01;                   // get LSB
            digitalWrite(pThis->ps2Ctrl.dataPin, dataBit);              // send start bit
            pThis->ps2Ctrl.parity += dataBit;                           // another one received ?
            pThis->ps2Ctrl.shiftReg >>= 1;                              // right _SHIFT one place for next bit
        } else
        // BIT 10 - PARITY BIT
        if(pThis->ps2Ctrl.bitCount == 10)
        {
            // Parity - Send LSB if 1 = odd number of 1's so ps2Ctrl.parity should be 0
            digitalWrite( pThis->ps2Ctrl.dataPin, ( ~pThis->ps2Ctrl.parity & 1 ) );
        } else
        // BIT 11 - STOP BIT
        if(pThis->ps2Ctrl.bitCount == 11)
        {
            // Stop bit write change to input pull up for high stop bit
            digitalWrite( pThis->ps2Ctrl.dataPin, HIGH );
            pinMode( pThis->ps2Ctrl.dataPin, INPUT );
        } else
        // BIT 12 - ACK BIT
        if(pThis->ps2Ctrl.bitCount == 12)
        {
            // Acknowledge bit low we cannot do anything if high instead of low
            // clear modes to receive again
            pThis->ps2Ctrl.mode &= ~_TX_MODE;
            pThis->ps2Ctrl.bitCount = 0;                                // end of byte
        } else
        {
            // in case of weird error and end of byte reception re-sync
            pThis->ps2Ctrl.bitCount = 0;
        }
    }
    
    // RECEIVE MODE.
    else
    {
        // Read latest bit.   
        dataBit = digitalRead( pThis->ps2Ctrl.dataPin );
       
        // Get current time.
        timeCurrent = millis( );

        // Reset the receive byte buffer pointer if the gap from the last received byte to the current time is greater than a packet interbyte delay.
        if(timeCurrent - timeLast > 100)
        {
            pThis->ps2Ctrl.rxPos = 0;
        }
     
        // Catch glitches, any clock taking longer than 250ms is either a glitch, an error or start of a new packet.
        if( timeCurrent - timeLast > 250 )
        {
            pThis->ps2Ctrl.bitCount = 0;
            pThis->ps2Ctrl.shiftReg = 0;
        } 

        // Store current time for next loop to detect timing issues.
        timeLast = timeCurrent;

        // Now point to next bit
        pThis->ps2Ctrl.bitCount++;

        // BIT 1 - START BIT
        if(pThis->ps2Ctrl.bitCount == 1)
        {
            // Start bit
            pThis->ps2Ctrl.parity = 0;
            pThis->ps2Ctrl.mode |= _PS2_BUSY;                    // set busy
        } else
        // BIT 2->9 - DATA BIT MSB->LSB
        if(pThis->ps2Ctrl.bitCount >= 2 && pThis->ps2Ctrl.bitCount <= 9)
        {
            // Data bits
            pThis->ps2Ctrl.parity += dataBit;                       // another one received ?
            pThis->ps2Ctrl.shiftReg >>= 1;                        // right _SHIFT one place for next bit
            pThis->ps2Ctrl.shiftReg |= ( dataBit ) ? 0x80 : 0;    // or in MSbit
        } else
        // BIT 10 - PARITY BIT
        if(pThis->ps2Ctrl.bitCount == 10)
        {
            // Parity check
            pThis->ps2Ctrl.parity &= 1;                             // Get LSB if 1 = odd number of 1's so ps2Ctrl.parity bit should be 0
            if( pThis->ps2Ctrl.parity == dataBit )                  // Both same ps2Ctrl.parity error
            pThis->ps2Ctrl.parity = 0xFD;                           // To ensure at next bit count clear and discard
        } else
        // BIT 11 - STOP BIT
        if(pThis->ps2Ctrl.bitCount == 11)
        {
            // Streaming mode, assemble the data into the buffer.
            if(pThis->ps2Ctrl.streamingEnabled)
            {
                if(pThis->ps2Ctrl.rxPos == 0 && pThis->streaming.newData == true) pThis->streaming.overrun = true;
                if(pThis->ps2Ctrl.rxPos == 0) pThis->streaming.mouseData.status     = pThis->ps2Ctrl.shiftReg;
                if(pThis->ps2Ctrl.rxPos == 1) pThis->streaming.mouseData.position.x = pThis->ps2Ctrl.shiftReg;
                if(pThis->ps2Ctrl.rxPos == 2) pThis->streaming.mouseData.position.y = pThis->ps2Ctrl.shiftReg;
                if(pThis->ps2Ctrl.rxPos == 3) pThis->streaming.mouseData.wheel      = pThis->ps2Ctrl.shiftReg;
                if( (pThis->ps2Ctrl.supportsIntelliMouseExtensions == false && pThis->ps2Ctrl.rxPos == 2) || (pThis->ps2Ctrl.supportsIntelliMouseExtensions == true && pThis->ps2Ctrl.rxPos == 3))
                {
                    pThis->streaming.newData         = true;
                    pThis->streaming.overrun         = false;
                    pThis->ps2Ctrl.rxPos             = 0;
                } else
                {
                    pThis->ps2Ctrl.rxPos++;
                }
            } else
            {
                // Save the received byte and parity, let consumer decide on it's validity.
                pThis->ps2Ctrl.rxBuf[pThis->ps2Ctrl.rxPos++] = (pThis->ps2Ctrl.parity << 8 | pThis->ps2Ctrl.shiftReg);
            }
            // Set mode and status for next receive byte
            pThis->ps2Ctrl.mode &= ~( _WAIT_RESPONSE );
            pThis->ps2Ctrl.mode &= ~_PS2_BUSY;
            pThis->ps2Ctrl.bitCount = 0;                // end of byte
        } else
        {
            // in case of weird error and end of byte reception re-sync
            pThis->ps2Ctrl.bitCount = 0;
        }
    }
}

// Method to write a byte (control or parameter) to the Mouse. This method encapsulates the protocol necessary
// to invoke Host -> PS/2 Mouse transmission and the interrupts, on falling clock edge, process the byte to send
// and bitbang accordingly.
//
void PS2Mouse::writeByte(uint8_t command)
{
    // Locals.
    //
    uint32_t currentTime = millis();

    // Test to see if a transmission is underway, block until the xmit buffer becomes available or timeout expires (no mouse).
    //
    while((ps2Ctrl.mode & _TX_MODE) && currentTime+100 > millis());

    // If TX_MODE has been reset, interrupt processing has occurred so line up next byte,
    //
    if((ps2Ctrl.mode & _TX_MODE) == 0)
    {
        // Initialise the ps2 control variables.
        ps2Ctrl.shiftReg = command;
        ps2Ctrl.bitCount = 1;
        ps2Ctrl.parity   = 0;
        ps2Ctrl.mode    |= _TX_MODE + _PS2_BUSY;
        ps2Ctrl.rxPos    = 0;

        // Initialise the streaming buffer.
        streaming.mouseData.valid      = false;
        streaming.mouseData.status     = 0;
        streaming.mouseData.position.x = 0;
        streaming.mouseData.position.y = 0;
        streaming.mouseData.wheel      = 0;
        streaming.newData              = false;
        streaming.overrun              = false;

        // STOP the interrupt handler - Setting pin output low will cause interrupt before ready
        detachInterrupt( digitalPinToInterrupt( ps2Ctrl.clkPin ) );

        // Set data and clock pins to output and high
        digitalWrite(ps2Ctrl.dataPin, HIGH);
        pinMode(ps2Ctrl.dataPin, OUTPUT);
        digitalWrite(ps2Ctrl.clkPin, HIGH);
        pinMode(ps2Ctrl.clkPin, OUTPUT);

        // Essential for PS2 spec compliance
        delayMicroseconds(10);

        // Set Clock LOW - trigger Host -> Mouse transmission. Mouse controls the clock but dragging clock low is used by the mouse to detect a host write and clock 
        // data in accordingly.
        digitalWrite( ps2Ctrl.clkPin, LOW );

        // Essential for PS2 spec compliance, set clock low for 60us
        delayMicroseconds(60);

        // Set data low - Start bit
        digitalWrite( ps2Ctrl.dataPin, LOW );

        // Set clock to input_pullup data stays output while writing to keyboard
        digitalWrite(ps2Ctrl.clkPin, HIGH);
        pinMode(ps2Ctrl.clkPin, INPUT);

        // Restart interrupt handler
        attachInterrupt( digitalPinToInterrupt( ps2Ctrl.clkPin ), ps2interrupt, FALLING );
    }

    // Everything is now processed in the interrupt handler.
    return;
}

// Setup and initialise the running object and Mouse hardware. This method must be called at startup and anytime a full reset is required.
//
void PS2Mouse::initialize()
{
    // Setup variables.
    ps2Ctrl.mode = 0;
    ps2Ctrl.supportsIntelliMouseExtensions = false;
    ps2Ctrl.streamingEnabled = false;
    ps2Ctrl.bitCount = 0;
    ps2Ctrl.shiftReg = 0;
    ps2Ctrl.parity = 0; 
    ps2Ctrl.rxPos = 0;
    // Clear the receive buffer.
    for(int idx=0; idx < 16; idx++) ps2Ctrl.rxBuf[idx] = 0x00;
  
    // Set data and clock pins to input.
    digitalWrite(ps2Ctrl.dataPin, HIGH);
    pinMode(ps2Ctrl.dataPin, INPUT);
    digitalWrite(ps2Ctrl.clkPin, HIGH);
    pinMode(ps2Ctrl.clkPin, INPUT);

    // Initialise the control structure.
    ps2Ctrl.bitCount = 0;
    ps2Ctrl.mode = 0;
    ps2Ctrl.rxPos = 0;

    // As the interrupt handler is static it wont have reference to the instantiated object methods so we need to store the object in a pointer 
    // which is then used by the interrupt handler.
    pThis = this;

    // Attach the clock line to a falling low interrupt trigger and handler. The Mouse toggles the clock line for each bit to be sent/received 
    // so we interrupt on each falling clock edge.
    attachInterrupt( digitalPinToInterrupt( ps2Ctrl.clkPin ), ps2interrupt, FALLING );             
   
    // Setup the mouse, make a reset, check and set Intellimouse extensions, set the resolution, scaling, sample rate to defaults and switch to remote (polled) mode.
    reset();
    checkIntelliMouseExtensions();
    setResolution(PS2_MOUSE_RESOLUTION_1_8);
    setScaling(PS2_MOUSE_SCALING_1_1);
    setSampleRate(PS2_MOUSE_SAMPLE_RATE_40);
    setRemoteMode();

    // All done.
    return;
}

// Public method to force a mouse reset. Used on startup and anytime the client believes the mouse has hungup.
//
bool PS2Mouse::reset(void)
{
    // Locals.
    //
    uint8_t       respBuf[5];
    bool          result = false;

    // Send command to reset the mouse, if it returns an ACK then reset succeeded.
    //
    if(sendCmd(MOUSE_CMD_RESET, 0, respBuf, DEFAULT_MOUSE_TIMEOUT))
    {
        result = true;
    }

    // Return result.
    return(result);
}

// Private method to check and see if the mouse suports Microsoft Intellimouse extensions. It sets an internal state flag accordingly.
//
bool PS2Mouse::checkIntelliMouseExtensions(void)
{
    // Locals.
    //
    char deviceId; 

    // IntelliMouse detection sequence, error checking isnt used.
    setSampleRate(PS2_MOUSE_SAMPLE_RATE_200);
    setSampleRate(PS2_MOUSE_SAMPLE_RATE_100);
    setSampleRate(PS2_MOUSE_SAMPLE_RATE_80);

    // Get device Id and if the mouse supports Intellimouse extensions, it will reveal itself as an INTELLI_MOUSE.
    deviceId = getDeviceId();
    ps2Ctrl.supportsIntelliMouseExtensions = (deviceId == INTELLI_MOUSE);

    // Return flag to indicate support (true) or no support (false).
    return(ps2Ctrl.supportsIntelliMouseExtensions);
}

// Public method to set the automatic sample rate.
//
bool PS2Mouse::setSampleRate(enum PS2_SAMPLING rate)
{
    // Locals.
    //
    uint8_t       respBuf[5];
    bool          result = false;

    // Sanity check.
    if(rate == PS2_MOUSE_SAMPLE_RATE_10 || rate == PS2_MOUSE_SAMPLE_RATE_20 || rate == PS2_MOUSE_SAMPLE_RATE_40 || rate == PS2_MOUSE_SAMPLE_RATE_60 || rate == PS2_MOUSE_SAMPLE_RATE_80 || rate == PS2_MOUSE_SAMPLE_RATE_100 || rate == PS2_MOUSE_SAMPLE_RATE_200)
    {
        // Send command to set the mouse resolution.
        //
        if(sendCmd(MOUSE_CMD_SET_SAMPLE_RATE, 0, respBuf, DEFAULT_MOUSE_TIMEOUT))
        {
            // Send the rate, if ACK is returned, then resolution set otherwise error.
            if(sendCmd((uint8_t)rate, 0, respBuf, DEFAULT_MOUSE_TIMEOUT))
            {
                result = true;
            }
        }
    }

    // Return result.
    return(result);
}

// Public method to request the mouse Id which can be used to identify the mouse capabilities.
//
char PS2Mouse::getDeviceId(void)
{
    // Locals.
    //
    uint8_t       respBuf[5];

    // Send command to set the mouse scaling, either 2:1 or 1:1.
    //
    if(sendCmd(MOUSE_CMD_GET_DEVICE_ID, 1, respBuf, DEFAULT_MOUSE_TIMEOUT) == false)
    {
        respBuf[0] = 0xFF;
    }

    // Return result.
    return(respBuf[0]);
}

// Public method to set the mouse scaling, either Normal 1:1 (scaling = 0) or non-linear 2:1 (scaling = 1).
//
bool PS2Mouse::setScaling(enum PS2_SCALING scaling) 
{
    // Locals.
    //
    uint8_t       respBuf[5];
    bool          result = false;

    // Sanity check.
    if(scaling >= PS2_MOUSE_SCALING_1_1 && scaling < PS2_MOUSE_SCALING_2_1)
    {
        // Send command to set the mouse scaling, either 2:1 or 1:1.
        //
        if(sendCmd((uint8_t)scaling, 0, respBuf, DEFAULT_MOUSE_TIMEOUT))
        {
            result = true;
        }
    }

    // Return result.
    return(result);
}

// Public method to request the mouse enters remote mode.
//
bool PS2Mouse::setRemoteMode(void)
{
    // Locals.
    //
    uint8_t       respBuf[5];

    // Simply pass on the request to the mouse to enter remote mode.
    return(sendCmd(MOUSE_CMD_SET_REMOTE_MODE, 1, respBuf, DEFAULT_MOUSE_TIMEOUT));
}

// Public method to request the mouse enters stream mode. This mode reports mouse movements as they change, albeit the streaming must also be enabled
// once set to Stream Mode via the enableStreaming method.
//
bool PS2Mouse::setStreamMode(void)
{
    // Locals.
    //
    uint8_t       respBuf[5];

    // Simply pass on the request to the mouse to enter stream mode.
    return(sendCmd(MOUSE_CMD_SET_STREAM_MODE, 1, respBuf, DEFAULT_MOUSE_TIMEOUT));
}

// Public methods to enable and disable streaming (constant rate packet transmission from mouse to host).
// This module accepts the data and updates an in object set which the caller queries. No buffering takes place
// so should the caller fail to read the data then the arrival of the next packet from the mouse will override
// the in object values.
//
bool PS2Mouse::enableStreaming(void)
{
    // Locals.
    //
    uint8_t       respBuf[5];

    // Sanity check.
    if(ps2Ctrl.streamingEnabled == false)
    {
        if(sendCmd(MOUSE_CMD_ENABLE_STREAMING, 0, respBuf, DEFAULT_MOUSE_TIMEOUT))
        {
            // Initialise the streaming buffer.
            streaming.mouseData.valid      = false;
            streaming.mouseData.status     = 0;
            streaming.mouseData.position.x = 0;
            streaming.mouseData.position.y = 0;
            streaming.mouseData.wheel      = 0;
            streaming.newData              = false;
            streaming.overrun              = false;
            ps2Ctrl.streamingEnabled       = true;
        }
    }

    // Return the enabled flag to indicate success.
    return(ps2Ctrl.streamingEnabled);
}
bool PS2Mouse::disableStreaming(void)
{
    // Locals.
    //
    uint8_t       respBuf[5];

    // Sanity check.
    if(ps2Ctrl.streamingEnabled == true)
    {
        if(sendCmd(MOUSE_CMD_DISABLE_STREAMING, 0, respBuf, DEFAULT_MOUSE_TIMEOUT))
        {
            ps2Ctrl.streamingEnabled              = false;
        }
    }

    // Return the enabled flag to indicate success.
    return(ps2Ctrl.streamingEnabled);
}

// Public method to set the mouse resolution in pixels per millimeter, valid values are o..3.
//
bool PS2Mouse::setResolution(enum PS2_RESOLUTION resolution)
{
    // Locals.
    //
    uint8_t       respBuf[5];
    bool          result = false;

    // Sanity check.
    if(resolution >= PS2_MOUSE_RESOLUTION_1_1 && resolution < PS2_MOUSE_RESOLUTION_1_8)
    {
        // Send command to set the mouse resolution.
        //
        if(sendCmd(MOUSE_CMD_SET_RESOLUTION, 0, respBuf, DEFAULT_MOUSE_TIMEOUT))
        {
            // Send the resolution, if ACK is returned, then resolution set otherwise error.
            if(sendCmd((uint8_t)resolution, 0, respBuf, DEFAULT_MOUSE_TIMEOUT))
            {
                result = true;
            }
        }
    }

    // Return result.
    return(result);
}

// Public method to get the current mouse status. The status code is 3 bytes wide and has the following format:
//
//             7     6        5      4       3       2         1      0
// Byte 1:     0    mode    enable  scaling  0    left btn   middle   right btn
// Byte 2: resolution
// Byte 3: sample rate
//
bool PS2Mouse::getStatus(uint8_t *respBuf)
{
    // Locals.
    //
    bool          result = false;

    // Sanity check.
    if(respBuf != NULL)
    {
        // Send command to set the mouse resolution.
        //
        if(sendCmd(MOUSE_CMD_GET_STATUS, 3, respBuf, DEFAULT_MOUSE_TIMEOUT))
        {
            result = true;
        }
    }

    // Return result.
    return(result);
}

// Public method to obtain current mouse state data.
//
PS2Mouse::MouseData PS2Mouse::readData(void)
{
    // Locals.
    MouseData           data;
    uint8_t             dataBuf[8] = {0,0,0,0,0,0,0,0};

    // If streaming mode enabled then set values according to data state. Data only valid if a new update has occurred since last call otherwise old data is returned and valid flag
    // is cleared.
    if(ps2Ctrl.streamingEnabled)
    {
        data.valid         = streaming.newData;
        data.overrun       = streaming.overrun;
        data.status        = streaming.mouseData.status;
        data.position.x    = streaming.mouseData.position.x;
        data.position.y    = streaming.mouseData.position.y;
        data.wheel         = ps2Ctrl.supportsIntelliMouseExtensions ? streaming.mouseData.wheel : 0;
        streaming.newData  = false;
        streaming.overrun  = false;

        // If a data callback has been setup execute it otherwise data is read by caller.
        //
        if(ps2Ctrl.mouseDataCallback != NULL && data.valid)
            ps2Ctrl.mouseDataCallback(data);
    } else
    // Single on-request data set from mouse.
    {
        // Request data from mouse via issuing get single data packet command.
        if(requestData(ps2Ctrl.supportsIntelliMouseExtensions ? 3 : 3, dataBuf, DEFAULT_MOUSE_TIMEOUT))
        {
            data.valid      = true;
            data.overrun    = false;
            data.status     = dataBuf[0];
            data.position.x = dataBuf[1];
            data.position.y = dataBuf[2];
            data.wheel      = ps2Ctrl.supportsIntelliMouseExtensions ? dataBuf[3] : 0;
        } else
        {
            data.valid      = false;
            data.overrun    = false;
        }
    }

    return data;
};

// Method to request the latest mouse movement, wheel and key data. The method blocks until data is available or the timeout is reached. A timeout of 0
// will only return when the data has been received.
bool PS2Mouse::requestData(uint8_t expectedBytes, uint8_t *respBuf, uint32_t timeout)
{
    // Locals.
    //

    // Simply pass on the request for the mouse to send data and await reply.
    return(sendCmd(MOUSE_CMD_REQUEST_DATA, expectedBytes, respBuf, timeout));
}

// Method to send a command to the Mouse and await it's reply. If an ACK isnt returned then a resend request is made otherwise wait until all bytes 
// arrive or we timeout.
//
bool PS2Mouse::sendCmd(uint8_t cmd, uint8_t expectedBytes, uint8_t *respBuf, uint32_t timeout)
{
    // Locals.
    //
    uint32_t currentTime = millis();
    uint32_t endTime     = millis() + timeout;
    uint8_t  *pBuf       = respBuf;
    bool     result      = false;

    // Send command.
    writeByte(cmd);

    // Wait for the expected number of bytes to arrive.
    while(((timeout == 0) || (currentTime < endTime)) && ps2Ctrl.rxPos <= expectedBytes)
    {
        // If an ACK isnt received, request a resend.
        if(ps2Ctrl.rxPos >= 1 && ps2Ctrl.rxBuf[0] != MOUSE_RESP_ACK) { writeByte(MOUSE_CMD_RESEND); }

        // Get latest time.
        currentTime = millis();
    }
  
    // Store the response in callers buffer.
    for(int idx=0; idx < expectedBytes; idx++)
    {
        (*pBuf) = ps2Ctrl.rxBuf[idx+1];
        pBuf++;
    }

    // Set return code, true if a valid packet was received.
    if(((timeout == 0) || (currentTime < endTime)) && ps2Ctrl.rxPos >= expectedBytes && ps2Ctrl.rxBuf[0] == MOUSE_RESP_ACK) result = true;

    // Debug print.
    //printf("%d:%d:%02x,%02x,%02x,%02x, %02x, %d, result=%d, %d, %d, %d\n", result, ps2Ctrl.rxPos, ps2Ctrl.rxBuf[0], ps2Ctrl.rxBuf[1], ps2Ctrl.rxBuf[2], ps2Ctrl.rxBuf[3],ps2Ctrl.rxBuf[4], ps2Ctrl.bitCount, result, timeout,  currentTime, endTime);
 
    // And complete with result!
    return(result);
}
