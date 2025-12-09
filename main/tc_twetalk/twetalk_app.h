/**
 * @file twetalk_app.h
 * @author hubertxxu (hubertxxu@tencent.com)
 * @brief
 * @version 0.1
 * @date 2025-07-31
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

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data_template_app.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "qcloud_iot_common.h"
#include "twetalk.h"
#include "twetalk_ws.h"
#include "utils_log.h"
#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
#include "esp_gmf_afe.h"
#endif /* CONFIG_KEY_PRESS_DIALOG_MODE */
#include "audio_processor.h"
#include "button_key.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_sys.h"
#include "esp_gmf_oal_thread.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ota_downloader.h"
#include "pca9557.h"
#include "qcloud_iot_wifi_config.h"

typedef enum
{
    LOCALPLAY_CONNECTING = 0,  // 正在联网
    LOCALPLAY_CONNECTED,       // 联网成功
    LOCALPLAY_HELLO,           // 收到hello
    LOCALPLAY_DONG,            // 收到dong
    LOCALPLAY_PAIR_NETWORK,    // 配网
    LOCALPLAY_CLEAR_NETWORK,   // 清除配网
    LOCALPLAY_ENTER_KEY_MODE,  // 进入按键模式
    LOCALPLAY_EXIT_KEY_MODE,   // 退出按键模式
    LOCALPLAY_MAX,
} LocalPlayE;
extern const char* tone_uri[];

/**
 * @brief 启动twetalk
 *
 * @param is_net_connected 0:未联网 1:已联网
 * @return 0 for success, negative for error
 */
int tc_twetalk_init(int is_net_connected);