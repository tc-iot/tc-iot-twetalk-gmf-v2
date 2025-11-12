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

static int _udp_send(int fd, uint8_t *buf, int len, const char *ip, const char *port)
{
    int rc = 0;
    Log_d("%.*s", len, buf);
    size_t udp_resend_cnt = 3;
    do {
        rc = TCI_HAL_UDP_Send(fd, buf, len, ip, port);
    } while (rc > 0 && (--udp_resend_cnt));
    return udp_resend_cnt != 2 ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

static int _udp_construct_reply(WifiConfigCmd cmd, WifiConfigCmdParams *params, char *buf, size_t buf_len)
{
    int rc = 0, reply_len = 0;

    DeviceInfo device_info = {0};

    rc = TCI_HAL_GetDevInfo((uint8_t *)&device_info, sizeof(DeviceInfo));
    if (rc) {
        return rc;
    }

    reply_len = TCI_HAL_Snprintf(buf, buf_len,
                             "{\"cmdType\":%d,\"productId\":\"%s\",\"deviceName\":\"%s\",\"protoVersion\":\"3.0\"", cmd,
                             device_info.product_id, device_info.device_name);
    switch (cmd) {
        case WIFI_CONFIG_CMD_DEVICE_REPLY:
            break;
        case WIFI_CONFIG_CMD_REPORT_WIFI_CONFIG_STATE:
            reply_len += TCI_HAL_Snprintf(buf + reply_len, buf_len - reply_len, ",\"wifiConfigState\":%d", params->state);
            break;
        default:
            return QCLOUD_ERR_FAILURE;
    }
    reply_len += TCI_HAL_Snprintf(buf + reply_len, buf_len - reply_len, "}");
    return reply_len;
}

// -------------------------------------------------------------------------------------------------
// API
// -------------------------------------------------------------------------------------------------

/**
 * @brief Udp socket init. Bind on server port.
 *
 * @return udp socket fd
 */
int iot_wifi_udp_init(void)
{
    return TCI_HAL_UDP_Bind(WIFI_UDP_SERVER_IP, WIFI_UDP_SERVER_PORT);
}

/**
 * @brief Udp socket deinit. Close socket.
 *
 * @param[in] fd udp socket fd
 */
void iot_wifi_udp_deinit(int fd)
{
    return TCI_HAL_UDP_Close(fd);
}

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
                      uint32_t timeout_ms)
{
    uint16_t udp_port = 0;
    memset(ip, 0, ip_len);
    memset(port, 0, port_len);
    int rc = TCI_HAL_UDP_Recv(fd, buf, buf_len, timeout_ms, ip, ip_len, &udp_port);
    TCI_HAL_Snprintf(port, port_len, "%" SCNu16, udp_port);
    return rc;
}

/**
 * @brief Send device reply message.
 *
 * @param[in] fd udp socket fd
 * @param[in] ip dst ip to send
 * @param[in] port dst port to send
 * @return @see IotReturnCode
 */
int iot_wifi_udp_device_reply(int fd, const char *ip, const char *port)
{
    int rc = 0;

    uint8_t reply_buf[512] = {0};

    WifiConfigCmdParams params = {0};
    rc = _udp_construct_reply(WIFI_CONFIG_CMD_DEVICE_REPLY, &params, (char *)reply_buf, sizeof(reply_buf));
    if (rc <= 0) {
        return QCLOUD_ERR_BUF_TOO_SHORT;
    }
    return _udp_send(fd, reply_buf, rc, ip, port);
}

/**
 * @brief Report wifi config state.
 *
 * @param[in] fd udp socket fd
 * @param[in] state state
 * @return @see IotReturnCode
 */
int iot_wifi_udp_broadcast_state(int fd, WifiConfigState state)
{
    int rc = 0;

    uint8_t reply_buf[512] = {0};

    WifiConfigCmdParams params = {.state = state};
    rc = _udp_construct_reply(WIFI_CONFIG_CMD_REPORT_WIFI_CONFIG_STATE, &params, (char *)reply_buf, sizeof(reply_buf));
    if (rc <= 0) {
        return QCLOUD_ERR_BUF_TOO_SHORT;
    }
    return _udp_send(fd, reply_buf, rc, WIFI_UDP_BROADCAST_IP, WIFI_UDP_BROADCAST_PORT);
}

/**
 * @brief Broadcast local ip to notify mini program.
 * protocol: | ssid_len + pwd_len + 9 1B | invalid mac fill 6B | big endian local ipv4 4B |
 * The unicast MAC address refers to the MAC address with the lowest bit of the first byte being 0.
 * Multicast MAC address refers to the MAC address with 1 in the lowest bit of the first byte.
 * The broadcast MAC address refers to the MAC address with 1 in each bit. Broadcast MAC address is a
 * special case of multicast MAC address.
 * @param[in] fd udp socket fd
 */
void iot_wifi_udp_broadcast_local_ip(int fd, size_t ssid_len, size_t pwd_len)
{
    uint32_t ipv4 = TCI_HAL_Wifi_Ipv4Get();
    if (!ipv4) {
        Log_e("can not find local ipv4.");
        return;
    }

    size_t i = 0;

    uint8_t broadcast_buf[11] = {0};
    // ssid_len + pwd_len + 9
    broadcast_buf[i++] = ssid_len + pwd_len + 9;
    // mac
    i += TCI_HAL_Wifi_MacGet(&broadcast_buf[i]);
    // ip
    broadcast_buf[i++] = (ipv4 >> 24) & 0xff;
    broadcast_buf[i++] = (ipv4 >> 16) & 0xff;
    broadcast_buf[i++] = (ipv4 >> 8) & 0xff;
    broadcast_buf[i++] = (ipv4 >> 0) & 0xff;

    _udp_send(fd, broadcast_buf, i, WIFI_UDP_BROADCAST_IP, WIFI_UDP_BROADCAST_PORT);
    Log_d("broadcast local ipv4 add4: %d.%d.%d.%d\r\n", broadcast_buf[7], broadcast_buf[8], broadcast_buf[9],
          broadcast_buf[10]);
    return;
}
