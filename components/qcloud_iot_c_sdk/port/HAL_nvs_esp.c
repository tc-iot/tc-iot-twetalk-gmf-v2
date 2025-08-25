/**
 * @file HAL_nvs_esp.c
 * @author hubertxxu (hubertxxu@tencent.com)
 * @brief 
 * @version 0.1
 * @date 2025-08-25
 * 
 * @copyright
 * Tencent is pleased to support the open source community by making IoT Hub available. 
 * Copyright(C) 2021 - 2026 THL A29 Limited, a Tencent company.All rights reserved.
 * Licensed under the MIT License(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "qcloud_iot_platform.h"

static const char *TAG = "HAL NVS";
// 固定的命名空间
#define NVS_NAMESPACE "qcloud_iot"

int HAL_NVS_Write(const char *key, const uint8_t *value, uint32_t length)
{
    esp_err_t err;
    nvs_handle_t handle;

    if (!key || !value) {
        ESP_LOGE(TAG, "Invalid input parameters");
        return -1;
    }

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle for namespace '%s': %s", NVS_NAMESPACE, esp_err_to_name(err));
        return -1;
    }

    err = nvs_set_blob(handle, key, value, length);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write NVS value for key '%s' in namespace '%s': %s", key, NVS_NAMESPACE, esp_err_to_name(err));
        nvs_close(handle);
        return -1;
    }

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes for namespace '%s': %s", NVS_NAMESPACE, esp_err_to_name(err));
        nvs_close(handle);
        return -1;
    }

    nvs_close(handle);
    return 0;
}

int HAL_NVS_Read(const char *key, uint8_t *value, uint32_t *length)
{
    esp_err_t err;
    nvs_handle_t handle;

    if (!key || !value || !length) {
        ESP_LOGE(TAG, "Invalid input parameters");
        return -1;
    }

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle for namespace '%s': %s", NVS_NAMESPACE, esp_err_to_name(err));
        return -1;
    }

    size_t required_size = 0;
    // First get the required size
    err = nvs_get_blob(handle, key, NULL, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "NVS key '%s' not found in namespace '%s'", key, NVS_NAMESPACE);
        nvs_close(handle);
        return -1;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get NVS blob size for key '%s' in namespace '%s': %s", key, NVS_NAMESPACE, esp_err_to_name(err));
        nvs_close(handle);
        return -1;
    }

    if (*length < required_size) {
        ESP_LOGE(TAG, "Provided buffer is too small for key '%s' in namespace '%s'. Required: %u, Provided: %lu",
                 key, NVS_NAMESPACE, required_size, *length);
        *length = required_size; // Return required size
        nvs_close(handle);
        return -1;
    }

    // Read the data
    err = nvs_get_blob(handle, key, value, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read NVS value for key '%s' in namespace '%s': %s", key, NVS_NAMESPACE, esp_err_to_name(err));
        nvs_close(handle);
        return -1;
    }

    *length = required_size;
    nvs_close(handle);
    return 0;
}

int HAL_NVS_Erase(const char *key)
{
    esp_err_t err;
    nvs_handle_t handle;

    if (!key) {
        ESP_LOGE(TAG, "Invalid input parameters");
        return -1;
    }

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle for namespace '%s': %s", NVS_NAMESPACE, esp_err_to_name(err));
        return -1;
    }

    err = nvs_erase_key(handle, key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS key '%s' in namespace '%s': %s", key, NVS_NAMESPACE, esp_err_to_name(err));
        nvs_close(handle);
        return -1;
    }

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes after erasing key '%s' in namespace '%s': %s", key, NVS_NAMESPACE, esp_err_to_name(err));
        nvs_close(handle);
        return -1;
    }

    nvs_close(handle);
    return 0;
}
