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
 * @file wifi_udp.c
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

#include "wifi_config.h"

// -------------------------------------------------------------------------------------------------
// function
// -------------------------------------------------------------------------------------------------

static int _softap_udp_msg_parse(WifiInfo *info, uint8_t *recv_buf, int recv_len)
{
    // {"cmdType":1,"ssid":"L-004","password":"你的路由器密码","token":"30a35fb58a64c02b39c250f9b9c49958"}
    int      rc = 0;
    uint32_t cmd_type;
    rc = utils_json_get_uint32("cmdType", sizeof("cmdType") - 1, (const char *)recv_buf, recv_len, &cmd_type);
    if (rc) {
        return QCLOUD_ERR_JSON_PARSE;
    }

    if (cmd_type != WIFI_CONFIG_CMD_SSID_PW_TOKEN) {
        return QCLOUD_ERR_FAILURE;
    }

    UtilsJsonValue ssid, pwd, token;
    rc = utils_json_value_get("ssid", sizeof("ssid") - 1, (const char *)recv_buf, recv_len, &ssid);
    rc |= utils_json_value_get("password", sizeof("password") - 1, (const char *)recv_buf, recv_len, &pwd);
    rc |= utils_json_value_get("token", sizeof("token") - 1, (const char *)recv_buf, recv_len, &token);
    if (rc) {
        return QCLOUD_ERR_JSON_PARSE;
    }

    // copy wifi info
    memset(info, 0, sizeof(WifiInfo));
    memcpy(info->ssid, ssid.value, ssid.value_len > MAX_WIFI_SSID_LENGTH ? MAX_WIFI_SSID_LENGTH : ssid.value_len);
    memcpy(info->pwd, pwd.value, pwd.value_len > MAX_WIFI_PWD_LENGTH ? MAX_WIFI_PWD_LENGTH : pwd.value_len);
    memcpy(info->token, token.value, token.value_len > MAX_WIFI_TOKEN_LENGTH ? MAX_WIFI_TOKEN_LENGTH : token.value_len);
    return QCLOUD_RET_SUCCESS;
}

static int _wifi_connect(const char *ssid, int ssid_len, const char *pwd, int pwd_len)
{
    Log_i("STA to connect ssid: %.*s, password : %.*s", ssid_len, ssid, pwd_len, pwd);
    int rc = TCI_HAL_Wifi_ModeSet(TCI_TCIOT_WIFI_MODE_STA);
    rc |= TCI_HAL_Wifi_StaInfoSet(ssid, ssid_len, pwd, pwd_len);
    rc |= TCI_HAL_Wifi_StaConnect(30000);
    return rc;
}

static int _softap_wifi_config(int fd, WifiInfo *info, IotWifiBindTime *bind_time, uint64_t timeout_ms)
{
    uint8_t udp_recv_buf[1024] = {0};
    int     rc = QCLOUD_ERR_FAILURE, select_err_cnt = 0;
    char    ip[WIFI_UDP_ADDR_MAX_LENGTH] = {0};
    char    port[6]                      = {0};

    TCI_Timer timer;
    TCI_HAL_TimerCountdownMs(&timer, timeout_ms);
    while (!TCI_HAL_TimerExpired(&timer)) {
        rc = iot_wifi_udp_recv(fd, ip, sizeof(ip), port, sizeof(port), udp_recv_buf, sizeof(udp_recv_buf), 1000);
        if (!rc) {  // nothing recv
            iot_wifi_udp_broadcast_local_ip(fd, 0, 0);
            continue;
        }

        if (rc < 0) {  // fail
            select_err_cnt++;
            Log_w("select-recv error cnt: %d", select_err_cnt);
            if (select_err_cnt > 3) {
                Log_e("select-recv error(%d) cnt: %d", rc, select_err_cnt);
                break;
            }
            TCI_HAL_SleepMs(500);
            continue;
        }

        Log_i("Received %d bytes from <%s:%s> msg: %s", rc, ip, port, udp_recv_buf);
        rc = _softap_udp_msg_parse(info, udp_recv_buf, rc);
        if (rc) {
            continue;
        }
        bind_time->get_ssid_time = bind_time->get_token_time = TCI_HAL_GetTimeMs();

        iot_wifi_udp_device_reply(fd, ip, port);
        TCI_HAL_SleepMs(1000);
        TCI_HAL_SoftAP_Stop();

        rc = _wifi_connect(info->ssid, strlen(info->ssid), info->pwd, strlen(info->pwd));
        if (!rc) {
            bind_time->wifi_connected_time = TCI_HAL_GetTimeMs();
        }
        return rc;
    }
    return rc;
}

// -------------------------------------------------------------------------------------------------
// API
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
int iot_wifi_config_softap(const IotWifiConfigParams *params, WifiInfo *info, IotWifiBindTime *bind_time,
                           uint64_t timeout_ms)
{
    int rc = 0;

    TCI_Timer timer;
    TCI_HAL_TimerCountdownMs(&timer, timeout_ms);
    bind_time->start_time = TCI_HAL_GetTimeMs();

    rc = TCI_HAL_SoftAP_Start(params->softap.ssid, params->softap.pwd, params->softap.ch);
    if (rc) {
        return rc;
    }

    int fd = iot_wifi_udp_init();
    if (fd < 0) {
        return QCLOUD_ERR_FAILURE;
    }
    rc = _softap_wifi_config(fd, info, bind_time, TCI_HAL_TimerRemain(&timer));
    iot_wifi_udp_deinit(fd);
    return rc;
}
