/**
 * @file HAL_SoftAP_config.c
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2022-09-06
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
 * 2022-09-06		1.0			hubertxxu		first commit
 * </table>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "qcloud_iot_platform.h"

static const char *TAG = "wifi softAP";

int HAL_SoftAP_Start(const char *ssid, const char *password, uint8_t ch)
{
    int rc = HAL_Wifi_Init();
    if (rc) {
        Log_e("wifi init fail %d", rc);
        return rc;
    }
    wifi_config_t wifi_config = {
        .ap =
            {
                .channel        = ch,
                .max_connection = 5,
                .authmode       = WIFI_AUTH_WPA_WPA2_PSK,
            },
    };
    strncpy((char *)wifi_config.ap.ssid, ssid, 32);
    wifi_config.ap.ssid_len = strlen(ssid);

    if (password) {
        strncpy((char *)wifi_config.ap.password, password, 64);
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    HAL_Wifi_ModeSet(TC_IOT_WIFI_MODE_AP);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d", ssid, password, ch);
    return QCLOUD_RET_SUCCESS;
}

int HAL_SoftAP_Stop(void)
{
    Log_i("stop SoftAP");
    ESP_ERROR_CHECK(esp_wifi_stop());
    return QCLOUD_RET_SUCCESS;
}
