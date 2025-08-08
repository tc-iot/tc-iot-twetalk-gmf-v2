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
 * @file wifi_config.c
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2023-01-04
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2023-01-04 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#include "qcloud_iot_wifi_config.h"
#include "wifi_config.h"

static int _check_dyn_reg(void)
{
    int        rc          = 0;
    DeviceInfo device_info = {0};

    rc = HAL_GetDevInfo(&device_info);
    if (rc) {
        return QCLOUD_ERR_FAILURE;
    }

    if (!strncmp(device_info.device_secret, "IOT_PSK", sizeof("IOT_PSK") - 1) || !device_info.device_secret[0]) {
        rc = IOT_DynReg_Device(&device_info);
        if (!rc) {
            rc = HAL_SetDevInfo(&device_info);
        }
    }
    return rc;
}

static void _wifi_bind_event_callback(IotWifiBindEvent event)
{
#ifdef WIFI_CONFIG_SOFT_AP_USED
    const WifiConfigState state[] = {
        [IOT_WIFI_BIND_EVENT_MQTT_CONNECT_BEGIN]        = WIFI_CONFIG_STATE_CONNECT_MQTT,
        [IOT_WIFI_BIND_EVENT_MQTT_CONNECT_FAIL]         = WIFI_CONFIG_STATE_CONNECT_MQTT_FAIL,
        [IOT_WIFI_BIND_EVENT_MQTT_REPORT_TOKEN_BEGIN]   = WIFI_CONFIG_STATE_REPORT_TOKEN,
        [IOT_WIFI_BIND_EVENT_MQTT_REPORT_TOKEN_SUCCESS] = WIFI_CONFIG_STATE_REPORT_TOKEN_SUCCESS,
        [IOT_WIFI_BIND_EVENT_MQTT_REPORT_TOKEN_FAIL]    = WIFI_CONFIG_STATE_REPORT_TOKEN_FAIL,
    };
    int fd = iot_wifi_udp_init();
    if (fd < 0) {
        return;
    }
    iot_wifi_udp_broadcast_state(fd, state[event]);
    HAL_SleepMs(1000);  // sleep for udp send
    iot_wifi_udp_deinit(fd);
#endif
}

/**
 * @brief Wifi config.
 *
 * @param[in] config_type @see IotWifiBindType
 * @param[in] params @see IotWifiConfigParams
 * @param[in] timeout_ms timeout for config
 * @return @see IotReturnCode
 */
int iot_wifi_config(IotWifiBindType config_type, const IotWifiConfigParams *params, uint64_t timeout_ms)
{
    int rc = 0;

    QcloudIotTimer  wifi_config_timer = {0};
    IotWifiBindTime bind_time         = {0};

    IOT_Timer_CountdownMs(&wifi_config_timer, timeout_ms);
    bind_time.start_time = HAL_Timer_CurrentMs();

    // 1. wifi connect & get token
    WifiInfo wifi_info = {0};

    const WifiConfigImp wifi_config_imp[] = {
#ifdef WIFI_CONFIG_SOFT_AP_USED
        [IOT_WIFI_BIND_TYPE_SOFT_AP] = iot_wifi_config_softap,
#else
        [IOT_WIFI_BIND_TYPE_SOFT_AP]    = NULL,
#endif
        [IOT_WIFI_BIND_TYPE_SMART_CONFIG] = NULL,
        [IOT_WIFI_BIND_TYPE_AIR_KISS]     = NULL,
#ifdef WIFI_CONFIG_BLE_LLSYNC_USED
        [IOT_WIFI_BIND_TYPE_LLSYNC_BLE] = iot_wifi_config_llsync,
#else
        [IOT_WIFI_BIND_TYPE_LLSYNC_BLE] = NULL,
#endif
        [IOT_WIFI_BIND_TYPE_SIMPLE_CONFIG] = NULL,
        [IOT_WIFI_BIND_TYPE_CUSTOM_BLE]    = NULL,
    };

    if (!wifi_config_imp[config_type]) {
        Log_e("no support yet!");
        return QCLOUD_ERR_INVAL;
    }

    rc = wifi_config_imp[config_type](params, &wifi_info, &bind_time, IOT_Timer_Remain(&wifi_config_timer));
    if (rc) {
        return rc;
    }

    // 2. check dyn reg
    rc = _check_dyn_reg();
    if (rc) {
        return rc;
    }

    // 3. connect mqtt&&send token to cloud
    return IOT_WifiBind_Sync(config_type, wifi_info.token, &bind_time, IOT_Timer_Remain(&wifi_config_timer),
                             _wifi_bind_event_callback);
}
