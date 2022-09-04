/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            NVS.cpp
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Base class for encapsulating the Espressif C API for the Non Volatile Storage.
// Credits:         
// Copyright:       (c) 2019-2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Mar 2022 - Initial write.
//            v1.01 May 2022 - Initial release version.
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
#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "driver/timer.h"
#include "sdkconfig.h"
#include "NVS.h"

// Method to externally take the NVS mutex for situations where another IDF module requires access to the NVS subsystem.
//
bool NVS::takeMutex(void)
{
    // Locals.
    bool   result = false;

    // Ensure a handle has been opened to the NVS.
    if(nvsCtrl.nvsHandle != (nvs_handle_t)0)
    {
        // Request exclusive access.
        if(xSemaphoreTake(nvsCtrl.mutexInternal, (TickType_t)1000) == pdTRUE)
        {
            result = true;
        }
    }
    return(result);
}

// Method to release the NVS mutex previously taken.
void NVS::giveMutex(void)
{
    // Locals.

    // Release mutex, external access now possible to the input devices.
    xSemaphoreGive(nvsCtrl.mutexInternal);
}

// Method to persist data into the NVS RAM. This method takes a pointer to any memory object and writes it into the NVS using the handle opened at initialisation time.
//
bool NVS::persistData(const char *key, void *pData, uint32_t size)
{
    // Locals.
    //
    esp_err_t    nvsStatus;
    bool         result = true;
    #define      NVSPERSISTTAG "persistData"
 
    // Ensure a handle has been opened to the NVS.
    if(nvsCtrl.nvsHandle != (nvs_handle_t)0)
    {
        // Ensure we have exclusive access before accessing NVS.
        if(xSemaphoreTake(nvsCtrl.mutexInternal, (TickType_t)1000) == pdTRUE)
        {
            // Write a binary blob of data straight from memory pointed to by pData for readSize bytes into the NVS. This allows for individual variables or entire structures.
            nvsStatus = nvs_set_blob(this->nvsCtrl.nvsHandle, key, pData, size);
            if(nvsStatus != ESP_OK)
            {
                ESP_LOGW(NVSPERSISTTAG, "Failed to persist NVS data, key:%s, size:%d, nvsStatus:%d", key, size, nvsStatus);
                result = false;
            }
        } else
        {
            result = false;
        }
    } else
    {
        result = false;
    }

    // NB: Mutex only released in COMMIT.
  
    // Return result code.
    return(result);
}

// Method to retrieve persisted data from the NVS RAM. This method takes a pointer to a pre-allocated memoery block along with size and retrieves a data block from NVS upto size bytes.
//
bool NVS::retrieveData(const char *key, void *pData, uint32_t size)
{
    // Locals.
    //
    esp_err_t    nvsStatus;
    size_t       readSize = size;
    bool         result = true;
    #define      NVSRTRVTAG "retrieveData"

    // Ensure a handle has been opened to the NVS.
    if(nvsCtrl.nvsHandle != (nvs_handle_t)0)
    {
        // Ensure we have exclusive access before accessing NVS.
        if(xSemaphoreTake(nvsCtrl.mutexInternal, (TickType_t)1000) == pdTRUE)
        {
            // Get a binary blob of data straight into the memory pointed to by pData for readSize. This allows for individual variables or entire structures.
            nvsStatus = nvs_get_blob(this->nvsCtrl.nvsHandle, key, pData, &readSize);
            if(nvsStatus != ESP_OK || readSize != size)
            {
                ESP_LOGW(NVSRTRVTAG, "Failed to retrieve NVS data, key:%s, size:%d, requested size:%d, nvsStatus:%d", key, readSize, size, nvsStatus);
                result = false;
            }
         
            // Release mutex, external access now possible to the input devices.
            xSemaphoreGive(nvsCtrl.mutexInternal);
        } else
        {
            result = false;
        }
    } else
    {
        result = false;
    }

    // Return result code.
    return(result);
}

// Method to ensure all data written to NVS is flushed and committed. This step is necessary as a write may be buffered and requires flushing to ensure persistence.
//
bool NVS::commitData(void)
{
    // Locals.
    //
    esp_err_t    nvsStatus;
    bool         result = true;
    #define      NVSCOMMITTAG "commitData"

    // Ensure a handle has been opened to the NVS.
    if(nvsCtrl.nvsHandle != (nvs_handle_t)0)
    {

        // Check that the Mutex has been taken, if we grab it then it hasnt been taken in the persistData method, so exit as a call to persistData is mandatory.
        if(xSemaphoreTake(nvsCtrl.mutexInternal, (TickType_t)0) == pdTRUE)
        {
            xSemaphoreGive(nvsCtrl.mutexInternal);
        } else
        {
            // Request a commit transaction and return response accordingly.
            nvsStatus = nvs_commit(this->nvsCtrl.nvsHandle);
            if(nvsStatus != ESP_OK)
            {
                ESP_LOGW(NVSCOMMITTAG, "Failed to commit pending NVS data.");
                result = false;
            }
           
            // Release mutex, external access now possible to the input devices.
            xSemaphoreGive(nvsCtrl.mutexInternal);
        }
    } else
    {
        result = false;
    }

    // Return result code.
    return(result);
}

// Method to erase all the NVS and return to factory default state. The method closes any open handle,
// de-initialises the NVS then performs a flash erase.
//
void NVS::eraseAll(void)
{
    // Locals.
    //
    #define      NVSERATAG "eraseAll"

    // Ensure we have exclusive access before accessing NVS.
    while(xSemaphoreTake(nvsCtrl.mutexInternal, (TickType_t)1000) != pdTRUE);

    // Ensure a handle has been opened to the NVS.
    if(nvsCtrl.nvsHandle != (nvs_handle_t)0)
    {
        // Close open handle.
        nvs_close(nvsCtrl.nvsHandle);
        nvsCtrl.nvsHandle = NULL;
    }

    // Stop the flash driver.
    nvs_flash_deinit();

    ESP_LOGW(NVSERATAG, "Erasing flash, disable for production!\n");
    ESP_ERROR_CHECK(nvs_flash_erase());
   
    // Release mutex, external access now possible to the input devices.
    xSemaphoreGive(nvsCtrl.mutexInternal);

    return;
}

// Method to initialise the NVS subsystem.
void NVS::init(void)
{
    // Locals.
    esp_err_t    nvsStatus;
    #define      NVSINITTAG "nvsInit"

    // Initialise variables.
    nvsCtrl.nvsHandle = (nvs_handle_t)0;

    //ESP_LOGW(NVSINITTAG, "Erasing flash, disable for production!\n");
    //ESP_ERROR_CHECK(nvs_flash_erase());

    // Initialize NVS
    ESP_LOGW(NVSINITTAG, "Initialising NVS.");
    nvsStatus = nvs_flash_init();
    if(nvsStatus == ESP_ERR_NVS_NO_FREE_PAGES || nvsStatus == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());

        // Retry nvs_flash_init
        nvsStatus = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvsStatus);   

    // Setup mutex's.
    nvsCtrl.mutexInternal = xSemaphoreCreateMutex();

    return;
}

// Method to open a namespace on the NVS given a key.
//
bool NVS::open(std::string keyName)
{
    // Locals.
    bool         result = true;
    #define      NVSOPENTAG "nvsOpen"
  
    // Only process if no handle has been opened. Currently only coded for one session at a time.
    if(nvsCtrl.nvsHandle == (nvs_handle_t)0)
    {
        // Store the key name under which all data is stored.
        this->nvsCtrl.nvsKeyName = keyName;

        // Open handle to persistence using the base-class name as the key which represents the global namespace. Sub-classes and objects accessing the public methods will
        // use there own class name as a sub-key which represents the class namespace within NVS. Data is then stored within the class namespace using a key:value pair.
        esp_err_t nvsStatus = nvs_open(nvsCtrl.nvsKeyName.c_str(), NVS_READWRITE, &this->nvsCtrl.nvsHandle);
        if (nvsStatus != ESP_OK)
        {
            ESP_LOGW(NVSOPENTAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(nvsStatus));
            result = false;
        }
    } else
    {
        result = false;
    }
    return(result);
}

// Basic constructor, init variables!
NVS::NVS(void)
{
    // Store the class name for later use, ie. NVS key access.
    this->nvsCtrl.nvsClassName = getClassName(__PRETTY_FUNCTION__);
}
