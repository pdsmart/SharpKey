/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            WiFi.h
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Header for the WiFi AP/Client logic.
// Credits:         
// Copyright:       (c) 2019-2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Mar 2022 - Initial write.
//            v1.01 May 2022 - Initial release version.
//            v1.02 Jun 2022 - Seperated out the WiFi Enable switch and made the WiFi module active/
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

#ifndef WIFI_H
#define WIFI_H

#if defined(CONFIG_IF_WIFI_ENABLED)
  #include "freertos/event_groups.h"
  #include "esp_system.h"
  #include "esp_wifi.h"
  #include "esp_event.h"
  #include "nvs_flash.h"
  #include "lwip/err.h"
  #include "lwip/sys.h"
  #include <esp_http_server.h>
  #include "esp_littlefs.h"
  #include <iostream>
  #include <sstream>
  #include <vector>
  #include <arpa/inet.h>
  #include "NVS.h"
  #include "LED.h"
  #include "HID.h"

  // Include the specification class.
  #include "KeyInterface.h"

  // Encapsulate the WiFi functionality.
  class WiFi {
      // Constants.
      #define WIFI_VERSION                    1.02
      #define OBJECT_VERSION_LIST_MAX         18
      #define FILEPACK_VERSION_FILE           "version.txt"
      #define WIFI_AP_DEFAULT_IP              "192.168.4.1"
      #define WIFI_AP_DEFAULT_GW              "192.168.4.1"
      #define WIFI_AP_DEFAULT_NETMASK         "255.255.255.0"
  
      // The event group allows multiple bits for each event, but we only care about two events:
      // - we are connected to the AP with an IP
      // - we failed to connect after the maximum amount of retries 
      #define WIFI_CONNECTED_BIT              BIT0
      #define WIFI_FAIL_BIT                   BIT1
    
      // Tag for ESP WiFi logging.
      #define                                 WIFITAG  "WiFi"
    
      // Menu selection types.
      enum WIFIMODES {
          WIFI_OFF                          = 0x00,                             // WiFi is disabled.
          WIFI_ON                           = 0x01,                             // WiFi is enabled.
          WIFI_CONFIG_AP                    = 0x02,                             // WiFi is set to enable Access Point to configure WiFi settings.
          WIFI_CONFIG_CLIENT                = 0x03                              // WiFi is set to enable Client mode using persisted settings.
      };
    
      // Default WiFi parameters.
      #define MAX_WIFI_SSID_LEN               31
      #define MAX_WIFI_PWD_LEN                63
      #define MAX_WIFI_IP_LEN                 15
      #define MAX_WIFI_NETMASK_LEN            15
      #define MAX_WIFI_GATEWAY_LEN            15
    
      // Buffer size for sending file data in chunks to the browser.
      #define MAX_CHUNK_SIZE                  4096
    
      // Max length a file path can have on the embedded storage device.
      #define FILE_PATH_MAX                   (15 + CONFIG_LITTLEFS_OBJ_NAME_LEN)

      public:
          // Types for holding and maintaining a class/object to version number array.
          typedef struct {
              std::string                 object;
              float                       version;
          } t_versionItem;
          typedef struct {
              int                         elements;
              t_versionItem              *item[OBJECT_VERSION_LIST_MAX];
          } t_versionList;

          // Prototypes.
                                          WiFi(KeyInterface *hdlKeyIf, KeyInterface *hdlMouseIf, bool defaultMode, NVS *nvs, LED *led, const char *fsPath, t_versionList *versionList);
                                          WiFi(void);
                                         ~WiFi(void);
          void                            run(void);

          // Primary encapsulated interface object handle.
          KeyInterface                   *keyIf;
         
          // Secondary encapsulated interface object handle.
          KeyInterface                   *mouseIf;
         
          // Non Volatile Storage handle.
          NVS                            *nvs;
         
          // LED activity handle.
          LED                            *led;

          // Method to return the class version number.
          float version(void)
          {
              return(WIFI_VERSION);
          }
  
      protected:
  
      private:

          // Type for key:value pairs.
          typedef struct {
              std::string name;
              std::string value;             
          } t_kvPair;

          // Structure to maintain wifi configuration data. This data is persisted through powercycles as needed.
          typedef struct {
              // Client access parameters, these, when valid, are used for binding to a known wifi access point.
              struct {
                  bool                    valid;
                  char                    ssid[MAX_WIFI_SSID_LEN+1];
                  char                    pwd[MAX_WIFI_PWD_LEN+1];
                  bool                    useDHCP;
                  char                    ip[MAX_WIFI_IP_LEN+1];
                  char                    netmask[MAX_WIFI_NETMASK_LEN+1];
                  char                    gateway[MAX_WIFI_GATEWAY_LEN+1];
              } clientParams;

              // Structure to maintain Access Point parameters. These are configurable to allow possibility of changing them.
              struct {
                  char                    ssid[MAX_WIFI_SSID_LEN+1];
                  char                    pwd[MAX_WIFI_PWD_LEN+1];
                  char                    ip[MAX_WIFI_IP_LEN+1];
                  char                    netmask[MAX_WIFI_NETMASK_LEN+1];
                  char                    gateway[MAX_WIFI_GATEWAY_LEN+1];
              } apParams;

              // General runtime control parameters.
              struct {
                  // Configured mode of the Wifi: Access Point or Client.
                  enum WIFIMODES          wifiMode;
              } params;
          } t_wifiConfig;

          // Configuration data.
          t_wifiConfig                    wifiConfig;

          // Structure to manage the WiFi control variables, signifying the state of the Client or Access Point, runtime dependent, and
          // necessary dedicated run variables (as opposed to locals).
          typedef struct {
              // Client mode variables, active when in client mode.
              struct {
                  int                     clientRetryCnt;
                  bool                    connected;
                  char                    ssid[MAX_WIFI_SSID_LEN+1];
                  char                    pwd[MAX_WIFI_PWD_LEN+1];
                  char                    ip[MAX_WIFI_IP_LEN+1];
                  char                    netmask[MAX_WIFI_NETMASK_LEN+1];
                  char                    gateway[MAX_WIFI_GATEWAY_LEN+1];
              } client;

              // Access Point mode variabls, active when in AP mode.
              struct {
                  char                    ssid[MAX_WIFI_SSID_LEN+1];
                  char                    pwd[MAX_WIFI_PWD_LEN+1];
                  char                    ip[MAX_WIFI_IP_LEN+1];
                  char                    netmask[MAX_WIFI_NETMASK_LEN+1];
                  char                    gateway[MAX_WIFI_GATEWAY_LEN+1];
              } ap;

              // HTTP session variables, parsed out of incoming connections. The sessions are synchronous so only maintain
              // one copy.
              struct {
                  std::string             host;
                  std::string             queryStr;
                  std::string             fileName;
                  std::string             filePath;
                  bool                    gzip;
                  bool                    deflate;
              } session;

              // Runtime variables, used for global control of the WiFi module.
              //
              struct {
                  // Default path on the underlying filesystem. This is where the NVS/SD partition is mounted and all files under this directory are accessible.
                  const char *            fsPath;
                  
                  // Version list of all objects used to build the SharpKey interface along with their version numbers.
                  t_versionList          *versionList;

                  // Run mode of the Wifi: Off, On or Access Point.
                  enum WIFIMODES          wifiMode;
                 
                  // Handle to http server.
                  httpd_handle_t          server;

                  // Class name, used for NVS keys.
                  std::string             thisClass;

                  // Flag to raise a reboot button on the displayed page.
                  bool                    rebootButton;

                  // Flag to indicate a hard reboot needed.
                  bool                    reboot;

                  // Base path of file storag.
                  char                    basePath[FILE_PATH_MAX];

                  // String to hold any response error message.
                  std::string             errorMsg;
              } run;
          } t_wifiControl;
         
          // Control data.
          t_wifiControl                   wifiCtrl;

          // Prototypes.
                    bool                  setupWifiClient(void);
                    bool                  setupWifiAP(void);
                    bool                  stopWifi(void);
                    bool                  startWebserver(void);
                    void                  stopWebserver(void);
                    float                 getVersionNumber(std::string name);
                    esp_err_t             expandAndSendFile(httpd_req_t *req, const char *basePath, std::string fileName);
                    esp_err_t             expandVarsAndSend(httpd_req_t *req, std::string str);
                    esp_err_t             sendKeyMapHeaders(httpd_req_t *req);
                    esp_err_t             sendKeyMapTypes(httpd_req_t *req);
                    esp_err_t             sendKeyMapCustomTypeFields(httpd_req_t *req);
                    esp_err_t             sendKeyMapData(httpd_req_t *req);
                    esp_err_t             sendKeyMapPopovers(httpd_req_t *req);
                    esp_err_t             sendMouseRadioChoice(httpd_req_t *req, const char *option);



                           esp_err_t      wifiDataPOSTHandler(httpd_req_t *req, std::vector<t_kvPair> pairs, std::string& resp);
                           esp_err_t      mouseDataPOSTHandler(httpd_req_t *req, std::vector<t_kvPair> pairs, std::string& resp);
                    static esp_err_t      defaultDataPOSTHandler(httpd_req_t *req);
                    static esp_err_t      defaultDataGETHandler(httpd_req_t *req);
          IRAM_ATTR static esp_err_t      otaFirmwareUpdatePOSTHandler(httpd_req_t *req);
          IRAM_ATTR static esp_err_t      otaFilepackUpdatePOSTHandler(httpd_req_t *req);
                    static esp_err_t      keymapUploadPOSTHandler(httpd_req_t *req);
                    static esp_err_t      keymapTablePOSTHandler(httpd_req_t *req);

                    static esp_err_t      defaultRebootHandler(httpd_req_t *req);
                    esp_err_t             getPOSTData(httpd_req_t *req, std::vector<t_kvPair> *pairs);

                    bool                  isFileExt(std::string fileName, std::string extension);
                    esp_err_t             setContentTypeFromFileType(httpd_req_t *req, std::string fileName);
                    esp_err_t             getPathFromURI(std::string& destPath, std::string& destFile, const char *basePath, const char *uri);
                    esp_err_t             getPathFromURI(std::string& destPath, const char *basePath, const char *uri);
                    static esp_err_t      defaultFileHandler(httpd_req_t *req);
                    std::string           esp32PartitionType(esp_partition_type_t type);
                    std::string           esp32PartitionSubType(esp_partition_subtype_t subtype);


          IRAM_ATTR static void           pairBluetoothDevice(void *pvParameters);
          IRAM_ATTR static void           wifiAPHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
          IRAM_ATTR static void           wifiClientHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

          // Method to split a string based on a delimiter and store in a vector.
          std::vector<std::string> split(std::string s, std::string delimiter)
          {
              // Locals.
              size_t                   pos_start = 0;
              size_t                   pos_end;
              size_t                   delim_len = delimiter.length();
              std::string              token;
              std::vector<std::string> res;

              // Loop through the string locating delimiters and split on each occurrence.
              while((pos_end = s.find (delimiter, pos_start)) != std::string::npos)
              {
                  token = s.substr (pos_start, pos_end - pos_start);
                  pos_start = pos_end + delim_len;
                  // Push each occurrence onto Vector.
                  res.push_back (token);
              }

              // Store last item in vector.
              res.push_back (s.substr (pos_start));
              return res;
          }

          // check if a given string is a numeric string or not
          bool isNumber(const std::string &str)
          {
              // `std::find_first_not_of` searches the string for the first character
              // that does not match any of the characters specified in its arguments
              return !str.empty() &&
                  (str.find_first_not_of("[0123456789]") == std::string::npos);
          }
           
          // Function to split string `str` using a given delimiter
          std::vector<std::string> split(const std::string &str, char delim)
          {
              auto i = 0;
              std::vector<std::string> list;
           
              auto pos = str.find(delim);
           
              while (pos != std::string::npos)
              {
                  list.push_back(str.substr(i, pos - i));
                  i = ++pos;
                  pos = str.find(delim, pos);
              }
           
              list.push_back(str.substr(i, str.length()));
           
              return list;
          }
           
          // Function to validate an IP address
          bool validateIP(std::string ip)
          {
              // split the string into tokens
              std::vector<std::string> list = split(ip, '.');
           
              // if the token size is not equal to four
              if (list.size() != 4) {
                  return false;
              }
           
              // validate each token
              for (std::string str: list)
              {
                  // verify that the string is a number or not, and the numbers
                  // are in the valid range
                  if (!isNumber(str) || std::stoi(str) > 255 || std::stoi(str) < 0) {
                      return false;
                  }
              }
           
              return true;
          }

          // Method to split an IP4 address into its components, checking each for validity.
          bool splitIP(std::string ip, int *a, int *b, int *c, int *d)
          {
              // Init.
              *a = *b = *c = *d = 0;

              // split the string into tokens
              std::vector<std::string> list = split(ip, '.');
           
              // if the token size is not equal to four
              if (list.size() != 4) {
printf("Size:%d\n", list.size());
                  return false;
              }
              // Loop through vector and check each number for validity before assigning.
              for(int idx=0; idx < 4; idx++)
              {
                  // verify that the string is a number or not, and the numbers
                  // are in the valid range
                  if (!isNumber(list.at(idx)) || std::stoi(list.at(idx)) > 255 || std::stoi(list.at(idx)) < 0) {
printf("Item:%d, %s\n", idx, list.at(idx).c_str());
                      return false;
                  }
                  int frag = std::stoi(list.at(idx));
                  if(idx == 0) *a = frag;
                  if(idx == 1) *b = frag;
                  if(idx == 2) *c = frag;
                  if(idx == 3) *d = frag;
              }
              return true;
          }

  };
#endif

#endif // WIFI_H
