/**
 * @file HAL_UDP_free_rtos.c
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2022-10-11
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
 * 2022-10-11		1.0			hubertxxu		first commit
 * </table>
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "qcloud_iot_common.h"

/* lwIP socket handle start from 0 */
#define WIFI_LWIP_SOCKET_FD_SHIFT 3

int HAL_UDP_Bind(const char *ip, uint16_t port)
{
    int fd = 0;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        Log_e("fail to establish udp");
        return -1;
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family         = AF_INET;
    server_addr.sin_addr.s_addr    = inet_addr(ip);
    server_addr.sin_port           = htons(port);
    if (bind(fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        Log_e("fail to bind udp");
        close(fd);
        return -1;
    }

    int iOptval = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &iOptval, sizeof(int)) < 0) {
        Log_e("setsockopt failed", strerror(errno));
    }
    Log_d("establish udp connection with server(host=%s|port=%d|fd=%d)", ip, port, fd);
    return fd + WIFI_LWIP_SOCKET_FD_SHIFT;
}

int HAL_UDP_Recv(int fd, uint8_t *p_data, uint32_t datalen, uint32_t timeout_ms, char *recv_ip_addr,
                 uint32_t recv_addr_len, uint16_t *recv_port)
{
    int                ret;
    struct timeval     tv;
    fd_set             read_fds;
    int                socket_id = -1;
    struct sockaddr_in source_addr;
    socklen_t          addrLen = sizeof(source_addr);
    int                len     = 0;

    fd -= WIFI_LWIP_SOCKET_FD_SHIFT;

    socket_id = (int)fd;

    if (socket_id < 0) {
        return -1;
    }

    FD_ZERO(&read_fds);
    FD_SET(socket_id, &read_fds);

    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    ret = select(socket_id + 1, &read_fds, NULL, NULL, timeout_ms == 0 ? NULL : &tv);

    /* Zero fds ready means we timed out */
    if (ret == 0) {
        return 0; /* receive timeout */
    }

    if (ret < 0) {
        if (errno == EINTR) {
            return -3; /* want read */
        }

        return QCLOUD_ERR_SSL_READ; /* receive failed */
    }

    /* This call will not block */
    len = recvfrom((int)fd, p_data, datalen, MSG_DONTWAIT, (struct sockaddr *)&source_addr, &addrLen);

    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, recv_ip_addr, recv_addr_len - 1);
    *recv_port = ntohs(source_addr.sin_port);

    return len;
}

int HAL_UDP_Send(int fd, const uint8_t *p_data, uint32_t datalen, const char *host, const char *port)
{
    int             rc = -1;
    struct addrinfo hints, *addr_list;

    memset((char *)&hints, 0x00, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family   = AF_INET;
    hints.ai_protocol = IPPROTO_UDP;

    if ((rc = getaddrinfo(host, port, &hints, &addr_list)) != 0) {
        Log_e("getaddrinfo error,errno:%s, rc:%d", strerror(errno), rc);
        return 0;
    }
    Log_i("HAL_UDP_Send host:%s|port:%s|fd:%d", host, port, fd);
    fd -= WIFI_LWIP_SOCKET_FD_SHIFT;
    rc = sendto(fd, p_data, datalen, 0, addr_list->ai_addr, addr_list->ai_addrlen);
    freeaddrinfo(addr_list);
    return rc;
}

void HAL_UDP_Close(int fd)
{
    long socket_id = -1;
    fd -= WIFI_LWIP_SOCKET_FD_SHIFT;
    socket_id = (int)fd;
    close(socket_id);
}
