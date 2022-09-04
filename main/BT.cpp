/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            BT.cpp
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Bluetooth base class.
//                  This source file contains the class to encapsulate the Bluetooth ESP API. Both
//                  BLE and BT Classic are supported. Allows for scanning, pairing and connection
//                  to a peripheral device such as a Keyboard or Mouse.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "Arduino.h"
#include "driver/gpio.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "BT.h"

#define SIZEOF_ARRAY(a) (sizeof(a) / sizeof(*a))

// Out of object pointer to a singleton class for use in the ESP IDF API callback routines which werent written for C++. Other methods can be used but this one is the simplest
// to understand and the class can only ever be singleton.
BT      *pBTThis = NULL;

// Method to locate a valid scan entry in the results list.
//
BT::t_scanListItem* BT::findValidScannedDevice(esp_bd_addr_t bda, std::vector<t_scanListItem> &scanList)
{
    // Locals.
    //

    // Loop through the scan results list looking for a valid entry, return entry if found.
    for(std::size_t idx = 0; idx < scanList.size(); idx++)
    {
        if (memcmp(bda, scanList[idx].bda, sizeof(esp_bd_addr_t)) == 0)
        {
            return &scanList[idx];
        }
    }
    return(nullptr);
}

#ifdef CONFIG_CLASSIC_BT_ENABLED
// Method to add a valid BT Classic device onto the scan list.
//
void BT::addBTScanDevice(esp_bd_addr_t bda, esp_bt_cod_t *cod, esp_bt_uuid_t *uuid, uint8_t *name, uint8_t name_len, int rssi)
{
    // Locals.
    t_scanListItem item;

    // Find a valid device in the BT Classic scan results. If a device is found then this callback is with new data.
    t_scanListItem* result = findValidScannedDevice(bda, btCtrl.btScanList);
    if(result)
    {
        // Information can be updated through several calls.
        if(result->name.length() == 0 && name && name_len)
        {
            result->name.assign((char *)name, name_len); 
        }
        if(result->bt.uuid.len == 0 && uuid->len)
        {
          memcpy(&result->bt.uuid, uuid, sizeof(esp_bt_uuid_t));
        }
        if(rssi != 0)
        {
          result->rssi = rssi;
        }
        return;
    }

    // Populate new list item with device results.
    item.transport = ESP_HID_TRANSPORT_BT;
    memcpy(item.bda,      bda,  sizeof(esp_bd_addr_t));
    memcpy(&item.bt.cod,  cod,  sizeof(esp_bt_cod_t));
    memcpy(&item.bt.uuid, uuid, sizeof(esp_bt_uuid_t));
    item.usage = esp_hid_usage_from_cod((uint32_t)cod);
    item.rssi  = rssi;
    item.name  = "";

    // Store device name if present. This is possibly provided in a seperate callback.
    if(name_len && name)
    {
        item.name.assign((char *)name, name_len); 
    }
 
    // Add new item onto list.
    btCtrl.btScanList.push_back(item);
    return;
}
#endif

// Method to add a valid BLE device to our scan list.
//
void BT::addBLEScanDevice(esp_bd_addr_t bda, esp_ble_addr_type_t addr_type, uint16_t appearance, uint8_t *name, uint8_t name_len, int rssi)
{
    // Locals.
    //
    t_scanListItem item;

    // See if the device is already in the list, exit if found as data updates with seperate callbacks not normal under BLE.
    if(findValidScannedDevice(bda, btCtrl.bleScanList))
    {
        ESP_LOGW(TAG, "Result already exists!");
        return;
    }

    // Populate the item with data.
    item.transport      = ESP_HID_TRANSPORT_BLE;
    memcpy(item.bda, bda, sizeof(esp_bd_addr_t));
    item.ble.appearance = appearance;
    item.ble.addr_type  = addr_type;
    item.usage          = esp_hid_usage_from_appearance(appearance);
    item.rssi           = rssi;
    item.name           = "";

    // Store device name if present.
    if(name_len && name)
    {
        item.name.assign((char *)name, name_len); 
    }

    // Add new item onto list.
    btCtrl.bleScanList.push_back(item);
    return;
}

#ifdef CONFIG_CLASSIC_BT_ENABLED
// Method to process a device data resulting from a BT scan.
//
void BT::processBTDeviceScanResult(esp_bt_gap_cb_param_t * param)
{
    // Locals
    //
    uint32_t       codv     = 0;
    esp_bt_cod_t  *cod      = (esp_bt_cod_t *)&codv;
    int8_t         rssi     = 0;
    uint8_t       *name     = nullptr;
    uint8_t        name_len = 0;
    esp_bt_uuid_t  uuid;
    uint8_t        len      = 0;
    uint8_t       *data     = 0;

    uuid.len         = ESP_UUID_LEN_16;
    uuid.uuid.uuid16 = 0;
  
    for (int i = 0; i < param->disc_res.num_prop; i++)
    {
        esp_bt_gap_dev_prop_t * prop = &param->disc_res.prop[i];
        if(prop->type != ESP_BT_GAP_DEV_PROP_EIR)
        {
        }
        if(prop->type == ESP_BT_GAP_DEV_PROP_BDNAME)
        {
            name = (uint8_t *) prop->val;
            name_len = strlen((const char *)name);
        } 
        else if(prop->type == ESP_BT_GAP_DEV_PROP_RSSI)
        {
            rssi = *((int8_t *) prop->val);
        } 
        else if(prop->type == ESP_BT_GAP_DEV_PROP_COD)
        {
            memcpy(&codv, prop->val, sizeof(uint32_t));
        } 
        else if(prop->type == ESP_BT_GAP_DEV_PROP_EIR)
        {
            data = esp_bt_gap_resolve_eir_data((uint8_t *) prop->val, ESP_BT_EIR_TYPE_CMPL_16BITS_UUID, &len);
      
            if(data == nullptr)
            {
                data = esp_bt_gap_resolve_eir_data((uint8_t *) prop->val, ESP_BT_EIR_TYPE_INCMPL_16BITS_UUID, &len);
            }
      
            if(data && len == ESP_UUID_LEN_16)
            {
                uuid.len = ESP_UUID_LEN_16;
                uuid.uuid.uuid16 = data[0] + (data[1] << 8);
                continue;
            }
      
            data = esp_bt_gap_resolve_eir_data((uint8_t *) prop->val, ESP_BT_EIR_TYPE_CMPL_32BITS_UUID, &len);
      
            if(data == nullptr)
            {
                data = esp_bt_gap_resolve_eir_data((uint8_t *) prop->val, ESP_BT_EIR_TYPE_INCMPL_32BITS_UUID, &len);
            }
      
            if(data && len == ESP_UUID_LEN_32)
            {
                uuid.len = len;
                memcpy(&uuid.uuid.uuid32, data, sizeof(uint32_t));
                continue;
            }
      
            data = esp_bt_gap_resolve_eir_data((uint8_t *) prop->val, ESP_BT_EIR_TYPE_CMPL_128BITS_UUID, &len);
      
            if(data == nullptr)
            {
                data = esp_bt_gap_resolve_eir_data((uint8_t *) prop->val, ESP_BT_EIR_TYPE_INCMPL_128BITS_UUID, &len);
            }
      
            if(data && len == ESP_UUID_LEN_128)
            {
                uuid.len = len;
                memcpy(uuid.uuid.uuid128, (uint8_t *)data, len);
                continue;
            }
      
            //try to find a name
            if (name == nullptr)
            {
                data = esp_bt_gap_resolve_eir_data((uint8_t *) prop->val, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);
      
                if (data == nullptr)
                {
                    data = esp_bt_gap_resolve_eir_data((uint8_t *) prop->val, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &len);
                }
      
                if (data && len)
                {
                    name = data;
                    name_len = len;
                }
            }
        }
    }
  
    // If the found device is a peripheral or a second call on an existing device, add/update the device.
    if ((cod->major == ESP_BT_COD_MAJOR_DEV_PERIPHERAL) || (findValidScannedDevice(param->disc_res.bda, btCtrl.btScanList) != nullptr))
    {
        addBTScanDevice(param->disc_res.bda, cod, &uuid, name, name_len, rssi);
    }
}
#endif

#ifdef CONFIG_CLASSIC_BT_ENABLED
// BT GAP Event Handler.
//
void BT::processBTGapEvent(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    // Locals.
    //
 
    switch(event)
    {
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        {
            ESP_LOGI(TAG, "BT GAP DISC_STATE %s", (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) ? "START" : "STOP");
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED)
            {
                // Release semaphore on which the initiator is waiting, this signals processing complete and results ready.
                xSemaphoreGive(pBTThis->btCtrl.bt_hidh_cb_semaphore);
            }
            break;
        }
        case ESP_BT_GAP_DISC_RES_EVT:
        {
            pBTThis->processBTDeviceScanResult(param);
            break;
        }
        case ESP_BT_GAP_KEY_NOTIF_EVT:
            ESP_LOGI(TAG, "BT GAP KEY_NOTIF passkey:%d", param->key_notif.passkey);
            if(pBTThis->btCtrl.pairingHandler != nullptr) (*pBTThis->btCtrl.pairingHandler)(param->key_notif.passkey, 1);
            break;
        case ESP_BT_GAP_MODE_CHG_EVT:
            ESP_LOGI(TAG, "BT GAP MODE_CHG_EVT mode:%d", param->mode_chg.mode);
            break;
        case ESP_BT_GAP_AUTH_CMPL_EVT:
            ESP_LOGI(TAG, "BT GAP MODE AUTH_CMPL:%s (%d)", param->auth_cmpl.device_name, param->auth_cmpl.stat);
            if(pBTThis->btCtrl.pairingHandler != nullptr) (*pBTThis->btCtrl.pairingHandler)((uint32_t)param->auth_cmpl.stat, 2);
            break;
        default:
            ESP_LOGI(TAG, "BT GAP EVENT %s", pBTThis->bt_gap_evt_str(event));
            break;
    }
}
#endif

// Method to process a device data resulting from a BLE scan.
//
void BT::processBLEDeviceScanResult(esp_ble_gap_cb_param_t *param)
{
    // Locals.
    //
    uint16_t  uuid           = 0;
    uint16_t  appearance     = 0;
    char      name[64]       = "";
    uint8_t   uuid_len       = 0;
    uint8_t  *uuid_d         = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_16SRV_CMPL, &uuid_len);
    uint8_t   appearance_len = 0;
    uint8_t  *appearance_d   = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_APPEARANCE, &appearance_len);
    uint8_t   adv_name_len   = 0;
    uint8_t  *adv_name       = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
  
    if (uuid_d != nullptr && uuid_len)
    {
      uuid = uuid_d[0] + (uuid_d[1] << 8);
    }
  
    if (appearance_d != nullptr && appearance_len)
    {
      appearance = appearance_d[0] + (appearance_d[1] << 8);
    }
  
    if (adv_name == nullptr)
    {
      adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_SHORT, &adv_name_len);
    }
  
    if (adv_name != nullptr && adv_name_len)
    {
      memcpy(name, adv_name, adv_name_len);
      name[adv_name_len] = 0;
    }
  
    if (uuid == ESP_GATT_UUID_HID_SVC)
    {
        addBLEScanDevice(param->scan_rst.bda, 
                            param->scan_rst.ble_addr_type, 
                            appearance, adv_name, adv_name_len, 
                            param->scan_rst.rssi);
     }
}

// BLE GAP Event Handler.
//
void BT::processBLEGapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t * param)
{
    switch(event)
    {
        // SCAN
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        {
            ESP_LOGI(TAG, "BLE GAP EVENT SCAN_PARAM_SET_COMPLETE");
         
            // Release semaphore, this releases the caller who initiated the scan as we are now complete.
            xSemaphoreGive(pBTThis->btCtrl.ble_hidh_cb_semaphore);
            break;
        }
        case ESP_GAP_BLE_SCAN_RESULT_EVT:
        {
            switch (param->scan_rst.search_evt)
            {
                case ESP_GAP_SEARCH_INQ_RES_EVT:
                {
                    pBTThis->processBLEDeviceScanResult(param);
                    break;
                }
                case ESP_GAP_SEARCH_INQ_CMPL_EVT:
                    ESP_LOGI(TAG, "BLE GAP EVENT SCAN DONE: %d", param->scan_rst.num_resps);
              
                    // Release semaphore, this releases the caller who initiated the scan as we are now complete.
                    xSemaphoreGive(pBTThis->btCtrl.ble_hidh_cb_semaphore);
                    break;
                default:
                    break;
            }
            break;
        }
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        {
            ESP_LOGI(TAG, "BLE GAP EVENT SCAN CANCELED");
            break;
        }
    
        // ADVERTISEMENT
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "BLE GAP ADV_DATA_SET_COMPLETE");
            break;
    
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "BLE GAP ADV_START_COMPLETE");
            break;
    
        // AUTHENTICATION
        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            if (!param->ble_security.auth_cmpl.success)
            {
                ESP_LOGE(TAG, "BLE GAP AUTH ERROR: 0x%x", param->ble_security.auth_cmpl.fail_reason);
            } 
            else
            {
                ESP_LOGI(TAG, "BLE GAP AUTH SUCCESS");
            }
            break;
    
        case ESP_GAP_BLE_KEY_EVT: //shows the ble key info share with peer device to the user.
            ESP_LOGI(TAG, "BLE GAP KEY type = %s", pBTThis->ble_key_type_str(param->ble_security.ble_key.key_type));
            break;
    
        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: // ESP_IO_CAP_OUT
            // The app will receive this evt when the IO has Output capability and the peer device IO has Input capability.
            // Show the passkey number to the user to input it in the peer device.
            ESP_LOGI(TAG, "BLE GAP PASSKEY_NOTIF passkey:%d", param->ble_security.key_notif.passkey);
            if(pBTThis->btCtrl.pairingHandler != nullptr) (*pBTThis->btCtrl.pairingHandler)(param->ble_security.key_notif.passkey, 3);
            break;
    
        case ESP_GAP_BLE_NC_REQ_EVT: // ESP_IO_CAP_IO
            // The app will receive this event when the IO has DisplayYesNO capability and the peer device IO also has DisplayYesNo capability.
            // show the passkey number to the user to confirm it with the number displayed by peer device.
            ESP_LOGI(TAG, "BLE GAP NC_REQ passkey:%d", param->ble_security.key_notif.passkey);
            esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
            break;
    
        case ESP_GAP_BLE_PASSKEY_REQ_EVT: // ESP_IO_CAP_IN
            // The app will receive this evt when the IO has Input capability and the peer device IO has Output capability.
            // See the passkey number on the peer device and send it back.
            ESP_LOGI(TAG, "BLE GAP PASSKEY_REQ");
            //esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, 1234);
            break;
    
        case ESP_GAP_BLE_SEC_REQ_EVT:
            ESP_LOGI(TAG, "BLE GAP SEC_REQ");
            // Send the positive(true) security response to the peer device to accept the security request.
            // If not accept the security request, should send the security response with negative(false) accept value.
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            break;

        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
             break;
    
        default:
            ESP_LOGI(TAG, "BLE GAP EVENT %s", pBTThis->ble_gap_evt_str(event));
            break;
      }
}

#ifdef CONFIG_CLASSIC_BT_ENABLED
// Method to scan for BT Classic devices.
//
esp_err_t BT::scanForBTDevices(uint32_t timeout)
{
    // Locals.
    //
    esp_err_t result = ESP_OK;

    // Start BT GAP Discovery, wait for 'timeout' seconds for a valid result.
    if((result = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, (int)(timeout / 1.28), 0)) != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_bt_gap_start_discovery failed: %d", result);
    }
    return(result);
}
#endif

// Method to scan for BLE Devices.
//
esp_err_t BT::scanForBLEDevices(uint32_t timeout)
{
    // Locals.
    //
    esp_err_t                      result = ESP_OK;
    // Setup BLE scan parameters structure, defined in ESP IDF documentation.
    static esp_ble_scan_params_t   hid_scan_params = {
                                                      .scan_type          = BLE_SCAN_TYPE_ACTIVE,
                                                      .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
                                                      .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
                                                      .scan_interval      = 0x50,
                                                      .scan_window        = 0x30,
                                                      .scan_duplicate     = BLE_SCAN_DUPLICATE_ENABLE,
                                                     };

    // Set scan parameters using populated structure.
    if((result = esp_ble_gap_set_scan_params(&hid_scan_params)) != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ble_gap_set_scan_params failed: %d", result);
        return(result);
    }

    // Wait for result, this is done by taking possession of a semaphore which is released in the callback when scan complete.
    xSemaphoreTake(btCtrl.ble_hidh_cb_semaphore, portMAX_DELAY);

    if((result = esp_ble_gap_start_scanning(timeout)) != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ble_gap_start_scanning failed: %d", result);
        return(result);
    }
    return(result);
}

// Method to scan for Bluetooth devices.
//
esp_err_t BT::scanForAllDevices(uint32_t timeout, size_t *noDevices, std::vector<t_scanListItem> &scanList)
{
    // Locals.
    //

    // Clear previous lists.
  #ifdef CONFIG_CLASSIC_BT_ENABLED
    btCtrl.btScanList.clear();
  #endif
    btCtrl.bleScanList.clear();

    // Scan for BLE devices.
    if(scanForBLEDevices(timeout) == ESP_OK)
    {
        // Wait for result, this is done by taking possession of a semaphore which is released in the callback when scan complete.
        xSemaphoreTake(btCtrl.ble_hidh_cb_semaphore, portMAX_DELAY);
    } 
    else
    {
        return(ESP_FAIL);
    }

  #ifdef CONFIG_CLASSIC_BT_ENABLED
    // Scan for BT devices
    if(scanForBTDevices(timeout) == ESP_OK)
    {
        // Wait for result, this is done by taking possession of a semaphore which is released in the callback when scan complete.
        xSemaphoreTake(btCtrl.bt_hidh_cb_semaphore, portMAX_DELAY);
    } 
    else 
    {
        return(ESP_FAIL);
    }
  #endif

    //esp_bt_gap_cancel_discovery();
    //esp_ble_gap_stop_scanning();
  
    // Process results into a merged list.
  #ifdef CONFIG_CLASSIC_BT_ENABLED
    for(std::size_t idx = 0; idx < btCtrl.btScanList.size(); idx++)
    {
        scanList.push_back(btCtrl.btScanList[idx]);
    }
  #endif
    for(std::size_t idx = 0; idx < btCtrl.bleScanList.size(); idx++)
    {
        scanList.push_back(btCtrl.bleScanList[idx]);
    }

    // Update the final list with display values.
    for(std::size_t idx = 0; idx < scanList.size(); idx++)
    {
        char buf[50];
        sprintf(buf, ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(scanList[idx].bda));
        scanList[idx].deviceAddr = buf;
        if(scanList[idx].transport == ESP_HID_TRANSPORT_BLE)
        {
            scanList[idx].deviceType = "BLE";
        }
      #ifdef CONFIG_CLASSIC_BT_ENABLED
        if(scanList[idx].transport == ESP_HID_TRANSPORT_BT)
        {
            scanList[idx].deviceType = "BT";
        }
      #endif
    }

    // Save number of entries.
    *noDevices = scanList.size();

    // Clear BT/BLE lists as data no longer needed.
  #ifdef CONFIG_CLASSIC_BT_ENABLED
    btCtrl.btScanList.clear();
  #endif
    btCtrl.bleScanList.clear();

    return(ESP_OK);
}

// Method to scan and build a list for all available devices.
void BT::getDeviceList(std::vector<t_scanListItem> &scanList, int waitTime)
{
    // Locals.
    //
    size_t                 devicesFound = 0;

    ESP_LOGD(TAG, "SCAN...");

    // Clear previous entries.
    scanList.clear();

    // Start scan for HID devices
    scanForAllDevices(waitTime, &devicesFound, scanList);

    ESP_LOGD(TAG, "SCAN: %u results", devicesFound);
}

// Method to configure Bluetooth and register required callbacks.
bool BT::setup(t_pairingHandler *handler)
{
    // Locals.
    //
    esp_err_t           result;
    const esp_bt_mode_t mode        = HIDH_BTDM_MODE;
    uint8_t             key_size    = 16;
    uint8_t             init_key    = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t             rsp_key     = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint32_t            passkey     = 123456;
    uint8_t             auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
    uint8_t             oob_support = ESP_BLE_OOB_DISABLE;
  
    // Check for multiple instantiations, only one instance allowed.
    if(pBTThis != nullptr)
    {
        ESP_LOGE(TAG, "Setup called more than once. Only one instance of BT is allowed.");
        return false;
    }
  
    // Store current object and handlers.
    pBTThis = this;
    btCtrl.pairingHandler = handler;
  
    // Bluetooth not enabled, exit.
    if(mode == HIDH_IDLE_MODE)
    {
        ESP_LOGE(TAG, "Please turn on BT HID host or BLE!");
        return false;
    }
  
  #ifdef CONFIG_CLASSIC_BT_ENABLED
    // Create BT Classic semaphore, used to halt caller whilst underlying receives and proceses data.
    btCtrl.bt_hidh_cb_semaphore = xSemaphoreCreateBinary();
    if (btCtrl.bt_hidh_cb_semaphore == nullptr)
    {
        ESP_LOGE(TAG, "xSemaphoreCreateMutex BT failed!");
        return false;
    }
  #endif
  
    // Create BLE semaphore, used to halt caller whilst underlying receives and proceses data.
    btCtrl.ble_hidh_cb_semaphore = xSemaphoreCreateBinary();
    if(btCtrl.ble_hidh_cb_semaphore == nullptr)
    {
        ESP_LOGE(TAG, "xSemaphoreCreateMutex BLE failed!");

      #ifdef CONFIG_CLASSIC_BT_ENABLED
        // Delete BT semaphore as both BT and BLE need to be active, return fail to caller.
        vSemaphoreDelete(btCtrl.bt_hidh_cb_semaphore);
        btCtrl.bt_hidh_cb_semaphore = nullptr;
      #endif
        return false;
    }
  
  #ifdef CONFIG_CLASSIC_BT_ENABLED
    // Setup default config for BT Classic.
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    bt_cfg.mode             = mode;
    bt_cfg.bt_max_acl_conn  = 3;
    bt_cfg.bt_max_sync_conn = 3;
  
    // Configure Bluetooth controller for BT Classic operation.
    if((result = esp_bt_controller_init(&bt_cfg)))
    {
        ESP_LOGE(TAG, "esp_bt_controller_init failed: %d", result);
        return false;
    }
  
    // Enable Bluetooth Classic mode.
    if((result = esp_bt_controller_enable(mode)))
    {
        ESP_LOGE(TAG, "esp_bt_controller_enable failed: %d", result);
        return false;
    }
    esp_bredr_tx_power_set(ESP_PWR_LVL_P9, ESP_PWR_LVL_P9);
  #endif
  
    // Setup and initialise Bluetooth BLE mode.
    if((result = esp_bluedroid_init()))
    {
        ESP_LOGE(TAG, "esp_bluedroid_init failed: %d", result);
        return false;
    }
    if((result = esp_bluedroid_enable()))
    {
        ESP_LOGE(TAG, "esp_bluedroid_enable failed: %d", result);
        return false;
    }
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  
  #ifdef CONFIG_CLASSIC_BT_ENABLED
    // Classic Bluetooth GAP
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
  
    // Set default parameters for Legacy Pairing
    // Use fixed pin code
    // 
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code;
    pin_code[0] = '1';
    pin_code[1] = '2';
    pin_code[2] = '3';
    pin_code[3] = '4';
    esp_bt_gap_set_pin(pin_type, 4, pin_code);
  
    if((result = esp_bt_gap_register_callback(processBTGapEvent)))
    {
        ESP_LOGE(TAG, "esp_bt_gap_register_callback failed: %d", result);
        return false;
    }
  
    // Allow BT devices to connect back to us
    if((result = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE)))
    {
        ESP_LOGE(TAG, "esp_bt_gap_set_scan_mode failed: %d", result);
        return false;
    }  
  #endif
  
    // BLE GAP
    if((result = esp_ble_gap_register_callback(processBLEGapEvent)))
    {
      ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %d", result);
      return false;
    }

    // Setup security, no password.
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;                                 // Bonding with peer device after authentication
    esp_ble_io_cap_t iocapble = ESP_IO_CAP_NONE;                                                // Set the IO capability to No output No input
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY,             &passkey,     sizeof(uint32_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE,                &auth_req,    sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,                     &iocapble,    sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE,                   &key_size,    sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT,                    &oob_support, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY,                   &init_key,    sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY,                    &rsp_key,     sizeof(uint8_t));
 
    // Initialise parameters.
    btCtrl.batteryLevel = -1;
    return true;
}

// Basic constructor, do nothing! 
BT::BT(void)
{
    btCtrl.hidhDevHdl = NULL;
  #ifdef CONFIG_CLASSIC_BT_ENABLED
    btCtrl.pairingHandler = nullptr;
    btCtrl.bt_hidh_cb_semaphore = nullptr;
  #endif
    btCtrl.ble_hidh_cb_semaphore = nullptr;
    pBTThis = NULL;
    //
}

// Basic destructor, do nothing! Only ever called for instantiation of uninitialsed class to prove version data.Used for probing versions etc.
BT::~BT(void)
{
    //
}
