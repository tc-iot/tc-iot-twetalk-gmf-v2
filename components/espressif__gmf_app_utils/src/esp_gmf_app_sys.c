/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static bool monitor_run;

static void sys_monitor_task(void *para)
{
    while (monitor_run) {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        if (monitor_run == false) {
            break;
        }
        ESP_GMF_MEM_SHOW("MONITOR");
        esp_gmf_oal_sys_get_real_time_stats(1000, false);
    }
    vTaskDelete(NULL);
}

void esp_gmf_app_sys_monitor_start(void)
{
    monitor_run = true;
    xTaskCreatePinnedToCore(sys_monitor_task, "sys_monitor_task", (4 * 1024), NULL, 1, NULL, 1);
}

void esp_gmf_app_sys_monitor_stop(void)
{
    monitor_run = false;
}
