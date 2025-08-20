/**
 * @file HAL_wifi.c
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2022-10-31
 *
 * @copyright
 *
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2018 - 2021 THL A29 Limited, a Tencent company.All rights reserved.
 *
 * Licensed under the MIT License(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @par Change Log:
 * <table>
 * Date				Version		Author			Description
 * 2022-10-31		1.0			hubertxxu		first commit
 * </table>
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

static const char *TAG = "HAL wifi";

#define WIFI_CONNECTED_BIT        BIT0
#define WIFI_FAIL_BIT             BIT1
#define EXAMPLE_ESP_MAXIMUM_RETRY 20

static EventGroupHandle_t s_wifi_event_group;
static int                sg_retry_num    = 0;
static uint32_t           local_ipv4_addr = 0xC0A80401;  // 192.168.4.1

static esp_err_t save_wifi_info(const char *ssid, size_t ssid_len, const char *password, size_t passwd_len)
{
    nvs_handle_t my_handle;
    esp_err_t    err;
    // open
    err = nvs_open("wifi_info", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(__func__, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        nvs_set_str(my_handle, "ssid", (const char *)ssid);
        nvs_set_str(my_handle, "password", (const char *)password);
        nvs_commit(my_handle);
        nvs_close(my_handle);
    }
    return err;
}

static esp_err_t get_wifi_info(char *ssid, char *password)
{
    nvs_handle_t my_handle;
    esp_err_t    err;
    size_t       length;
    // open
    err = nvs_open("wifi_info", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(__func__, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        length = 32;
        err    = nvs_get_str(my_handle, "ssid", ssid, &length);
        length = 64;
        err |= nvs_get_str(my_handle, "password", password, &length);
        nvs_close(my_handle);
    }
    return err;
}

void erase_wifi_info(void)
{
    // 只清除WiFi信息，而不是整个NVS分区
    esp_err_t err;
    nvs_handle_t handle;

    // 打开WiFi信息命名空间
    err = nvs_open("wifi_info", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        // 清除ssid和password
        nvs_erase_key(handle, "ssid");
        nvs_erase_key(handle, "password");

        // 提交更改
        err = nvs_commit(handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "WiFi info cleared successfully");
        } else {
            ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(err));
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS wifi_info namespace: %s", esp_err_to_name(err));
    }
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (sg_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            sg_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        sg_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static int connect_wifi(const char *ssid, const char *password, uint32_t timeout_ms)
{
    esp_err_t err = ESP_OK;
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta =
            {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg            = {.capable = true, .required = false},
            },
    };
    memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));
    memcpy(wifi_config.sta.password, password, strlen(password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
                                           timeout_ms / portTICK_PERIOD_MS);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", ssid, password);
        err = ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s, password:%s", ssid, password);
        err = ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        err = ESP_FAIL;
    }
    return err;
}

int HAL_Wifi_Init(void)
{
    return 0;
}

int HAL_Wifi_ModeSet(TCIoTWifiMode mode)
{
    return 0;
}

int HAL_Wifi_StaInfoSet(const char *ssid, uint8_t ssid_len, const char *passwd, uint8_t passwd_len)
{
    save_wifi_info(ssid, ssid_len, passwd, passwd_len);
    return 0;
}

int HAL_Wifi_StaConnect(uint32_t timeout_ms)
{
    char      ssid[32]     = {0};
    char      password[64] = {0};
    esp_err_t err          = get_wifi_info(ssid, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get WiFi info");
        return err;
    }
    err = connect_wifi(ssid, password, timeout_ms);
    return err;
}

int HAL_Wifi_LogGet(void)
{
    // nothing todo
    return 0;
}

uint32_t HAL_Wifi_Ipv4Get(void)
{
    return local_ipv4_addr;
}

size_t HAL_Wifi_MacGet(uint8_t *mac)
{
    return 6;
}

int HAL_Wifi_StartStaConnect(const char *ssid, const char *passwd, uint32_t timeout_ms)
{
    esp_err_t err;
    if (ssid == NULL && passwd == NULL) {
        char ssid1[32]     = {0};
        char password1[64] = {0};
        err                = get_wifi_info(ssid1, password1);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get WiFi info");
            return err;
        }
        err = connect_wifi(ssid1, password1, timeout_ms);
        return err;
    }

    return connect_wifi(ssid, passwd, timeout_ms);
}