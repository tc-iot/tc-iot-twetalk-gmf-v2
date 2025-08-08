/**
 * @copyright
 *
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2018 - 2022 THL A29 Limited, a Tencent company.All rights reserved.
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
 * @file HAL_TCP_module.c
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2022-01-24
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2022-01-24 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "qcloud_iot_common.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "qcloud_iot_common.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

/* lwIP socket handle start from 0 */
#define LWIP_SOCKET_FD_SHIFT 3
/**
 * @brief TCP connect in linux
 *
 * @param[in] host host to connect
 * @param[out] port port to connect
 * @return socket fd
 */
int HAL_TCP_Connect(const char *host, const char *port)
{
    int rc;
    int fd = 0;

    struct addrinfo hints, *addr_list = NULL, *cur = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    rc = getaddrinfo(host, port, &hints, &addr_list);
    if (rc) {
        Log_e("getaddrinfo(%s:%s) error: %s", STRING_PTR_PRINT_SANITY_CHECK(host), STRING_PTR_PRINT_SANITY_CHECK(port),
              strerror(errno));
        freeaddrinfo(addr_list);
        return QCLOUD_ERR_TCP_UNKNOWN_HOST;
    }

    for (cur = addr_list; cur; cur = cur->ai_next) {
        fd = (int)socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (fd < 0) {
            rc = QCLOUD_ERR_TCP_SOCKET_FAILED;
            continue;
        }

        rc = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
        if (rc) {
            Log_e("set socket non block failed %d", rc);
            close(fd);
            rc = QCLOUD_ERR_TCP_SOCKET_FAILED;
            continue;
        }

        rc = connect(fd, cur->ai_addr, cur->ai_addrlen);
        if (!rc) {
            rc = fd + LWIP_SOCKET_FD_SHIFT;
            break;
        }

        if (errno == EINPROGRESS) {
            // IO select to wait for connect result
            struct timeval timeout;
            timeout.tv_sec  = QCLOUD_IOT_MQTT_COMMAND_TIMEOUT / 1000;
            timeout.tv_usec = 0;

            fd_set sets;
            FD_ZERO(&sets);
            FD_SET(fd, &sets);

            rc = select(fd + 1, NULL, &sets, NULL, &timeout);
            if (rc > 0) {
                int       so_error;
                socklen_t len = sizeof(so_error);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
                if (FD_ISSET(fd, &sets) && so_error == 0) {
                    rc = fd + LWIP_SOCKET_FD_SHIFT;
                    break;
                }
            }
        }

        close(fd);
        rc = QCLOUD_ERR_TCP_CONNECT;
    }

    freeaddrinfo(addr_list);
    return rc;
}

/**
 * @brief TCP disconnect
 *
 * @param[in] fd socket fd
 * @return 0 for success
 */
int HAL_TCP_Disconnect(int fd)
{
    int rc;

    fd -= LWIP_SOCKET_FD_SHIFT;

    /* Shutdown both send and receive operations. */
    rc = shutdown((int)fd, 2);
    if (0 != rc) {
        Log_e("shutdown error: %s", STRING_PTR_PRINT_SANITY_CHECK(strerror(errno)));
    }

    rc = close((int)fd);
    if (0 != rc) {
        Log_e("closesocket error: %s", STRING_PTR_PRINT_SANITY_CHECK(strerror(errno)));
        return -1;
    }

    return 0;
}

/**
 * @brief TCP write
 *
 * @param[in] fd socket fd
 * @param[in] buf buf to write
 * @param[in] len buf len
 * @param[in] timeout_ms timeout
 * @return @see IotReturnCode
 */
int HAL_TCP_Write(int fd, const uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    int            rc = 0;
    uint32_t       len_sent;
    QcloudIotTimer timer_send;
    fd_set         sets;
    struct timeval timeout;

    fd -= LWIP_SOCKET_FD_SHIFT;

    IOT_Timer_CountdownMs(&timer_send, timeout_ms);
    len_sent = 0;

    /* send one time if timeout_ms is value 0 */
    while ((len_sent < len) && !IOT_Timer_Expired(&timer_send)) {
        timeout.tv_sec  = IOT_Timer_Remain(&timer_send) / 1000;
        timeout.tv_usec = IOT_Timer_Remain(&timer_send) % 1000 * 1000;

        FD_ZERO(&sets);
        FD_SET(fd, &sets);

        rc = select(fd + 1, NULL, &sets, NULL, &timeout);
        if (!rc) {
            rc = QCLOUD_ERR_TCP_WRITE_TIMEOUT;
            Log_e("select-write timeout %d", (int)fd);
            break;
        }

        if (rc < 0) {
            if (EINTR != errno) {
                rc = QCLOUD_ERR_TCP_WRITE_FAIL;
                Log_e("select-write fail: %s", strerror(errno));
                break;
            }
            Log_e("EINTR be caught");
            continue;
        }

        rc = send(fd, buf + len_sent, len - len_sent, 0);
        if (rc < 0) {
            if (EINTR == errno) {
                Log_e("EINTR be caught");
                continue;
            }
            rc = (EPIPE == errno || ECONNRESET == errno) ? QCLOUD_ERR_TCP_PEER_SHUTDOWN : QCLOUD_ERR_TCP_WRITE_FAIL;
            Log_e("send fail: %s", strerror(errno));
            break;
        }

        len_sent += rc;
    }

    // We always know hom much should write.
    return len_sent == len ? QCLOUD_RET_SUCCESS : rc;
}

/**
 * @brief TCP read.
 *
 * @param[in] fd socket fd
 * @param[out] buf buffer to save read data
 * @param[in] len buffer len
 * @param[in] timeout_ms timeout
 * @param[out] read_len length of data read
 * @return @see IotReturnCode
 */
int HAL_TCP_Read(int fd, unsigned char *buf, uint32_t len, uint32_t timeout_ms, size_t *read_len)
{
    int            rc;
    uint32_t       len_recv;
    QcloudIotTimer timer_recv;
    fd_set         sets;
    struct timeval timeout;

    fd -= LWIP_SOCKET_FD_SHIFT;

    IOT_Timer_CountdownMs(&timer_recv, timeout_ms);
    len_recv = 0;

    do {
        FD_ZERO(&sets);
        FD_SET(fd, &sets);

        timeout.tv_sec  = IOT_Timer_Remain(&timer_recv) / 1000;
        timeout.tv_usec = IOT_Timer_Remain(&timer_recv) % 1000 * 1000;

        rc = select(fd + 1, &sets, NULL, NULL, &timeout);
        if (!rc) {
            rc = QCLOUD_ERR_TCP_READ_TIMEOUT;
            break;
        }

        if (rc < 0) {
            if (EINTR != errno) {
                rc = QCLOUD_ERR_TCP_READ_FAIL;
                Log_e("select-recv fail: %s", strerror(errno));
                break;
            }
            Log_e("EINTR be caught");
            continue;
        }

        rc = recv(fd, buf + len_recv, len - len_recv, 0);
        if (rc <= 0) {
            if (!rc) {
                Log_e("connection is closed by server");
                rc = QCLOUD_ERR_TCP_PEER_SHUTDOWN;
                break;
            }

            if (EINTR == errno) {
                Log_e("EINTR be caught");
                continue;
            }
            Log_e("recv error: %s", strerror(errno));
            rc = (EPIPE == errno || ECONNRESET == errno) ? QCLOUD_ERR_TCP_PEER_SHUTDOWN : QCLOUD_ERR_TCP_READ_FAIL;
            break;
        }
        len_recv += rc;
    } while (len_recv < len);

    *read_len = (size_t)len_recv;

    if (rc == QCLOUD_ERR_TCP_READ_TIMEOUT && len_recv == 0) {
        rc = QCLOUD_ERR_TCP_NOTHING_TO_READ;
    }
    // We always don't know hom much should read.
    return (len_recv > 0) ? QCLOUD_RET_SUCCESS : rc;
}
