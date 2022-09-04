/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            SharpKey.cpp
// Created:         Jan 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     HID (PS/2, Bluetooth) to Sharp Keyboard and Mouse Interface.
//                  This source file contains the application logic to interface several Sharp MZ/X
//                  machines to a HID (PS/2 keyboard, PS/2 Mouse, Bluetooth Keyboard/Mouse). The type
//                  of host is determined by the host i/o lines  and the necessary control threads 
//                  instantiated accordingly.
//
//                  Please see the individual classes (singleton obiects) for a specific host logic.
//
//                  The application uses the Espressif Development environment with Arduino components.
//                  This is necessary for the PS2KeyAdvanced class, which may in future be converted to
//                  use esp-idf library calls rather than Arduino. I wrote the PS2Mouse class using
//                  Arduino as well, so both will need conversion eventually.
//
//                  The Espressif environment is necessary in order to have more control over the build.
//                  It is important, for timing, that Core 1 is dedicated to the MZ 2599/2800 Interface 
//                  logic due to timing constraints and Core 0 is used for all RTOS/Interrupts tasks. 
//                  Other host interface classes freely use both cores.
//
//                  The application is configured via the Kconfig system. Use 'idf.py menuconfig' to 
//                  configure.
// Credits:         
// Copyright:       (c) 2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Jan 2022 - Initial write.
//                  Feb 2022 - Updates and fixes. Added logic to detect PS/2 disconnect and reconnect,
//                             added 3 alternative maps selected by ALT+F1 (MZ2500), ALT+F2(MZ2000)
//                             ALT+F3(MZ80B) due to slight differences in the key layout.
//                             Added framework for wifi so that the interface can enter AP mode to
//                             acquire local net parameters then connect to local net. Needs the web
//                             interface writing.
//                  Mar 2022 - Split from mz25key and modularised to allow multiple host targets.
//                  Apr 2022 - Rewrote the application as C++ classes, the host interfaces are based
//                             on a Base class (KeyInterface) which virtualises the interface and
//                             provides some base methods and variables, each host inherits the Base class
//                             to form individual singleton objects.
//                  Apr 2022 - Added X1, X68000 Keyboard functionality and Mouse functionality.
//                  Apr 2022 - Started on tests for using Bluetooth, moved all input devices (Human
//                             Input Device) into its own class to encapsulate PS/2 Keyboard, Mouse and
//                             Bluetooth Keyboard/Mouse.
//            v1.01 May 2022 - More objectifying the fundamental components to make it easier to upgrade.
//            v1.02 May 2022 - Initial release version.
//            v1.03 May 2022 - Added feature security and efuse build data.
//            v1.04 Jun 2022 - Reworked the Wifi so that when requested, the SharpKey reboots and 
//                             immediately starts up in WiFi mode without enabling BT or hardware I/F.
//                             This is necessary due to shared antenna in the ESP32 and also clashes
//                             in the IDF library stack.
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
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <iterator>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_efuse_custom_table.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "Arduino.h"
#include "driver/gpio.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "sdkconfig.h"
#include "MZ2528.h"
#include "X1.h"
#include "X68K.h"
#include "MZ5665.h"
#include "PC9801.h"
#include "Mouse.h"
#include "HID.h"
#include "NVS.h"
#include "WiFi.h"

//////////////////////////////////////////////////////////////////////////
// Important:
//
// All configuration is performed via the 'idf.py menuconfig' command.
// The file 'sdkconfig' contains the configured parameter defines.
//////////////////////////////////////////////////////////////////////////

// Constants.
#define SHARPKEY_NAME                  "SharpKey"
#define SHARPKEY_VERSION               1.04
#define SHARPKEY_MODULES               "SharpKey MZ2528 X1 X68K MZ5665 PC9801 Mouse KeyInterface HID NVS LED SWITCH WiFi FilePack"

// Tag for ESP main application logging.
#define MAINTAG                        "sharpkey"

// LittleFS default parameters.
#define LITTLEFS_DEFAULT_PATH          "/littlefs"
#define LITTLEFS_DEFAULT_PARTITION     "filesys"

// Structure for configuration information stored in NVS.
struct SharpKeyConfig {
    struct {
        uint8_t                 bootMode;               // Flag to indicate the mode SharpKey should boot into.
                                                        // 0 = Interface, 1 = WiFi (configured), 2 = WiFi (default).
    } params;
} sharpKeyConfig;

// Overloads for the EFUSE Custom MAC definitions. Limited Efuse space and Custom MAC not needed in eFuse in this design
// so we overload with custom flags.
// 0-7 Reserved
// 8-15 defined as Base configuration and enhanced set 1.
static const esp_efuse_desc_t ENABLE_BT[] = {
    {EFUSE_BLK3, 8, 1},
};
static const esp_efuse_desc_t ENABLE_MZ5665[] = {
    {EFUSE_BLK3, 9, 1},
};
static const esp_efuse_desc_t ENABLE_PC9801[] = {
    {EFUSE_BLK3, 10, 1},
};
static const esp_efuse_desc_t ENABLE_MOUSE[] = {
    {EFUSE_BLK3, 11, 1},
};
static const esp_efuse_desc_t ENABLE_X68000[] = {
    {EFUSE_BLK3, 12, 1},
};
static const esp_efuse_desc_t ENABLE_X1[] = {
    {EFUSE_BLK3, 13, 1},
};
static const esp_efuse_desc_t ENABLE_MZ2800[] = {
    {EFUSE_BLK3, 14, 1},
};
static const esp_efuse_desc_t ENABLE_MZ2500[] = {
    {EFUSE_BLK3, 15, 1},
};
const esp_efuse_desc_t* ESP_EFUSE_ENABLE_BT[] = {
    &ENABLE_BT[0],
    NULL
};
const esp_efuse_desc_t* ESP_EFUSE_ENABLE_MZ5665[] = {
    &ENABLE_MZ5665[0],
    NULL
};
const esp_efuse_desc_t* ESP_EFUSE_ENABLE_PC9801[] = {
    &ENABLE_PC9801[0],
    NULL
};
const esp_efuse_desc_t* ESP_EFUSE_ENABLE_X68000[] = {
    &ENABLE_X68000[0],
    NULL
};
const esp_efuse_desc_t* ESP_EFUSE_ENABLE_X1[] = {
    &ENABLE_X1[0],
    NULL
};
const esp_efuse_desc_t* ESP_EFUSE_ENABLE_MZ2800[] = {
    &ENABLE_MZ2800[0],
    NULL
};
const esp_efuse_desc_t* ESP_EFUSE_ENABLE_MZ2500[] = {
    &ENABLE_MZ2500[0],
    NULL
};
const esp_efuse_desc_t* ESP_EFUSE_ENABLE_MOUSE[] = {
    &ENABLE_MOUSE[0],
    NULL
};

// Revision control, stored in EFUSE block. 
typedef struct {
    uint16_t   hardwareRevision;    // Hardware revision, stored as x/1000 - gives range 0.000 - 64.999
    uint16_t   serialNo;            // Serial Number of the device.
    uint8_t    buildDate[3];        // Build date of the hardware.
    bool       disableRestrictions; // Flag to indicate all firmware restrictions should be disabled.
    bool       enableMZ2500;        // Flag to indicate MZ-2500 functionality should be enabled.
    bool       enableMZ2800;        // Flag to indicate MZ-2800 functionality should be enabled.
    bool       enableX1;            // Flag to indicate X1 functionality should be enabled.
    bool       enableX68000;        // Flag to indicate X68000 functionality should be enabled.
    bool       enableMouse;         // Flag to indicate Mouse functionality should be enabled.
    bool       enableBluetooth;     // Flag to indicate Bluetooth functionality should be enabled.
    bool       enableMZ5665;        // Flag to indicate MZ-6500 functionality should be enabled.
    bool       enablePC9801;        // Flag to indicate NEC PC-9801 functionality should be enabled.
} t_EFUSE;

// Method to check the efuse coding scheme is disabled. For this project it should be disabled.
bool checkEFUSE(void)
{
    // Locals.
    bool    result = false;
    size_t  secureVersion = 0;

    // Check the efuse coding scheme, should be NONE and the security version should be 0 for this project.
    esp_efuse_coding_scheme_t coding_scheme = esp_efuse_get_coding_scheme(EFUSE_BLK3);
    if(coding_scheme == EFUSE_CODING_SCHEME_NONE)
    {
        ESP_ERROR_CHECK(esp_efuse_read_field_cnt(ESP_EFUSE_SECURE_VERSION, &secureVersion));
        if(secureVersion == 0)
        {
            result = true;
        }
    }

    // True = efuse present and correct, false = not recognised.
    return(result);
}

// Method to read out the stored configuration from EFUSE into the configuration structure 
// for later appraisal.
bool readEFUSE(t_EFUSE &sharpkeyEfuses)
{
    // Locals.
    bool       result = true;

    // Manually read each fuse value into the given structure, any failures treat as a complete failure.
    result = esp_efuse_read_field_blob(ESP_EFUSE_HARDWARE_REVISION,    &sharpkeyEfuses.hardwareRevision, 16) == ESP_OK   ? result : false; sharpkeyEfuses.hardwareRevision = __builtin_bswap16(sharpkeyEfuses.hardwareRevision);
    result = esp_efuse_read_field_blob(ESP_EFUSE_SERIAL_NO,            &sharpkeyEfuses.serialNo, 16) == ESP_OK           ? result : false; sharpkeyEfuses.serialNo = __builtin_bswap16(sharpkeyEfuses.serialNo);
    result = esp_efuse_read_field_blob(ESP_EFUSE_BUILD_DATE,           &sharpkeyEfuses.buildDate, 24) == ESP_OK          ? result : false;
    result = esp_efuse_read_field_blob(ESP_EFUSE_DISABLE_RESTRICTIONS, &sharpkeyEfuses.disableRestrictions, 1) == ESP_OK ? result : false;
    result = esp_efuse_read_field_blob(ESP_EFUSE_ENABLE_BT,            &sharpkeyEfuses.enableBluetooth, 1) == ESP_OK     ? result : false;
    result = esp_efuse_read_field_blob(ESP_EFUSE_ENABLE_MZ2500,        &sharpkeyEfuses.enableMZ2500, 1) == ESP_OK        ? result : false;
    result = esp_efuse_read_field_blob(ESP_EFUSE_ENABLE_MZ2800,        &sharpkeyEfuses.enableMZ2800, 1) == ESP_OK        ? result : false;
    result = esp_efuse_read_field_blob(ESP_EFUSE_ENABLE_X1,            &sharpkeyEfuses.enableX1, 1) == ESP_OK            ? result : false;
    result = esp_efuse_read_field_blob(ESP_EFUSE_ENABLE_X68000,        &sharpkeyEfuses.enableX68000, 1) == ESP_OK        ? result : false;
    result = esp_efuse_read_field_blob(ESP_EFUSE_ENABLE_MZ5665,        &sharpkeyEfuses.enableMZ5665, 1) == ESP_OK        ? result : false;
    result = esp_efuse_read_field_blob(ESP_EFUSE_ENABLE_PC9801,        &sharpkeyEfuses.enablePC9801, 1) == ESP_OK        ? result : false;
    result = esp_efuse_read_field_blob(ESP_EFUSE_ENABLE_MOUSE,         &sharpkeyEfuses.enableMouse, 1) == ESP_OK         ? result : false;

    // Return true = successful read, false = failed to read efuse or values.
    return(result);
}

// Method to write the configuration to one-time programmable FlashRAM EFuses. This setting persists for the life of the SharpKey
// and so minimal information is stored which cant be wiped, everything else uses reprogrammable FlashRAM via NVS.
bool writeEFUSE(t_EFUSE &sharpkeyEfuses)
{
    // Locals.
    bool    result = true;
#ifdef CONFIG_EFUSE_VIRTUAL

    // Write out the configuration structure member at a time.
    result = esp_efuse_write_field_blob(ESP_EFUSE_HARDWARE_REVISION,    &sharpkeyEfuses.hardwareRevision, 16) == ESP_OK   ? result : false;
    result = esp_efuse_write_field_blob(ESP_EFUSE_SERIAL_NO,            &sharpkeyEfuses.serialNo, 16) == ESP_OK           ? result : false;
    result = esp_efuse_write_field_blob(ESP_EFUSE_BUILD_DATE,           &sharpkeyEfuses.buildDate, 24) == ESP_OK          ? result : false;
    result = esp_efuse_write_field_blob(ESP_EFUSE_DISABLE_RESTRICTIONS, &sharpkeyEfuses.disableRestrictions, 1) == ESP_OK ? result : false;
    result = esp_efuse_write_field_blob(ESP_EFUSE_ENABLE_BT,            &sharpkeyEfuses.enableBluetooth, 1) == ESP_OK     ? result : false;
    result = esp_efuse_write_field_blob(ESP_EFUSE_ENABLE_MZ2500,        &sharpkeyEfuses.enableMZ2500, 1) == ESP_OK        ? result : false;
    result = esp_efuse_write_field_blob(ESP_EFUSE_ENABLE_MZ2800,        &sharpkeyEfuses.enableMZ2800, 1) == ESP_OK        ? result : false;
    result = esp_efuse_write_field_blob(ESP_EFUSE_ENABLE_X1,            &sharpkeyEfuses.enableX1, 1) == ESP_OK            ? result : false;
    result = esp_efuse_write_field_blob(ESP_EFUSE_ENABLE_X68000,        &sharpkeyEfuses.enableX68000, 1) == ESP_OK        ? result : false;
    result = esp_efuse_write_field_blob(ESP_EFUSE_ENABLE_MZ5665,        &sharpkeyEfuses.enableMZ5665, 1) == ESP_OK        ? result : false;
    result = esp_efuse_write_field_blob(ESP_EFUSE_ENABLE_PC9801,        &sharpkeyEfuses.enablePC9801, 1) == ESP_OK        ? result : false;
    result = esp_efuse_write_field_blob(ESP_EFUSE_ENABLE_MOUSE,         &sharpkeyEfuses.enableMouse, 1) == ESP_OK         ? result : false;

#endif // CONFIG_EFUSE_VIRTUAL
    // Return true for success, false for 1 or more failures.
    return(result);
}

// Method to return the application version number.
float version(void)
{
    return(SHARPKEY_VERSION);
}

// Method to startup the WiFi interface.
// Starting the WiFi method requires no Bluetooth or running host interface threads. It is started after a fresh boot. This is necessary due to the ESP IDF
// and hardware antenna constraints.
//
#if defined(CONFIG_IF_WIFI_ENABLED)
void startWiFi(NVS &nvs, LED *led, bool defaultMode, uint32_t ifMode)
{
    // Locals.
    //
    KeyInterface             *keyIf = NULL;
    KeyInterface             *mouseIf = NULL;
    HID                      *hid  = NULL;
    SWITCH                   *sw   = NULL;
    WiFi                     *wifi = NULL;

    // The WiFi interface needs to report version numbers so an end user can view which version of an object is built-in, used for error tracking and firmware upgrades.
    // In order to do this, build up a structure of object and version numbers which is passed into the WiFi interface object. The structure is defined within wifi.h as technically
    // it belongs to that object but needs to be evaluated in main as it has access to all class/objects forming the SharpKey.
    WiFi::t_versionList *versionList = new WiFi::t_versionList;
    std::istringstream list(SHARPKEY_MODULES);
    std::vector<std::string> modules{std::istream_iterator<std::string>{list}, std::istream_iterator<std::string>{}};
    for(int idx = 0; idx < modules.size() && idx < OBJECT_VERSION_LIST_MAX; idx++, versionList->elements=idx)
    {
        versionList->item[idx] = new WiFi::t_versionItem;
        versionList->item[idx]->object = modules[idx];
        if(modules[idx].compare("SharpKey") == 0)
        {
            versionList->item[idx]->version = version();
        }
        else if(modules[idx].compare("HID") == 0)
        {
            hid = new HID();
            versionList->item[idx]->version = hid->version();
            delete hid;
        }
        else if(modules[idx].compare("NVS") == 0)
        {
            versionList->item[idx]->version = nvs.version();
        }
        else if(modules[idx].compare("LED") == 0)
        {
            versionList->item[idx]->version = led->version();
        }
        else if(modules[idx].compare("SWITCH") == 0)
        {
            sw = new SWITCH;
            versionList->item[idx]->version = sw->version();
            delete sw;
        }
        else if(modules[idx].compare("MZ2528") == 0)
        {
            keyIf = new MZ2528();
            versionList->item[idx]->version = keyIf->version();
            delete keyIf;
        }
        else if(modules[idx].compare("X1") == 0)
        {
            keyIf = new X1();
            versionList->item[idx]->version = keyIf->version();
            delete keyIf;
        }
        else if(modules[idx].compare("X68K") == 0)
        {
            keyIf = new X68K();
            versionList->item[idx]->version = keyIf->version();
            delete keyIf;
        }
        else if(modules[idx].compare("MZ5665") == 0)
        {
            keyIf = new MZ5665();
            versionList->item[idx]->version = keyIf->version();
            delete keyIf;
        }
        else if(modules[idx].compare("PC9801") == 0)
        {
            keyIf = new PC9801();
            versionList->item[idx]->version = keyIf->version();
            delete keyIf;
        }
        else if(modules[idx].compare("Mouse") == 0)
        {
            keyIf = new Mouse();
            versionList->item[idx]->version = keyIf->version();
            delete keyIf;
        }
        else if(modules[idx].compare("WiFi") == 0)
        {
            WiFi *wifiIf = new WiFi();
            versionList->item[idx]->version = wifiIf->version();
            delete wifiIf;
        }
        else if(modules[idx].compare("KeyInterface") == 0)
        {
            keyIf = new KeyInterface();
            versionList->item[idx]->version = keyIf->version();
            delete keyIf;
        }
        else if(modules[idx].compare("FilePack") == 0)
        {
            // Look on the filesystem for the version file and read the first line contents as the version number.
            std::string version = "0.00";
            std::stringstream fqfn; fqfn << LITTLEFS_DEFAULT_PATH << "/" << FILEPACK_VERSION_FILE;
            std::ifstream inFile;   inFile.open(fqfn.str());
            if(inFile.is_open())
            {
                std::getline(inFile, version);
            }
            inFile.close();
            versionList->item[idx]->version = std::stof(version);
        }
        else
        {
            ESP_LOGE(MAINTAG, "Unknown class name in module configuration list:%s", modules[idx].c_str());
        }
    }
    keyIf = NULL;

    // Create a basic hid object for config persistence and retrieval.
    hid = new HID(&nvs);

    // Create basic host interface objects without hardware configuration. This is needed as the WiFi object probes them for configuration parameters and to update
    // the parameters.
    switch(ifMode)
    {
        // MZ-2500 or MZ-2800 Host.
        case 2500:
        case 2800:
        {
            keyIf = new MZ2528(&nvs, hid, LITTLEFS_DEFAULT_PATH);
            break;
        }

        // Sharp X1 Host.
        case 1:
        {
            keyIf = new X1(&nvs, hid, LITTLEFS_DEFAULT_PATH);
            break;
        }

        // Sharp X68000 Host.
        case 68000:
        {
            keyIf   = new X68K(&nvs, hid, LITTLEFS_DEFAULT_PATH);
            mouseIf = new Mouse(&nvs, hid);
            break;
        }

        // MZ-5600/MZ-6500 Host.
        case 5600:
        case 6500:
        {
            keyIf = new MZ5665(&nvs, hid, LITTLEFS_DEFAULT_PATH);
            break;
        }
       
        // PC-9801
        case 9801:
        {
            keyIf = new PC9801(&nvs, hid, LITTLEFS_DEFAULT_PATH);
            break;
        }
       
        // Mouse
        case 2:
        {
            mouseIf = new Mouse(&nvs, hid);
            break;
        }

        // Unknown host, so just bring up a basic WiFi configuration without interface object configuration.
        default:
        {
            keyIf = new KeyInterface(&nvs, hid);
            break;
        }
    }
    
    // There are a number of issues with the ESP32 WiFi and code base which you have to work around, some are hardware issues but others will no doubt be
    // resolved in later IDF releases. On the MZ-2500/MZ-2800 there is an ESP32 issue regarding WiFi Client mode and ADC2. If the pins are input and wrong
    // value the WiFi Client mode wont connect, it goes into a state where it wont connect and errors out - the same kind of error is seen if the voltage/current
    // supplied to the ESP32 is out of parameter. 
    if(keyIf != NULL && (ifMode == 2500 || ifMode == 2800 || ifMode == 68000 || ifMode == 5600 || ifMode == 6500 || ifMode == 9801))
    {
        keyIf->reconfigADC2Ports(true);
    }
   
    // Create a new WiFi object.
    wifi = new WiFi(keyIf, mouseIf, defaultMode, &nvs, led, LITTLEFS_DEFAULT_PATH, versionList);

    // Pass control, only returns if a reboot is needed.
    wifi->run();
}
#endif

// Method to determine which host the SharpKey is connected to. This is done by examining the host I/O for tell tale signals
// or inputs wired in a fixed combination.
//
uint32_t getHostType(bool eFuseInvalid, t_EFUSE sharpkeyEfuses)
{
    // Locals.
    //
    uint32_t                 RTSNI_MASK = (1 << (CONFIG_HOST_RTSNI - 32));
    uint32_t                 MPXI_MASK  = (1 << CONFIG_HOST_MPXI);
    uint32_t                 KDB0_MASK  = (1 << CONFIG_HOST_KDB0);
  //uint32_t                 KDB1_MASK  = (1 << CONFIG_HOST_KDB1);
  //uint32_t                 KDB2_MASK  = (1 << CONFIG_HOST_KDB2);
  //uint32_t                 KDB3_MASK  = (1 << CONFIG_HOST_KDB3);

  // Build selectable target. This software can be built to run on the SharpKey or mz25key interfaces. If a resistor is connected from MPX input to the ESP32 IO12 pin 14 then
  // the SharpKey build can be used even though the mz25key only supports one target at a time. If no resistor is connected then you will need to build for a specific target
  // as the detection logic will not be able to determine if it is connected to an MZ-2500 or MZ-2800. Use menuconfig to select the target.
  #ifdef CONFIG_SHARPKEY

    // Connected host detection. 
    //
    // MZ-2800 - RTSN Goes High and Low, MPX goes High and Low. RTSN, depending upon the machine mode may only oscillate a few times per second, so need to count
    //           MPX pulses to determine MZ-2800 mode.
    // MZ-2500 - RTSN Goes High and Low. MPX goes High and Low. On the falling edge of RTSN sample MPX 50ns in, if it is high then
    //           select MZ-2500 mode.
    // X1      - Output 1010 onto KDO[3:0] and read KDB[3:0] - if match then X1 mode.
    // X68000  = KD4 = low,  MPX = low, RTSN = high
    // Mouse   = KD4 = high, MPX = low, RTSN = high
    //
    // NB: The above tests ASSUME the interface is plugged into the host, only powered by the host and the host is switched on. Development cycles where the interface 
    //     is powered by the UART adapter and/or the host is switched off will not detect the correct host.
    //
    // First up, sample RTSN and MPX to see if they are alternating. The ratio of RTSN to MPX will yield the type of host.
    uint32_t cntRTSN = 0;
    uint32_t cntMPX  = 0;
    uint32_t ifMode  = 0;
    volatile uint32_t gpioIN, gpioINLast;
    gpioIN = gpioINLast = REG_READ(GPIO_IN_REG);
    for(uint32_t idx=0; idx < 400; idx++)
    {
        gpioIN = REG_READ(GPIO_IN_REG);
        if((gpioIN & MPXI_MASK) && (gpioIN & MPXI_MASK) != (gpioINLast & MPXI_MASK))  cntMPX++;
        gpioINLast = gpioIN;
    }
    gpioIN = gpioINLast = REG_READ(GPIO_IN1_REG);
    for(uint32_t idx=0; idx < 400; idx++)
    {
        gpioIN = REG_READ(GPIO_IN1_REG);
        if((gpioIN & RTSNI_MASK) && (gpioIN & RTSNI_MASK) != (gpioINLast & RTSNI_MASK)) cntRTSN++;
        gpioINLast = gpioIN;
    }

    // If RTSN and MPX are alternating then the number identifies the host, the MZ-2500 has 1 RTSN per 1 MPX repeating every 1.2uS whereas 
    // the MZ-2800 has 1 RTSN per ~14 or more MPX cycles albeit there are periods of 1ms where no activity can be seen.
    if(cntMPX > 1)
    {
        if(cntRTSN > 20 && cntMPX > 20 && eFuseInvalid == false && (sharpkeyEfuses.disableRestrictions == true || sharpkeyEfuses.enableMZ2500 == true))
            ifMode = 2500;

        // RTSN may not oscillate in the small capture window depending on run mode, so check MPX and if it is oscillating, at a lower rate than the MZ-2500, select MZ-2800 mode.
        else if(cntMPX > 5 && eFuseInvalid == false && (sharpkeyEfuses.disableRestrictions == true || sharpkeyEfuses.enableMZ2800 == true))
            ifMode = 2800;

        if(ifMode > 0)
            ESP_LOGW(MAINTAG, "Detected MZ-%d host, counts:RTSN=%d, MPX=%d.", ifMode, cntRTSN, cntMPX);
    } else
    {
        // Check for X1 - this is accomplished by writing a value to KDO and reading it back on KDB. This works because RTSN is tied low on the X1 cable as the X1 protocol is output only.
        // Clear all KDO bits - clear state = '0'
        GPIO.out_w1tc = (1 << CONFIG_HOST_KDO7) | (1 << CONFIG_HOST_KDO6) | (1 << CONFIG_HOST_KDO5) | (1 << CONFIG_HOST_KDO4) | 
                        (1 << CONFIG_HOST_KDO3) | (1 << CONFIG_HOST_KDO2) | (1 << CONFIG_HOST_KDO1) | (1 << CONFIG_HOST_KDO0);
        vTaskDelay(1);
        // Set the test pattern. KDO[3:0] = 1010.
        GPIO.out_w1ts = (1 << CONFIG_HOST_KDO7) | (1 << CONFIG_HOST_KDO6) | (1 << CONFIG_HOST_KDO5) | (1 << CONFIG_HOST_KDO4) | 
                        (1 << CONFIG_HOST_KDO3) | (0 << CONFIG_HOST_KDO2) | (1 << CONFIG_HOST_KDO1) | (0 << CONFIG_HOST_KDO0);
        vTaskDelay(1);
        // Now read back KDB.
        gpioIN = REG_READ(GPIO_IN_REG);
        if((gpioIN & (1 << CONFIG_HOST_KDB3)) && (gpioIN & (1 << CONFIG_HOST_KDB2)) == 0 && (gpioIN & (1 << CONFIG_HOST_KDB1)) && (gpioIN & (1 << CONFIG_HOST_KDB0)) == 0 && 
           eFuseInvalid == false && (sharpkeyEfuses.disableRestrictions == true || sharpkeyEfuses.enableX1 == true))
        {
            ifMode = 1;
        }
        else
        {
            // Need to reconfigure the Mouse CTRL pin so we can detect counts.
            gpio_config_t          io_conf;
            io_conf.intr_type    = GPIO_INTR_DISABLE;
            io_conf.mode         = GPIO_MODE_INPUT;
            io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDB0); 
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
            gpio_config(&io_conf);

            // Count the number of pulses on the TxD line (X68000) or Mouse Ctrl line. If there are pulses then the host is a mouse port.
            uint32_t cntCtrl= 0;
            gpioIN = gpioINLast = REG_READ(GPIO_IN_REG);
            for(uint32_t idx=0; idx < 400000; idx++)
            {
                gpioIN = REG_READ(GPIO_IN_REG);
                if((gpioIN & KDB0_MASK) && (gpioIN & KDB0_MASK) != (gpioINLast & KDB0_MASK))  cntCtrl++;
                gpioINLast = gpioIN;
            }
            
            // Restore config.
            io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
            io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
            gpio_config(&io_conf);

            // Check for X68000 - KD4 = low, MPX = low, RTSN = high
            gpioIN = REG_READ(GPIO_IN_REG);
            if(cntCtrl <= 1 && (gpioIN & (1 << CONFIG_HOST_MPXI)) == 0 && (REG_READ(GPIO_IN1_REG) & RTSNI_MASK) != 0 && 
               eFuseInvalid == false && (sharpkeyEfuses.disableRestrictions == true || sharpkeyEfuses.enableX68000 == true))
            {
                ifMode = 68000;
            }
            else
            {
                // Check for Mouse - KD4 = high, MPX = low, RTSN = high
                gpioIN = REG_READ(GPIO_IN_REG);
                if(cntCtrl > 1 && (gpioIN & (1 << CONFIG_HOST_KDI4)) != 0 && (gpioIN & (1 << CONFIG_HOST_MPXI)) == 0 && (REG_READ(GPIO_IN1_REG) & RTSNI_MASK) != 0 && 
                   eFuseInvalid == false && (sharpkeyEfuses.disableRestrictions == true || sharpkeyEfuses.enableMouse == true))
                {
                    ifMode = 2;
                }
            }
        }
    }
    //ESP_LOGW(MAINTAG, "RTSN(%d) and MPX(%d) counts.", cntRTSN, cntMPX);
  #endif

  // Target build for an MZ-2500 using the mz25key hardware.
  #ifdef CONFIG_MZ25KEY_MZ2500
    uint32_t ifMode = 2500;
  #endif
  
  // Target build for an MZ-2500 using the mz25key hardware.
  #ifdef CONFIG_MZ25KEY_MZ2800
    uint32_t ifMode = 2800;
  #endif
ifMode = 9801; 
    // Return a value which represents the detected host type.
    return(ifMode);
}

// Method triggered on a WiFi Enable switch event. Set the boot mode and restart to enter WiFi handler and webserver.
void wifiEnableCallback(void)
{
    ESP_LOGW(MAINTAG, "Setting WiFi Enable mode.");
    sharpKeyConfig.params.bootMode = 1;
}

// Method triggered on a WiFi Default Mode Enable switch event. Set the boot mode and restart to enter WiFi handler and webserver.
void wifiDefaultCallback(void)
{
    ESP_LOGW(MAINTAG, "Setting WiFi Default Enable mode.");
    sharpKeyConfig.params.bootMode = 2;
}

// Method triggered on a Clear NVS switch event. Close the NVS and erase its contents setting the SharpKey back to factory default.
void clearNVSCallback(void)
{
    ESP_LOGW(MAINTAG, "Clearing NVS...");
    sharpKeyConfig.params.bootMode = 255;
}

// Setup method to configure ports, devices and threads prior to application run.
// Configuration:
//      PS/2 Keyboard over 2 wire interface
//      PS/2 Mouse over 2 wire interface
//      Power/Status LED
//      Bluetooth, in-built.
//      4 bit input  - MZ-2500/2800 Row Number
//      8 bit output - MZ-2500/2800 Scan data
//      1 bit input  - RTSN strobe line, low indicating a new Row Number available.
//      1 bit input  - KD4, High = Key scan data required, Low = AND of all key matrix rows required.
//
void setup(NVS &nvs)
{
    // Locals.
    uint32_t                 ifMode;
    bool                     eFuseInvalid = false;
    KeyInterface             *keyIf = NULL;
    KeyInterface             *mouseIf = NULL;
    HID                      *hid  = NULL;
    LED                      *led  = NULL;
    SWITCH                   *sw   = NULL;
    t_EFUSE                  sharpkeyEfuses;
    esp_vfs_littlefs_conf_t  lfsConf;
    esp_err_t                lfsStatus;    
    gpio_config_t            io_conf;
    #define                  SETUPTAG "setup"


    // Check the efuse and retrieve configured values for later appraisal.
    if(checkEFUSE() == false) { eFuseInvalid = true; }
    memset((void *)&sharpkeyEfuses, 0x00, sizeof(t_EFUSE));
    if(readEFUSE(sharpkeyEfuses) == true)
    { 
        // If the hw revision, build date and/or serial number havent been set, ie. an unconfigured ESP32 eFuse, obsfucate it.
        if(sharpkeyEfuses.hardwareRevision == 0) { sharpkeyEfuses.hardwareRevision = 1300; }
        if(sharpkeyEfuses.buildDate[0] == 0) { sharpkeyEfuses.buildDate[0] = 1; sharpkeyEfuses.buildDate[1] = 6; sharpkeyEfuses.buildDate[2] = 22; }
        if(sharpkeyEfuses.serialNo == 0) { sharpkeyEfuses.serialNo = (uint16_t)((rand() * 65534) + 1); }
        // Bug in Efuse programming workaround.
        if(sharpkeyEfuses.buildDate[0] == 31 && sharpkeyEfuses.buildDate[1] == 6) { sharpkeyEfuses.buildDate[0] = 1; }
        ESP_LOGW(SETUPTAG, "EFUSE:Hardware Rev=%f, Build Date:%d/%d/%d, Serial Number:%05d %s%s%s%s%s%s%s%s%s",
                           ((float)sharpkeyEfuses.hardwareRevision)/1000, 
                           sharpkeyEfuses.buildDate[0],sharpkeyEfuses.buildDate[1],sharpkeyEfuses.buildDate[2],
                           sharpkeyEfuses.serialNo,
                           sharpkeyEfuses.disableRestrictions == true ? "disableRestrictions" : " ",
                           sharpkeyEfuses.enableMZ2500 == true ? "enableMZ2500" : " ",
                           sharpkeyEfuses.enableMZ2800 == true ? "enableMZ2800" : " ",
                           sharpkeyEfuses.enableX1 == true ? "enableX1" : " ",
                           sharpkeyEfuses.enableX68000 == true ? "enableX68000" : " ",
                           sharpkeyEfuses.enableMouse == true ? "enableMouse" : " ",
                           sharpkeyEfuses.enableBluetooth == true ? "enableBluetooth" : " ",
                           sharpkeyEfuses.enableMZ5665 == true ? "enableMZ5665" : " ",
                           sharpkeyEfuses.enablePC9801 == true ? "enablePC9801" : "");
    } else
    {
        eFuseInvalid = true;
        ESP_LOGW(SETUPTAG, "EFUSE not programmed/readable.");
    }
    #if defined(CONFIG_DISABLE_FEATURE_SECURITY)
        sharpkeyEfuses.disableRestrictions = true;
    #endif

    // Configure 4 inputs to be the Strobe Row Number which is used to index the virtual key matrix and the strobe data returned.
    #if !defined(CONFIG_DEBUG_DISABLE_KDB)
        ESP_LOGW(SETUPTAG, "Configuring 4 bit (KDB[3:0] Row Number Inputs.");
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDB0); 
        io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDB1); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDB2); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDB3); 
        gpio_config(&io_conf);
    #endif

    #if !defined(CONFIG_DEBUG_DISABLE_KDO)
        ESP_LOGW(SETUPTAG, "Configuring 8 bit KDO[7:0] Strobe data Outputs.");
        GPIO.out_w1ts        = (1 << CONFIG_HOST_KDO7) | (1 << CONFIG_HOST_KDO6) | (1 << CONFIG_HOST_KDO5) | (1 << CONFIG_HOST_KDO4) | 
                               (1 << CONFIG_HOST_KDO3) | (1 << CONFIG_HOST_KDO2) | (1 << CONFIG_HOST_KDO1) | (1 << CONFIG_HOST_KDO0);
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_OUTPUT; 
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDO0); 
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDO1); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDO2); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDO3); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDO4); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDO5); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDO6); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDO7); 
        gpio_config(&io_conf);
    #endif

    #if !defined(CONFIG_DEBUG_DISABLE_KDI)
        ESP_LOGW(SETUPTAG, "Configuring KD4 Input.");
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_KDI4); 
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
    #endif

    #if !defined(CONFIG_DEBUG_DISABLE_RTSNI)
        ESP_LOGW(SETUPTAG, "Configuring RTSN Input.");
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_INPUT; 
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_RTSNI);  // NB: This is a 64bit bit mask so no need to subtract 32 for GPIO1.
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
    #endif

    #if !defined(CONFIG_DEBUG_DISABLE_MPXI)
        ESP_LOGW(SETUPTAG, "Configuring MPX Input.");
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL<<CONFIG_HOST_MPXI); 
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
    #endif

    #if defined(CONFIG_IF_WIFI_ENABLED)
        ESP_LOGW(SETUPTAG, "Configuring WiFi Enable Switch.");
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_INPUT; 
        io_conf.pin_bit_mask = (1ULL<<CONFIG_IF_WIFI_EN_KEY); 
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
    #endif

        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_INPUT; 
        io_conf.pin_bit_mask = (1ULL<<CONFIG_PS2_HW_DATAPIN); 
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
        io_conf.mode         = GPIO_MODE_OUTPUT; 
        io_conf.pin_bit_mask = (1ULL<<CONFIG_PS2_HW_CLKPIN); 
        gpio_config(&io_conf);

    // Filesystem configuration.
    lfsConf = {
        .base_path              = LITTLEFS_DEFAULT_PATH,
        .partition_label        = LITTLEFS_DEFAULT_PARTITION,
        .format_if_mount_failed = true,
        .dont_mount             = false,
    };    
   
     // Use settings defined above to initialize and mount LittleFS filesystem.
    ESP_LOGW(SETUPTAG, "Initializing LittleFS");
    lfsStatus = esp_vfs_littlefs_register(&lfsConf);

    // Depending on the result, we either enable the WiFi module or disable it, a filesystem is needed so WiFi cannot run without it.
    if(lfsStatus != ESP_OK)
    {
        if (lfsStatus == ESP_FAIL)
        {
            ESP_LOGE(SETUPTAG, "Failed to mount or format filesystem");
        }
        else if (lfsStatus == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(SETUPTAG, "Failed to find LittleFS partition");
        }
        else
        {
            ESP_LOGE(SETUPTAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(lfsStatus));
        }
    } else
    { 
        // For debug and development purposes, print out the partition details.
        size_t total = 0, used = 0;
        lfsStatus = esp_littlefs_info(lfsConf.partition_label, &total, &used);
        if(lfsStatus == ESP_OK)
        {
            ESP_LOGW(SETUPTAG, "Partition size: total: %d, used: %d", total, used);
        }       
    }
   
    // Setup activity LED first to show life.
    ESP_LOGW(MAINTAG, "Configuring Status LED.");
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_OUTPUT; 
    io_conf.pin_bit_mask = (1ULL<<CONFIG_PWRLED); 
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)CONFIG_PWRLED, 1);

    // Instantiate the activity LED control object.
    led = new LED(CONFIG_PWRLED);
 
    // Initialize NVS, most important it is open.
  //  nvs = new NVS();
    nvs.init();

    // Open handle to persistence using the application name as the key which represents the global namespace. Data is then stored within the application namespace using a key:value pair.
    if(nvs.open(SHARPKEY_NAME) == false)
    {
        ESP_LOGW(SETUPTAG, "Error opening NVS handle with key (%s)!\n", SHARPKEY_NAME);
    }

    // Retrieve configuration, if it doesnt exist, set defaults.
    //
    if(nvs.retrieveData(SHARPKEY_NAME, &sharpKeyConfig, sizeof(struct SharpKeyConfig)) == false)
    {
        ESP_LOGW(SETUPTAG, "SharpKey configuration set to default, no valid config found in NVS.");
        sharpKeyConfig.params.bootMode = 0;

        // Persist the data for next time.
        if(nvs.persistData(SHARPKEY_NAME, &sharpKeyConfig, sizeof(struct SharpKeyConfig)) == false)
        {
            ESP_LOGW(SETUPTAG, "Persisting Default SharpKey configuration data failed, check NVS setup.");
        }
        // No other updates so make a commit here to ensure data is flushed and written.
        else if(nvs.commitData() == false)
        {
            ESP_LOGW(SETUPTAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
        }
    }

    // Get the host type SharpKey is connected with.
    ifMode = getHostType(eFuseInvalid, sharpkeyEfuses);

    // If bootMode is for Wifi, start it. This has to be seperate due to a conflict with Bluetooth and WiFi which shares the same antenna.
    // Code is written to allow co-existence but it doesnt work so well in this project.
    //
    if(sharpKeyConfig.params.bootMode == 1 || sharpKeyConfig.params.bootMode == 2)
    {
        bool defaultMode = sharpKeyConfig.params.bootMode == 2 ? true : false;

        // Reset boot mode, WiFi is one time per key press.
        //
        sharpKeyConfig.params.bootMode = 0;
        if(nvs.persistData(SHARPKEY_NAME, &sharpKeyConfig, sizeof(struct SharpKeyConfig)) == false)
        {
            ESP_LOGW(SETUPTAG, "Persisting SharpKey configuration data failed, updates will not persist in future power cycles.");
        } else
        // Few other updates so make a commit here to ensure data is flushed and written.
        if(nvs.commitData() == false)
        {
            ESP_LOGW(SETUPTAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
        }
      #if defined(CONFIG_IF_WIFI_ENABLED)
        // Fire up WiFi.
        startWiFi(nvs, led, defaultMode, ifMode);
      #endif

        // Any exit from the WiFi module requires a reboot so the SharpKey starts up with WiFi disabled and Interface mode running.
        esp_restart();
      
    } else
    {
        // Create a new Switch object, used to allow user to select SharpKey options by pressing the key for a fixed period of time.
        sw = new SWITCH(led);

        // Add switch callbacks which are handled in this module.
        sw->setWifiEnEventCallback(&wifiEnableCallback);
        sw->setWifiDefEventCallback(&wifiDefaultCallback);
        sw->setClearNVSEventCallback(&clearNVSCallback);

        // Initialise the HID and find out what input device is connected. Bluetooth can support two devices, keyboard and mouse, so this
        // can be used by selected hosts to provide both keyboard/mouse simulateously.
        if(ifMode == 2)
        {
            // When the detected host is a Mouse port then only one service, a mouse service, can be provided.
            hid = new HID(HID::HID_DEVICE_TYPE_MOUSE, &nvs, led, sw);
        } else 
        {
            // When the detected host is a Keyboard port then it is possible, if using Bluetooth, to simultaneously offer a Mouse service at the same time, host dependent.
            hid = new HID(HID::HID_DEVICE_TYPE_KEYBOARD, &nvs, led, sw);
        }
    
        // Setup host interface according to the detected host. We run the interface regardless of optional extras such as LittleFS/WiFi as 
        // keyboard protocol conversion is this devices priority.
        switch(ifMode)
        {
            // MZ-2500 Host.
            case 2500:
            {
                keyIf = new MZ2528(ifMode, &nvs, led, hid, LITTLEFS_DEFAULT_PATH);
                break;
            }
    
            // MZ-8500 Host.
            case 2800:
            {
                keyIf = new MZ2528(ifMode, &nvs, led, hid, LITTLEFS_DEFAULT_PATH);
                break;
            }
    
            // Sharp X1 Host.
            case 1:
            {
                ESP_LOGW(SETUPTAG, "Detected Sharp X1 host.");
                keyIf = new X1(ifMode, &nvs, led, hid, LITTLEFS_DEFAULT_PATH);
                break;
            }
    
            // Sharp X68000 Host.
            case 68000:
            {
                ESP_LOGW(SETUPTAG, "Detected Sharp X68000 host.");
                keyIf = new X68K(ifMode, &nvs, led, hid, LITTLEFS_DEFAULT_PATH);
    
                // See if Bluetooth is available, if yes then we can offer mouse services simultaneously.
                if(hid->isBluetooth())
                {
                    mouseIf = new  Mouse(ifMode, &nvs, led, hid, true);
                }
                break;
            }
    
            // MZ-5600/MZ-6500 Host.
            case 5600:
            case 6500:
            {
                ESP_LOGW(SETUPTAG, "Detected Sharp MZ-5600/MZ-6500 host.");
                keyIf = new MZ5665(ifMode, &nvs, led, hid, LITTLEFS_DEFAULT_PATH);
                break;
            }
           
            // PC-9801
            case 9801:
            {
                ESP_LOGW(SETUPTAG, "Detected NEC PC-9801 host.");
                keyIf = new PC9801(ifMode, &nvs, led, hid, LITTLEFS_DEFAULT_PATH);
                break;
            }
           
            // Mouse
            case 2:
            {
                ESP_LOGW(SETUPTAG, "Detected Mouse.");
                mouseIf = new Mouse(ifMode, &nvs, led, hid, false);
                break;
            }
    
            // Unknown host or detected interface feature not enabled. Log the details, flash the LED and if WiFi is built-in,enable it and then just wait for RESET or user interaction with the WiFi.
            default:
            {
                ESP_LOGW(SETUPTAG, "Connected host is unknown.");
                ESP_LOGW(SETUPTAG, "GPIO:%08x, %08x", REG_READ(GPIO_IN_REG),  REG_READ(GPIO_IN_REG)  & (1 << CONFIG_HOST_MPXI));
                ESP_LOGW(SETUPTAG, "GPIO1:%08x,%08x", REG_READ(GPIO_IN1_REG), REG_READ(GPIO_IN1_REG) & (1 << CONFIG_HOST_RTSNI));
    
                // Initialise a base object so we have access to NVS and the LED, these are needed for the WiFi.
                keyIf = new KeyInterface(ifMode, &nvs, led, hid);
                break;
            }
        }
    
        // Disable the brownout detector, when WiFi starts up it randomly triggers the brownout even though the voltage at the WROOM input is 3.3V. It is posisbly a hardware bug 
        // as adding larger capacitors doesnt solve it.
        //
        WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    }

    // All running, wont reach here if WiFi is enabled.
    //
    ESP_LOGW(SETUPTAG, "Free Heap (%d)", xPortGetFreeHeapSize());
}

// ESP-IDF Application entry point.
//
extern "C" void app_main()
{
    // Locals.
    NVS    nvs;

    // Setup hardware and start primary control threads,
    setup(nvs);

    // Loop waiting on callback events and action accordingly.
    while(true)
    {
        // Change in boot mode requires persisting and reboot.
        //
        if(sharpKeyConfig.params.bootMode == 1 || sharpKeyConfig.params.bootMode == 2)
        {
            // Set boot mode to wifi, save and restart.
            //
            ESP_LOGW(MAINTAG, "Persisting WiFi mode.");
            if(nvs.persistData(SHARPKEY_NAME, &sharpKeyConfig, sizeof(struct SharpKeyConfig)) == false)
            {
                ESP_LOGW(SETUPTAG, "Persisting SharpKey configuration data failed, updates will not persist in future power cycles.");
            } else
            // Few other updates so make a commit here to ensure data is flushed and written.
            if(nvs.commitData() == false)
            {
                ESP_LOGW(SETUPTAG, "NVS Commit writes operation failed, some previous writes may not persist in future power cycles.");
            }
            
            // Restart and the SharpKey will come up in Wifi mode.
            esp_restart();
        }

        // Piggy backing off the bootMode is a flag to indicate NVS flash erase and reboot.
        //
        if(sharpKeyConfig.params.bootMode == 255)
        {
            // Close out NVS and erase.
            nvs.eraseAll();

            // Need to reboot as the in-memory config still holds the old settings.
            esp_restart();
        }

        // Sleep, not much to be done other than look at event flags.
        vTaskDelay(500);
    }

    // Lost in space.... this thread is no longer required!
}
