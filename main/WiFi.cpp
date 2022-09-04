/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            WiFi.cpp
// Created:         Jan 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     The WiFi AP/Client Interface.
//                  This source file contains the application logic to provide WiFi connectivity in
//                  order to allow remote query and configuration of the sharpkey interface.
//
//                  The module provides Access Point (AP) functionality to allow initial connection
//                  in order to configure local WiFi credentials.
//
//                  The module provides Client functionality, using the configured credentials,
//                  to connect to a local Wifi net and present a browser session for querying and
//                  mapping configuration of the sharpkey interface.
// Credits:         
// Copyright:       (c) 2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Jan 2022 - Initial write.
//                  Mar 2022 - Split out from main.cpp.
//            v1.01 May 2022 - Initial release version.
//            v1.02 Jun 2022 - Seperated out the WiFi Enable switch and made the WiFi module active
//                             via a reboot process. This is necessary now that Bluetooth is inbuilt
//                             as the ESP32 shares an antenna and both operating together electrically
//                             is difficult but also the IDF stack conflicts as well.
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

// This is an optional compile time module, only compile if configured.
#include "sdkconfig.h"
#if defined(CONFIG_IF_WIFI_ENABLED)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <regex>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <map>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_ota_ops.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "Arduino.h"
#include "driver/gpio.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include <sys/param.h>
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include "esp_littlefs.h"
#include "WiFi.h"

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t  s_wifi_event_group;

// Template to convert a given type into a std::string.
//
template<typename Type> std::string to_str(const Type & t, int precision, int base)
{
    // Locals.
    //
    std::ostringstream os;

    if(precision != 0)
    {
        os << std::fixed << std::setw(precision) << std::setprecision(precision) << std::setfill('0') << t;
    } else
    {
        if(base == 16)
        {
            os << "0x" << std::hex << t;
        } else
        {
            os << t;
        }
    }
    return os.str();
}

// Method to convert the idf internal partition type to a readable string for output to the browser.
//
std::string WiFi::esp32PartitionType(esp_partition_type_t type)
{
    // Locals.
    //
    std::string   result = "Unknown";
    
    switch(static_cast<int>(type))
    {
        case ESP_PARTITION_TYPE_APP:
            result = "App";
            break;

        case ESP_PARTITION_TYPE_DATA:
            result = "Data";
            break;

        default:
            result = "n/a";
            break;
    }

    // Return the string version of the enum.
    return(result);
}

// Method to convert the idf internal partition subtype to a readable string for output to the browser.
//
std::string WiFi::esp32PartitionSubType(esp_partition_subtype_t subtype)
{
    // Locals.
    //
    std::string   result = "Unknown";

    switch(static_cast<int>(subtype))
    {
        case ESP_PARTITION_SUBTYPE_APP_FACTORY:
            result = "Factory";
            break;

        case ESP_PARTITION_SUBTYPE_DATA_PHY:
            result = "phy";
            break;

        case ESP_PARTITION_SUBTYPE_DATA_NVS:
            result = "nvs";
            break;

        case ESP_PARTITION_SUBTYPE_DATA_COREDUMP:
            result = "core";
            break;

        case ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS:
            result = "nvs_keys";
            break;

        case ESP_PARTITION_SUBTYPE_DATA_EFUSE_EM:
            result = "efuse";
            break;

        case ESP_PARTITION_SUBTYPE_DATA_ESPHTTPD:
            result = "httpd";
            break;

        case ESP_PARTITION_SUBTYPE_DATA_FAT:
            result = "fat";
            break;

        case ESP_PARTITION_SUBTYPE_DATA_SPIFFS:
            result = "spiffs";
            break;

        case ESP_PARTITION_SUBTYPE_APP_OTA_0:
        case ESP_PARTITION_SUBTYPE_APP_OTA_1:
        case ESP_PARTITION_SUBTYPE_APP_OTA_2:
        case ESP_PARTITION_SUBTYPE_APP_OTA_3:
        case ESP_PARTITION_SUBTYPE_APP_OTA_4:
        case ESP_PARTITION_SUBTYPE_APP_OTA_5:
        case ESP_PARTITION_SUBTYPE_APP_OTA_6:
        case ESP_PARTITION_SUBTYPE_APP_OTA_7:
        case ESP_PARTITION_SUBTYPE_APP_OTA_8:
        case ESP_PARTITION_SUBTYPE_APP_OTA_9:
        case ESP_PARTITION_SUBTYPE_APP_OTA_10:
        case ESP_PARTITION_SUBTYPE_APP_OTA_11:
        case ESP_PARTITION_SUBTYPE_APP_OTA_12:
        case ESP_PARTITION_SUBTYPE_APP_OTA_13:
        case ESP_PARTITION_SUBTYPE_APP_OTA_14:
        case ESP_PARTITION_SUBTYPE_APP_OTA_15:
        case ESP_PARTITION_SUBTYPE_APP_OTA_MAX:
            result = "ota_" + to_str(subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN, 0, 10);
            break;

        default:
            result = to_str(subtype, 0, 10);
            break;
    }

    // Return the string version of the enum.
    return(result);
}

// Method to return the version number of a given module.
float WiFi::getVersionNumber(std::string name)
{
    // Locals.
    //
    int    idx = 0;

    // Loop through the version list looking for FilePack.
    while(idx < wifiCtrl.run.versionList->elements && wifiCtrl.run.versionList->item[idx]->object.compare(name) != 0)
    {
        idx++;
    }

    // Return the version number if found.
    return(idx == wifiCtrl.run.versionList->elements ? 0.00 : wifiCtrl.run.versionList->item[idx]->version);
}

// Method to request KeyMap table headers from the underlying interface and send as a Javascript array.
// 
esp_err_t WiFi::sendKeyMapHeaders(httpd_req_t *req)
{
    // Locals.
    //
    esp_err_t                  result = ESP_OK;
    std::string                jsArray;
    std::vector<std::string>   headerList;
   
    // Call the underlying interface to fill the vector with a list of header names.
    keyIf->getKeyMapHeaders(headerList);

    // Build up a javascript array and send to the browser direct.
    //
    jsArray = "[";
    for(std::size_t idx = 0; idx < headerList.size(); idx++)
    {
        jsArray.append("\"").append(headerList[idx]).append("\"");
        if(idx == headerList.size()-1)
        {
            jsArray.append(",\"<i class=\\\"fa fa-check\\\"></i>\"]");
        } else
        {
            jsArray.append(",");
        }
    }

    // Send array and return result.
    result=httpd_resp_send_chunk(req, jsArray.c_str(), jsArray.size());

    // Send result, ESP_OK = all successful, anything else a transmission or data error occurred.
    return(result);
}

// Method to request KeyMap table data types from the underlying interface and send as a Javascript array.
esp_err_t WiFi::sendKeyMapTypes(httpd_req_t *req)
{
    // Locals.
    //
    int                        startPos;
    esp_err_t                  result = ESP_OK;
    std::string                jsArray;
    std::vector<std::string>   typeList;

    // Call the underlying interface to fill a vector with the type of each keymap column.
    keyIf->getKeyMapTypes(typeList);

    // Build up a javascript array containing the column types mapping if needed to an EditTable value.
    //
    jsArray = "[";
    for(std::size_t idx = 0; idx < typeList.size(); idx++)
    {
        // Strip out the custom tag, (custom_ttp_ where tt = type, ie. rd = Radio, cb = Checkbox, p = polarity, p = positive, = negative) not needed, use as an internal marker to identify custom fields.
        if((startPos = typeList[idx].find("custom_")) >= 0)
        {
            jsArray.append("\"").append(typeList[idx].substr(startPos+11, std::string::npos)).append("\"");
        } else
        {
            jsArray.append("\"").append(typeList[idx]).append("\"");
        }

        if(idx == typeList.size()-1)
        {
            jsArray.append(",\"checkbox\"]");
        } else
        {
            jsArray.append(",");
        }
    }

    // Send array and return result.
    result=httpd_resp_send_chunk(req, jsArray.c_str(), jsArray.size());

    // Send result, ESP_OK = all successful, anything else a transmission or data error occurred.
    return(result);
}

// Method to expand the custom type fields in the interface to custom fields in the EditTable configuration code.
// This code is injected into the javascript setup and will invoke a custom popover or select UI.
esp_err_t WiFi::sendKeyMapCustomTypeFields(httpd_req_t *req)
{
    // Locals.
    //
    int                        startPos;
    esp_err_t                  result = ESP_OK;
    std::string                typeStr = "";
    std::vector<std::string>   typeList;

    // Call the underlying interface to fill a vector with the type of each keymap column.
    keyIf->getKeyMapTypes(typeList);

    for(std::size_t idx = 0; idx < typeList.size(); idx++)
    {
        // Custom field?
        if((startPos = typeList[idx].find("custom")) >= 0)
        {
            // Find any duplicate by searching the vector just processed.
            bool duplicate = false;
            for(std::size_t idx2 = 0; idx2 < idx; idx2++) { if(typeList[idx].compare(typeList[idx2]) == 0) { duplicate = true; break; } }
            if(duplicate) continue;

            // Negative or positive value?
            bool negate = (typeList[idx].substr(startPos+9, 1)[0] == 'p' ? false : true);

            // Build the custom type definition which is injected into the javascript setup of EditTable.
            typeStr += "        '" + typeList[idx].substr(startPos+11, std::string::npos) + "' : { \n" + 
                       "            html: '<input type=\"text\" class=\"" + typeList[idx].substr(startPos+11, std::string::npos) + "\" data-placement=\"left\"/>', \n";

            // As some of the interface parameters are negative active, if the 'custom' label is followed by a minus '-' this means the value sent or received needs to be negated.
            // This is because the UI works in positive values. If the 'custom' label is followed by an underscore '_' then no data change is made.
            typeStr.append("            getValue: function (input) { \n");
            typeStr.append("                var $thisVal = $(input).val(); \n");
            if(negate)
                typeStr.append("            return hexConvert($thisVal, true);\n");
            else
                typeStr.append("            return hexConvert($thisVal, false);\n");
            typeStr.append("            }, \n");
            typeStr.append("            setValue: function (input, inVal) { \n");
            if(negate)
                typeStr.append("            var $thisVal = $(input).attr(\"value\", hexConvert(inVal, true));\n");
            else
                typeStr.append("            var $thisVal = $(input).attr(\"value\", hexConvert(inVal, false));\n");
            typeStr.append("                return $thisVal; \n");
            typeStr.append("            } \n");
            typeStr.append("        },\n");
        }
    }

    // Send array and return result.
    result=httpd_resp_send_chunk(req, typeStr.c_str(), typeStr.size());

    // Send result, ESP_OK = all successful, anything else a transmission or data error occurred.
    return(result);
}

// Method to request KeyMap table entries, row at a time, from the underlying interface and send as a Javascript array.
// This method could involve large amounts of data which may overflow the heap so data is requested and sent row by row.
esp_err_t WiFi::sendKeyMapData(httpd_req_t *req)
{
    // Locals.
    //
    esp_err_t                  result = ESP_OK;
    bool                       startMode = true;
    bool                       firstRow = true;
    int                        row = 0;
    std::string                jsArray = "";
    std::vector<uint32_t>      data;
   
    // Initiate a loop, calling the underlying interface to return data row by row until the end of the keymap data.
    while(result == ESP_OK && keyIf->getKeyMapData(data, &row, startMode) == false)
    {
        // At start, we initialise the data retrieval and also setup the Javascript array designator.
        if(startMode == true)
        {
            startMode = false;
            jsArray = "[";
        }
        if(firstRow == false)
        {
            jsArray = ",";
        } else
        {
            firstRow = false;
        }
        jsArray.append("[");
        for(std::size_t idx = 0; idx < data.size(); idx++)
        {
            jsArray.append("\"").append(to_str(data[idx], 0, 16)).append("\"");
            if(idx == data.size()-1)
            {
                jsArray.append(",false]");
            } else
            {
                jsArray.append(",");
            }
        }
        data.clear();

        // Send array and return result.
        result=httpd_resp_send_chunk(req, jsArray.c_str(), jsArray.size());
    }

    // At the end we need to close the javascript array designator. No way to do this in the loop as the data get method doesnt provide next state information.
    if(result == ESP_OK)
    {
        jsArray = "]";
        // Send array and return result.
        result=httpd_resp_send_chunk(req, jsArray.c_str(), jsArray.size());
    }

    // Send result, ESP_OK = all successful, anything else a transmission or data error occurred.
    return(result);
}

// Method for building up the popover modals which are used to enable a user to select values by tick rather than work out a hex value.
//
esp_err_t WiFi::sendKeyMapPopovers(httpd_req_t *req)
{
    // Locals.
    //
    int                                      startPos;
    esp_err_t                                result = ESP_OK;
    std::string                              jsArray;
    std::string                              jsClass;
    std::vector<std::string>                 headerList;
    std::vector<std::string>                 typeList;
    std::vector<std::pair<std::string, int>> selectList;

    // Get list of column headers, these are used as the popover title.
    keyIf->getKeyMapHeaders(headerList);

    // Get list of types, these are needed to setup a popup for each custom field.
    keyIf->getKeyMapTypes(typeList);

    // Loop through the types, process any custom field into a popover modal.
    for(std::size_t idx = 0; result == ESP_OK && idx < typeList.size(); idx++)
    {
        // Custom field? Skip if not custom type.
        if((startPos = typeList[idx].find("custom_")) >= 0)
        {
            // Find any duplicate by searching the vector just processed.
            bool duplicate = false;
            for(std::size_t idx2 = 0; idx2 < idx; idx2++) { if(typeList[idx].compare(typeList[idx2]) == 0) { duplicate = true; break; } }
            if(duplicate) continue;

            jsClass = typeList[idx].substr(startPos+11, std::string::npos);
            jsArray =  "<div class=\"popover-markup\" id=\"popover-" + jsClass + "\">\n" + 
                       "    <div class=\"head hide\">" + headerList[idx] + "</div>\n" + 
                       "    <div class=\"content hide\">\n" + 
                       "        <form role=\"form\">\n" + 
                       "            <div class=\"row\">\n" + 
                       "                <div class=\"col-xs-6 col-md-8\">\n";

            // Get the select list of values for the current custom type.
            keyIf->getKeyMapSelectList(selectList, typeList[idx]);
           
            // Add in all the check boxes.
            if(typeList[idx].find("custom_cb") != std::string::npos)
            {
                for(auto iter = std::begin(selectList); iter != std::end(selectList); iter++)
                {
                    jsArray.append("                    <div class=\"checkbox\">\n")
                           .append("                        <label>\n")
                           .append("                            <input  id=\"").append(jsClass).append("_").append(iter->first).append("\" type=\"checkbox\" data-value=\"").append(to_str(iter->second, 0, 10)).append("\"/> ").append(iter->first).append("\n")
                           .append("                        </label>\n")
                           .append("                    </div>\n");
                }
            }
            else if(typeList[idx].find("custom_rd") != std::string::npos)
            {
                for(auto iter = std::begin(selectList); iter != std::end(selectList); iter++)
                {
                    jsArray.append("                    <div class=\"radio\">\n")
                           .append("                        <label>\n")
                           .append("                            <input  id=\"").append(jsClass).append("_").append(iter->first).append("\" type=\"radio\" name=\"").append(jsClass).append("\" data-value=\"").append(to_str(iter->second, 0, 10)).append("\"/> ").append(iter->first).append("\n")
                           .append("                        </label>\n")
                           .append("                    </div>\n");
                }
            }

            // Finish off by closing up the opened DIV blocks.
            jsArray.append("                </div>\n")
                   .append("            </div>\n")
                   .append("        </form>\n")
                   .append("    </div>\n")
                   .append("</div>\n");

            // Send array and return result.
            result=httpd_resp_send_chunk(req, jsArray.c_str(), jsArray.size());

            // Free up memory for next iteration.
            selectList.clear();
        }
    }

    // Send result, ESP_OK = all successful, anything else a transmission or data error occurred.
    return(result);
}

// Method to render the radio select for Mouse Host Scaling.
//
esp_err_t WiFi::sendMouseRadioChoice(httpd_req_t *req, const char *option)
{
    // Locals.
    //
    int                                      startPos;
    esp_err_t                                result = ESP_OK;
    std::string                              typeStr = "";
    std::string                              typeHead = "";
    std::string                              typeBody = "";
    std::vector<std::string>                 typeList;
    std::vector<std::pair<std::string, int>> selectList;
    KeyInterface                            *activeMouseIf = (mouseIf == NULL ? keyIf : mouseIf);

    // Call the underlying interface to fill a vector with the type of config parameters.
    activeMouseIf->getMouseConfigTypes(typeList);

    for(std::size_t idx = 0; idx < typeList.size(); idx++)
    {
        // Custom field?
        if((startPos = typeList[idx].find(option)) >= 0)
        {
            // Find any duplicate by searching the vector just processed.
            bool duplicate = false;
            for(std::size_t idx2 = 0; idx2 < idx; idx2++) { if(typeList[idx].compare(typeList[idx2]) == 0) { duplicate = true; break; } }
            if(duplicate) continue;

            // Get the select list of values for the current config type.
            activeMouseIf->getMouseSelectList(selectList, typeList[idx]);
            typeStr.append("<div class=\"form-check form-check-inline\">\n");
            for(auto iter = std::begin(selectList); iter != std::end(selectList); iter++)
            {
                // Skip current value item.
                if(iter == std::begin(selectList)) continue;

                typeStr.append("    <input class=\"form-check-input radio-mouse\"  id=\"").append(typeList[idx]).append("_").append(iter->first).append("\" type=\"radio\" name=\"").append(typeList[idx]).append("\" value=\"").append(to_str(iter->second, 0, 10)).append("\"").append(selectList[0].second == iter->second ? "checked" : "").append("/>")
                       .append("    <label class=\"form-check-label radio-mouse\" for=\"").append(typeList[idx]).append("_").append(iter->first).append("\">").append(iter->first).append("</label>\n");
            }
            typeStr.append("</div><br>");
        }
    }

    // Send array and return result.
    result=httpd_resp_send_chunk(req, typeStr.c_str(), typeStr.size());

    // Send result, ESP_OK = all successful, anything else a transmission or data error occurred.
    return(result);
}

// Method to expand variable macros into variable values within a string buffer. The buffer will contain HTML/CSS text prior to despatch to a browser.
//
esp_err_t WiFi::expandVarsAndSend(httpd_req_t *req, std::string str)
{
    // Locals.
    //
    bool                  largeMacroDetected = false;
    int                   startPos;
    t_kvPair              keyValue;
    esp_err_t             result = ESP_OK;
    std::vector<t_kvPair> pairs;
    
    // Build up the list of pairs, place holder to value, this is used to expand the given string with latest runtime values.
    // Certain macros return a lot of data so they cannot be added into the mapping vector due to RAM constraints, these are handled in-situ.
    //
    keyValue.name  = "%SK_WIFIMODEAP%";         keyValue.value = (wifiCtrl.run.wifiMode == WIFI_CONFIG_AP ? "checked" : "");                                                         pairs.push_back(keyValue);
    keyValue.name  = "%SK_WIFIMODECLIENT%";     keyValue.value = (wifiCtrl.run.wifiMode == WIFI_CONFIG_CLIENT ? "checked" : "");                                                     pairs.push_back(keyValue);
    keyValue.name  = "%SK_CLIENTSSID%";         keyValue.value = wifiConfig.clientParams.ssid;                                                                                       pairs.push_back(keyValue);
    keyValue.name  = "%SK_CLIENTPWD%";          keyValue.value = wifiConfig.clientParams.pwd;                                                                                        pairs.push_back(keyValue);
    keyValue.name  = "%SK_CLIENTDHCPON%";       keyValue.value = (wifiConfig.clientParams.useDHCP == true ? "checked" : "");                                                         pairs.push_back(keyValue);
    keyValue.name  = "%SK_CLIENTDHCPOFF%";      keyValue.value = (wifiConfig.clientParams.useDHCP == false ? "checked" : "");                                                        pairs.push_back(keyValue);
    keyValue.name  = "%SK_CLIENTIP%";           keyValue.value = wifiConfig.clientParams.ip;                                                                                         pairs.push_back(keyValue);
    keyValue.name  = "%SK_CLIENTNM%";           keyValue.value = wifiConfig.clientParams.netmask;                                                                                    pairs.push_back(keyValue);
    keyValue.name  = "%SK_CLIENTGW%";           keyValue.value = wifiConfig.clientParams.gateway;                                                                                    pairs.push_back(keyValue);
    keyValue.name  = "%SK_APSSID%";             keyValue.value = wifiConfig.apParams.ssid;                                                                                           pairs.push_back(keyValue);
    keyValue.name  = "%SK_APPWD%";              keyValue.value = wifiConfig.apParams.pwd;                                                                                            pairs.push_back(keyValue);
    keyValue.name  = "%SK_APIP%";               keyValue.value = wifiConfig.apParams.ip;                                                                                             pairs.push_back(keyValue);
    keyValue.name  = "%SK_APNM%";               keyValue.value = wifiConfig.apParams.netmask;                                                                                        pairs.push_back(keyValue);
    keyValue.name  = "%SK_APGW%";               keyValue.value = wifiConfig.apParams.gateway;                                                                                        pairs.push_back(keyValue);
    keyValue.name  = "%SK_CURRENTSSID%";        keyValue.value = (wifiCtrl.run.wifiMode == WIFI_CONFIG_AP ? wifiCtrl.ap.ssid    : wifiCtrl.client.ssid);                             pairs.push_back(keyValue);
    keyValue.name  = "%SK_CURRENTPWD%";         keyValue.value = (wifiCtrl.run.wifiMode == WIFI_CONFIG_AP ? wifiCtrl.ap.pwd     : wifiCtrl.client.pwd);                              pairs.push_back(keyValue);
    keyValue.name  = "%SK_CURRENTIP%";          keyValue.value = (wifiCtrl.run.wifiMode == WIFI_CONFIG_AP ? wifiCtrl.ap.ip      : wifiCtrl.client.ip);                               pairs.push_back(keyValue);
    keyValue.name  = "%SK_CURRENTNM%";          keyValue.value = (wifiCtrl.run.wifiMode == WIFI_CONFIG_AP ? wifiCtrl.ap.netmask : wifiCtrl.client.netmask);                          pairs.push_back(keyValue);
    keyValue.name  = "%SK_CURRENTGW%";          keyValue.value = (wifiCtrl.run.wifiMode == WIFI_CONFIG_AP ? wifiCtrl.ap.gateway : wifiCtrl.client.gateway);                          pairs.push_back(keyValue);
    keyValue.name  = "%SK_CURRENTIF%";          keyValue.value = keyIf->ifName().append(" ");                                                                                        pairs.push_back(keyValue);
    keyValue.name  = "%SK_SECONDIF%";           keyValue.value = (mouseIf != NULL ? mouseIf->ifName().append(" ") : "");                                                             pairs.push_back(keyValue);
    keyValue.name  = "%SK_REBOOTBUTTON%";       keyValue.value = (wifiCtrl.run.rebootButton == true ? "block" : "none");                                                             pairs.push_back(keyValue);
    keyValue.name  = "%SK_ERRMSG%";             keyValue.value = wifiCtrl.run.errorMsg;                                                                                              pairs.push_back(keyValue);
    keyValue.name  = "%SK_PRODNAME%";           keyValue.value = (wifiCtrl.run.versionList->elements > 1 ? wifiCtrl.run.versionList->item[0]->object : "Unknown");                   pairs.push_back(keyValue);
    keyValue.name  = "%SK_PRODVERSION%";        keyValue.value = (wifiCtrl.run.versionList->elements > 1 ? to_str(wifiCtrl.run.versionList->item[0]->version, 2, 10) : "Unknown");   pairs.push_back(keyValue);
    keyValue.name  = "%SK_MODULES%";            if(wifiCtrl.run.versionList->elements > 1)
                                                {
                                                    std::ostringstream list;
                                                    list << "<table class=\"table table-borderless table-sm\"><tbody><tr>";
                                                    for(int idx=0, cols=0; idx < wifiCtrl.run.versionList->elements; idx++)
                                                    { 
                                                        // Ignore SharpKey/FilePack, they are  part of our version list but not relevant as a module.
                                                        if(wifiCtrl.run.versionList->item[idx]->object.compare("SharpKey") == 0 || wifiCtrl.run.versionList->item[idx]->object.compare("FilePack") == 0)
                                                            continue; 

                                                        if((cols++ % 6) == 0)
                                                        {
                                                            list << "</tr>";
                                                            if(idx < wifiCtrl.run.versionList->elements) { list << "<tr>"; }
                                                        }
                                                        list << "<td><span style=\"color: blue;\">" << wifiCtrl.run.versionList->item[idx]->object << "</span> <i>(v" <<  to_str(wifiCtrl.run.versionList->item[idx]->version, 2, 10) << ")&nbsp;&nbsp;&nbsp;</i></td> ";
                                                    }
                                                    list << "</tr></tbody?></table>";
                                                    keyValue.value = list.str();
                                                } else { keyValue.value = "Unknown"; };                                                                                              pairs.push_back(keyValue);
    keyValue.name  = "%SK_FILEPACK%";           {
                                                    std::ostringstream list;
                                                    list << "<table class=\"table table-borderless table-sm\"><tbody><tr>";
                                                    list << "<td><span style=\"color: blue;\">FilePack</span> <i>(v" <<  to_str(getVersionNumber("FilePack"), 2, 10) << ")</i></td> ";
                                                    list << "</tr></tbody?></table>";
                                                    keyValue.value = list.str();
                                                }                                                                                                                                    pairs.push_back(keyValue);
    keyValue.name  = "%SK_PARTITIONS%";         {
                                                    std::ostringstream list;
                                                    const esp_partition_t *runPart = esp_ota_get_running_partition();
                                                    esp_partition_iterator_t it;
                                                    it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
                                                    esp_err_t err;
                                                    esp_app_desc_t appDesc;
                                                    for (; it != NULL; it = esp_partition_next(it)) {
                                                        const esp_partition_t *part = esp_partition_get(it);
                                                        err = esp_ota_get_partition_description(part, &appDesc);
                                                        list << "<tr>" 
                                                             <<   "<td>" << part->label                                      << "</td>" 
                                                             <<   "<td>" << esp32PartitionType(part->type)                   << "</td>"
                                                             <<   "<td>" << esp32PartitionSubType(part->subtype)             << "</td>"
                                                             <<   "<td>" << to_str(part->address, 0, 16)                     << "</td>"
                                                             <<   "<td>" << to_str(part->size, 0, 16)                        << "</td>"
                                                             <<   "<td>" << (err == ESP_OK ? appDesc.version : part->subtype == ESP_PARTITION_SUBTYPE_DATA_SPIFFS ? to_str(getVersionNumber("FilePack"), 2, 10) : "") << "</td>"
                                                             <<   "<td>" << (err == ESP_OK ? appDesc.date : "")              << " "           
                                                             <<             (err == ESP_OK ? appDesc.time : "")              << "</td>"
                                                             <<   "<td>" << (runPart->address == part->address ? "Yes" : "") << "</td>"
                                                             << "</tr>";
                                                    }
                                                    esp_partition_iterator_release(it);

                                                    keyValue.value = list.str();
                                                }                                                                                                                                    pairs.push_back(keyValue);
    keyValue.name  = "%SK_KEYMAPHEADER%";       keyValue.value = "";                                                                                                                 pairs.push_back(keyValue);
    keyValue.name  = "%SK_KEYMAPTYPES%";        keyValue.value = "";                                                                                                                 pairs.push_back(keyValue);
    keyValue.name  = "%SK_KEYMAPJSFIELDS%";     keyValue.value = "";                                                                                                                 pairs.push_back(keyValue);
    keyValue.name  = "%SK_KEYMAPDATA%";         keyValue.value = "";                                                                                                                 pairs.push_back(keyValue);
    keyValue.name  = "%SK_KEYMAPPOPOVER%";      keyValue.value = "";                                                                                                                 pairs.push_back(keyValue);
    keyValue.name  = "%SK_MOUSEHOSTSCALING%";   keyValue.value = "";                                                                                                                 pairs.push_back(keyValue);
    keyValue.name  = "%SK_MOUSEPS2SCALING%";    keyValue.value = "";                                                                                                                 pairs.push_back(keyValue);
    keyValue.name  = "%SK_MOUSEPS2RESOLUTION%"; keyValue.value = "";                                                                                                                 pairs.push_back(keyValue);
    keyValue.name  = "%SK_MOUSEPS2SAMPLERATE%"; keyValue.value = "";                                                                                                                 pairs.push_back(keyValue);

    // Go through list of place holders to expand and replace. 
    //
    for(auto pair : pairs)
    {
        // If the varname exists, replace with value.
        if((startPos = str.find(pair.name)) >= 0)
        {
            // Dont expand large data macros yet, they can potentially generate too much data for the limited ESP32 RAM.
            if(pair.name.compare("%SK_KEYMAPHEADER%") != 0 && pair.name.compare("%SK_KEYMAPTYPES%") != 0 && pair.name.compare("%SK_KEYMAPDATA%") != 0 && 
               pair.name.compare("%SK_KEYMAPJSFIELDS%") != 0 && pair.name.compare("%SK_KEYMAPPOPOVER%") != 0 && pair.name.compare("%SK_MOUSEHOSTSCALING%") != 0 &&
               pair.name.compare("%SK_MOUSEPS2SCALING%") != 0 && pair.name.compare("%SK_MOUSEPS2RESOLUTION%") != 0 && pair.name.compare("%SK_MOUSEPS2SAMPLERATE%") != 0
              ) 
            {
                str.replace(startPos, pair.name.length(), pair.value);
            } else
            {
                largeMacroDetected = true;
            }
        }
    }
    // Complete line ready for transmission.
    str.append("\n");

    // Normal macros have been expanded, if no large macros were detected, send the expanded string and return.
    //
    if(largeMacroDetected == false)
    {
        // Send as a chunk to the browser.
        if(str.size() > 0)
        {
            result=httpd_resp_send_chunk(req, str.c_str(), str.size());
        }
    } else
    {
        // Repeat the key:value search, locating the large macro, only 1 is allowed per line.
        for(auto pair : pairs)
        {
            // If the macro name exists, process, it will be the large macro, only 1 allowed per line.
            if((startPos = str.find(pair.name)) >= 0)
            {
                // Ease of reading.
                int endMacroPos = startPos+pair.name.size();
                int sizeMacro = pair.name.size();
                int sizeEndStr = str.size() - startPos - sizeMacro;
                if(sizeEndStr < 0) sizeEndStr = 0;

                // Send the first part of the string upto but excluding the macro.
                if(startPos > 0)
                    result=httpd_resp_send_chunk(req, str.substr(0, startPos).c_str(), startPos);
                if(result == ESP_OK)
                {
                    // Keymap Table header. The underlying interface converts its keyboard mapping table into a javascript format list of column headers.
                    if(pair.name.compare("%SK_KEYMAPHEADER%") == 0)
                    {
                        result = sendKeyMapHeaders(req);
                    }
                    // Keymap Table types. The underlying interface converts its keyboard mapping table into a javascript format list of column types.
                    if(pair.name.compare("%SK_KEYMAPTYPES%") == 0)
                    {
                        result = sendKeyMapTypes(req);
                    }
                    // Keymap field definition for custom fields.
                    if(pair.name.compare("%SK_KEYMAPJSFIELDS%") == 0)
                    {
                        result = sendKeyMapCustomTypeFields(req);
                    }
                    // Keymap Table data. This is the big one where the underlying interface converts its keyboard mapping table into a javascript format list of column types.
                    if(pair.name.compare("%SK_KEYMAPDATA%") == 0)
                    {
                        result = sendKeyMapData(req);
                    }
                    // Popover boxes, aid data input in a more user friendly manner.
                    if(pair.name.compare("%SK_KEYMAPPOPOVER%") == 0)
                    {
                        result = sendKeyMapPopovers(req);
                    }
                    // Mouse host scaling - Radio selection of the scaling required for adaption of the PS/2 mouse data to host.
                    if(pair.name.compare("%SK_MOUSEHOSTSCALING%") == 0)
                    {
                        result = sendMouseRadioChoice(req, "host_scaling");
                    }
                    if(pair.name.compare("%SK_MOUSEPS2SCALING%") == 0)
                    {
                        result = sendMouseRadioChoice(req, "mouse_scaling");
                    }
                    if(pair.name.compare("%SK_MOUSEPS2RESOLUTION%") == 0)
                    {
                        result = sendMouseRadioChoice(req, "mouse_resolution");
                    }
                    if(pair.name.compare("%SK_MOUSEPS2SAMPLERATE%") == 0)
                    {
                        result = sendMouseRadioChoice(req, "mouse_sampling");
                    }

                    // If the input string had any data after the macro then send it to complete transmission.
                    if(result == ESP_OK && sizeEndStr > 0)
                    {
                        result=httpd_resp_send_chunk(req, str.substr(endMacroPos, std::string::npos).c_str(), sizeEndStr);
                    }
                }
                break;
            }
        }
    }

    // Debug, track heap size.
    ESP_LOGD(WIFITAG, "After expansion Free Heap (%d)", xPortGetFreeHeapSize());

    // Return result of expansion/transmission.
    return(result);
}

// A method to open and read a file line by line, expanding any macros therein and sending the result to the open socket connection.
//
esp_err_t WiFi::expandAndSendFile(httpd_req_t *req, const char *basePath, std::string fileName)
{
    // Locals.
    //
    std::string   line;
    std::ifstream inFile;
    esp_err_t     result = ESP_OK;
   
    // Build the FQFN for reading.
    std::string fqfn = basePath; fqfn += "/"; fqfn += fileName;

    // Ensure the content type is set correctly.
    setContentTypeFromFileType(req, fileName);

    // Read the file into an input stream, read a line, expand it and r and then store into a string buffer to be returned to caller.
    inFile.open(fqfn.c_str());
    while(result == ESP_OK && std::getline(inFile, line))
    {
        // Call method to output line after expanding, in-situ, any macros into variable values.
        if((result=expandVarsAndSend(req, line)) != ESP_OK)
        {
            // Abort sending file.
            httpd_resp_sendstr_chunk(req, NULL);

            // Respond with 500 Internal Server Error.
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            break;
        }
    }

    // Successful, end the response with a NULL string.
    if(result == ESP_OK)
    {
        result = httpd_resp_send_chunk(req, NULL, 0);
    }

    // Tidy up for exit.
    inFile.close();

    // Return result code.
    return(result);
}

bool WiFi::isFileExt(std::string fileName, std::string extension)
{
    // Locals.
    //
    bool            result = false;

    // Match the extension.
    if(strcasecmp(fileName.substr(fileName.find_last_of(".")).c_str(), extension.substr(extension.find_last_of(".")).c_str()) == 0)
    {
        // If this is a multi part extension, match the whole extension too.
        //
        result = true;
        if(extension.find_first_of(".") != extension.find_last_of(".") && strcasecmp(fileName.substr(fileName.find_first_of(".")).c_str(), extension.c_str()) != 0)
        {
            result = false;
        }
    }

    // Extension match?
    return(result);
}

// Method to set the HTTP response content type according to file extension.
//
esp_err_t WiFi::setContentTypeFromFileType(httpd_req_t *req, std::string fileName)
{
    if (isFileExt(fileName, ".pdf"))
    {
        return httpd_resp_set_type(req, "application/pdf");
    }
    else if (isFileExt(fileName, ".html"))
    {
        return httpd_resp_set_type(req, "text/html");
    }
    else if (isFileExt(fileName, ".css"))
    {
        return httpd_resp_set_type(req, "text/css");
    }
    else if (isFileExt(fileName, ".js"))
    {
        return httpd_resp_set_type(req, "application/javascript");
    }
    else if (isFileExt(fileName, ".ico"))
    {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    else if (isFileExt(fileName, ".jpeg") || isFileExt(fileName, ".jpg"))
    {
        return httpd_resp_set_type(req, "image/jpeg");
    }
    else if (isFileExt(fileName, ".bin"))
    {
        return httpd_resp_set_type(req, "application/octet-stream");
    }
    else if (isFileExt(fileName, ".bmp"))
    {
        return httpd_resp_set_type(req, "image/bmp");
    }
    else if (isFileExt(fileName, ".gif"))
    {
        return httpd_resp_set_type(req, "image/gif");
    }
    else if (isFileExt(fileName, ".jar"))
    {
        return httpd_resp_set_type(req, "application/java-archive");
    }
    else if (isFileExt(fileName, ".js"))
    {
        return httpd_resp_set_type(req, "text/javascript");
    }
    else if (isFileExt(fileName, ".json"))
    {
        return httpd_resp_set_type(req, "application/json");
    }
    else if (isFileExt(fileName, ".png"))
    {
        return httpd_resp_set_type(req, "image/png");
    }
    else if (isFileExt(fileName, ".php"))
    {
        return httpd_resp_set_type(req, "application/x-httod-php");
    }
    else if (isFileExt(fileName, ".rtf"))
    {
        return httpd_resp_set_type(req, "application/rtf");
    }
    else if (isFileExt(fileName, ".tif") || isFileExt(fileName, ".tiff"))
    {
        return httpd_resp_set_type(req, "image/tiff");
    }
    else if (isFileExt(fileName, ".txt"))
    {
        return httpd_resp_set_type(req, "text/plain");
    }
    else if (isFileExt(fileName, ".xml"))
    {
        return httpd_resp_set_type(req, "application/xml");
    }
    else if (isFileExt(fileName, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    // Default to plain text.
    return httpd_resp_set_type(req, "text/plain");
}

// Locates the path within URI anc copies it into a string.
//
esp_err_t WiFi::getPathFromURI(std::string& destPath, std::string& destFile, const char *basePath, const char *uri)
{
    // Locals.
    //
    size_t       pathlen      = strlen(uri);
    const char  *question     = strchr(uri, '?');
    const char  *hash         = strchr(uri, '#');

    // Question in the URI - skip. 
    if(question)
    {
        pathlen = MIN(pathlen, question - uri);
    }
    // Hash in the URI - skip.
    if(hash)
    {
        pathlen = MIN(pathlen, hash - uri);
    }

    // Construct full path (base + path)
    destPath = basePath;
    destPath.append(uri, pathlen);

    // Extract filename.
    destFile = "";
    destFile.append(uri, 1, pathlen-1);

    // Result, fail if no path extracted.
    return(destFile.size() == 0 ? ESP_FAIL : ESP_OK);
}

// Overloaded method to get the remaining URI from the triggering base path.
//
esp_err_t WiFi::getPathFromURI(std::string& destPath, const char *basePath, const char *uri)
{
    // Locals.
    //
    esp_err_t    result       = ESP_OK;
    size_t       pathlen      = strlen(uri);
    const char  *question     = strchr(uri, '?');
    const char  *hash         = strchr(uri, '#');

    // Question in the URI - skip. 
    if(question)
    {
        pathlen = MIN(pathlen, question - uri);
    }
    // Hash in the URI - skip.
    if(hash)
    {
        pathlen = MIN(pathlen, hash - uri);
    }

    // Extract the path without starting base path and without trailing variables.
    destPath = "";
    destPath.append(uri, pathlen);
    result = (destPath.find(basePath) != std::string::npos ? ESP_OK : ESP_FAIL);
    if(result == ESP_OK)
    {
        destPath.erase(0, strlen(basePath));
    }

    // Result, fail if no base path was found.
    return(result);
}

// Handler to read and send static files. HTML/CSS are expanded with embedded vars.
//
esp_err_t WiFi::defaultFileHandler(httpd_req_t *req)
{
    // Locals.
    //
    FILE       *fd = NULL;
    struct stat file_stat;
    char       *buf;
    int         bufLen;
    std::string gzipFile = "";
    std::string disposition = "";
  
    // Retrieve pointer to object in order to access data.
    WiFi* pThis = (WiFi*)req->user_ctx;
 
    // Get required Header values for processing.
    bufLen = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if(bufLen > 1)
    {
        buf = new char[bufLen];
        // Copy null terminated value string into buffer
        if(httpd_req_get_hdr_value_str(req, "Host", buf, bufLen) == ESP_OK)
        {
            // Assign to control structure for later use.
            pThis->wifiCtrl.session.host = buf;
        }

        // Free up memory to complete.
        delete buf;
    }
    // Get encoding methods.
    bufLen = httpd_req_get_hdr_value_len(req, "Accept-Encoding") + 1;
    if(bufLen > 1)
    {
        buf = new char[bufLen];
        // Set flags to indicate allowed encoding methods.
        if(httpd_req_get_hdr_value_str(req, "Accept-Encoding", buf, bufLen) == ESP_OK)
        {
            pThis->wifiCtrl.session.gzip    = (strstr(buf, "gzip") != NULL ? true : false);
            pThis->wifiCtrl.session.deflate = (strstr(buf, "deflate") != NULL ? true : false);
        }

        // Free up memory to complete.
        delete buf;
    }

    // Get and store the URL query string.
    bufLen = httpd_req_get_url_query_len(req) + 1;
    if (bufLen > 1) 
    {
        buf = new char[bufLen];
        if (httpd_req_get_url_query_str(req, buf, bufLen) == ESP_OK)
        {
            pThis->wifiCtrl.session.queryStr = buf;
            ESP_LOGI(WIFITAG, "Found URL query => %s", pThis->wifiCtrl.session.queryStr.c_str());
        }
       
        // Free up memory to complete.
        delete buf;
    }

    // Look for a filename in the URI and construct the file path returning both. If filename isnt valid, respond with 500 Internal Server Error and exit.
    if(pThis->getPathFromURI(pThis->wifiCtrl.session.filePath, pThis->wifiCtrl.session.fileName, pThis->wifiCtrl.run.basePath, req->uri) == ESP_FAIL)
    {
        // Check for root URL.
        if(strlen(req->uri) == 1 && req->uri[0] == '/')
        {
            pThis->wifiCtrl.session.fileName = "/";
        } else
        {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename invalid");
            return(ESP_FAIL);
        }
    }

    // See if the provided name matches a static handler, such as root, if so execute response directly.
    if(pThis->wifiCtrl.session.fileName.compare("/") == 0 || pThis->wifiCtrl.session.fileName.compare("index.html") == 0 || pThis->wifiCtrl.session.fileName.compare("index.htm") == 0)
    {
        // Open the given file, read and expand macros and send to open connection.
        return pThis->expandAndSendFile(req, pThis->wifiCtrl.run.basePath, "index.html");
    }

    // Is this a macro to specify keymap file? Keymap file name changes depending on runmode so adjust filename accordingly.
    if(pThis->wifiCtrl.session.fileName.compare("keymap") == 0)
    {
        pThis->wifiCtrl.session.fileName = std::regex_replace(pThis->wifiCtrl.session.fileName, std::regex("keymap"), pThis->keyIf->getKeyMapFileName());
        pThis->wifiCtrl.session.filePath = std::regex_replace(pThis->wifiCtrl.session.filePath, std::regex("keymap"), pThis->keyIf->getKeyMapFileName());
        disposition = "attachment; filename=" + pThis->keyIf->getKeyMapFileName();
        if(httpd_resp_set_hdr(req, "Content-Disposition", disposition.c_str()) != ESP_OK)
        {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set content disposition filename failed");
            return(ESP_FAIL);
        }
    }

    // Get details of the file, throw error 404 - File Not Found on error.
    if(stat(pThis->wifiCtrl.session.filePath.c_str(), &file_stat) == -1)
    {
        // Prepare gzip version, size remains 0 if normal file is found.
        if(pThis->wifiCtrl.session.gzip)
            gzipFile = pThis->wifiCtrl.session.filePath + ".gz";

        // Check to see if the file is compressed. Tag on .gz and retry, if success then set encoding content and carry on as normal.
        //
        if(pThis->wifiCtrl.session.gzip == true && stat(gzipFile.c_str(), &file_stat) == -1)
        {
            // Respond with 404 Not Found.
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
            return(ESP_FAIL);
        }
       
        // Set the content encoding to gzip to comply with specs.
        // WARNING: Do not gzip html or library css files as they get parsed and expanded.
        if(httpd_resp_set_hdr(req, "Content-Encoding", "gzip") != ESP_OK)
        {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set content encoding to gzip failed");
            return(ESP_FAIL);
        }
    }

    // If the file is HTML, JS or CSS then process externally as we need to subsitute embedded variables as required. Note the guard around static evaluation, ie. gzipFile.size. This is to cater for gzipped html, js or css as we cannot
    // parse and expand, it is served 'as is'.
    if((pThis->isFileExt(pThis->wifiCtrl.session.fileName, ".html") || (pThis->isFileExt(pThis->wifiCtrl.session.fileName, ".js") && !pThis->isFileExt(pThis->wifiCtrl.session.fileName, ".min.js")) || (pThis->isFileExt(pThis->wifiCtrl.session.fileName, ".css") && !pThis->isFileExt(pThis->wifiCtrl.session.fileName, ".min.css"))) && gzipFile.size() == 0)
    {
        // Open the given file, read and expand macros and send to open connection.
        pThis->expandAndSendFile(req, pThis->wifiCtrl.run.basePath, pThis->wifiCtrl.session.fileName);
    } else
    {
        // Try to open the file, we performed a stat so is does exist but perhaps a FAT corruption occurred?.
        fd = fopen(gzipFile.size() > 0 ? gzipFile.c_str() : pThis->wifiCtrl.session.filePath.c_str(), "r");
        if(!fd)
        {
            // Respond with 500 Internal Server Error
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
            return(ESP_FAIL);
        }

        ESP_LOGI(WIFITAG, "Sending %sfile : %s (%ld bytes)...", gzipFile.size() > 0 ? "gzip " : " ", pThis->wifiCtrl.session.fileName.c_str(), file_stat.st_size);
        pThis->setContentTypeFromFileType(req, pThis->wifiCtrl.session.fileName);

        // Allocate a buffer for chunking the file. The file could be binary, so unlike the HTML/CSS handler, strings cant be used
        // thus we read chunks according to our buffer size and send accordingly.
        char *chunk = new char[MAX_CHUNK_SIZE];
        size_t chunksize;
        do {
            // Read file in chunks into the temporary buffer.
            chunksize = fread(chunk, 1, MAX_CHUNK_SIZE, fd);

            if (chunksize > 0)
            {
                // Send the buffer contents as HTTP response chunk.
                if(httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK)
                {

                    // Release memory and close files, error!!
                    fclose(fd);
                    delete chunk;

                    // Abort sending file.
                    httpd_resp_sendstr_chunk(req, NULL);

                    // Respond with 500 Internal Server Error.
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                    return(ESP_FAIL);
               }
            }

            // Keep looping till the whole file is sent.
        } while (chunksize != 0);

        // Release memory to complete.
        delete chunk;

        // Close file after sending complete.
        fclose(fd);
        ESP_LOGI(WIFITAG, "File sending complete");
    }

    // Respond with an empty chunk to signal HTTP response completion.
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// Handler to send data sets. The handler is triggered on the /data URI and subpaths define the data to be sent.
//
esp_err_t WiFi::defaultDataGETHandler(httpd_req_t *req)
{
    // Locals.
    //
    char                 *buf;
    int                   bufLen;
    esp_err_t             result = ESP_OK;
    std::string           uriStr;
  
    // Retrieve pointer to object in order to access data.
    WiFi* pThis = (WiFi*)req->user_ctx;
 
    // Get required Header values for processing.
    bufLen = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if(bufLen > 1)
    {
        buf = new char[bufLen];
        // Copy null terminated value string into buffer
        if(httpd_req_get_hdr_value_str(req, "Host", buf, bufLen) == ESP_OK)
        {
            // Assign to control structure for later use.
            pThis->wifiCtrl.session.host = buf;
        }

        // Free up memory to complete.
        delete buf;
    }

    // Get and store the URL query string.
    bufLen = httpd_req_get_url_query_len(req) + 1;
    if (bufLen > 1) 
    {
        buf = new char[bufLen];
        if (httpd_req_get_url_query_str(req, buf, bufLen) == ESP_OK)
        {
            pThis->wifiCtrl.session.queryStr = buf;
            ESP_LOGI(WIFITAG, "Found URL query => %s", pThis->wifiCtrl.session.queryStr.c_str());
        }
       
        // Free up memory to complete.
        delete buf;
    }

    // Get the subpath from the URI.
    if(pThis->getPathFromURI(uriStr, "/data/", req->uri) == ESP_FAIL)
    {
        // Respond with 500 Internal Server Error.
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to extract URI");
        return(ESP_FAIL);
    }

    // Match URI and execute required data retrieval and return.
    if(uriStr.compare("keymap/table/headers") == 0)
    {
        result = pThis->sendKeyMapHeaders(req);
    } else
    if(uriStr.compare("keymap/table/types") == 0)
    {
        result = pThis->sendKeyMapTypes(req);
    } else
    if(uriStr.compare("keymap/table/data") == 0)
    {
        result = pThis->sendKeyMapData(req);
    } else
    {
        result = ESP_FAIL;
    }

    // Check the result, if the data send failed we need to tidy up and send an error code.
    if(result != ESP_OK)
    {
        // Abort sending file.
        httpd_resp_sendstr_chunk(req, NULL);

        // Respond with 500 Internal Server Error.
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send data.");
    } else
    {
        // Successful, end the response with a NULL string.
        result = httpd_resp_send_chunk(req, NULL, 0);
    }

    // Return result, successful data send == ESP_OK.
    return(result);
}

// A method, activated on a client side POST using AJAX file upload, to accept incoming data and write it into the next free OTA partition. If successful
// set the active partition to the newly loaded one.
//
IRAM_ATTR esp_err_t WiFi::otaFirmwareUpdatePOSTHandler(httpd_req_t *req)
{
    // Locals.
    //
    esp_err_t              ret = ESP_OK;
    std::string            resp = "";
    bool                   checkImageHeader = true;
    esp_ota_handle_t       updateHandle = 0 ;
    esp_app_desc_t         newAppInfo;
    esp_app_desc_t         runningAppInfo;
    esp_app_desc_t         invalidAppInfo;
    const esp_partition_t *lastInvalidApp;
    const esp_partition_t *runningApp;
    const esp_partition_t *updatePartition;

    // Retrieve pointer to object in order to access data.
    //WiFi* pThis = (WiFi*)req->user_ctx;

    // Get current configuration and next available partition in round-robin style.
    lastInvalidApp = esp_ota_get_last_invalid_partition();
    runningApp = esp_ota_get_running_partition();
    updatePartition = esp_ota_get_next_update_partition(NULL);
    if(runningApp == NULL || updatePartition == NULL)
    {
        // Respond with 500 Internal Server Error as we couldnt get primary information on running partition or next available partition for update.
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to resolve current/next NVS OTA partition information.");
        return(ESP_FAIL);
    }

    // Allocate heap space for our receive buffer.
    //
    char *chunk = new char[MAX_CHUNK_SIZE];
    size_t chunkSize;

    // Use the Content length as the size of the file to be uploaded.
    int remaining = req->content_len;

    // Loop while data is still expected.
    while(remaining > 0)
    {
        ESP_LOGI(WIFITAG, "Remaining size : %d", remaining);

        // The file is received in chunks according to the free memory available for a buffer. It has to be at least the size of the firmware application header
        // so that it can be read and evaluated in one chunk.
        if((chunkSize = httpd_req_recv(req, chunk, MIN(remaining, MAX_CHUNK_SIZE))) <= 0)
        {
            // Retry if timeout occurred.
            if (chunkSize == HTTPD_SOCK_ERR_TIMEOUT)
                continue;

            // Release memory, error!!
            delete chunk;

            // Respond with 500 Internal Server Error when a file error occurs.
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return(ESP_FAIL);
        }

        // If this is the first read, check the header information and make sure we are not uploading a bad file or one the same as the current image.
        //
        if(checkImageHeader == true)
        {
            // The size should be at least that of the application header structures.
            //
            if (chunkSize > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
            {
                // Check current version being downloaded.
                memcpy(&newAppInfo, &chunk[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                if(newAppInfo.magic_word != ESP_APP_DESC_MAGIC_WORD)
                {
                    // Release memory, error!!
                    delete chunk;

                    /* Respond with 500 Internal Server Error */
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File image is not a valid firmware file.");
                    return(ESP_FAIL);
                }

                // Get information on the running application image.
                if(esp_ota_get_partition_description(runningApp, &runningAppInfo) == ESP_OK)
                {
                    ESP_LOGI(WIFITAG, "Running firmware version: %s, new: %s", runningAppInfo.version, newAppInfo.version);
                }

                // Compare and make sure we are not trying to upload the same image as the running image.
                if(strcmp(newAppInfo.version, runningAppInfo.version) == 0)
                {
                    // Release memory, error!!
                    delete chunk;

                    // Respond with 500 Internal Server Error - Same firmware version as running image being uploaded.
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware version is same as current version.");
                    return(ESP_FAIL);
                }

                // This part is crucial. If a previous image upload failed to boot, it is marked as bad and the previous working image is rebooted.
                // Checks need to be made to ensure we are not trying to upload the same bad image as we may not be so lucky next time detecting it as bad!
                //
                if (esp_ota_get_partition_description(lastInvalidApp, &invalidAppInfo) == ESP_OK)
                {
                    ESP_LOGI(WIFITAG, "Last invalid firmware version: %s", invalidAppInfo.version);
                }

                // Check current version with last invalid partition version. On first factory load there wont be a last partition.
                if(lastInvalidApp != NULL)
                {
                    if (memcmp(invalidAppInfo.version, newAppInfo.version, sizeof(newAppInfo.version)) == 0)
                    {
                        // Release memory, error!!
                        delete chunk;

                        // Respond with 500 Internal Server Error - New image is a previous bad image, cannot upload.
                        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware version is known as bad, it previously failed to boot.");
                        return(ESP_FAIL);
                    }
                }

                // Dont repeat the check!
                checkImageHeader = false;

                // Start the update procedure, data is written as it arrives from the client in chunks.
                ret = esp_ota_begin(updatePartition, OTA_WITH_SEQUENTIAL_WRITES, &updateHandle);
                if(ret != ESP_OK)
                {
                    esp_ota_abort(updateHandle);
                    
                    // Release memory, error!!
                    delete chunk;

                    // Respond with 500 Internal Server Error - Failed to initialise NVS for writing.
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to initialise NVS OTA partition for writing.");
                    return(ESP_FAIL);
                }
            } else
            {
                esp_ota_abort(updateHandle);
                
                // Release memory, error!!
                delete chunk;
               
                // Respond with 500 Internal Server Error - Failed to receive sufficient bytes from file to identify header.
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive sufficient bytes from file to identify image.");
                return(ESP_FAIL);
            }
        }
        // Write out this chunk of data.
        ret = esp_ota_write( updateHandle, (const void *)chunk, chunkSize);
        if(ret != ESP_OK)
        {
            esp_ota_abort(updateHandle);
           
            // Release memory, error!!
            delete chunk;

            // Respond with 500 Internal Server Error - Failed to write packet to NVS OTA partition.
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write data to NVS partition.");
            return(ESP_FAIL);
        }

        // Keep track of remaining size to download.
        remaining -= chunkSize;
    }
    // Release memory, all done!
    delete chunk;

    // Complete the NVS write transaction.
    ret = esp_ota_end(updateHandle);
    if(ret != ESP_OK)
    {
        if(ret == ESP_ERR_OTA_VALIDATE_FAILED)
        {
            // Respond with 500 Internal Server Error - Image validation failed.
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Image validation failed, image is corrupt.");
            return(ESP_FAIL);
        } else
        {
            // Respond with 500 Internal Server Error - Image completion failed.
            std::string errMsg = "Image completion failed:"; errMsg += esp_err_to_name(ret);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, errMsg.c_str());
            return(ESP_FAIL);
        }
    }

    // The image has been successfully downloaded, it is not duplicate or a known dud and validation has passed so set it up as the next boot partition.
    ret = esp_ota_set_boot_partition(updatePartition);
    if(ret != ESP_OK)
    {
        std::string errMsg = "Set boot parition to new image failed:"; errMsg += esp_err_to_name(ret);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, errMsg.c_str());
        return(ESP_FAIL);
    }
  
    // Done, send positive status.
    vTaskDelay(500);
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_sendstr(req, "");

    return(ret);
}

// Method to update the LittleFS filesystem OTA. The image file is pushed via an AJAX xhttp POST, received in chunks and written direct
// to the parition. There is no rollback, any failure will see the filesystem corrupted. On a bigger ESP32 Flash chip, rollback may be
// possible but not with the 4MB standard IC.
IRAM_ATTR esp_err_t WiFi::otaFilepackUpdatePOSTHandler(httpd_req_t *req)
{
    // Locals.
    //
    esp_err_t              ret = ESP_OK;
    std::string            resp = "";
    bool                   checkImageHeader = true;
    uint32_t               partStartAddr;
    uint32_t               partOffsetAddr = 0;
    uint32_t               partSize = 0;

    // Retrieve pointer to object in order to access data.
    WiFi* pThis = (WiFi*)req->user_ctx;

    // Find the filesystem partition.
    esp_partition_iterator_t it;
    it = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    if(it == NULL)
    {
        // Respond with 500 Internal Server Error - Couldnt find the filesystem partition.
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Couldnt find a filesysten partition, is this ESP32 configured?");
        return(ESP_FAIL);
    }

    // Get the partition information from the iterator.
    const esp_partition_t *part = esp_partition_get(it);

    // Setup the addresses of the partition.
    partStartAddr = part->address;
    partSize = part->size;

    // Check to ensure the file to upload is not larger than the partition.
    //
    if(req->content_len > partSize)
    {
        // Respond with 500 Internal Server Error - File is too large.
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload file size too large.");
        return(ESP_FAIL);
    }

    // Erase the partition.
    ret = esp_partition_erase_range(part, partOffsetAddr, partSize);
    if(ret != ESP_OK)
    {
        // Respond with 500 Internal Server Error - Partition erase failure.
        std::string errMsg = "Failed to erase partition:"; errMsg += esp_err_to_name(ret); errMsg += ",  you may need to connect external programmer."; 
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, errMsg.c_str());
        return(ESP_FAIL);
    }

    // Allocate heap space for our receive buffer.
    //
    char *chunk = new char[MAX_CHUNK_SIZE];
    size_t chunkSize;

    // Use the Content length as the size of the file to be uploaded.
    int remaining = req->content_len;

    // Loop while data is still expected.
    while(remaining > 0)
    {
        ESP_LOGI(WIFITAG, "Remaining size : %d", remaining);

        // The file is received in chunks according to the free memory available for a buffer. It has to be at least the size of the firmware application header
        // so that it can be read and evaluated in one chunk.
        if((chunkSize = httpd_req_recv(req, chunk, MIN(remaining, MAX_CHUNK_SIZE))) <= 0)
        {
            // Retry if timeout occurred.
            if (chunkSize == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
            
            // Release memory, error!!
            delete chunk;

            // Respond with 500 Internal Server Error when a file error occurs.
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return(ESP_FAIL);
        }

        // If this is the first read, check the header information and make sure we are not uploading a bad file or one the same as the current image.
        //
        if(checkImageHeader == true)
        {
            // Simple check, look for the base path name in the image. Also the max size was checked earlier, smaller sizes are fine as the littlefs 
            // filestructure is valid but larger files will overwrite NVS data.
            if(strncmp(pThis->wifiCtrl.run.fsPath+1, &chunk[8], strlen(pThis->wifiCtrl.run.fsPath)-1) != 0)
            {
                // Release memory, error!!
                delete chunk;

                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filepack image is not a valid file.");
                return(ESP_FAIL);
            }

            // Dont repeat the check!
            checkImageHeader = false;
        }

        // Write out the chunk we received, any errors we abort - probably means the user needs to use an external programmer to correct to error.
        ret = esp_partition_write_raw(part, partOffsetAddr, (const void *)chunk, chunkSize);
        if(ret != ESP_OK)
        {
            // Release memory, error!!
            delete chunk;

            // Respond with 500 Internal Server Error - Write failure.
            std::string errMsg = "Write failure: "; errMsg += esp_err_to_name(ret); errMsg += " @ "; errMsg += to_str(partStartAddr+partOffsetAddr, 0, 16);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, errMsg.c_str());
            return(ESP_FAIL);
        }

        // Update counters.
        partOffsetAddr += chunkSize;
        remaining -= chunkSize;
    }
    // Release memory, all done!
    delete chunk;

    // Done, send positive status.
    vTaskDelay(500);
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_sendstr(req, "");

    // Send result.
    return(ESP_OK);
}

// Method to upload a file and store it onto the filesystem tagged according to the current running I/F. 
// This method could be merged with the Firmware/Filepack methods but at the moment kept seperate to allow for any 
// unseen requirements.
// The data should be passed to the I/F object so that it can verify 
//
esp_err_t WiFi::keymapUploadPOSTHandler(httpd_req_t *req)
{
    // Locals.
    //
    std::string            resp = "";
    std::fstream           keyFileOut;

    // Retrieve pointer to object in order to access data.
    WiFi* pThis = (WiFi*)req->user_ctx;

    // Attempt to open the keymap file for writing.
    //
    if(pThis->keyIf->createKeyMapFile(keyFileOut) == false)
    {
        // Respond with 500 Internal Server Error - File creation error.
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create temporary keymap file");
        return(ESP_FAIL);
    }
    
    // Allocate heap space for our receive buffer.
    //
    char *chunk = new char[MAX_CHUNK_SIZE];
    size_t chunkSize;

    // Use the Content length as the size of the file to be uploaded.
    int remaining = req->content_len;

    // Loop while data is still expected.
    while(remaining > 0)
    {
        ESP_LOGI(WIFITAG, "Remaining size : %d", remaining);

        // The file is received in chunks according to the free memory available for a buffer.
        if((chunkSize = httpd_req_recv(req, chunk, MIN(remaining, MAX_CHUNK_SIZE))) <= 0)
        {
            // Retry if timeout occurred.
            if (chunkSize == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
            
            // Release memory, error!!
            delete chunk;

            // Respond with 500 Internal Server Error when a reception error occurs.
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return(ESP_FAIL);
        }

        // Store the data chunk into the keymap file.
        if(pThis->keyIf->storeDataToKeyMapFile(keyFileOut, chunk, chunkSize) == false)
        {
            // Cleanup the mess!
            pThis->keyIf->closeAndCommitKeyMapFile(keyFileOut, true);
            
            // Release memory, error!!
            delete chunk;

            // Respond with 500 Internal Server Error when a file error occurs.
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write data into file");
            return(ESP_FAIL);
        }

        // Update counters.
        remaining -= chunkSize;
    }
    // Release memory, all done!
    delete chunk;

    // Close and commit the file.
    if(pThis->keyIf->closeAndCommitKeyMapFile(keyFileOut, false) == false)
    {
        // Respond with 500 Internal Server Error if the commit fails.
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write data into file");
        return(ESP_FAIL);
    }

    // Done, send positive status.
    vTaskDelay(500);
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_sendstr(req, "");

    // Send result.
    return(ESP_OK);
}


// Method to store the keymap table data. The POST data is captured in chunks and sent to the underlying interface method 
// for parsing and extraction.
esp_err_t WiFi::keymapTablePOSTHandler(httpd_req_t *req)
{
    // Locals.
    //
    int                    startPos;
    int                    endPos;
    int                    commaPos;
    std::string            resp = "";
    std::string            jsonData = "";
    std::string            jsonArray = "";
    std::fstream           keyFileOut;
    std::vector<uint32_t>  dataArray;

    // Retrieve pointer to object in order to access data.
    WiFi* pThis = (WiFi*)req->user_ctx;

    // Attempt to open the keymap file for writing.
    //
    if(pThis->keyIf->createKeyMapFile(keyFileOut) == false)
    {
        // Respond with 500 Internal Server Error - File creation error.
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create temporary keymap file");
        return(ESP_FAIL);
    }

    // Allocate heap space for our receive buffer.
    //
    char *chunk = new char[MAX_CHUNK_SIZE];
    size_t chunkSize;

    // Use the Content length as the size of the JSON array to be uploaded.
    int remaining = req->content_len;

    // Loop while data is still expected.
    while(remaining > 0)
    {
        ESP_LOGI(WIFITAG, "Remaining size : %d", remaining);

        // The file is received in chunks according to the free memory available for a buffer.
        if((chunkSize = httpd_req_recv(req, chunk, MIN(remaining, MAX_CHUNK_SIZE))) <= 0)
        {
            // Retry if timeout occurred.
            if (chunkSize == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
           
            // Release memory, error!!
            delete chunk;

            // Respond with 500 Internal Server Error when a reception error occurs.
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return(ESP_FAIL);
        }

        // Build up the JSON Data array and process piecemeal to keep overall memory usage low.
        jsonData.append(chunk, chunkSize);
        do {
            startPos = jsonData.find("[[");
            if(startPos != std::string::npos) { startPos++; } else { startPos = jsonData.find("["); }
            endPos = jsonData.find("]");

            if(startPos != std::string::npos && endPos != std::string::npos)
            {
                // Extract the array and parse into bytes.
                jsonArray = jsonData.substr(startPos, endPos+1);
                do {
                    commaPos = jsonArray.find("\"");
                    if(commaPos != std::string::npos)
                    {
                        jsonArray.erase(0, commaPos+1);
                        commaPos = jsonArray.find("\"");
                        if(commaPos != std::string::npos)
                        {
                            std::istringstream iss(jsonArray.substr(0, commaPos));
                            uint32_t word;
                            iss >> std::hex >> word;
                            dataArray.push_back(word);
                        }
                        commaPos = jsonArray.find(",");
                        if(commaPos != std::string::npos)
                        {
                            jsonArray.erase(0, commaPos+1);
                        }
                    }
                } while(jsonArray.size() > 0 && commaPos != std::string::npos);

                // Remove the array and the comma (or ending ]) seperator.
                jsonData.erase(0, endPos + 2);
            }
        } while(startPos != std::string::npos && endPos != std::string::npos);
     
        // Store the data chunk into the keymap file.
        if(pThis->keyIf->storeDataToKeyMapFile(keyFileOut, dataArray) == false)
        {
            // Cleanup the mess!
            pThis->keyIf->closeAndCommitKeyMapFile(keyFileOut, true);
           
            // Release memory, error!!
            delete chunk;

            // Respond with 500 Internal Server Error when a file error occurs.
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write data into file");
            return(ESP_FAIL);
        }

        // Free up vector for next loop.
        dataArray.clear();

        // Update counters.
        remaining -= chunkSize;
    }
    // Release memory, all done!
    delete chunk;
 
    // Close and commit the file.
    if(pThis->keyIf->closeAndCommitKeyMapFile(keyFileOut, false) == false)
    {
        // Respond with 500 Internal Server Error if the commit fails.
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write data into file");
        return(ESP_FAIL);
    }

    // Done, send positive status.
    vTaskDelay(500);
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_sendstr(req, "");

    // Send result.
    return(ESP_OK);
}

// Method to extract the Key/Value pairs from a received POST request.
//
esp_err_t WiFi::getPOSTData(httpd_req_t *req, std::vector<t_kvPair> *pairs)
{
    // Locals.
    //
    char                  buf[100];
    int                   ret;
    int                   rcvBytes = req->content_len;
    std::string           post;
    t_kvPair              keyValue;

    // Loop retrieving the POST in chunks and assemble into a string.
    while(rcvBytes > 0)
    {
        // Read data for the request.
        if((ret = httpd_req_recv(req, buf, MIN(rcvBytes, sizeof(buf)-1))) <= 0)
        {
            if(ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return(ESP_FAIL);
        }
        buf[ret] = '\0';

        // Add to our string for tokenising.
        post = post + buf;

        // Update bytes still to arrive.
        rcvBytes -= ret;
    }

    // Split the POST into key pairs.
    std::vector<std::string> keys = this->split(post, "&");
    for(auto key : keys)
    {
        size_t pos     = key.find('=');
        keyValue.name  = key.substr(0, pos);
        keyValue.value = key.substr(pos + 1);
        pairs->push_back(keyValue);
    }

    // Successful extraction of the Key/Value pairs from the POST.
    return(ESP_OK);
}

// Method to process POST data specifically for the WiFi configuration. The key:value pairs are parsed, data extracted
// and validated. Any errors are sent back to the UI/Browser.
esp_err_t WiFi::wifiDataPOSTHandler(httpd_req_t *req, std::vector<t_kvPair> pairs, std::string& resp)
{
    // Locals.
    //
    bool                  dataError = false;

    // Retrieve pointer to object in order to access data.
    WiFi* pThis = (WiFi*)req->user_ctx;

    // Loop through all the URI key pairs, updating configuration values as necessary.
    for(auto pair : pairs)
    {
        //printf("%s->%s\n", pair.name.c_str(), pair.value.c_str());
        if(pair.name.compare("wifiMode") == 0)
        {
            if(pair.value.compare("ap") == 0)
            {
                pThis->wifiConfig.params.wifiMode = WIFI_CONFIG_AP;
                dataError = false;
            } else
            {
                pThis->wifiConfig.params.wifiMode = WIFI_CONFIG_CLIENT;
                dataError = false;
            }
        }
        if(pair.name.compare("clientSSID") == 0)
        {
            strncpy(pThis->wifiConfig.clientParams.ssid, pair.value.c_str(), MAX_WIFI_SSID_LEN);
        }
        if(pair.name.compare("apSSID") == 0)
        {
            strncpy(pThis->wifiConfig.apParams.ssid, pair.value.c_str(), MAX_WIFI_SSID_LEN);
        }
        if(pair.name.compare("clientPWD") == 0)
        {
            strncpy(pThis->wifiConfig.clientParams.pwd, pair.value.c_str(), MAX_WIFI_PWD_LEN);
        }
        if(pair.name.compare("apPWD") == 0)
        {
            strncpy(pThis->wifiConfig.apParams.pwd, pair.value.c_str(), MAX_WIFI_PWD_LEN);
        }
        if(pair.name.compare("dhcpMode") == 0)
        {
            if(pair.value.compare("on") == 0)
            {
                pThis->wifiConfig.clientParams.useDHCP = true;
            } else
            {
                pThis->wifiConfig.clientParams.useDHCP = false;
            }
        }
        if(pair.name.compare("clientIP") == 0)
        {
            strncpy(pThis->wifiConfig.clientParams.ip, pair.value.c_str(), MAX_WIFI_IP_LEN);
        }
        if(pair.name.compare("apIP") == 0)
        {
            strncpy(pThis->wifiConfig.apParams.ip, pair.value.c_str(), MAX_WIFI_IP_LEN);
        }
        if(pair.name.compare("clientNETMASK") == 0)
        {
            strncpy(pThis->wifiConfig.clientParams.netmask, pair.value.c_str(), MAX_WIFI_NETMASK_LEN);
        }
        if(pair.name.compare("apNETMASK") == 0)
        {
            strncpy(pThis->wifiConfig.apParams.netmask, pair.value.c_str(), MAX_WIFI_NETMASK_LEN);
        }
        if(pair.name.compare("clientGATEWAY") == 0)
        {
            // Gateway isnt mandatory in client mode.
            if(pair.value.size() > 0 && pThis->wifiConfig.params.wifiMode == WIFI_CONFIG_CLIENT)
            {
                strncpy(pThis->wifiConfig.clientParams.gateway, pair.value.c_str(), MAX_WIFI_GATEWAY_LEN);
            }
        }
        if(pair.name.compare("apGATEWAY") == 0)
        {
            // Access point mode, if no gateway is given, assign the IP address as the gateway.
            strncpy(pThis->wifiConfig.apParams.gateway, pThis->wifiConfig.apParams.ip, MAX_WIFI_GATEWAY_LEN+1);
        }
    }

    // Validate the data if no error was raised for individual fields.
    if(dataError == false)
    {
        if(pThis->wifiConfig.params.wifiMode == WIFI_CONFIG_AP)
        {
            if(strlen(pThis->wifiConfig.apParams.ssid) == 0)
            {
                resp = resp + (resp.size() > 0  ? "," : "");
                resp = resp + "SSID not given!";
                dataError = true;
            }
            if(strlen(pThis->wifiConfig.apParams.pwd) == 0)
            {
                resp = resp + (resp.size() > 0  ? "," : "");
                resp = resp + "Password not given!";
                dataError = true;
            }
            if(!pThis->validateIP(pThis->wifiConfig.apParams.ip))
            {
                resp = resp + (resp.size() > 0  ? "," : "");
                resp = resp + "Illegal AP IP address(" + pThis->wifiConfig.apParams.ip + ")";
                dataError = true;
            }
            if(!pThis->validateIP(pThis->wifiConfig.apParams.netmask))
            {
                resp = resp + (resp.size() > 0  ? "," : "");
                resp = resp + "Illegal AP Netmask address(" + pThis->wifiConfig.apParams.netmask + ")";
                dataError = true;
            }
            // Gateway isnt mandatory, but if filled in, validate it.
            if(strlen(pThis->wifiConfig.apParams.gateway) == 0 || !pThis->validateIP(pThis->wifiConfig.apParams.gateway))
            {
                resp = resp + (resp.size() > 0  ? "," : "");
                resp = resp + "Illegal AP Gateway address(" + pThis->wifiConfig.clientParams.gateway + ")";
                dataError = true;
            }
        }
        // Only verify client parameters when active.
        else if(pThis->wifiConfig.params.wifiMode == WIFI_CONFIG_CLIENT)
        {
            if(pThis->wifiConfig.clientParams.useDHCP == false)
            {
                if(strlen(pThis->wifiConfig.clientParams.ssid) == 0)
                {
                    resp = resp + (resp.size() > 0  ? "," : "");
                    resp = resp + "SSID not given!";
                    dataError = true;
                }
                if(strlen(pThis->wifiConfig.clientParams.pwd) == 0)
                {
                    resp = resp + (resp.size() > 0  ? "," : "");
                    resp = resp + "Password not given!";
                    dataError = true;
                }
                if(!pThis->validateIP(pThis->wifiConfig.clientParams.ip))
                {
                    resp = resp + (resp.size() > 0  ? "," : "");
                    resp = resp + "Illegal IP address(" + pThis->wifiConfig.clientParams.ip + ")";
                    dataError = true;
                }
                if(!pThis->validateIP(pThis->wifiConfig.clientParams.netmask))
                {
                    resp = resp + (resp.size() > 0  ? "," : "");
                    resp = resp + "Illegal Netmask address(" + pThis->wifiConfig.clientParams.netmask + ")";
                    dataError = true;
                }
                // Gateway isnt mandatory, but if filled in, validate it.
                if(strlen(pThis->wifiConfig.clientParams.gateway) > 0 && !pThis->validateIP(pThis->wifiConfig.clientParams.gateway))
                {
                    resp = resp + (resp.size() > 0  ? "," : "");
                    resp = resp + "Illegal Gateway address(" + pThis->wifiConfig.clientParams.gateway + ")";
                    dataError = true;
                }
            }
        } else
        {
            resp = resp + (resp.size() > 0  ? "," : "");
            resp = resp + "Unknown WiFi Mode (" + to_str(pThis->wifiConfig.params.wifiMode, 0, 10) + "), internal coding error, please contact support.";
            dataError = true;
        }
    }

    // No errors, save wifi configuration.
    if(dataError == false)
    {
        // Mark data as valid.
        pThis->wifiConfig.clientParams.valid      = true;
        if(pThis->nvs->persistData(pThis->wifiCtrl.run.thisClass.c_str(), &pThis->wifiConfig, sizeof(t_wifiConfig)) == false)
        {
            ESP_LOGI(WIFITAG, "Persisting SharpKey(%s) configuration data failed, updates will not persist in future power cycles.", pThis->wifiCtrl.run.thisClass.c_str());
            pThis->led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_10, 200, 1000L, 0L);
        } else
        // Few other updates so make a commit here to ensure data is flushed and written.
        if(pThis->nvs->commitData() == false)
        {
            ESP_LOGI(WIFITAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
            pThis->led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_10, 200, 500L, 0L);
        }
    }

    return(dataError == false ? ESP_OK : ESP_FAIL);
}

// Method to process POST data specifically for the Mouse interface. The key:value pairs are sent to the Mouse
// interface for parsing and storing, any errors are sent back to the UI/Browser.
esp_err_t WiFi::mouseDataPOSTHandler(httpd_req_t *req, std::vector<t_kvPair> pairs, std::string& resp)
{
    // Locals.
    //
    bool                  dataError = false;
    KeyInterface          *activeMouseIf = (mouseIf == NULL ? keyIf : mouseIf);

    // Run through pairs and send to the mouse interface to interpret.
    resp = "";
    for(auto pair : pairs)
    {
        // Call the Mouse configuration handler tp validate and set the parameter.
        dataError = activeMouseIf->setMouseConfigValue(pair.name, pair.value);
        if(dataError)
        {
            resp.append("Variable:" + pair.name + " has an invalid value:" + pair.value);
            dataError = false;
        }
    }

    // Update success status.
    if(resp.size() > 0) dataError = true;

    // Persist the values if no errors occurred.
    if(dataError == false)
    {
        dataError = activeMouseIf->persistConfig() ? false : true;
        if(dataError)
        {
            resp.append("Save config to NVS RAM failed, retry, if 2nd attempt fails, power cycle the interface.");
        }
    }

    return(dataError == false ? ESP_OK : ESP_FAIL);
}

// /data POST handler. Process the request and call service as required.
//
esp_err_t WiFi::defaultDataPOSTHandler(httpd_req_t *req)
{
    // Locals.
    //
    std::vector<t_kvPair> pairs;
    esp_err_t             ret = ESP_OK;
    std::string           resp = "";
    std::string           uriStr;

    // Retrieve pointer to object in order to access data.
    WiFi* pThis = (WiFi*)req->user_ctx;

    // Get the subpath from the URI.
    if(pThis->getPathFromURI(uriStr, "/data/", req->uri) == ESP_FAIL)
    {
        // Respond with 500 Internal Server Error.
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to extract URI");
        return(ESP_FAIL);
    }

    // Process the POST into key/value pairs.
    ret = pThis->getPOSTData(req, &pairs);
    if(ret == ESP_OK)
    {
        if(uriStr.compare("wifi") == 0)
        {
            if((ret = pThis->wifiDataPOSTHandler(req, pairs, resp)) == ESP_OK)
            {
                // Success so add reboot button.
                pThis->wifiCtrl.run.rebootButton = true;
                resp = "Data values accepted. Press 'Reboot' to initiate network connection with the new configuration.";
            }
        }
        if(uriStr.compare("mouse") == 0)
        {
            if((ret = pThis->mouseDataPOSTHandler(req, pairs, resp)) == ESP_OK)
            {
                // Success so indicate all ok.
                pThis->wifiCtrl.run.rebootButton = true;
                resp = "Data values accepted. Press 'Reboot' to restart interface with new values.";
            }
        }
    } else
    {
        resp = "<p>No values in POST, check browser!</p>";
    }

    // Add in an error message if one has been generated.
    pThis->wifiCtrl.run.errorMsg = "<font size=\"2\" face=\"verdana\" color=\"";
    pThis->wifiCtrl.run.errorMsg += (ret == ESP_OK ? "green" : "red");
    pThis->wifiCtrl.run.errorMsg += "\">" + resp + "</font>";

    // Send message directly, it will appear as an error or success message.
    httpd_resp_send_chunk(req, pThis->wifiCtrl.run.errorMsg.c_str(), pThis->wifiCtrl.run.errorMsg.size()+1);

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return(ret);
}

// /reboot POST handler.
// Simple handler, send a message indicating reboot taking place with a reload URL statement.
//
esp_err_t WiFi::defaultRebootHandler(httpd_req_t *req)
{
    // Locals.
    //
    esp_err_t             ret = ESP_OK;
    std::string           resp = "";

    // Retrieve pointer to object in order to access data.
    WiFi* pThis = (WiFi*)req->user_ctx;

    // Build a response message.
    if(pThis->wifiConfig.clientParams.useDHCP == false)
    {
        resp  = "<head> <meta http-equiv=\"refresh\" content=\"10; URL=http://";
        resp += pThis->wifiConfig.clientParams.ip;
        resp += "/\" /> </head><body><font size=\"5\" face=\"verdana\" color=\"red\"/>Rebooting... </font><font size=\"5\" face=\"verdana\" color=\"black\"/>Please wait.</font></body>";
    } else
    {
        resp  = "<head> </head><body><font size=\"5\" face=\"verdana\" color=\"red\"/>Rebooting... </font><br><font size=\"4\" face=\"verdana\" color=\"black\"/><p>Please look in your router admin panel for the assigned IP address and enter http://&lt;router assigned ip address&gt; into browser to continue.</p></font></body>";
    }

    // Send the response and wait a while, then request reboot.
    httpd_resp_send(req, resp.c_str(), resp.size()+1);
    vTaskDelay(100);
    pThis->wifiCtrl.run.reboot = true;
    
    // Get out, a reboot will occur very soon.
    return(ret);
}

// Method to start the basic HTTP webserver.
//
bool WiFi::startWebserver(void)
{
    // Locals.
    //
    bool           result = false;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Tweak default settings.
    config.stack_size = 10240;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 12;

    // Setup the required paths and descriptors then register them with the server.
    const httpd_uri_t dataPOST = {
        .uri       = "/data",
        .method    = HTTP_POST,
        .handler   = defaultDataPOSTHandler,
        .user_ctx  = this
    };
    const httpd_uri_t dataSubPOST = {
        .uri       = "/data/*",
        .method    = HTTP_POST,
        .handler   = defaultDataPOSTHandler,
        .user_ctx  = this
    };
    const httpd_uri_t dataGET = {
        .uri       = "/data",
        .method    = HTTP_GET,
        .handler   = defaultDataGETHandler,
        .user_ctx  = this
    };
    const httpd_uri_t dataSubGET = {
        .uri       = "/data/*",
        .method    = HTTP_GET,
        .handler   = defaultDataGETHandler,
        .user_ctx  = this
    };
    const httpd_uri_t keymapTablePOST = {
        .uri       = "/keymap/table",
        .method    = HTTP_POST,
        .handler   = keymapTablePOSTHandler,
        .user_ctx  = this
    };
    const httpd_uri_t keymap = {
        .uri       = "/keymap",
        .method    = HTTP_POST,
        .handler   = keymapUploadPOSTHandler,
        .user_ctx  = this
    };
    const httpd_uri_t otafw = {
        .uri       = "/ota/firmware",
        .method    = HTTP_POST,
        .handler   = otaFirmwareUpdatePOSTHandler,
        .user_ctx  = this
    };
    const httpd_uri_t otafp = {
        .uri       = "/ota/filepack",
        .method    = HTTP_POST,
        .handler   = otaFilepackUpdatePOSTHandler,
        .user_ctx  = this
    };
    const httpd_uri_t rebootPOST = {
        .uri       = "/reboot",
        .method    = HTTP_POST,
        .handler   = defaultRebootHandler,
        .user_ctx  = this
    };
    const httpd_uri_t rebootGET = {
        .uri       = "/reboot",
        .method    = HTTP_GET,
        .handler   = defaultRebootHandler,
        .user_ctx  = this
    };
    const httpd_uri_t root = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = defaultFileHandler,
        .user_ctx  = this
    };
    // Catch all, assume files if no handler setup.
    const httpd_uri_t files = {
        .uri       = "/*",
        .method    = HTTP_GET,
        .handler   = defaultFileHandler,
        .user_ctx  = this
    };

    // Store the file system basepath on t
    strlcpy(this->wifiCtrl.run.basePath, this->wifiCtrl.run.fsPath, sizeof(this->wifiCtrl.run.basePath));

    // Start the web server.
    ESP_LOGI(WIFITAG, "Starting server on port: '%d'", config.server_port);

    if (httpd_start(&wifiCtrl.run.server, &config) == ESP_OK) 
    {
        // Set URI handlers
        ESP_LOGI(WIFITAG, "Registering URI handlers");

        // Root directory handler. Equivalent to index.html/index.htm. The method, based on the current mode (AP/Client) decides on which
        // file to serve.
        httpd_register_uri_handler(wifiCtrl.run.server, &root);

        // POST handlers.
        httpd_register_uri_handler(wifiCtrl.run.server, &dataSubPOST);
        httpd_register_uri_handler(wifiCtrl.run.server, &dataPOST);
        httpd_register_uri_handler(wifiCtrl.run.server, &dataSubGET);
        httpd_register_uri_handler(wifiCtrl.run.server, &dataGET);
        httpd_register_uri_handler(wifiCtrl.run.server, &keymapTablePOST);
        httpd_register_uri_handler(wifiCtrl.run.server, &keymap);
        httpd_register_uri_handler(wifiCtrl.run.server, &otafw);
        httpd_register_uri_handler(wifiCtrl.run.server, &otafp);
        httpd_register_uri_handler(wifiCtrl.run.server, &rebootPOST);
        httpd_register_uri_handler(wifiCtrl.run.server, &rebootGET);

        // If no URL matches then default to serving files.
        httpd_register_uri_handler(wifiCtrl.run.server, &files);
        result = true;
    }

    // Return result of startup.
    return(result);
}

// Method to stop the basic HTTP webserver.
//
void WiFi::stopWebserver(void)
{
    // Stop the web server and set the handle to NULL to indicate state.
    httpd_stop(wifiCtrl.run.server);
    wifiCtrl.run.server = NULL;
    return;
}

// Event handler for Client mode Wifi event callback.
//
IRAM_ATTR void WiFi::wifiClientHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    // Locals.
    //
   
    // Retrieve pointer to object in order to access data.
    WiFi* pThis = (WiFi*)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
        if(pThis->wifiCtrl.client.clientRetryCnt < CONFIG_IF_WIFI_MAX_RETRIES)
        {
            esp_wifi_connect();
            pThis->wifiCtrl.client.clientRetryCnt++;
            ESP_LOGI(WIFITAG, "retry to connect to the AP");
        } else 
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WIFITAG,"connect to the AP fail");
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

        // Copy details into control structure for ease of use in rendering pages.
        strncpy(pThis->wifiCtrl.ap.ssid,        pThis->wifiConfig.clientParams.ssid,    MAX_WIFI_SSID_LEN+1);
        strncpy(pThis->wifiCtrl.ap.pwd,         pThis->wifiConfig.clientParams.pwd,     MAX_WIFI_PWD_LEN+1);
        sprintf(pThis->wifiCtrl.client.ip,      IPSTR, IP2STR(&event->ip_info.ip));
        sprintf(pThis->wifiCtrl.client.netmask, IPSTR, IP2STR(&event->ip_info.netmask));
        sprintf(pThis->wifiCtrl.client.gateway, IPSTR, IP2STR(&event->ip_info.gw));
        pThis->wifiCtrl.client.connected = true;
        pThis->wifiCtrl.client.clientRetryCnt = 0;

        ESP_LOGI(WIFITAG, "got ip:" IPSTR " Netmask:" IPSTR " Gateway:" IPSTR, IP2STR(&event->ip_info.ip), IP2STR(&event->ip_info.netmask), IP2STR(&event->ip_info.gw));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        // Start the webserver if it hasnt been configured.
        if(pThis->wifiCtrl.run.server == NULL)
        {
            pThis->startWebserver();
        }
    }
    return;
}

// Event handler for Access Point mode Wifi event callback.
//
IRAM_ATTR void WiFi::wifiAPHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    // Locals.
    //

    // Retrieve pointer to object in order to access data.
    WiFi* pThis = (WiFi*)arg;

    if (event_id == WIFI_EVENT_AP_STACONNECTED) 
    {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(WIFITAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);

        // Start the webserver if it hasnt been configured.
        if(pThis->wifiCtrl.run.server == NULL)
        {
            pThis->startWebserver();
        }
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(WIFITAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
    return;
}

// Method to initialise the interface as a client to a known network, the SSID
// and password have already been setup.
//
bool WiFi::setupWifiClient(void)
{
    // Locals.
    //
    wifi_init_config_t           wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
    esp_netif_t                  *netConfig;
    esp_netif_ip_info_t          ipInfo;
    esp_event_handler_instance_t instID;
    esp_event_handler_instance_t instIP;
    EventBits_t                  bits;
    wifi_config_t                wifiConfig = { .sta =  {
                                     /* ssid            */ {},
                                     /* password        */ {},
                                     /* scan_method     */ {},
                                     /* bssid_set       */ {},
                                     /* bssid           */ {},
                                     /* channel         */ {},
                                     /* listen_interval */ {},
                                     /* sort_method     */ {},
                                     /* threshold       */ {
                                     /* rssi            */     {},
                                     /* authmode        */     WIFI_AUTH_WPA2_PSK
                                                           },
                                     /* pmf_cfg         */ {
                                     /* capable         */     true,
                                     /* required        */     false
                                                           },
                                     /* rm_enabled      */ {},
                                     /* btm_enabled     */ {},
                                     /* mbo_enabled     */ {}, // For IDF 4.4 and higher
                                     /* reserved        */ {}
                                                        }
                                              };

    // Add in configured SSID/Password parameters.
    strncpy((char *)wifiConfig.sta.ssid, this->wifiConfig.clientParams.ssid, MAX_WIFI_SSID_LEN+1);
    strncpy((char *)wifiConfig.sta.password, this->wifiConfig.clientParams.pwd, MAX_WIFI_PWD_LEN+1);

    //nvs_handle_t net80211_handle;
    //nvs_open("nvs.net80211", NVS_READWRITE, &net80211_handle);
    //nvs_erase_all(net80211_handle);
    //nvs_commit(net80211_handle);
    //nvs_close(net80211_handle);

    // Initialise control structure.
    //
    wifiCtrl.client.connected = false;
    wifiCtrl.client.ip[0] = '\0';
    wifiCtrl.client.netmask[0] = '\0';
    wifiCtrl.client.gateway[0] = '\0';
   
    // Create an event handler group to manage callbacks.
    s_wifi_event_group = xEventGroupCreate();

    // Setup the network interface.
    if(esp_netif_init())
    {
        ESP_LOGI(WIFITAG, "Couldnt initialise netif, disabling WiFi.");
        return(false);
    }

    // Setup the event loop.
    if(esp_event_loop_create_default())
    {
        ESP_LOGI(WIFITAG, "Couldnt initialise event loop, disabling WiFi.");
        return(false);
    }

    // Setup the wifi client (station).
    netConfig = esp_netif_create_default_wifi_sta();
    // If fixed IP is configured, set it up.
    if(!this->wifiConfig.clientParams.useDHCP)
    {
        int a, b, c, d;
        esp_netif_dhcpc_stop(netConfig);
        if(!splitIP(this->wifiConfig.clientParams.ip, &a, &b, &c, &d))
        {
            ESP_LOGI(WIFITAG, "Client IP invalid:%s", this->wifiConfig.clientParams.ip);
            return false;
        }
        IP4_ADDR(&ipInfo.ip, a, b, c, d);

        if(!splitIP(this->wifiConfig.clientParams.netmask, &a, &b, &c, &d))
        {
            ESP_LOGI(WIFITAG, "Client NETMASK invalid:%s", this->wifiConfig.clientParams.netmask);
            return false;
        }
        IP4_ADDR(&ipInfo.netmask, a, b, c, d);

        if(!splitIP(this->wifiConfig.clientParams.gateway, &a, &b, &c, &d))
        {
            ESP_LOGI(WIFITAG, "Client GATEWAY invalid:%s", this->wifiConfig.clientParams.gateway);
            return false;
        }
        IP4_ADDR(&ipInfo.gw, a, b, c, d);
        esp_netif_set_ip_info(netConfig, &ipInfo);
    }
    
    // Set TX power to max.
    esp_wifi_set_max_tx_power(127);

    // Setup the config for wifi.
    wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
    if(esp_wifi_init(&wifiInitConfig))
    {
        ESP_LOGI(WIFITAG, "Couldnt initialise wifi with default parameters, disabling WiFi.");
        return(false);
    }

    // Register event handlers.
    if(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiClientHandler, this, &instID))
    {
        ESP_LOGI(WIFITAG, "Couldnt register event handler for ID, disabling WiFi.");
        return(false);
    }
    if(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiClientHandler, this, &instIP)) 
    {
        ESP_LOGI(WIFITAG, "Couldnt register event handler for IP, disabling WiFi.");
        return(false);
    }
    if(esp_wifi_set_mode(WIFI_MODE_STA))
    {
        ESP_LOGI(WIFITAG, "Couldnt set Wifi mode to Client, disabling WiFi.");
        return(false);
    }
    if(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig))
    {
        ESP_LOGI(WIFITAG, "Couldnt configure client mode, disabling WiFi.");
        return(false);
    }
    if(esp_wifi_start())
    {
        ESP_LOGI(WIFITAG, "Couldnt start Client session, disabling WiFi.");
        return(false);
    }

    // Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
    // number of re-tries (WIFI_FAIL_BIT). The bits are set by wifiClientHandler() (see above)
    bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    // xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
    // happened.
    if(bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(WIFITAG, "Connected: SSID:%s password:%s", this->wifiConfig.clientParams.ssid, this->wifiConfig.clientParams.pwd);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(WIFITAG, "Connection Fail: SSID:%s, password:%s.", this->wifiConfig.clientParams.ssid, this->wifiConfig.clientParams.pwd);
        return(false);
    } 
    else
    {
        ESP_LOGE(WIFITAG, "Unknown evemt, bits:%d", bits);
        return(false);
    }

    // No errors.
    return(true);
}

// Method to initialise the interface as a Soft Access point with a given SSID
// and password.
// The Access Point mode is basically to bootstrap a Client connection where the 
// client connecting provides the credentials in order to connect as a client to 
// another AP to join a local network.
//
bool WiFi::setupWifiAP(void)
{
    // Locals.
    //
    esp_err_t                    retcode;
    wifi_init_config_t           wifiInitConfig;
    esp_netif_t                 *wifiAP;
    esp_netif_ip_info_t          ipInfo;
    wifi_config_t                wifiConfig = { .ap =  {
                                     /* ssid            */ CONFIG_IF_WIFI_SSID,
                                     /* password        */ CONFIG_IF_WIFI_DEFAULT_SSID_PWD,
                                     /* ssid_len        */ strlen(CONFIG_IF_WIFI_SSID),
                                     /* channel         */ CONFIG_IF_WIFI_AP_CHANNEL,
                                     /* authmode        */ WIFI_AUTH_WPA_WPA2_PSK,
                                     /* hidden          */ CONFIG_IF_WIFI_SSID_HIDDEN,
                                     /* max_connection  */ CONFIG_IF_WIFI_MAX_CONNECTIONS,
                                     /* beacon_interval */ 100,
                                     /* pairwise_cipher */ WIFI_CIPHER_TYPE_TKIP,
                                     /* ftm_responder   */ 0,
                                  // /* pmf_cfg         */ {
                                  // /* capable         */     true,
                                  // /* required        */     false
                                  //                       }
                                                       }
                                              };

    // Intialise the network interface.
    if(esp_netif_init())
    {
        ESP_LOGI(WIFITAG, "Couldnt initialise network interface, disabling WiFi.");
        return(false);
    }
    if((retcode = esp_event_loop_create_default()))
    {
        ESP_LOGI(WIFITAG, "Couldnt create default loop(%d), disabling WiFi.", retcode);
        return(false);
    }

    // Create the default Access Point.
    //
    wifiAP = esp_netif_create_default_wifi_ap();
 
    // Setup the base parameters of the Access Point which may differ from ESP32 defaults.
    int a, b, c, d;
    if(!splitIP(this->wifiConfig.apParams.ip, &a, &b, &c, &d))
    {
        ESP_LOGI(WIFITAG, "AP IP invalid:%s", this->wifiConfig.apParams.ip);
        return false;
    }
    IP4_ADDR(&ipInfo.ip, a, b, c, d);

    if(!splitIP(this->wifiConfig.apParams.netmask, &a, &b, &c, &d))
    {
        ESP_LOGI(WIFITAG, "AP NETMASK invalid:%s", this->wifiConfig.apParams.netmask);
        return false;
    }
    IP4_ADDR(&ipInfo.netmask, a, b, c, d);

    if(!splitIP(this->wifiConfig.apParams.gateway, &a, &b, &c, &d))
    {
        ESP_LOGI(WIFITAG, "AP GATEWAY invalid:%s", this->wifiConfig.apParams.gateway);
        return false;
    }
    IP4_ADDR(&ipInfo.gw, a, b, c, d);

    // Update the SSID/Password from NVS.
    strncpy((char *)wifiConfig.ap.ssid,     this->wifiConfig.apParams.ssid, MAX_WIFI_SSID_LEN+1);
    strncpy((char *)wifiConfig.ap.password, this->wifiConfig.apParams.pwd,  MAX_WIFI_PWD_LEN+1);
    wifiConfig.ap.ssid_len = (uint8_t)strlen(this->wifiConfig.apParams.ssid);

    // Copy the configured params into the runtime params, just in case they change prior to next boot.
    // (this) used for clarity as wifi config local have similar names to global persistence names.
    strncpy(this->wifiCtrl.ap.ssid,       this->wifiConfig.apParams.ssid,    MAX_WIFI_SSID_LEN+1);
    strncpy(this->wifiCtrl.ap.pwd,        this->wifiConfig.apParams.pwd,     MAX_WIFI_PWD_LEN+1);
    strncpy(this->wifiCtrl.ap.ip,         this->wifiConfig.apParams.ip,      MAX_WIFI_IP_LEN+1);
    strncpy(this->wifiCtrl.ap.netmask,    this->wifiConfig.apParams.netmask, MAX_WIFI_NETMASK_LEN+1);
    strncpy(this->wifiCtrl.ap.gateway,    this->wifiConfig.apParams.gateway, MAX_WIFI_GATEWAY_LEN+1);

    // Reconfigure the DHCP Server.
	esp_netif_dhcps_stop(wifiAP);
	esp_netif_set_ip_info(wifiAP, &ipInfo);
	esp_netif_dhcps_start(wifiAP);    
   
    // Set TX power to max.
    esp_wifi_set_max_tx_power(127);

    // Initialise AP with default parameters.
    wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
    if(esp_wifi_init(&wifiInitConfig))
    {
        ESP_LOGI(WIFITAG, "Couldnt setup AP with default parameters, disabling WiFi.");
        return(false);
    }
   
    // Setup callback handlers for wifi events.
    if(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiAPHandler, this, NULL))
    {
        ESP_LOGI(WIFITAG, "Couldnt setup event handlers, disabling WiFi.");
        return(false);
    }

    // If there is no password for the access point set authentication to open.
    if (strlen(CONFIG_IF_WIFI_DEFAULT_SSID_PWD) == 0) 
    {
        wifiConfig.ap.authmode = WIFI_AUTH_OPEN;
    }

    // Setup as an Access Point.
    if(esp_wifi_set_mode(WIFI_MODE_AP))
    {
        ESP_LOGI(WIFITAG, "Couldnt set mode to Access Point, disabling WiFi.");
        return(false);
    }
    // Configure the Access Point
    if(esp_wifi_set_config(WIFI_IF_AP, &wifiConfig))
    {
        ESP_LOGI(WIFITAG, "Couldnt configure Access Point, disabling WiFi.");
        return(false);
    }
    // Start the Access Point.
    if(esp_wifi_start())
    {
        ESP_LOGI(WIFITAG, "Couldnt start Access Point session, disabling WiFi.");
        return(false);
    }

    // No errors.
    return(true);
}

// Method to disable the wifi turning the transceiver off.
//
bool WiFi::stopWifi(void)
{
    if(esp_wifi_stop())
    {
        ESP_LOGI(WIFITAG, "Couldnt stop the WiFi, reboot needed.");
        return(false);
    }
    if(esp_wifi_deinit())
    {
        ESP_LOGI(WIFITAG, "Couldnt deactivate WiFi, reboot needed.");
        return(false);
    }

    // No errors.
    return(true);
}

// WiFi interface runtime logic. This method provides a browser interface to the SharpKey for status query and configuration.
//
void WiFi::run(void)
{
    // Locals.
    #define           WIFIIFTAG       "wifiRun"

    // If Access Point mode has been forced, set the config parameter to AP so that Access Point mode is entered regardless of NVS setting.
    if(wifiCtrl.run.wifiMode == WIFI_CONFIG_AP)
    {
        wifiConfig.params.wifiMode = WIFI_CONFIG_AP;

        // Reset the configured addresses, SSID and password of the Access Point to factory default. This ensures known connection data.
        strncpy(wifiConfig.apParams.ssid,    CONFIG_IF_WIFI_SSID,              MAX_WIFI_SSID_LEN);
        strncpy(wifiConfig.apParams.pwd,     CONFIG_IF_WIFI_DEFAULT_SSID_PWD,  MAX_WIFI_PWD_LEN);
        strncpy(wifiConfig.apParams.ip,      WIFI_AP_DEFAULT_IP,               MAX_WIFI_IP_LEN);
        strncpy(wifiConfig.apParams.netmask, WIFI_AP_DEFAULT_NETMASK,          MAX_WIFI_NETMASK_LEN);
        strncpy(wifiConfig.apParams.gateway, WIFI_AP_DEFAULT_GW,               MAX_WIFI_GATEWAY_LEN);
    }

    // Enable Access Point mode if configured.
    if(wifiConfig.params.wifiMode == WIFI_CONFIG_AP)
    {
        if(!setupWifiAP())
        {
            wifiCtrl.run.reboot     = true;
        } else
        {
            wifiCtrl.run.wifiMode = wifiConfig.params.wifiMode;
            
            // Flash LED to indicate access point mode is active.
            //
            led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_10, 5, 10000L, 500L);
        }
    } else
    {
        // Setup as a client for general browser connectivity.
        if(!setupWifiClient())
        {
            wifiCtrl.run.reboot     = true;
        } else
        {
            wifiCtrl.run.wifiMode = wifiConfig.params.wifiMode;

            // Flash LED to indicate client mode is active.
            //
            led->setLEDMode(LED::LED_MODE_BLINK_ONESHOT, LED::LED_DUTY_CYCLE_50, 5, 10000L, 500L);
        }
    }

    // Enter a loop, only exitting if a reboot is required.
    do {
        // Let other tasks run. NB. This value affects the debounce counter, update as necessary.
        vTaskDelay(500);
    } while(wifiCtrl.run.reboot == false);

    return;
}

// Constructor. No overloading methods.
WiFi::WiFi(KeyInterface *hdlKeyIf, KeyInterface *hdlMouseIf, bool defaultMode, NVS *nvs, LED *led, const char *fsPath, t_versionList *versionList)
{
    // Initialise variables.
    //
    wifiCtrl.client.clientRetryCnt = 0;
    wifiCtrl.run.server            = NULL;
    wifiCtrl.run.errorMsg          = "";
    wifiCtrl.run.rebootButton      = false;
    wifiCtrl.run.reboot            = false;
    wifiCtrl.run.wifiMode          = (defaultMode == true ? WIFI_CONFIG_AP : WIFI_ON);

    // The Non Volatile Storage object is bound to this object for storage and retrieval of configuration data.
    this->nvs = nvs;

    // The LED activity indicator object.
    this->led = led;

    // Setup the default path on the underlying filesystem.
    this->wifiCtrl.run.fsPath = fsPath;
  
    // Store the version list, used in html variable expansion for version number reporting.
    this->wifiCtrl.run.versionList = versionList;

    // Store the classname, used for NVS keys.
    this->wifiCtrl.run.thisClass = keyIf->getClassName(__PRETTY_FUNCTION__);

    // Retrieve configuration, if it doesnt exist, set defaults.
    //
    if(nvs->retrieveData(this->wifiCtrl.run.thisClass.c_str(), &this->wifiConfig, sizeof(t_wifiConfig)) == false)
    {
        ESP_LOGI(WIFITAG, "Wifi configuration set to default, no valid config in NVS found.");
        // Empty set for the client parameters until configured.
        wifiConfig.clientParams.valid       = false;
        wifiConfig.clientParams.ssid[0]     = '\0';
        wifiConfig.clientParams.pwd[0]      = '\0';
        wifiConfig.clientParams.ip[0]       = '\0';
        wifiConfig.clientParams.netmask[0]  = '\0';
        wifiConfig.clientParams.gateway[0]  = '\0';
        strncpy(wifiConfig.apParams.ssid,    CONFIG_IF_WIFI_SSID,              MAX_WIFI_SSID_LEN);
        strncpy(wifiConfig.apParams.pwd,     CONFIG_IF_WIFI_DEFAULT_SSID_PWD,  MAX_WIFI_PWD_LEN);
        strncpy(wifiConfig.apParams.ip,      WIFI_AP_DEFAULT_IP,               MAX_WIFI_IP_LEN);
        strncpy(wifiConfig.apParams.netmask, WIFI_AP_DEFAULT_NETMASK,          MAX_WIFI_NETMASK_LEN);
        strncpy(wifiConfig.apParams.gateway, WIFI_AP_DEFAULT_GW,               MAX_WIFI_GATEWAY_LEN);
        wifiConfig.params.wifiMode          = WIFI_CONFIG_AP;

        // Persist the data for next time.
        if(nvs->persistData(wifiCtrl.run.thisClass.c_str(), &this->wifiConfig, sizeof(t_wifiConfig)) == false)
        {
            ESP_LOGI(WIFITAG, "Persisting Default Wifi configuration data failed, check NVS setup.");
        }
        // Commit data, ensuring values are written to NVS and the mutex is released.
        else if(nvs->commitData() == false)
        {
            ESP_LOGI(WIFITAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
        }
    }

    // The interface objects are bound to this object so that configuration and rendering of web pages can take place. As the KeyInterface class knows about its 
    // data set and configuration requirements it is the only object which can render web pages for it.
    //
    keyIf   = NULL;
    mouseIf = NULL;
    if(hdlKeyIf != NULL && hdlMouseIf == NULL)
    {
        this->keyIf = hdlKeyIf;
    }
    else if(hdlKeyIf == NULL && hdlMouseIf != NULL)
    {
        this->keyIf = mouseIf;
    }
    else if(hdlKeyIf != NULL && hdlMouseIf != NULL)
    {
        this->keyIf   = hdlKeyIf;
        this->mouseIf = hdlMouseIf;
    }
}

// Constructor, used for version reporting so no hardware is initialised.
WiFi::WiFi(void)
{
    return;
}

// Destructor - only ever called when the class is used for version reporting.
WiFi::~WiFi(void)
{
    return;
}

// End of compile time enabled build of the WiFi module.
#endif
