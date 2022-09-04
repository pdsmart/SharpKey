/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            BTHID.cpp
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Bluetooth Keyboard Class.
//                  This source file contains the class to encapsulate a Bluetooth keyboard as a sub
//                  class of the BT base class.
//                  It provides connection, key retrieval and first stage mapping to be compatible
//                  with a PS/2 keyboard prior to host mapping.
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
//                  Jun 2022 - Updated with latest findings. Now checks the bonded list and opens 
//                             connections or scans for new devices if no connections exist.
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

#include "BTHID.h"

// Out of object pointer to a singleton class for use in the ESP IDF API callback routines which werent written for C++. Other methods can be used but this one is the simplest
// to understand and the class can only ever be singleton.
BTHID     *pBTHID = NULL;

// Method to open a connection with a paired device.
//
bool BTHID::openDevice(esp_bd_addr_t bda, esp_hid_transport_t transport, esp_ble_addr_type_t addrType)
{
    // Locals.
    //
    bool                    found = false;
    t_activeDev             device;

    // Call underlying IDF API to open the device. Store handle for future use.
    device.hidhDevHdl = esp_hidh_dev_open(bda, transport, addrType);

    // Add device to list of known devices.
    for(std::size_t idx = 0; idx < pBTHID->btHIDCtrl.devices.size(); idx++)
    {
        // Already on list?
        if (memcmp(bda, btHIDCtrl.devices[idx].bda, sizeof(esp_bd_addr_t)) == 0)
        {
            found = true;
        }
    }
    if(!found && device.hidhDevHdl != 0)
    {
        memcpy(device.bda, bda, sizeof(esp_bd_addr_t));
        device.transport = transport;
        device.addrType = addrType;
        device.open = true;
        device.nextCheckTime = milliSeconds() + 5000L;
        btHIDCtrl.devices.push_back(device);
    }
 
    // Return connection status.
    return(device.hidhDevHdl == NULL ? false : true);
}

// Method to close a connection with a paired device.
//
bool BTHID::closeDevice(esp_bd_addr_t bda)
{
    // Locals.
    //
    esp_err_t  result = ESP_OK;

    // Locate device and close it out.
    for(std::size_t idx = 0; idx < pBTHID->btHIDCtrl.devices.size(); idx++)
    {
        // Already on list?
        if (memcmp(bda, btHIDCtrl.devices[idx].bda, sizeof(esp_bd_addr_t)) == 0)
        {
            if(btHIDCtrl.devices[idx].hidhDevHdl != NULL)
            {
                result = esp_hidh_dev_close(btHIDCtrl.devices[idx].hidhDevHdl);
                btHIDCtrl.devices[idx].open = false;
            }
        }
    }

    return(result);
}

// Callback to handle Bluetooth HID data. This method is called whenever an event occurs, such as a new device being opened or existing one closed. Also
// called with data reports from connected devices. The method determines source of data and routes it to the correct channel (keyboard or mouse).
//
// Example BT Classic Device
// BDA:17:27:6d:85:25:e9, Status: OK, Connected: YES, Handle: 0, Usage: KEYBOARD
// Name: , Manufacturer: , Serial Number: 
// PID: 0x7021, VID: 0x04e8, VERSION: 0x011b
// Report Map Length: 295
//   CCONTROL   INPUT REPORT, ID: 255, Length:   1
//   CCONTROL   INPUT REPORT, ID:   5, Length:   1
//    GENERIC   INPUT REPORT, ID:   4, Length:   1
//   CCONTROL   INPUT REPORT, ID:   3, Length:   7
//   CCONTROL   INPUT REPORT, ID:   2, Length:   3
//   KEYBOARD  OUTPUT   BOOT, ID:   1, Length:   1
//   KEYBOARD  OUTPUT REPORT, ID:   1, Length:   1
//   KEYBOARD   INPUT   BOOT, ID:   1, Length:   8
//   KEYBOARD   INPUT REPORT, ID:   1, Length:   8
//           
// Example BLE Device
// BDA:cf:4a:a8:5c:5e:8c, Appearance: 0x03c0, Connection ID: 0
// Name: M585/M590, Manufacturer: Logitech, Serial Number: A2C4E0DBF89C6DFA
// PID: 0xb01b, VID: 0x046d, VERSION: 0x0011
// Battery: Handle: 29, CCC Handle: 0
// Report Maps: 1
//   Report Map Length: 140
//       VENDOR  OUTPUT REPORT, ID: 17, Length:  19, Permissions: 0x0e, Handle:  56, CCC Handle:   0
//       VENDOR   INPUT REPORT, ID: 17, Length:  19, Permissions: 0x12, Handle:  52, CCC Handle:  53
//        MOUSE   INPUT REPORT, ID:  2, Length:   7, Permissions: 0x12, Handle:  48, CCC Handle:  49
//     KEYBOARD   INPUT REPORT, ID:  1, Length:   7, Permissions: 0x12, Handle:  44, CCC Handle:  45
//        MOUSE   INPUT   BOOT, ID:  2, Length:   3, Permissions: 0x12, Handle:  39, CCC Handle:  40
//     KEYBOARD  OUTPUT   BOOT, ID:  0, Length:   8, Permissions: 0x0e, Handle:  37, CCC Handle:   0
//     KEYBOARD   INPUT   BOOT, ID:  1, Length:   8, Permissions: 0x12, Handle:  34, CCC Handle:  35
//   
void BTHID::hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void * event_data)
{
    // Locals.
    //
    t_activeDev             device;
    esp_hidh_event_t        event = (esp_hidh_event_t) id;
    esp_hidh_event_data_t  *param = (esp_hidh_event_data_t *) event_data;
  
    switch (event)
    {
        case ESP_HIDH_OPEN_EVENT: 
        { 
            const uint8_t   *bda   = esp_hidh_dev_bda_get(param->open.dev);
            esp_hid_usage_t  usage = esp_hidh_dev_usage_get(param->open.dev);
            if (param->open.status == ESP_OK)
            {
                // Update status of device in list.
                bool found = false;
                for(std::size_t idx = 0; idx < pBTHID->btHIDCtrl.devices.size(); idx++)
                {
                    // Try and re-open closed devices.
                    if(memcmp(bda, pBTHID->btHIDCtrl.devices[idx].bda, sizeof(esp_bd_addr_t)) == 0)
                    {
                        pBTHID->btHIDCtrl.devices[idx].open = true;
                        pBTHID->btHIDCtrl.devices[idx].usage = usage;
                        found = true;
                        break;
                    }
                }
                // If device not found on device list it will be a previous pairing which has woken up so add to list.
                if(found == false)
                {
                    memcpy(device.bda, bda, sizeof(esp_bd_addr_t));
                    device.transport = esp_hidh_dev_transport_get(param->open.dev);
                    device.addrType = BLE_ADDR_TYPE_RANDOM;
                    device.open = true;
                    device.usage = usage;
                    pBTHID->btHIDCtrl.devices.push_back(device);
                }
                
                // Ask for the current LED status on keyboards, this is used to pre-set the function locks.
                if(usage == ESP_HID_USAGE_KEYBOARD)
                {
                    esp_hidh_dev_get_report(param->open.dev, 0, 0x1, ESP_HID_REPORT_TYPE_OUTPUT, 10);
                }
                ESP_LOGD(TAG, ESP_BD_ADDR_STR " OPEN: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->open.dev));
                esp_hidh_dev_dump(param->open.dev, stdout);
                vTaskDelay(100);
            } else
            {
                // Update status of device in list.
                for(std::size_t idx = 0; idx < pBTHID->btHIDCtrl.devices.size(); idx++)
                {
                    // Try and re-open closed devices.
                    if(bda != NULL && pBTHID->btHIDCtrl.devices[idx].bda != NULL && memcmp(bda, pBTHID->btHIDCtrl.devices[idx].bda, sizeof(esp_bd_addr_t)) == 0)
                    {
                        pBTHID->btHIDCtrl.devices[idx].open = false;
                    }
                }
                ESP_LOGE(TAG, " OPEN failed!");
              //  pBTHID->closeDevice();
            }
            break;
        }
        case ESP_HIDH_BATTERY_EVENT:
        {
            const uint8_t *bda = esp_hidh_dev_bda_get(param->battery.dev);
            ESP_LOGD(TAG, ESP_BD_ADDR_STR " BATTERY: %d%%", ESP_BD_ADDR_HEX(bda), param->battery.level);
            pBTHID->setBatteryLevel(param->battery.level);
            break;
        }
        case ESP_HIDH_INPUT_EVENT:
        {
            const uint8_t *bda = esp_hidh_dev_bda_get(param->input.dev);
           ESP_LOGD(TAG, ESP_BD_ADDR_STR " INPUT: %8s, MAP: %2u, ID: %3u, Len: %d, Data:", 
                         ESP_BD_ADDR_HEX(bda), 
                         esp_hid_usage_str(param->input.usage), 
                         param->input.map_index, 
                         param->input.report_id, 
                         param->input.length);
           ESP_LOG_BUFFER_HEX_LEVEL(TAG, param->input.data, param->input.length, ESP_LOG_DEBUG);

            // Add data to queue for later filtering and processing.
            pBTHID->pushKeyToFIFO(param->input.usage, param->input.dev, param->input.data, param->input.length);
            break;
        }
        case ESP_HIDH_FEATURE_EVENT: 
        {
            const uint8_t *bda = esp_hidh_dev_bda_get(param->feature.dev);

            for(std::size_t idx = 0; idx < pBTHID->btHIDCtrl.devices.size(); idx++)
            {
                // Matched device?
                if(memcmp(bda, pBTHID->btHIDCtrl.devices[idx].bda, sizeof(esp_bd_addr_t)) == 0)
                {
                    // Is this an LED update report?
                    if(pBTHID->btHIDCtrl.devices[idx].usage == ESP_HID_USAGE_KEYBOARD && param->feature.map_index == 0 && param->feature.report_id == 0x01 && param->feature.length == 0x01)
                    {
                        if(param->feature.data[0] & BT_LED_NUMLOCK)
                        {
                            pBTHID->btHIDCtrl.kbd.btFlags |= BT_NUM_LOCK;
                        } else
                        {
                            pBTHID->btHIDCtrl.kbd.btFlags &= ~BT_NUM_LOCK;
                        }
                        if(param->feature.data[0] & BT_LED_CAPSLOCK)
                        {
                            pBTHID->btHIDCtrl.kbd.btFlags |= BT_CAPS_LOCK;
                        } else
                        {
                            pBTHID->btHIDCtrl.kbd.btFlags &= ~BT_CAPS_LOCK;
                        }
                        if(param->feature.data[0] & BT_LED_SCROLLLOCK)
                        {
                            pBTHID->btHIDCtrl.kbd.btFlags |= BT_SCROLL_LOCK;
                        } else
                        {
                            pBTHID->btHIDCtrl.kbd.btFlags &= ~BT_SCROLL_LOCK;
                        }
                        pBTHID->btHIDCtrl.kbd.statusLED = param->feature.data[0];
                    }
                    break;
                }
            }
            ESP_LOGD(TAG, ESP_BD_ADDR_STR " FEATURE: %8s, MAP: %2u, ID: %3u, Len: %d", 
                          ESP_BD_ADDR_HEX(bda),
                          esp_hid_usage_str(param->feature.usage), 
                          param->feature.map_index, 
                          param->feature.report_id,
                          param->feature.length);
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, param->feature.data, param->feature.length, ESP_LOG_DEBUG);
            break;
        }
        case ESP_HIDH_CLOSE_EVENT:
        {
            const uint8_t *bda = esp_hidh_dev_bda_get(param->close.dev);
            if(bda != NULL)
            {
                for(std::size_t idx = 0; idx < pBTHID->btHIDCtrl.devices.size(); idx++)
                {
                    // Device which has closed?
                    if(memcmp(bda, pBTHID->btHIDCtrl.devices[idx].bda, sizeof(esp_bd_addr_t)) == 0)
                    {
                        ESP_LOGD(TAG, "Closing device:%d,%s", idx, esp_hidh_dev_name_get(param->close.dev));
                        pBTHID->btHIDCtrl.devices[idx].open = false;
                    }
                }
                ESP_LOGD(TAG, ESP_BD_ADDR_STR " CLOSE: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->close.dev));
            }
            break;
        }
        default:
            ESP_LOGD(TAG, "EVENT: %d", event);
            break;
      }
}

// Method to process a received key or mouse movement, keys go onto an internal FIFO queue buffering until the application requests them,
// mouse movements are dispatched immediately via callback as latency is important with a mouse.
// NB: Overflow data is lost so application needs to process data in a timely fashion.
//
void BTHID::pushKeyToFIFO(esp_hid_usage_t src, esp_hidh_dev_t *hdlDev, uint8_t *keys, uint8_t size)
{
    // Locals.
    KeyInfo             keyInfo;
    PS2Mouse::MouseData mouseData;

    // Use FreeRTOS queue manager to push the key record onto the FIFO.
    if(src == ESP_HID_USAGE_KEYBOARD || src == ESP_HID_USAGE_CCONTROL)
    {
        for(int idx=0; idx < MAX_KEYBOARD_DATA_BYTES; idx++)
        {
            if(idx < size)
            {
                keyInfo.keys[idx] = keys[idx];
            } else
            {
                keyInfo.keys[idx] = 0x00;
            }
        }
        keyInfo.length = size;
        keyInfo.cControl = (src == ESP_HID_USAGE_CCONTROL ? true : false);
        keyInfo.hdlDev = hdlDev;
        xQueueSendFromISR(btHIDCtrl.kbd.rawKeyQueue, &keyInfo, 0);
    }
    else if(src == ESP_HID_USAGE_MOUSE)
    {
        // Mouse data is processed realtime. It is massaged into PS/2 data, encapsulated, then passed to the provided
        // callback which handles it.
        mouseData.overrun    = false;
        mouseData.valid      = true;

        // Ensure a movement report.
        if(size > 3)
        {
            // Bit 3 is always set on PS/2 messages.
            mouseData.status     = keys[0] | 0x08;

            // The resolution of a BT mouse is typically 12bit signed, ie. -2048 .. +2047 on both axis. PS/2 was typically 9bit, ie. -255 .. +254 and the Sharp
            // hosts are typically -128 .. +127 so the values need to be scaled down after applying any configurable setting.
            mouseData.position.x =   keys[3] & 0x08 ? (-2048 + ((((keys[3]&0x07) << 8)) | keys[2]))  : (((keys[3]&0x07) << 8) | keys[2]);
            mouseData.position.y = -(keys[4] & 0x80 ? (-2048 + (((keys[4]&0x7f) << 4) | ((keys[3]&0xf0) >> 4))) : (((keys[4]&0x7f) << 4) | ((keys[3]&0xf0) >> 4)));

            // Apply any PS/2 configurable settings which have meaning.
            //
            mouseData.position.x = mouseData.position.x * btHIDCtrl.ms.scaling;
            mouseData.position.y = mouseData.position.y * btHIDCtrl.ms.scaling;
            mouseData.position.x = mouseData.position.x * btHIDCtrl.ms.resolution;
            mouseData.position.y = mouseData.position.y * btHIDCtrl.ms.resolution;

            // Set the wheel value.
            mouseData.wheel      = keys[5];
        }

        // If a data callback has been setup, invoke otherwise data is wasted.
        //
        if(btHIDCtrl.ms.mouseDataCallback != NULL)
            btHIDCtrl.ms.mouseDataCallback(mouseData);
    }
    return;
}

// Method to check devices for connectivity. This generally entails re-opening closed devices as BT links are self maintaining until closure.
//
void BTHID::checkBTDevices(void)
{
    // Locals.
    //
    bool                            nonFound = true;
    std::vector<BT::t_scanListItem> scanList;

    // Loop through list of known devices and open a connection with them. If no devices exist or no connection can be opened, start
    // a scan for new devices. Normally, bonded devices when activated will connect but sometimes a physical open is needed hence this 
    // logic.
    for(std::size_t idx = 0; idx < btHIDCtrl.devices.size(); idx++)
    {
        if(btHIDCtrl.devices[idx].open == true)
        {
            nonFound = false;
        } else
        {
            // If the timer has expired on this entry, make an open attempt.
            if(btHIDCtrl.devices[idx].nextCheckTime <= milliSeconds())
            {
                ESP_LOGI(TAG, ESP_BD_ADDR_STR " PAIREDOPEN", ESP_BD_ADDR_HEX(btHIDCtrl.devices[idx].bda));
                if(openDevice(btHIDCtrl.devices[idx].bda, btHIDCtrl.devices[idx].transport, btHIDCtrl.devices[idx].addrType) == true)
                {
                    btHIDCtrl.devices[idx].open = true;
                } else
                {
                    btHIDCtrl.devices[idx].nextCheckTime = milliSeconds() + 5000L;
                }
            }
            nonFound = false;
        }
    }
    if(nonFound)
    {
        // Get list of devices which can be seen by bluetooth receiver and try to connect to known/pairing devices.
        getDeviceList(scanList, 5);

        for(int idx = 0; idx < scanList.size(); idx++)
        {
            ESP_LOGI(TAG, ESP_BD_ADDR_STR " SCANOPEN", ESP_BD_ADDR_HEX(scanList[idx].bda));
            openDevice(scanList[idx].bda, scanList[idx].transport, scanList[idx].ble.addr_type);
        }
    }
    return;
}

//*********************************************************************************************************************************************
//** Mouse handler Methods.
//*********************************************************************************************************************************************
//
// Protocol: Finding accurate information for the Mouse low level protocol is not so easy and what you do find doesnt match the data reports
//           sent by the ESP HIDH. The protocol as worked out (so far) is:
//
// <Byte 0><Byte 1><Byte 2><Byte 3><Byte 4><Byte 5><Byte 6>
// <Byte 0> = Status or Button Report. Bit 2 = Wheel/Middle Button, Bit 1 = Right Button, Bit 0 = Left Button. '1' = Button pressed.
// <Byte 1> = 0x00
// <Byte 2> = LSB [7:0] of X co-ordinate, Signed range -2048:+2047
// <Byte 3> = MSNIBBLE [11:8] contained in bits [3:0] of x co-ordinate. LSNIBBLE [3:0] contained in bits [7:4] or y co-ordinate.
// <Byte 4> = MSB [11:4] of Y co-ordinate, Signed range -2048:+2047
// <Byte 5> = 0x00
// <Byte 6> = 0x00
// Reading the data, sample rate is 125 frames per second.
//

// Public method to set the mouse resolution in pixels per millimeter, valid values are 0..3.
// This method is for compatibility with a PS/2 Mouse, any use of the value has to be programmtical in this module prior to delivery
// of the fixed data streamed from the BT HID.
//
bool BTHID::setResolution(enum PS2Mouse::PS2_RESOLUTION resolution)
{
    // Locals.
    //
    bool          result = false;

    // Sanity check.
    if(resolution >= PS2Mouse::PS2_MOUSE_RESOLUTION_1_1 && resolution < PS2Mouse::PS2_MOUSE_RESOLUTION_1_8)
    {
        switch(to_underlying(resolution))
        {
            case 0: // 1pixel per mm.
                btHIDCtrl.ms.resolution = 1;
                break;
            case 1: // 2pixles per mm.
                btHIDCtrl.ms.resolution = 2;
                break;
            case 2: // 4pixels per mm.
                btHIDCtrl.ms.resolution = 4;
                break;
            case 3: // 8pixels per mm.
            default:
                btHIDCtrl.ms.resolution = 8;
                break;
        }
        result = true;
    }

    // Return result.
    return(result);
}

// Public method to set the mouse scaling, either Normal 1:1 (scaling = 0) or non-linear 2:1 (scaling = 1).
// This method is for compatibility with a PS/2 Mouse, any use of the value has to be programmtical in this module prior to delivery
// of the fixed data streamed from the BT HID.
//
bool BTHID::setScaling(enum PS2Mouse::PS2_SCALING scaling) 
{
    // Locals.
    //
    bool          result = false;

    // Sanity check.
    if(scaling >= PS2Mouse::PS2_MOUSE_SCALING_1_1 && scaling < PS2Mouse::PS2_MOUSE_SCALING_2_1)
    {
        btHIDCtrl.ms.scaling = to_underlying(scaling)+1;
        result = true;
    }

    // Return result.
    return(result);
}

// Public method to set the automatic sample rate.
// This method is for compatibility with a PS/2 Mouse, any use of the value has to be programmtical in this module prior to delivery
// of the fixed data streamed from the BT HID.
//
bool BTHID::setSampleRate(enum PS2Mouse::PS2_SAMPLING rate)
{
    // Locals.
    //
    bool          result = false;

    // Sanity check.
    if(rate == PS2Mouse::PS2_MOUSE_SAMPLE_RATE_10 || rate == PS2Mouse::PS2_MOUSE_SAMPLE_RATE_20 || rate == PS2Mouse::PS2_MOUSE_SAMPLE_RATE_40 || rate == PS2Mouse::PS2_MOUSE_SAMPLE_RATE_60 || rate == PS2Mouse::PS2_MOUSE_SAMPLE_RATE_80 || rate == PS2Mouse::PS2_MOUSE_SAMPLE_RATE_100 || rate == PS2Mouse::PS2_MOUSE_SAMPLE_RATE_200)
    {
        btHIDCtrl.ms.sampleRate = to_underlying(rate);
        result = true;
    }

    // Return result.
    return(result);
}

//*********************************************************************************************************************************************
//** Keyboard handler Methods.
//*********************************************************************************************************************************************

// Method to map a Bluetooth Media Key (ESP HIDH specific) Scan Code to its PS/2 equivalent or 0x0000 if not mappable.
uint16_t BTHID::mapBTMediaToPS2(uint32_t key)
{
    // Locals.
    //
    uint16_t       retKey = 0x0000;

    // Loop through mapping table to find a match.
    for(int idx=0; idx < btHIDCtrl.kbd.kmeMediaRows; idx++)
    {
        if(btHIDCtrl.kbd.kmeMedia[idx].mediaKey == key)
        {
            retKey = (btHIDCtrl.kbd.kmeMedia[idx].ps2Ctrl << 8) | btHIDCtrl.kbd.kmeMedia[idx].ps2Key;
            break;
        }
    }

    // Return map result or 0x00 if not mappable.
    return(retKey);
}

// Method to map a Bluetooth Scan Code to its PS/2 equivalent or 0x00 if not mappable.
uint16_t BTHID::mapBTtoPS2(uint8_t key)
{
    // Locals.
    //
    uint16_t      retKey = 0x0000;
  
    // Loop through mapping table to find a match.
    for(int idx=0; idx < btHIDCtrl.kbd.kmeRows && retKey == 0x0000; idx++)
    {
        // Find a match.
        if(btHIDCtrl.kbd.kme[idx].btKeyCode == key && (btHIDCtrl.kbd.kme[idx].btCtrl == btHIDCtrl.kbd.btFlags || btHIDCtrl.kbd.kme[idx].btCtrl == BT_NONE))
        {
            retKey = (uint16_t)btHIDCtrl.kbd.kme[idx].ps2KeyCode;
            if((retKey <= PS2_KEY_SPACE || retKey >= PS2_KEY_F1) && retKey != PS2_KEY_BTICK && retKey != PS2_KEY_HASH && retKey != PS2_KEY_EUROPE2) retKey |= PS2_FUNCTION;
            if(btHIDCtrl.kbd.btFlags & BT_CTRL_LEFT || btHIDCtrl.kbd.btFlags & BT_CTRL_RIGHT)   retKey |= PS2_CTRL;
            if(btHIDCtrl.kbd.btFlags & BT_SHIFT_LEFT || btHIDCtrl.kbd.btFlags & BT_SHIFT_RIGHT) retKey |= PS2_SHIFT;
            if(btHIDCtrl.kbd.btFlags & BT_ALT_LEFT)                                             retKey |= PS2_ALT;
            if(btHIDCtrl.kbd.btFlags & BT_ALT_RIGHT)                                            retKey |= PS2_ALT_GR;
            if(btHIDCtrl.kbd.btFlags & BT_GUI_LEFT || btHIDCtrl.kbd.btFlags & BT_GUI_RIGHT)     retKey |= PS2_GUI;
        }
    }

    // Return map result or 0x00 if not mappable.
    return(retKey);
}

// Method to set a status LED on the keyboard.
//
void BTHID::setStatusLED(esp_hidh_dev_t *dev, uint8_t led)
{
    // Locals

    // Set flag in LED status byte then forward to the keyboard for actual display.
    btHIDCtrl.kbd.statusLED |= led;
    esp_hidh_dev_output_set(dev, 0, 0x1, &btHIDCtrl.kbd.statusLED, 1);
    return;
}

// Method to clear a status LED on the keyboard.
//
void BTHID::clearStatusLED(esp_hidh_dev_t *dev, uint8_t led)
{
    // Locals

    // Clear flag in LED status byte then forward to the keyboard for actual display.
    btHIDCtrl.kbd.statusLED &= ~led;
    esp_hidh_dev_output_set(dev, 0, 0x1, &btHIDCtrl.kbd.statusLED, 1);
    return;
}

// Method to process the incoming Bluetooth keyboard data stream and convert it into PS/2 compatible values. 
//
// Protocol (received after pre-processing by the BT module)
// --------
// KEYBOARD:
// <Modifier><0x00><Scan code 1><Scan code 2><Scan code 3><Scan code 4><Scan code 5><Scan code 6>
// All scan codes are set to overflow (0x01) if more than 6 keys are pressed.
//
// Modifier Byte:
// Bit 7     Bit 6     Bit 5       Bit 4      Bit 3    Bit 2    Bit 1      Bit 0
// Right GUI Right Alt Right Shift Right Ctrl Left GUI Left Alt Left Shift Left Ctrl
// 1 = Key Active, 0 = Inactive.
//
// CCONTROL: (the esp idf splits the bluetooth report of keys into keys and media control)
// ESP havent documented the HIDH module so the values below are worked out, needs updating when they provide documentation.
// <Byte 1><Byte 2><Byte 3>
// 
// <Byte 1> Bit 7     Bit 6     Bit 5       Bit 4      Bit 3    Bit 2    Bit 1      Bit 0
//                              SEARCH                 HOME
// <Byte 2> Bit 7     Bit 6     Bit 5       Bit 4      Bit 3    Bit 2    Bit 1      Bit 0
//          BRITEDN   BRITEUP   
// <Byte 3> Bit 7     Bit 6     Bit 5       Bit 4      Bit 3    Bit 2    Bit 1      Bit 0
//                    MUTE      VOL DOWN    VOL UP                                  TRK PREV
//          
// 
// A down event sees the scan code appear in the list, an up event it disappears. For the modifier bits, the bit is set for down event and cleared for up event.
//
// Control mapping - BT modifier needs to be mapped to these bits:
//      Define name bit     description
//      PS2_BREAK   15      1 = Break key code
//                 (MSB)    0 = Make Key code
//      PS2_SHIFT   14      1 = Shift key pressed as well (either side)
//                          0 = NO shift key
//      PS2_CTRL    13      1 = Ctrl key pressed as well (either side)
//                          0 = NO Ctrl key
//      PS2_CAPS    12      1 = Caps Lock ON
//                          0 = Caps lock OFF
//      PS2_ALT     11      1 = Left Alt key pressed as well
//                          0 = NO Left Alt key
//      PS2_ALT_GR  10      1 = Right Alt (Alt GR) key pressed as well
//                          0 = NO Right Alt key
//      PS2_GUI      9      1 = GUI key pressed as well (either)
//                          0 = NO GUI key
//      PS2_FUNCTION 8      1 = FUNCTION key non-printable character (plus space, tab, enter)
//                          0 = standard character key
//
// Mapped data/events is pushed onto a queue which is read by the calling API.
//
void BTHID::processBTKeys(void)
{
    // Locals.
    uint16_t   genKey;
    uint32_t   mediaKey;
    KeyInfo    keyInfo;

    // Process all the queued event data.
    while(xQueueReceive(btHIDCtrl.kbd.rawKeyQueue, &keyInfo, 0) == pdTRUE)
    {
        // Process normal scancodes.
        if(keyInfo.cControl == false)
        {
            // Only process if the size is correct.
            if(keyInfo.length <= MAX_KEYBOARD_DATA_BYTES)
            {
                // Process control keys and set flags.
                if(keyInfo.keys[0] & BT_CTRL_LEFT)   btHIDCtrl.kbd.btFlags |= BT_CTRL_LEFT;    else btHIDCtrl.kbd.btFlags &= ~(BT_CTRL_LEFT);
                if(keyInfo.keys[0] & BT_SHIFT_LEFT)  btHIDCtrl.kbd.btFlags |= BT_SHIFT_LEFT;   else btHIDCtrl.kbd.btFlags &= ~(BT_SHIFT_LEFT);
                if(keyInfo.keys[0] & BT_ALT_LEFT)    btHIDCtrl.kbd.btFlags |= BT_ALT_LEFT;     else btHIDCtrl.kbd.btFlags &= ~BT_ALT_LEFT;
                if(keyInfo.keys[0] & BT_GUI_LEFT)    btHIDCtrl.kbd.btFlags |= BT_GUI_LEFT;     else btHIDCtrl.kbd.btFlags &= ~BT_GUI_LEFT;
                if(keyInfo.keys[0] & BT_CTRL_RIGHT)  btHIDCtrl.kbd.btFlags |= BT_CTRL_RIGHT;   else btHIDCtrl.kbd.btFlags &= ~(BT_CTRL_RIGHT);
                if(keyInfo.keys[0] & BT_SHIFT_RIGHT) btHIDCtrl.kbd.btFlags |= BT_SHIFT_RIGHT;  else btHIDCtrl.kbd.btFlags &= ~(BT_SHIFT_RIGHT);
                if(keyInfo.keys[0] & BT_ALT_RIGHT)   btHIDCtrl.kbd.btFlags |= BT_ALT_RIGHT;    else btHIDCtrl.kbd.btFlags &= ~(BT_ALT_RIGHT);
                if(keyInfo.keys[0] & BT_GUI_RIGHT)   btHIDCtrl.kbd.btFlags |= BT_GUI_RIGHT;    else btHIDCtrl.kbd.btFlags &= ~(BT_GUI_RIGHT);

                // Process the control(modifier) keys and generate events.
                // CTRL keys
                if((keyInfo.keys[0] & BT_CTRL_LEFT) != 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_CTRL_LEFT) == 0)
                {
                    // First time key was pressed send a Make event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_CTRL | PS2_FUNCTION | PS2_KEY_L_CTRL;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                if((keyInfo.keys[0] & BT_CTRL_LEFT) == 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_CTRL_LEFT) != 0)
                {
                    // Key being released generates a BREAK event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_BREAK | PS2_FUNCTION | PS2_KEY_L_CTRL;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                if((keyInfo.keys[0] & BT_CTRL_RIGHT) != 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_CTRL_RIGHT) == 0)
                {
                    // First time key was pressed send a Make event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_CTRL | PS2_FUNCTION | PS2_KEY_R_CTRL;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                if((keyInfo.keys[0] & BT_CTRL_RIGHT) == 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_CTRL_RIGHT) != 0)
                {
                    // Key being released generates a BREAK event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_BREAK | PS2_FUNCTION | PS2_KEY_R_CTRL;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                // SHIFT Keys
                if((keyInfo.keys[0] & BT_SHIFT_LEFT) != 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_SHIFT_LEFT) == 0)
                {
                    // First time key was pressed send a Make event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_SHIFT | PS2_FUNCTION | PS2_KEY_L_SHIFT;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                if((keyInfo.keys[0] & BT_SHIFT_LEFT) == 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_SHIFT_LEFT) != 0)
                {
                    // Key being released generates a BREAK event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_BREAK | PS2_FUNCTION | PS2_KEY_L_SHIFT;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                if((keyInfo.keys[0] & BT_SHIFT_RIGHT) != 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_SHIFT_RIGHT) == 0)
                {
                    // First time key was pressed send a Make event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_SHIFT | PS2_FUNCTION | PS2_KEY_R_SHIFT;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                if((keyInfo.keys[0] & BT_SHIFT_RIGHT) == 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_SHIFT_RIGHT) != 0)
                {
                    // Key being released generates a BREAK event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_BREAK | PS2_FUNCTION | PS2_KEY_R_SHIFT;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                // ALT Keys
                if((keyInfo.keys[0] & BT_ALT_LEFT) != 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_ALT_LEFT) == 0)
                {
                    // First time key was pressed send a Make event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_ALT | PS2_FUNCTION | PS2_KEY_L_ALT;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                if((keyInfo.keys[0] & BT_ALT_LEFT) == 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_ALT_LEFT) != 0)
                {
                    // Key being released generates a BREAK event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_BREAK | PS2_FUNCTION | PS2_KEY_L_ALT;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                if((keyInfo.keys[0] & BT_ALT_RIGHT) != 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_ALT_RIGHT) == 0)
                {
                    // First time key was pressed send a Make event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_ALT_GR | PS2_FUNCTION | PS2_KEY_R_ALT;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                if((keyInfo.keys[0] & BT_ALT_RIGHT) == 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_ALT_RIGHT) != 0)
                {
                    // Key being released generates a BREAK event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_BREAK | PS2_FUNCTION | PS2_KEY_R_ALT;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                // GUI Keys
                if((keyInfo.keys[0] & BT_GUI_LEFT) != 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_GUI_LEFT) == 0)
                {
                    // First time key was pressed send a Make event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_GUI | PS2_FUNCTION | PS2_KEY_L_GUI;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                if((keyInfo.keys[0] & BT_GUI_LEFT) == 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_GUI_LEFT) != 0)
                {
                    // Key being released generates a BREAK event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_BREAK | PS2_FUNCTION | PS2_KEY_L_GUI;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                if((keyInfo.keys[0] & BT_GUI_RIGHT) != 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_GUI_RIGHT) == 0)
                {
                    // First time key was pressed send a Make event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_GUI | PS2_FUNCTION | PS2_KEY_R_GUI;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }
                if((keyInfo.keys[0] & BT_GUI_RIGHT) == 0 && (btHIDCtrl.kbd.lastKeys[0] & BT_GUI_RIGHT) != 0)
                {
                    // Key being released generates a BREAK event.
                    genKey = (btHIDCtrl.kbd.ps2Flags & 0xFF00) | PS2_BREAK | PS2_FUNCTION | PS2_KEY_R_GUI;
                    xQueueSend(btHIDCtrl.kbd.keyQueue, &genKey, 0);
                }

                // Loop through the 6 scan codes and if a code appears in this set but not in the last generate a Make event.
                //
                for(int idx=1; idx < MAX_KEYBOARD_DATA_BYTES; idx++)
                {
                    if(keyInfo.keys[idx] != 0)
                    {
                        bool found = false;
                        for(int idx2=1; idx2 < MAX_KEYBOARD_DATA_BYTES; idx2++)
                        {
                            if(keyInfo.keys[idx] == btHIDCtrl.kbd.lastKeys[idx2]) found = true;
                        }
                        if(!found)
                        {
                            // Process CAPS Lock.
                            if(keyInfo.keys[idx] == BT_KEY_CAPSLOCK && (btHIDCtrl.kbd.btFlags & BT_CAPS_LOCK) == 0)
                            {
                                btHIDCtrl.kbd.btFlags |= BT_CAPS_LOCK;
                                setStatusLED(keyInfo.hdlDev, BT_LED_CAPSLOCK);
                            }
                            else if(keyInfo.keys[idx] == BT_KEY_CAPSLOCK && (btHIDCtrl.kbd.btFlags & BT_CAPS_LOCK) != 0)
                            {
                                btHIDCtrl.kbd.btFlags &= ~(BT_CAPS_LOCK);
                                clearStatusLED(keyInfo.hdlDev, BT_LED_CAPSLOCK);
                            }
                           
                            // Process NUM Lock.
                            if(keyInfo.keys[idx] == BT_KEY_NUMLOCK && (btHIDCtrl.kbd.btFlags & BT_NUM_LOCK) == 0)
                            {
                                btHIDCtrl.kbd.btFlags |= BT_NUM_LOCK;
                                setStatusLED(keyInfo.hdlDev, BT_LED_NUMLOCK);
                            }
                            else if(keyInfo.keys[idx] == BT_KEY_NUMLOCK && (btHIDCtrl.kbd.btFlags & BT_NUM_LOCK) != 0)
                            {
                                btHIDCtrl.kbd.btFlags &= ~(BT_NUM_LOCK);
                                clearStatusLED(keyInfo.hdlDev, BT_LED_NUMLOCK);
                            }
                           
                            // Process SCROLL Lock.
                            if(keyInfo.keys[idx] == BT_KEY_SCROLLLOCK && (btHIDCtrl.kbd.btFlags & BT_SCROLL_LOCK) == 0)
                            {
                                btHIDCtrl.kbd.btFlags |= BT_SCROLL_LOCK;
                                setStatusLED(keyInfo.hdlDev, BT_LED_SCROLLLOCK);
                            }
                            else if(keyInfo.keys[idx] == BT_KEY_SCROLLLOCK && (btHIDCtrl.kbd.btFlags & BT_SCROLL_LOCK) != 0)
                            {
                                btHIDCtrl.kbd.btFlags &= ~(BT_SCROLL_LOCK);
                                clearStatusLED(keyInfo.hdlDev, BT_LED_SCROLLLOCK);
                            }

                            // Mimicking the PS/2 class, set Function for certain mapped keys.
                            uint16_t mapKey = mapBTtoPS2(keyInfo.keys[idx]);
                            ESP_LOGI(TAG, "BTKEYMAP:%02x:%04x -> %04x", keyInfo.keys[idx], btHIDCtrl.kbd.btFlags, mapKey);
                                    
                            // Do not forward certain keys.
                            if(mapKey != 0x0000 && keyInfo.keys[idx] != BT_KEY_NUMLOCK)
                            {
                                // Create a Make event.
                                xQueueSend(btHIDCtrl.kbd.keyQueue, &mapKey, 0);
                            }
                        }
                    }
                    // Now repeat in reverse, has a break event occurred?
                    if(btHIDCtrl.kbd.lastKeys[idx] != 0)
                    {
                        bool found = false;
                        for(int idx2=1; idx2 < MAX_KEYBOARD_DATA_BYTES; idx2++)
                        {
                            if(btHIDCtrl.kbd.lastKeys[idx] == keyInfo.keys[idx2]) found = true;
                        }
                        if(!found)
                        {
                            uint16_t mapKey = mapBTtoPS2(btHIDCtrl.kbd.lastKeys[idx]);
                            mapKey |= PS2_BREAK; // Send break event by adding PS2_BREAK control flag.
                                    
                            // Do not forward certain keys.
                            if(mapKey != 0x0000 && btHIDCtrl.kbd.lastKeys[idx] != BT_KEY_NUMLOCK)
                            {
                                // Create a Break event.
                                xQueueSend(btHIDCtrl.kbd.keyQueue, &mapKey, 0);
                            }
                        }
                    }
                }
            }
        }
        // Media control keys, for some reason these come as a seperate BT report and are 24bits wide.
        else
        {
            // Only process if size is correct.
            if(keyInfo.length == MAX_CCONTROL_DATA_BYTES)
            {
                // Assemble 24bit map, easier to work with.
                mediaKey = (keyInfo.keys[0] << 16) | (keyInfo.keys[1] << 8) | (keyInfo.keys[2]);

                // Check for key Make events.
                for(int idx=0; idx < 23; idx++)
                {
                    uint32_t mask = (1 << idx);
                  
                    // Make event.
                    if((mediaKey & mask) != 0 && (btHIDCtrl.kbd.lastMediaKey & mask) == 0)
                    {
                        uint16_t mapKey = mapBTMediaToPS2(mediaKey & mask);
                        xQueueSend(btHIDCtrl.kbd.keyQueue, &mapKey, 0);

                    }
                    // Break event.
                    if((mediaKey & mask) == 0 && (btHIDCtrl.kbd.lastMediaKey & mask) != 0)
                    {
                        uint16_t mapKey = mapBTMediaToPS2(btHIDCtrl.kbd.lastMediaKey & mask);
                        xQueueSend(btHIDCtrl.kbd.keyQueue, &mapKey, 0);
                    }
                }

                // Store last processed keymap for next loop.
                btHIDCtrl.kbd.lastMediaKey = mediaKey;
            }
        }

        // Copy current to last.
        for(int idx=0; idx < MAX_KEYBOARD_DATA_BYTES; idx++)
        {
            btHIDCtrl.kbd.lastKeys[idx] = keyInfo.keys[idx];
        }
    }

    return;
}

// Method to retrieve a key from the BT stack. The key is mapped from BT scancodes to PS/2 scancodes.
//
uint16_t BTHID::getKey(uint32_t timeout)
{
    // Locals.
    //
    uint16_t  key;
    bool      result = false;
    uint32_t  timeCurrent = milliSeconds();

    // Loop processing BT keys until a key received or timeout occurs.
    do {
        // Process latest BT keys.
        processBTKeys();

        // Get the next key from the processed queue and return to caller.
        result = (xQueueReceive(btHIDCtrl.kbd.keyQueue, &key, 0) == pdTRUE ? true : false);
    } while(timeout > 0 && timeCurrent+timeout > milliSeconds() && result == false);
  
    // Return key if one has been read else 0x00.
    return(result == true ? key : 0x00); 
}

// Method to configure Bluetooth and register required callbacks.
bool BTHID::setup(t_pairingHandler *handler)
{
    // Locals.
    //
    bool           result = false;

    // Check for multiple instantiations, only one instance allowed.
    if(pBTHID != nullptr)
    {
        ESP_LOGE(TAG, "Setup called more than once. Only one instance of BTHID is allowed.");
    } else
    {      
        // Invoke the base class method which sets up the bluetooth layer.
        BT::setup(handler);

        // Store current object for use in callback handlers.
        pBTHID = this;
      
        // Create a FIFO queue to store incoming keyboard keys and mouse movements.
        btHIDCtrl.kbd.rawKeyQueue   = xQueueCreate(10, sizeof(KeyInfo));
        btHIDCtrl.kbd.keyQueue      = xQueueCreate(10, sizeof(uint16_t));

        ESP_ERROR_CHECK(esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler));
        esp_hidh_config_t config = {
          .callback         = hidh_callback,
          .event_stack_size = 4*1024,         
          .callback_arg     = nullptr
        };
        ESP_ERROR_CHECK(esp_hidh_init(&config));
        result = true;
      
        // Go through bonded lists and add to our control vector or known devices.
        // First BLE devices.
        int bleDevNum = esp_ble_get_bond_device_num();
        esp_ble_bond_dev_t *bleDevList = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * bleDevNum);
        esp_ble_get_bond_device_list(&bleDevNum, bleDevList);
        for (int idx = 0; idx < bleDevNum; idx++)
        {
            t_activeDev device;
            memcpy(device.bda, bleDevList[idx].bd_addr, sizeof(esp_bd_addr_t));
            device.transport = ESP_HID_TRANSPORT_BLE;
            device.addrType = BLE_ADDR_TYPE_RANDOM;
            device.open = false;
            device.nextCheckTime = milliSeconds() + 3000L;
            btHIDCtrl.devices.push_back(device);
            ESP_LOGW(TAG, "BLE BONDED DEVICE: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(bleDevList[idx].bd_addr));
        }
        free(bleDevList);

        // Next BT devices.
        int btDevNum = esp_bt_gap_get_bond_device_num();
        esp_bd_addr_t *btDevList = (esp_bd_addr_t *)malloc(sizeof(esp_bd_addr_t) * btDevNum);
        esp_bt_gap_get_bond_device_list(&btDevNum, btDevList);
        for (int idx = 0; idx < btDevNum; idx++)
        {
            t_activeDev device;
            memcpy(device.bda, btDevList[idx], sizeof(esp_bd_addr_t));
            device.transport = ESP_HID_TRANSPORT_BT;
            device.addrType = BLE_ADDR_TYPE_RANDOM;
            device.open = false;
            device.nextCheckTime = milliSeconds() + 3000L;
            btHIDCtrl.devices.push_back(device);
            ESP_LOGW(TAG, "BT BONDED DEVICE: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(btDevList[idx]));
        }
        free(btDevList);
    }
  
    // False = failed to setup, true = success.
    return(result);
}

// Basic constructor, do nothing! 
BTHID::BTHID(void)
{
    btHIDCtrl.kbd.rawKeyQueue   = NULL;
    btHIDCtrl.kbd.keyQueue      = NULL;
    memset((void *)&btHIDCtrl.kbd.lastKeys, 0x00, 6);
    btHIDCtrl.kbd.lastMediaKey  = 0x00000000;
    btHIDCtrl.kbd.ps2Flags      = 0x0000;
    btHIDCtrl.kbd.btFlags       = 0x0000;
    btHIDCtrl.kbd.statusLED     = 0x00;
    btHIDCtrl.kbd.kme           = BTKeyToPS2.kme;
    btHIDCtrl.kbd.kmeRows       = MAX_BT2PS2_MAP_ENTRIES;
    btHIDCtrl.kbd.kmeMedia      = MediaKeyToPS2.kme;
    btHIDCtrl.kbd.kmeMediaRows  = MAX_BTMEDIA2PS2_MAP_ENTRIES;
    btHIDCtrl.ms.mouseDataCallback = NULL;
    btHIDCtrl.ms.resolution     = 8;
    btHIDCtrl.ms.scaling        = 1;
    btHIDCtrl.ms.sampleRate     = 100;
    btHIDCtrl.ms.xDivisor       = 8;
    btHIDCtrl.ms.yDivisor       = 8;

 //   btHIDCtrl.repeatPeriod = pdMS_TO_TICKS(120);
}

// Basic destructor, do nothing! Only ever called for instantiation of uninitialsed class to prove version data.Used for probing versions etc.
BTHID::~BTHID(void)
{
    //
}
