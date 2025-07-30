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

void app_main(void)
{
    log_clear();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // esp_gmf_app_sys_monitor_start();
    // while (1) {
    //     printf("get_real_time_stats\r\n\r\n");
    //     esp_gmf_oal_sys_get_real_time_stats(1000, true);
    //     vTaskDelay(pdMS_TO_TICKS(60000));
    //     printf("\r\n\r\n");
    // }
}
