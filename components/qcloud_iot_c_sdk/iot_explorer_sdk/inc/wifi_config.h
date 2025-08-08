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
 * @file wifi_config.h
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

#ifndef IOT_HUB_DEVICE_C_SDK_COMPONENT_WIFI_CONFIG_INC_WIFI_CONFIG_H_
#define IOT_HUB_DEVICE_C_SDK_COMPONENT_WIFI_CONFIG_INC_WIFI_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "qcloud_iot_common.h"
#include "qcloud_iot_wifi_config.h"

typedef enum {
    WIFI_CONFIG_STATE_CONNECT_AP = 0,     // When the ssid&password is obtained, try to connect to it
    WIFI_CONFIG_STATE_CONNECT_MQTT,       // Try to connect to the MQTT server
    WIFI_CONFIG_STATE_CONNECT_MQTT_FAIL,  // Try to connect to the MQTT server fail
    WIFI_CONFIG_STATE_REPORT_TOKEN,       // Try to connect and report token
    WIFI_CONFIG_STATE_REPORT_TOKEN_FAIL,
    WIFI_CONFIG_STATE_REPORT_TOKEN_SUCCESS
} WifiConfigState;

typedef enum {
    WIFI_CONFIG_CMD_TOKEN_ONLY = 0,            // Token only for smart config
    WIFI_CONFIG_CMD_SSID_PW_TOKEN,             // SSID/PW/TOKEN for softAP
    WIFI_CONFIG_CMD_DEVICE_REPLY,              // device reply
    WIFI_CONFIG_CMD_LOG_QUERY,                 // app query log
    WIFI_CONFIG_CMD_AUTHINFO_QUERY,            // query auth info
    WIFI_CONFIG_CMD_AUTHINFO_REPLY,            // reply auth info
    WIFI_CONFIG_CMD_RESPOSE_TOKEN,             // response token from router
    WIFI_CONFIG_CMD_REPORT_WIFI_CONFIG_STATE,  // when wifi connected ap, report state to wechat app
} WifiConfigCmd;

typedef union {
    WifiConfigState state;
} WifiConfigCmdParams;

#define MAX_WIFI_SSID_LENGTH  32
#define MAX_WIFI_PWD_LENGTH   64
#define MAX_WIFI_TOKEN_LENGTH 32

typedef struct {
    char ssid[MAX_WIFI_SSID_LENGTH + 1];
    char pwd[MAX_WIFI_PWD_LENGTH + 1];
    char token[MAX_WIFI_TOKEN_LENGTH + 1];
} WifiInfo;

/**
 * @brief Get wifi info & connect to wifi.
 *
 * @param[in] params @see SoftApInitParams
 * @param[out] info @see WifiInfo
 * @param[out] bind_time @see bind_time
 * @param[in] timeout_ms timeout for config
 * @return @see IotReturnCode
 */
typedef int (*WifiConfigImp)(const IotWifiConfigParams *params, WifiInfo *info, IotWifiBindTime *bind_time,
                             uint64_t timeout_ms);

// -------------------------------------------------------------------------------------------------
// softap
// -------------------------------------------------------------------------------------------------

/**
 * @brief SoftAp: Get wifi info & connect to wifi.
 *
 * @param[in] params @see SoftApInitParams
 * @param[out] info @see WifiInfo
 * @param[out] bind_time @see bind_time
 * @param[in] timeout_ms timeout for config
 * @return @see IotReturnCode
 */
int iot_wifi_config_softap(const IotWifiConfigParams *params, WifiInfo *info, IotWifiBindTime *bind_time,
                           uint64_t timeout_ms);

// -------------------------------------------------------------------------------------------------
// ble llsync
// -------------------------------------------------------------------------------------------------

/**
 * @brief BLE LLSYNC: Get wifi info & connect to wifi.
 *
 * @param[in] params @see IotWifiConfigParams
 * @param[out] info @see WifiInfo
 * @param[out] bind_time @see bind_time
 * @param[in] timeout_ms timeout for config
 * @return @see IotReturnCode
 */
int iot_wifi_config_llsync(const IotWifiConfigParams *params, WifiInfo *info, IotWifiBindTime *bind_time,
                           uint64_t timeout_ms);

// -------------------------------------------------------------------------------------------------
// udp
// -------------------------------------------------------------------------------------------------

#define WIFI_UDP_SERVER_IP       "0.0.0.0"
#define WIFI_UDP_SERVER_PORT     8266
#define WIFI_UDP_BROADCAST_IP    "255.255.255.255"
#define WIFI_UDP_BROADCAST_PORT  "18266"
#define WIFI_UDP_ADDR_MAX_LENGTH 16
#define WIFI_UDP_PORT_MAX_LENGTH 6

/**
 * @brief Udp socket init.
 *
 * @return udp socket fd
 */
int iot_wifi_udp_init(void);

/**
 * @brief Udp socket deinit. Close socket.
 *
 * @param[in] fd udp socket fd
 */
void iot_wifi_udp_deinit(int fd);

/**
 * @brief Recv form udp port.
 *
 * @param[in] fd udp socket fd
 * @param[out] ip received ip
 * @param[in] ip_len ip length
 * @param[in] port received port
 * @param[in] port_len port length
 * @param[in] buf recv buf
 * @param[in] buf_len recv buf length
 * @param[in] timeout_ms timeout for recv
 * @return > 0 for recv length
 */
int iot_wifi_udp_recv(int fd, char *ip, size_t ip_len, char *port, size_t port_len, uint8_t *buf, size_t buf_len,
                      uint32_t timeout_ms);

/**
 * @brief Send device reply message.
 *
 * @param[in] fd udp socket fd
 * @param[in] ip dst ip to send
 * @param[in] port dst port to send
 * @return @see IotReturnCode
 */
int iot_wifi_udp_device_reply(int fd, const char *ip, const char *port);

/**
 * @brief Report wifi config state.
 *
 * @param[in] fd udp socket fd
 * @param[in] state state
 * @return @see IotReturnCode
 */
int iot_wifi_udp_broadcast_state(int fd, WifiConfigState state);

/**
 * @brief Broadcast local ip to notify mini program.
 * protocol: | ssid_len + pwd_len + 9 1B | invalid mac fill 6B | big endian local ipv4 4B |
 * The unicast MAC address refers to the MAC address with the lowest bit of the first byte being 0.
 * Multicast MAC address refers to the MAC address with 1 in the lowest bit of the first byte.
 * The broadcast MAC address refers to the MAC address with 1 in each bit. Broadcast MAC address is a
 * special case of multicast MAC address.
 * @param[in] fd udp socket fd
 */
void iot_wifi_udp_broadcast_local_ip(int fd, size_t ssid_len, size_t pwd_len);

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_COMPONENT_WIFI_CONFIG_INC_WIFI_CONFIG_H_
