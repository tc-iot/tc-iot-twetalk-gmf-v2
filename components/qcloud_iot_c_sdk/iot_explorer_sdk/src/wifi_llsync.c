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
 * @file wifi_llsync.c
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2023-01-05
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2023-01-05 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#include "wifi_config.h"
//     // -------------------------------------------------------------------------------------------------
//     // test
//     // -------------------------------------------------------------------------------------------------

//     /**
//      * @brief Test function
//      *
//      *
//      */
//     typedef struct {
//     int (*set_wifi_mode)(TCIoTWifiMode mode, void *usr_data);
//     int (*set_wifi_info)(const char *ssid, size_t ssid_len, const char *pwd, size_t pwd_len, void *usr_data);
//     int (*connect_wifi)(void *usr_data);
//     int (*set_wifi_token)(const char *token, size_t token_len, void *usr_data);
//     int (*get_wifi_log)(void *usr_data);
// } IotLlsyncWifiCallback;
// static int  iot_llsync_init(void) {}
// static int  iot_llsync_register_wifi_callback(IotLlsyncWifiCallback callback, void *usr_data) {}
// static void iot_llsync_unregister_wifi_callback(void) {}
#include "qcloud_iot_llsync.h"

// -------------------------------------------------------------------------------------------------
// callback
// -------------------------------------------------------------------------------------------------
typedef struct {
    WifiInfo        *info;
    IotWifiBindTime *bind_time;
    IotBool          recv_result;
    int              result;
} LlsyncWifiConfigResult;

static int _set_wifi_mode(TCIoTWifiMode mode, void *usr_data)
{
    POINTER_SANITY_CHECK(usr_data, QCLOUD_ERR_INVAL);

    LlsyncWifiConfigResult *config_result = (LlsyncWifiConfigResult *)usr_data;

    int rc = HAL_Wifi_ModeSet(mode);
    if (rc) {
        config_result->result      = rc;
        config_result->recv_result = IOT_BOOL_TRUE;
    }
    return rc;
}

static int _set_wifi_info(const char *ssid, size_t ssid_len, const char *pwd, size_t pwd_len, void *usr_data)
{
    POINTER_SANITY_CHECK(usr_data, QCLOUD_ERR_INVAL);

    LlsyncWifiConfigResult *config_result   = (LlsyncWifiConfigResult *)usr_data;
    config_result->bind_time->get_ssid_time = HAL_Timer_CurrentMs();
    memcpy(config_result->info->ssid, ssid, ssid_len);
    memcpy(config_result->info->pwd, ssid, ssid_len);
    int rc = HAL_Wifi_StaInfoSet(ssid, ssid_len, pwd, pwd_len);
    if (rc) {
        config_result->result      = rc;
        config_result->recv_result = IOT_BOOL_TRUE;
    }
    return rc;
}

static int _connect_wifi(size_t timeout_ms, void *usr_data)
{
    POINTER_SANITY_CHECK(usr_data, QCLOUD_ERR_INVAL);

    LlsyncWifiConfigResult *config_result         = (LlsyncWifiConfigResult *)usr_data;
    config_result->bind_time->wifi_connected_time = HAL_Timer_CurrentMs();

    int rc = HAL_Wifi_StaConnect(timeout_ms);
    if (rc) {
        config_result->result      = rc;
        config_result->recv_result = IOT_BOOL_TRUE;
    }
    return rc;
}

static int _set_wifi_token(const char *token, size_t token_len, void *usr_data)
{
    POINTER_SANITY_CHECK(usr_data, QCLOUD_ERR_INVAL);

    LlsyncWifiConfigResult *config_result    = (LlsyncWifiConfigResult *)usr_data;
    config_result->bind_time->get_token_time = HAL_Timer_CurrentMs();
    memcpy(config_result->info->token, token, token_len);
    config_result->result      = QCLOUD_RET_SUCCESS;  // 0 for success
    config_result->recv_result = IOT_BOOL_TRUE;
    return QCLOUD_RET_SUCCESS;
}

static int _get_wifi_log(void *usr_data)
{
    return HAL_Wifi_LogGet();
}

// -------------------------------------------------------------------------------------------------
// api
// -------------------------------------------------------------------------------------------------

/**
 * @brief Get wifi info & connect to wifi.
 *
 * @param[in] params @see IotWifiConfigParams
 * @param[out] info @see WifiInfo
 * @param[out] bind_time @see bind_time
 * @param[in] timeout_ms timeout for config
 * @return @see IotReturnCode
 */
int iot_wifi_config_llsync(const IotWifiConfigParams *params, WifiInfo *info, IotWifiBindTime *bind_time,
                           uint64_t timeout_ms)
{
    int rc = 0;
    // 1. ble check init
    if (!iot_llsync_is_init()) {
        IOTBLELLsyncInitParams llsync_init_params = DEFAULT_LLSYNC_INIT_PARAMS;
        static DeviceInfo      dev_info;
        HAL_GetDevInfo(&dev_info);
        llsync_init_params.dev_info  = &dev_info;
        llsync_init_params.start_adv = timeout_ms / 1000;
        rc                           = iot_llsync_init(llsync_init_params);
        if (rc) {
            Log_e("iot_llsync_init fail. rc : %d", rc);
            return rc;
        }
    } else {
        iot_llsync_start_adv(timeout_ms / 1000);
    }

    // 2. wifi init
    rc = HAL_Wifi_Init();
    if (rc) {
        return rc;
    }

    // 3. register callback
    LlsyncWifiConfigResult config_result = {
        .info        = info,
        .bind_time   = bind_time,
        .recv_result = IOT_BOOL_FALSE,
        .result      = QCLOUD_ERR_FAILURE,
    };

    IOTBLELLsyncWifiCallback callback = {
        .connect_wifi   = _connect_wifi,
        .set_wifi_mode  = _set_wifi_mode,
        .set_wifi_info  = _set_wifi_info,
        .set_wifi_token = _set_wifi_token,
        .get_wifi_log   = _get_wifi_log,
    };
    iot_llsync_register_wifi_callback(callback, &config_result);

    // 4. wait for token
    QcloudIotTimer timer = 0;
    IOT_Timer_CountdownMs(&timer, timeout_ms);
    do {
        HAL_SleepMs(100);
    } while (!IOT_Timer_Expired(&timer) && !config_result.recv_result);

    iot_llsync_unregister_wifi_callback();
    return config_result.result;
}
