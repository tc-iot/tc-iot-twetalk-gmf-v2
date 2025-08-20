/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "audio_processor.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_gmf_app_sys.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_sys.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "time.h"
#include "twetalk_app.h"

static char *TAG = "main";

extern int HAL_Wifi_StartStaConnect(const char *ssid, const char *passwd, uint32_t timeout_ms);

static void log_clear(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
}

static void spiffs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/sdcard", .partition_label = "storage", .max_files = 4, .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d Kbytes, used: %d Kbytes", total >> 10, used >> 10);
    }
}

void app_main(void)
{
    int wifi_conneted = 1;
    log_clear();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    spiffs_init();
#ifdef CONFIG_TWETALK_USE_NETCONFIG
    err = HAL_Wifi_StartStaConnect(NULL, NULL, 120 * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wifi connect failed");
        wifi_conneted = 0;
    }
#else
    err = HAL_Wifi_StartStaConnect(CONFIG_EXAMPLE_WIFI_SSID, CONFIG_EXAMPLE_WIFI_PASSWORD, 120 * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wifi connect failed");
        wifi_conneted = 0;
        return;  // 退出程序，不进行后续操作
    }
#endif  //  TWETALK_USE_NETCONFIG
    tc_twetalk_init(wifi_conneted);

    // esp_gmf_app_sys_monitor_start();
    // while (1) {
    //     printf("get_real_time_stats\r\n\r\n");
    //     esp_gmf_oal_sys_get_real_time_stats(1000, true);
    //     vTaskDelay(pdMS_TO_TICKS(60000));
    //     printf("\r\n\r\n");
    // }
}
