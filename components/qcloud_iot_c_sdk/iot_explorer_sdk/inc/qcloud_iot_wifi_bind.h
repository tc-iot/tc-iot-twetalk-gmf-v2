/**
 * @copyright
 *
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2018 - 2023 THL A29 Limited, a Tencent company.All rights reserved.
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
 * @file qcloud_iot_wifi_bind.h
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2023-01-03
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2023-01-03 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_EXPLORER_QCLOUD_IOT_WIFI_BIND_H_
#define IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_EXPLORER_QCLOUD_IOT_WIFI_BIND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "qcloud_iot_common.h"

/**
 * @brief Wifi bind type support by mini program.
 *
 */
typedef enum {
    IOT_WIFI_BIND_TYPE_SOFT_AP,
    IOT_WIFI_BIND_TYPE_SMART_CONFIG,
    IOT_WIFI_BIND_TYPE_AIR_KISS,
    IOT_WIFI_BIND_TYPE_LLSYNC_BLE,
    IOT_WIFI_BIND_TYPE_SIMPLE_CONFIG,
    IOT_WIFI_BIND_TYPE_CUSTOM_BLE,
} IotWifiBindType;

/**
 * @brief Bind time using for debug.
 *
 */
typedef struct {
    uint64_t start_time;
    uint64_t get_ssid_time;
    uint64_t wifi_connected_time;
    uint64_t get_token_time;
} IotWifiBindTime;

/**
 * @brief Bind event.
 *
 */
typedef enum {
    IOT_WIFI_BIND_EVENT_MQTT_CONNECT_BEGIN,
    IOT_WIFI_BIND_EVENT_MQTT_CONNECT_FAIL,
    IOT_WIFI_BIND_EVENT_MQTT_REPORT_TOKEN_BEGIN,
    IOT_WIFI_BIND_EVENT_MQTT_REPORT_TOKEN_SUCCESS,
    IOT_WIFI_BIND_EVENT_MQTT_REPORT_TOKEN_FAIL,
} IotWifiBindEvent;

/**
 * @brief Send wifi bind msg and wait for reply.
 *
 * @param[in] type @see IotWifiBindType
 * @param[in] token token get from mini program
 * @param[in] bind_time using for debug @see IotWifiBindTime
 * @param[in] timeout_ms max timeout wait for reply
 * @return @see IotReturnCode
 */
int IOT_WifiBind_Sync(IotWifiBindType type, const char *token, const IotWifiBindTime *bind_time, uint64_t timeout_ms,
                      void (*event_callback)(IotWifiBindEvent event));

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_EXPLORER_QCLOUD_IOT_WIFI_BIND_H_
