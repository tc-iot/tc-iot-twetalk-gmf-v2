/*****************************************************************************
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
 *
 * Licensed under the MIT License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "tc_iot_hal.h"
#include "utils_log.h"
#include "tc_iot_ret_code.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

/* lwIP socket handle start from 0 */
#define LWIP_SOCKET_FD_SHIFT 3

static uint32_t _time_left(uint32_t t_end, uint32_t t_now)
{
    uint32_t t_left;

    if (t_end > t_now) {
        t_left = t_end - t_now;
    } else {
        t_left = 0;
    }

    return t_left;
}

/**
 * @brief 获取主机地址信息并解析IP地址
 * 
 * 该函数通过DNS解析主机名，获取对应的IP地址。支持IPv4和IPv6地址解析。
 * 
 * @param host       [输入] 主机名或IP地址字符串，例如："www.example.com" 或 "192.168.1.1"
 * @param port       [输入] 端口号
 * @param ip_str     [输出] 用于存储解析后的IP地址字符串的缓冲区，大小至少为80字节
 *                   成功时将包含点分十进制格式的IPv4地址（如"192.168.1.1"）
 *                   或冒号分隔的IPv6地址（如"2001:db8::1"）
 * 
 * @return QCLOUD_RET_SUCCESS 成功解析并获取IP地址
 * @return QCLOUD_ERR_FAILURE 解析失败（参数错误、DNS解析失败或不支持的地址类型）
 * 
 * @note 调用者需要确保ip_str缓冲区至少有80字节空间
 */
int HAL_GetAddrInfo(const char *host, uint16_t port, char ip_str[80])
{
    int             ret;
    struct addrinfo hints;
    struct addrinfo *addr_list = NULL;
    char            port_str[6];

    // 参数校验
    if (host == NULL || ip_str == NULL) {
        Log_e("invalid parameters");
        return QCLOUD_ERR_FAILURE;
    }

    // 将端口号转换为字符串
    HAL_Snprintf(port_str, sizeof(port_str), "%d", port);

    // 设置地址信息查询条件
    memset(&hints, 0x00, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;    // 支持IPv4和IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP流式套接字
    hints.ai_protocol = IPPROTO_TCP;  // TCP协议

    // 执行DNS解析，获取地址信息链表
    ret = getaddrinfo(host, port_str, &hints, &addr_list);
    if (ret != 0) {
        Log_e("getaddrinfo(%s:%s) failed, error: %d", STR_SAFE_PRINT(host), port_str, ret);
        return QCLOUD_ERR_FAILURE;
    }

    // 检查是否成功获取地址信息
    if (addr_list == NULL) {
        Log_e("getaddrinfo returned NULL address list");
        return QCLOUD_ERR_FAILURE;
    }

    // 根据地址族类型提取并转换IP地址
    if (addr_list->ai_family == AF_INET) {
        // IPv4地址：将网络字节序的二进制地址转换为点分十进制字符串
        struct sockaddr_in *sa = (struct sockaddr_in *)(addr_list->ai_addr);
        if (inet_ntop(AF_INET, &(sa->sin_addr), ip_str, 80) == NULL) {
            Log_e("inet_ntop for IPv4 failed");
            freeaddrinfo(addr_list);
            return QCLOUD_ERR_FAILURE;
        }
    } else if (addr_list->ai_family == AF_INET6) {
        // IPv6地址：将网络字节序的二进制地址转换为冒号分隔的字符串
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)(addr_list->ai_addr);
        if (inet_ntop(AF_INET6, &(sa6->sin6_addr), ip_str, 80) == NULL) {
            Log_e("inet_ntop for IPv6 failed");
            freeaddrinfo(addr_list);
            return QCLOUD_ERR_FAILURE;
        }
    } else {
        // 不支持的地址族类型
        Log_e("unsupported address family: %d", addr_list->ai_family);
        freeaddrinfo(addr_list);
        return QCLOUD_ERR_FAILURE;
    }

    // 释放地址信息链表
    freeaddrinfo(addr_list);
    
    Log_d("resolved %s to %s:%d", host, ip_str, port);

    return QCLOUD_RET_SUCCESS;
}

uintptr_t HAL_TCP_Connect(const char *host, uint16_t port)
{
    int             ret;
    struct addrinfo hints, *addr_list, *cur;
    int             fd = 0;

    char port_str[6];
    HAL_Snprintf(port_str, 6, "%d", port);

    memset(&hints, 0x00, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    ret = getaddrinfo(host, port_str, &hints, &addr_list);
    if (ret) {
        Log_e("getaddrinfo(%s:%s) error", STR_SAFE_PRINT(host), port_str);
        return 0;
    }

    for (cur = addr_list; cur != NULL; cur = cur->ai_next) {
        fd = (int)socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (fd < 0) {
            ret = 0;
            continue;
        }

        if (connect(fd, cur->ai_addr, cur->ai_addrlen) == 0) {
            ret = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
            if (ret) {
                Log_e("set socket nonblock mode failed %d", ret);
            }
            ret = fd + LWIP_SOCKET_FD_SHIFT;
            break;
        }

        close(fd);
        ret = 0;
    }

    struct sockaddr_in *sa = (struct sockaddr_in *)(cur->ai_addr);
    const char *ip_str = inet_ntoa(sa->sin_addr);
    if (ret == 0) {
        Log_e("failed to connect with TCP server: %s(%s):%s", host, ip_str, port_str);
    } else {
        /* reduce log print due to frequent server connect/disconnect */
        if (port != COMMON_HTTP_SERVER_PORT)
            Log_i("connected with TCP server: %s(%s):%s", host, ip_str, port_str);
    }

    freeaddrinfo(addr_list);

    return (uintptr_t)ret;
}

int HAL_TCP_Disconnect(uintptr_t fd)
{
    int rc;

    fd -= LWIP_SOCKET_FD_SHIFT;

    /* Shutdown both send and receive operations. */
    rc = shutdown((int)fd, 2);
    if (0 != rc) {
        Log_e("shutdown error: %s", STR_SAFE_PRINT(strerror(errno)));
        return -1;
    }

    rc = close((int)fd);
    if (0 != rc) {
        Log_e("closesocket error: %s", STR_SAFE_PRINT(strerror(errno)));
        return -1;
    }

    return 0;
}

int HAL_TCP_Write(uintptr_t fd, const unsigned char *buf, uint32_t len, uint32_t timeout_ms, size_t *written_len)
{
    int      ret;
    uint32_t len_sent;
    uint32_t t_end, t_left;
    fd_set   sets;

    fd -= LWIP_SOCKET_FD_SHIFT;

    t_end    = HAL_GetTimeMs() + timeout_ms;
    len_sent = 0;
    ret      = 1; /* send one time if timeout_ms is value 0 */

    do {
        t_left = _time_left(t_end, HAL_GetTimeMs());

        if (0 != t_left) {
            struct timeval timeout;

            FD_ZERO(&sets);
            FD_SET(fd, &sets);

            timeout.tv_sec  = t_left / 1000;
            timeout.tv_usec = (t_left % 1000) * 1000;

            ret = select(fd + 1, NULL, &sets, NULL, &timeout);
            if (ret > 0) {
                if (0 == FD_ISSET(fd, &sets)) {
                    Log_e("Should NOT arrive");
                    /* If timeout in next loop, it will not sent any data */
                    ret = 0;
                    continue;
                }
            } else if (0 == ret) {
                ret = QCLOUD_ERR_TCP_WRITE_TIMEOUT;
                Log_e("select-write timeout %d", (int)fd);
                break;
            } else {
                if (EINTR == errno) {
                    Log_e("EINTR be caught");
                    continue;
                }

                ret = QCLOUD_ERR_TCP_WRITE_FAIL;
                Log_e("select-write fail: %s", STR_SAFE_PRINT(strerror(errno)));
                break;
            }
        } else {
            ret = QCLOUD_ERR_TCP_WRITE_TIMEOUT;
        }

        if (ret > 0) {
            ret = send(fd, buf + len_sent, len - len_sent, 0);
            if (ret > 0) {
                len_sent += ret;
            } else if (0 == ret) {
                Log_e("No data be sent. Should NOT arrive");
            } else {
                if (EINTR == errno) {
                    Log_e("EINTR be caught");
                    continue;
                }

                ret = QCLOUD_ERR_TCP_WRITE_FAIL;
                Log_e("send fail: %s", STR_SAFE_PRINT(strerror(errno)));
                break;
            }
        }
    } while ((len_sent < len) && (_time_left(t_end, HAL_GetTimeMs()) > 0));

    *written_len = (size_t)len_sent;

    return len_sent > 0 ? QCLOUD_RET_SUCCESS : ret;
}

int HAL_TCP_Read(uintptr_t fd, unsigned char *buf, uint32_t len, uint32_t timeout_ms, size_t *read_len)
{
    int            ret, err_code;
    uint32_t       len_recv;
    uint32_t       t_end, t_left;
    fd_set         sets;
    struct timeval timeout;

    fd -= LWIP_SOCKET_FD_SHIFT;
    t_end    = HAL_GetTimeMs() + timeout_ms;
    len_recv = 0;
    err_code = 0;

    do {
        t_left = _time_left(t_end, HAL_GetTimeMs());

        FD_ZERO(&sets);
        FD_SET(fd, &sets);

        timeout.tv_sec  = t_left / 1000;
        timeout.tv_usec = (t_left % 1000) * 1000;

        ret = select(fd + 1, &sets, NULL, NULL, &timeout);
        if (ret > 0) {
            ret = recv(fd, buf + len_recv, len - len_recv, 0);
            if (ret > 0) {
                len_recv += ret;
            } else if (0 == ret) {
                struct sockaddr_in peer;
                socklen_t          sLen      = sizeof(peer);
                int                peer_port = 0;
                getpeername(fd, (struct sockaddr *)&peer, &sLen);
                peer_port = ntohs(peer.sin_port);

                /* reduce log print due to frequent server connect/disconnect */
                if (peer_port != COMMON_HTTP_SERVER_PORT)
                    Log_e("connection is closed by server: %s:%d",
                          STR_SAFE_PRINT(inet_ntoa(peer.sin_addr)), peer_port);

                err_code = QCLOUD_ERR_TCP_PEER_SHUTDOWN;
                break;
            } else {
                if (EINTR == errno) {
                    Log_e("EINTR be caught");
                    continue;
                }
                Log_e("recv error: %s", STR_SAFE_PRINT(strerror(errno)));
                err_code = QCLOUD_ERR_TCP_READ_FAIL;
                break;
            }
        } else if (0 == ret) {
            err_code = QCLOUD_ERR_TCP_READ_TIMEOUT;
            break;
        } else {
            Log_e("select-recv error: %s", STR_SAFE_PRINT(strerror(errno)));
            err_code = QCLOUD_ERR_TCP_READ_FAIL;
            break;
        }
    } while ((len_recv < len) && t_left > 0);

    *read_len = (size_t)len_recv;

    if (err_code == QCLOUD_ERR_TCP_READ_TIMEOUT && len_recv == 0)
        err_code = QCLOUD_ERR_TCP_NOTHING_TO_READ;

    return (len_recv > 0) ? QCLOUD_RET_SUCCESS : err_code;
}