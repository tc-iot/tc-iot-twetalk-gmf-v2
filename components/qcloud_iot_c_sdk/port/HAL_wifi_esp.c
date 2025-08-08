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

#define EXAMPLE_ESP_MAXIMUM_RETRY 10

static int      sg_retry_num    = 0;
static IotBool  sg_gotip_flag   = IOT_BOOL_FALSE;
static uint32_t local_ipv4_addr = 0xC0A80401;  // 192.168.4.1

static esp_err_t save_wifi_info(const uint8_t *ssid, const uint8_t *password)
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

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_config_t cfg;
    // sta
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        esp_wifi_get_config(WIFI_IF_STA, &cfg);
        ESP_LOGI(TAG, "connect to the AP[%s/%s]", cfg.sta.ssid, cfg.sta.password);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_get_config(WIFI_IF_STA, &cfg);
        if (sg_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            sg_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP[%s/%s]", cfg.sta.ssid, cfg.sta.password);
        } else {
            ESP_LOGI(TAG, "connect to the AP fail[%s/%s]", cfg.sta.ssid, cfg.sta.password);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        esp_wifi_get_config(WIFI_IF_STA, &cfg);
        save_wifi_info(cfg.sta.ssid, cfg.sta.password);
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        sg_retry_num    = 0;
        sg_gotip_flag   = IOT_BOOL_TRUE;
        local_ipv4_addr = event->ip_info.ip.addr;
    }
    // soft ap
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

int HAL_Wifi_Init(void)
{
    static IotBool init_flag = IOT_BOOL_FALSE;
    if (init_flag) {
        return QCLOUD_RET_SUCCESS;
    }
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));
    // ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi init finished.");
    init_flag = IOT_BOOL_TRUE;
    return 0;
}

int HAL_Wifi_ModeSet(TCIoTWifiMode mode)
{
    if (mode == TC_IOT_WIFI_MODE_STA) {
        esp_netif_create_default_wifi_sta();
    } else if (mode == TC_IOT_WIFI_MODE_AP) {
        esp_netif_create_default_wifi_ap();
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    return 0;
}

int HAL_Wifi_StaInfoSet(const char *ssid, uint8_t ssid_len, const char *passwd, uint8_t passwd_len)
{
    wifi_config_t wifi_config = {
        .sta =
            {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg            = {.capable = IOT_BOOL_TRUE, .required = IOT_BOOL_FALSE},
            },
    };

    ESP_LOGI(TAG, "wifi info set");
    memset(wifi_config.sta.ssid, 0, sizeof(wifi_config.sta.ssid));
    memset(wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));
    memcpy(wifi_config.sta.ssid, ssid, ssid_len);
    memcpy(wifi_config.sta.password, passwd, passwd_len);
    ESP_LOGI(TAG, "wifi info ssid: %s, password: %s", wifi_config.sta.ssid, wifi_config.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    return 0;
}

int HAL_Wifi_StaConnect(uint32_t timeout_ms)
{
    int rc        = 0;
    sg_gotip_flag = IOT_BOOL_FALSE;
    QcloudIotTimer timer;
    IOT_Timer_CountdownMs(&timer, timeout_ms);
    ESP_LOGI(TAG, "wifi connect");
    ESP_ERROR_CHECK(esp_wifi_connect());
    while (!IOT_Timer_Expired(&timer)) {
        ESP_LOGI(TAG, "wait wifi connect ...%d", rc++);
        if (sg_gotip_flag) {
            rc = QCLOUD_RET_SUCCESS;
            break;
        }
        HAL_SleepMs(500);
    }
    return rc;
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
    int rc = 0;
    rc     = HAL_Wifi_Init();
    if (rc != 0) {
        return rc;
    }
    rc = HAL_Wifi_ModeSet(TC_IOT_WIFI_MODE_STA);
    if (rc != 0) {
        return rc;
    }
    rc = HAL_Wifi_StaInfoSet(ssid, strlen(ssid), passwd, strlen(passwd));
    if (rc != 0) {
        return rc;
    }
    rc = HAL_Wifi_StaConnect(timeout_ms);
    if (rc != 0) {
        return rc;
    }
    return rc;
}