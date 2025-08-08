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

// 使用SPIFFS作为文件系统，否则使用SD卡
#define USE_SPIFFS

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT        BIT0
#define WIFI_FAIL_BIT             BIT1
#define EXAMPLE_ESP_MAXIMUM_RETRY 20

static char *TAG = "main";


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
#if 0
    // check file exist
    DIR* dir = opendir("/sdcard/model");
    assert(dir != NULL);
    while (true) {
        struct dirent* pe = readdir(dir);
        if (!pe)
            break;
        ESP_LOGI(__FUNCTION__, "d_name=%s d_ino=%d d_type=%x", pe->d_name, pe->d_ino, pe->d_type);
    }
    closedir(dir);
#endif
}

void app_main(void)
{
    log_clear();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    spiffs_init();
    extern int HAL_Wifi_StartStaConnect(const char *ssid, const char *passwd, uint32_t timeout_ms);
    err = HAL_Wifi_StartStaConnect(CONFIG_EXAMPLE_WIFI_SSID, CONFIG_EXAMPLE_WIFI_PASSWORD, 10000);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Wifi connect failed");
        return;
    }

    tc_twetalk_init();

    // esp_gmf_app_sys_monitor_start();
    // while (1) {
    //     printf("get_real_time_stats\r\n\r\n");
    //     esp_gmf_oal_sys_get_real_time_stats(1000, true);
    //     vTaskDelay(pdMS_TO_TICKS(60000));
    //     printf("\r\n\r\n");
    // }
}
