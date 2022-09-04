/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            BT.h
// Created:         Jan 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Header file for the Bluetooth Class.
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
#ifndef BT_H_
#define BT_H_

#include <string>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_hidh.h"
#include "esp_hid_common.h"
#include "esp_gap_bt_api.h"
#include "esp_gap_ble_api.h"

// Bluetooth interface class. Provides Mouse and Keyboard functionality via the Bluetooth wireless interface.
class BT {
    #define SIZEOF_ARRAY(a) (sizeof(a) / sizeof(*a))

    public:
        typedef void t_pairingHandler(uint32_t code, uint8_t trigger);

        // Structure to contain details of a single device forming a scanned device list.
        //
        typedef struct {
            esp_bd_addr_t                  bda;
            std::string                    name;
            int8_t                         rssi;
            esp_hid_usage_t                usage;
            esp_hid_transport_t            transport;            //BT, BLE or USB
          
            union {
                struct {
                    esp_bt_cod_t           cod;
                    esp_bt_uuid_t          uuid;
                } bt;
                struct {
                    esp_ble_addr_type_t    addr_type;
                    uint16_t               appearance;
                } ble;
            };
            
            // Display format values.
            std::string                    deviceAddr;           // MAC address of the Bluetooth device.
            std::string                    deviceType;           // BT, BLE or USB
        } t_scanListItem; 


        // Prototypes.
                                           BT(void);
        virtual                            ~BT(void);
        void                               getDeviceList(std::vector<t_scanListItem> &scanList, int waitTime);
        bool                               setup(t_pairingHandler *handler = nullptr);

        inline uint8_t                     getBatteryLevel() { return btCtrl.batteryLevel; }
        inline void                        setBatteryLevel(uint8_t level) { btCtrl.batteryLevel = level; }
        
    private:
        static constexpr char const        *TAG = "BT";
      #ifdef CONFIG_CLASSIC_BT_ENABLED
        const char                        *gap_bt_prop_type_names[5] = { "", "BDNAME", "COD", "RSSI", "EIR" };
        const char                        *bt_gap_evt_names[10]      = { "DISC_RES", "DISC_STATE_CHANGED", "RMT_SRVCS", "RMT_SRVC_REC", "AUTH_CMPL", "PIN_REQ", "CFM_REQ", "KEY_NOTIF", "KEY_REQ", "READ_RSSI_DELTA" };
      #endif
        const char                        *ble_gap_evt_names[28]     = { "ADV_DATA_SET_COMPLETE", "SCAN_RSP_DATA_SET_COMPLETE", "SCAN_PARAM_SET_COMPLETE", "SCAN_RESULT", "ADV_DATA_RAW_SET_COMPLETE",
                                                                         "SCAN_RSP_DATA_RAW_SET_COMPLETE", "ADV_START_COMPLETE", "SCAN_START_COMPLETE", "AUTH_CMPL", "KEY", 
                                                                         "SEC_REQ", "PASSKEY_NOTIF", "PASSKEY_REQ", "OOB_REQ", "LOCAL_IR", 
                                                                         "LOCAL_ER", "NC_REQ", "ADV_STOP_COMPLETE", "SCAN_STOP_COMPLETE", "SET_STATIC_RAND_ADDR", 
                                                                         "UPDATE_CONN_PARAMS", "SET_PKT_LENGTH_COMPLETE", "SET_LOCAL_PRIVACY_COMPLETE", "REMOVE_BOND_DEV_COMPLETE", "CLEAR_BOND_DEV_COMPLETE", 
                                                                         "GET_BOND_DEV_COMPLETE", "READ_RSSI_COMPLETE", "UPDATE_WHITELIST_COMPLETE" };
        const char                        *ble_addr_type_names[4]    = { "PUBLIC", "RANDOM", "RPA_PUBLIC", "RPA_RANDOM" };

        // Define possible HIDH host modes.
        static const esp_bt_mode_t         HIDH_IDLE_MODE            = (esp_bt_mode_t) 0x00;
        static const esp_bt_mode_t         HIDH_BLE_MODE             = (esp_bt_mode_t) 0x01;
        static const esp_bt_mode_t         HIDH_BT_MODE              = (esp_bt_mode_t) 0x02;
        static const esp_bt_mode_t         HIDH_BTDM_MODE            = (esp_bt_mode_t) 0x03;

        // Structure to maintain control variables.
        typedef struct {
          #ifdef CONFIG_CLASSIC_BT_ENABLED
            std::vector<t_scanListItem>    btScanList;
          #endif
            std::vector<t_scanListItem>    bleScanList;

            t_pairingHandler              *pairingHandler;
            esp_hidh_dev_t                *hidhDevHdl;

            int8_t                         batteryLevel;

          #ifdef CONFIG_CLASSIC_BT_ENABLED
            xSemaphoreHandle               bt_hidh_cb_semaphore;
          #endif
            xSemaphoreHandle               ble_hidh_cb_semaphore;

            BT                            *pThis;
        } t_btCtrl;

        // All control variables are stored in a struct for ease of reference.
        t_btCtrl                           btCtrl;


        // Prototypes.
        static void                        processBTGapEvent(esp_bt_gap_cb_event_t event,  esp_bt_gap_cb_param_t * param);
        static void                        processBLEGapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t * param);
        t_scanListItem*                    findValidScannedDevice(esp_bd_addr_t bda, std::vector<t_scanListItem> &scanList);
        void                               processBLEDeviceScanResult(esp_ble_gap_cb_param_t * scan_rst);
        void                               addBLEScanDevice(esp_bd_addr_t bda, esp_ble_addr_type_t addr_type, uint16_t appearance, uint8_t *name, uint8_t name_len, int rssi);

      #ifdef CONFIG_CLASSIC_BT_ENABLED
        void                               processBTDeviceScanResult(esp_bt_gap_cb_param_t  * param);
        void                               addBTScanDevice(esp_bd_addr_t bda, esp_bt_cod_t *cod, esp_bt_uuid_t *uuid, uint8_t *name, uint8_t name_len, int rssi);
      #endif
        esp_err_t                          scanForBLEDevices(uint32_t timeout);
        esp_err_t                          scanForBTDevices(uint32_t timeout);
        esp_err_t                          scanForAllDevices(uint32_t timeout, size_t *noDevices, std::vector<t_scanListItem> &scanList);
        void                               printUUID(esp_bt_uuid_t * uuid);

        const char *ble_addr_type_str(esp_ble_addr_type_t ble_addr_type)
        {
            if (ble_addr_type > BLE_ADDR_TYPE_RPA_RANDOM)
            {
                return "UNKNOWN";
            }
            return ble_addr_type_names[ble_addr_type];
        }

        const char *ble_gap_evt_str(uint8_t event)
        {
            if (event >= SIZEOF_ARRAY(ble_gap_evt_names)) 
            {
                return "UNKNOWN";
            }
            return ble_gap_evt_names[event];
        }

      #ifdef CONFIG_CLASSIC_BT_ENABLED
        const char *bt_gap_evt_str(uint8_t event)
        {
            if (event >= SIZEOF_ARRAY(bt_gap_evt_names))
            {
                return "UNKNOWN";
            }
            return bt_gap_evt_names[event];
        }
      #endif

        const char *ble_key_type_str(esp_ble_key_type_t key_type)
        {
            const char *key_str = nullptr;
            switch (key_type)
            {
                case ESP_LE_KEY_NONE:
                    key_str = "ESP_LE_KEY_NONE";
                    break;
                case ESP_LE_KEY_PENC:
                    key_str = "ESP_LE_KEY_PENC";
                    break;
                case ESP_LE_KEY_PID:
                    key_str = "ESP_LE_KEY_PID";
                    break;
                case ESP_LE_KEY_PCSRK:
                    key_str = "ESP_LE_KEY_PCSRK";
                    break;
                case ESP_LE_KEY_PLK:
                    key_str = "ESP_LE_KEY_PLK";
                    break;
                case ESP_LE_KEY_LLK:
                    key_str = "ESP_LE_KEY_LLK";
                    break;
                case ESP_LE_KEY_LENC:
                    key_str = "ESP_LE_KEY_LENC";
                    break;
                case ESP_LE_KEY_LID:
                    key_str = "ESP_LE_KEY_LID";
                    break;
                case ESP_LE_KEY_LCSRK:
                    key_str = "ESP_LE_KEY_LCSRK";
                    break;
                default:
                    key_str = "INVALID BLE KEY TYPE";
                    break;
            }
          
            return key_str;
        }
};

#endif // BT_H_
