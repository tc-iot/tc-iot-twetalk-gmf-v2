/**
 * @file qcloud_iot_wifi_config.h
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2022-09-05
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
 * 2022-09-05		1.0			hubertxxu		first commit
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_INCLUDE_COMPONENT_QCLOUD_IOT_WIFI_CONFIG_H_
#define IOT_HUB_DEVICE_C_SDK_INCLUDE_COMPONENT_QCLOUD_IOT_WIFI_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "qcloud_iot_common.h"
#include "qcloud_iot_wifi_bind.h"

/**
 * @brief Params for wifi config.
 *
 */
typedef union {
    struct {
        const char *ssid;
        const char *pwd;
        uint8_t     ch;
    } softap;
} IotWifiConfigParams;

/**
 * @brief Wifi config.
 *
 * @param[in] config_type @see IotWifiBindType
 * @param[in] params @see IotWifiConfigParams
 * @param[in] timeout_ms timeout for config
 * @return @see IotReturnCode
 */
int iot_wifi_config(IotWifiBindType config_type, const IotWifiConfigParams *params, uint64_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_INCLUDE_COMPONENT_QCLOUD_IOT_WIFI_CONFIG_H_
