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

#include "esp_log.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/platform.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "netdb.h"
#include "tc_iot_hal.h"
#include "tc_iot_ret_code.h"

#define TAG "TLS"

// 默认不开启mbedtls调试
#undef MBEDTLS_DEBUG_C

// #define USE_MBEDTLS_NET_FUNCTIONS

// mbedtls 使用自定义内存分配函数
// bk7258必须使用自定义内存分配函数，否则会出现内存不够
#if defined(PLATFORM_BK_RTOS)
#define MBEDTLS_USE_CUSTOM_MALLOC
#else
#undef MBEDTLS_USE_CUSTOM_MALLOC
#endif

static const int ciphersuites[] = {MBEDTLS_TLS_PSK_WITH_AES_128_CBC_SHA, MBEDTLS_TLS_PSK_WITH_AES_256_CBC_SHA, 0};

/**
 * @brief data structure for mbedtls SSL connection
 */
typedef struct {
    mbedtls_net_context socket_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config ssl_conf;
    mbedtls_x509_crt ca_cert;
    mbedtls_x509_crt client_cert;
    mbedtls_pk_context private_key;
} TLSDataParams;

/**
 * @brief free memory/resources allocated by mbedtls
 */
static void _free_mbedtls(TLSDataParams *pParams)
{
    mbedtls_net_free(&(pParams->socket_fd));
    mbedtls_x509_crt_free(&(pParams->client_cert));
    mbedtls_x509_crt_free(&(pParams->ca_cert));
    mbedtls_pk_free(&(pParams->private_key));
    mbedtls_ssl_free(&(pParams->ssl));
    mbedtls_ssl_config_free(&(pParams->ssl_conf));
    mbedtls_ctr_drbg_free(&(pParams->ctr_drbg));
    mbedtls_entropy_free(&(pParams->entropy));

    HAL_Free(pParams);
}

#if defined(MBEDTLS_DEBUG_C)

#define DEBUG_LEVEL 4
static void _ssl_debug(void *ctx, int level, const char *file, int line, const char *str)
{
    HAL_Printf("[mbedTLS]:[%s]:[%d]: %s\r\n", STR_SAFE_PRINT(file), line, STR_SAFE_PRINT(str));
}

#endif

#ifdef MBEDTLS_USE_CUSTOM_MALLOC
static void *_tls_calloc(size_t num, size_t size)
{
    void *ptr = HAL_Malloc(num * size);
    if (ptr) {
        memset(ptr, 0, num * size);
    }
    return ptr;
}

static void _tls_free(void *ptr)
{
    HAL_Free(ptr);
}
#endif

/**
 * @brief mbedtls SSL client init
 *
 * 1. call a series of mbedtls init functions
 * 2. init and set seed for random functions
 * 3. load CA file, cert files or PSK
 *
 * @param pDataParams       mbedtls TLS parmaters
 * @param pConnectParams    device info for TLS connection
 * @return                  QCLOUD_RET_SUCCESS when success, or err code for
 * failure
 */
static int _mbedtls_client_init(TLSDataParams *pDataParams, TLSConnectParams *pConnectParams)
{
    int ret = QCLOUD_RET_SUCCESS;
#ifdef MBEDTLS_USE_CUSTOM_MALLOC
    static uint8_t _mbedtls_calloc_init_flag = 0;
    if (!_mbedtls_calloc_init_flag) {
        mbedtls_platform_set_calloc_free(_tls_calloc, _tls_free);
        _mbedtls_calloc_init_flag = 1;
    }
#endif
    mbedtls_net_init(&(pDataParams->socket_fd));
    mbedtls_ssl_init(&(pDataParams->ssl));
    mbedtls_ssl_config_init(&(pDataParams->ssl_conf));
    mbedtls_ctr_drbg_init(&(pDataParams->ctr_drbg));
    mbedtls_x509_crt_init(&(pDataParams->ca_cert));
    mbedtls_x509_crt_init(&(pDataParams->client_cert));
    mbedtls_pk_init(&(pDataParams->private_key));

    mbedtls_entropy_init(&(pDataParams->entropy));

#if defined(MBEDTLS_DEBUG_C)
    mbedtls_debug_set_threshold(DEBUG_LEVEL);
    mbedtls_ssl_conf_dbg(&pDataParams->ssl_conf, _ssl_debug, NULL);
#endif

    // custom parameter is NULL for now
    if ((ret = mbedtls_ctr_drbg_seed(&(pDataParams->ctr_drbg), mbedtls_entropy_func, &(pDataParams->entropy), NULL,
                                     0)) != 0) {
        HAL_Printf("[ERR] mbedtls_ctr_drbg_seed failed returned 0x%04x\n", ret < 0 ? -ret : ret);
        return QCLOUD_ERR_SSL_INIT;
    }

    if (pConnectParams->ca_crt != NULL) {
        if ((ret = mbedtls_x509_crt_parse(&(pDataParams->ca_cert), (const unsigned char *)pConnectParams->ca_crt,
                                          (pConnectParams->ca_crt_len + 1)))) {
            HAL_Printf("[ERR] parse ca crt failed returned 0x%04x\n", ret < 0 ? -ret : ret);
            return QCLOUD_ERR_SSL_CERT;
        }
    }

    if (pConnectParams->psk != NULL && strlen(pConnectParams->psk_id)) {
        const char *psk_id = pConnectParams->psk_id;
        ret                = mbedtls_ssl_conf_psk(&(pDataParams->ssl_conf), (unsigned char *)pConnectParams->psk,
                                                  pConnectParams->psk_length, (const unsigned char *)psk_id, strlen(psk_id));
    } else {
        // HAL_Printf("[DBG] psk/pskid is empty!\n");
    }

    if (0 != ret) {
        HAL_Printf("[ERR] mbedtls_ssl_conf_psk fail: 0x%x\n", ret < 0 ? -ret : ret);
        return ret;
    }

    return QCLOUD_RET_SUCCESS;
}

#ifndef USE_MBEDTLS_NET_FUNCTIONS
static int _mbedtls_net_connect_with_timeout(mbedtls_net_context *ctx, const char *host, const char *port, int proto,
                                             int timeout_s)
{
    int ret;
    struct addrinfo hints, *addr_list, *cur;
    int port_num = atoi(port);

    HAL_Signal(SIGPIPE, SIG_IGN);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET; /* IPv4 only. */
    hints.ai_socktype = proto == MBEDTLS_NET_PROTO_UDP ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_protocol = proto == MBEDTLS_NET_PROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP;
    ret               = getaddrinfo(host, port, &hints, &addr_list);
    if (ret) {
        HAL_Printf("[ERR] getaddrinfo failed for host %s port : %s ret : %d\n", host, port, ret);
        return (MBEDTLS_ERR_NET_UNKNOWN_HOST);
    }

    /* Try the sockaddrs until a connection succeeds */
    ret = MBEDTLS_ERR_NET_UNKNOWN_HOST;
    for (cur = addr_list; cur != NULL; cur = cur->ai_next) {
        ctx->fd = (int)socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (ctx->fd < 0) {
            ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
            continue;
        }

        // set non-block mode socket
        ret = fcntl(ctx->fd, F_SETFL, fcntl(ctx->fd, F_GETFL, 0) | O_NONBLOCK);
        if (ret) {
            HAL_Printf("[ERR] set socket non block faliled %d\n", ret);
            close(ctx->fd);
            ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
            continue;
        }

        struct sockaddr_in *sa = (struct sockaddr_in *)(cur->ai_addr);
        ret                    = connect(ctx->fd, cur->ai_addr, cur->ai_addrlen);
        if (ret == 0) {
            // success immediately, maybe local host
            HAL_Printf("[DBG] connect success to %s\n", inet_ntoa(sa->sin_addr));
            ret = 0;
            break;
        } else if (errno == EINPROGRESS) {
            // IO select to wait for connect result
            struct timeval timeout;
            fd_set sets;
            FD_ZERO(&sets);
            FD_SET(ctx->fd, &sets);

            timeout.tv_sec  = timeout_s;
            timeout.tv_usec = 0;
            ret             = select(ctx->fd + 1, NULL, &sets, NULL, &timeout);
            if (ret > 0) {
                int so_error;
                socklen_t len = sizeof(so_error);
                getsockopt(ctx->fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
                if (FD_ISSET(ctx->fd, &sets) && so_error == 0) {
                    if (COMMON_HTTP_SERVER_PORT_TLS != port_num)
                        HAL_Printf("[DBG] connect to %s(%s:%d) fd %d\n", host, inet_ntoa(sa->sin_addr), port_num,
                                   (int)ctx->fd);
                    ret = 0;
                    break;
                }
                HAL_Printf("[ERR] connect error invalid fd %d %s\n", (int)ctx->fd, inet_ntoa(sa->sin_addr));
            } else if (0 == ret) {
                HAL_Printf("[ERR] connect timeout fd %d %s\n", (int)ctx->fd, inet_ntoa(sa->sin_addr));
            } else {
                HAL_Printf("[ERR] connect fail fd %d %d %s\n", (int)ctx->fd, errno, inet_ntoa(sa->sin_addr));
            }
        }

        close(ctx->fd);
        ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
    }

    freeaddrinfo(addr_list);

    return (ret);
}
#endif

/**
 * @brief Setup TCP connection
 *
 * @param socket_fd  socket handle
 * @param host       server address
 * @param port       server port
 * @return QCLOUD_RET_SUCCESS when success, or err code for failure
 */
int _mbedtls_tcp_connect(mbedtls_net_context *socket_fd, const char *host, int port, int timeout_s)
{
    int ret = 0;
    char port_str[6];
    HAL_Snprintf(port_str, 6, "%d", port);
#ifndef USE_MBEDTLS_NET_FUNCTIONS
    // connect in non-block mode with timeout n seconds
    ret = _mbedtls_net_connect_with_timeout(socket_fd, host, port_str, MBEDTLS_NET_PROTO_TCP, timeout_s);
#else
    ret = mbedtls_net_connect(socket_fd, host, port_str, MBEDTLS_NET_PROTO_TCP);
#endif
    if (ret) {
        HAL_Printf("[ERR] tcp connect failed returned 0x%04x errno: %d\n", ret < 0 ? -ret : ret, errno);

        switch (ret) {
            case MBEDTLS_ERR_NET_SOCKET_FAILED:
                return QCLOUD_ERR_TCP_SOCKET_FAILED;
            case MBEDTLS_ERR_NET_UNKNOWN_HOST:
                return QCLOUD_ERR_TCP_UNKNOWN_HOST;
            default:
                return QCLOUD_ERR_TCP_CONNECT;
        }
    }

    if ((ret = mbedtls_net_set_nonblock(socket_fd)) != 0) {
        HAL_Printf("[ERR] set block faliled returned 0x%04x\n", ret < 0 ? -ret : ret);
        return QCLOUD_ERR_TCP_CONNECT;
    }

    return QCLOUD_RET_SUCCESS;
}

/**
 * @brief verify server certificate
 *
 * mbedtls has provided similar function mbedtls_x509_crt_verify_with_profile
 *
 * @return
 */
int _qcloud_server_certificate_verify(void *hostname, mbedtls_x509_crt *crt, int depth, uint32_t *flags)
{
    return *flags;
}

#ifndef USE_MBEDTLS_NET_FUNCTIONS
static int net_would_block(const mbedtls_net_context *ctx)
{
    int err = errno;

    /*
     * Never return 'WOULD BLOCK' on a blocking socket
     */
    if ((fcntl(ctx->fd, F_GETFL, 0) & O_NONBLOCK) != O_NONBLOCK) {
        errno = err;
        return (0);
    }

    return (1);
}

static int _linux_tcp_recv_timeout(void *ctx, unsigned char *buf, size_t len, uint32_t timeout_ms)
{
    int ret = MBEDTLS_ERR_SSL_TIMEOUT;
    uint32_t len_recv;
    uint64_t t_left;
    fd_set sets;
    struct timeval timeout;
    int fd = ((mbedtls_net_context *)ctx)->fd;

    if (fd < 0)
        return (MBEDTLS_ERR_NET_INVALID_CONTEXT);

    Timer timer;
    HAL_Timer_init(&timer);
    HAL_Timer_countdown_ms(&timer, timeout_ms);

    len_recv = 0;
    
    do {
        t_left = HAL_Timer_remain(&timer);
        FD_ZERO(&sets);
        FD_SET(fd, &sets);

        timeout.tv_sec  = t_left / 1000;
        timeout.tv_usec = (t_left % 1000) * 1000;

        ret = select(fd + 1, &sets, NULL, NULL, &timeout);
        if (ret > 0) {
            if (FD_ISSET(fd, &sets)) {
                ret = recv(fd, buf + len_recv, len - len_recv, 0);
                if (ret > 0) {
                    len_recv += ret;
                } else if (0 == ret) {
                    HAL_Printf("[ERR] connection is closed by server\n");
                    break;
                } else {
                    if (net_would_block(ctx) != 0) {
                        ret = MBEDTLS_ERR_SSL_WANT_READ;
                    } else if (errno == EPIPE || errno == ECONNRESET) {
                        ret = MBEDTLS_ERR_NET_CONN_RESET;
                    } else if (errno == EINTR) {
                        ret = MBEDTLS_ERR_SSL_WANT_READ;
                    } else {
                        ret = MBEDTLS_ERR_NET_RECV_FAILED;
                    }
                    HAL_Printf("[ERR] recv error: %s\n", STR_SAFE_PRINT(strerror(errno)));
                    HAL_SleepMs(100);
                    break;
                }
            }
        } else if (0 == ret) {
            ret = MBEDTLS_ERR_SSL_TIMEOUT;
            // HAL_Printf("[ERR] select-recv expect length %d timeout %d\n", len, timeout_ms);
            continue;
        } else {
            HAL_Printf("[ERR] select-recv error: %s\n", STR_SAFE_PRINT(strerror(errno)));

            if (errno == EINTR) {
                ret = MBEDTLS_ERR_SSL_WANT_READ;
            } else {
                ret = MBEDTLS_ERR_NET_RECV_FAILED;
            }
            break;
        }
    } while ((len_recv < len) && (t_left > 0));

    if ((len_recv > 0) && (len_recv < len)) {
        HAL_Printf("[WNR] receive len %d expect len %d time %d %ld\n", len_recv, len, timeout_ms, t_left);
    }

    return (len_recv > 0) ? len_recv : ret;
}

static int _linux_tcp_recv(void *ctx, unsigned char *buf, size_t len)
{
    return _linux_tcp_recv_timeout(ctx, buf, len, 5000);
}

static int _linux_tcp_send(void *ctx, const unsigned char *buf, size_t len)
{
    int ret;
    int32_t len_sent = 0;
    uint64_t t_left;
    fd_set sets;
    uint32_t timeout_ms = 5000;
    int fd              = ((mbedtls_net_context *)ctx)->fd;

    if (fd < 0)
        return (MBEDTLS_ERR_NET_INVALID_CONTEXT);

    Timer timer;
    HAL_Timer_init(&timer);
    HAL_Timer_countdown_ms(&timer, timeout_ms);

    /* send one time if timeout_ms is value 0 */
    do {
        t_left = HAL_Timer_remain(&timer);
        if (0 == t_left) {
            return MBEDTLS_ERR_SSL_TIMEOUT;
        }

        FD_ZERO(&sets);
        FD_SET(fd, &sets);

        struct timeval timeout;
        timeout.tv_sec  = t_left / 1000;
        timeout.tv_usec = (t_left % 1000) * 1000;

        ret = select(fd + 1, NULL, &sets, NULL, &timeout);
        if (ret > 0) {
            if (FD_ISSET(fd, &sets)) {
                ret = send(fd, buf + len_sent, len - len_sent, MSG_NOSIGNAL);
                if (ret > 0) {
                    len_sent += ret;
                } else if (0 == ret) {
                    HAL_Printf("[ERR] No data be sent. Should NOT arrive\n");
                } else {
                    if (errno == EINTR) {
                        // 由于信号中断，没写成功任何数据，继续
                        continue;
                    }
                    HAL_Printf("[ERR] send(%d) fail:%d %s\n", len - len_sent, ret, STR_SAFE_PRINT(strerror(errno)));
                    return MBEDTLS_ERR_NET_SEND_FAILED;
                }
            }
        } else if (0 == ret) {
            // ret = QCLOUD_ERR_TCP_WRITE_TIMEOUT;
            ret = MBEDTLS_ERR_SSL_TIMEOUT;
            HAL_Printf("[ERR] select-write timeout %d\n", (int)fd);
            continue;
        } else {
            if (errno == EINTR) {
                // 由于信号中断，没写成功任何数据，继续
                HAL_Printf("[ERR] select-write fail: %s\n", STR_SAFE_PRINT(strerror(errno)));
                continue;
            }
            return MBEDTLS_ERR_NET_SEND_FAILED;
        }
    } while (len_sent < len);

    // HAL_Printf("[DBG] len %d time %d\n", len_sent, timeout_ms);

    return (len_sent == len) ? len_sent : ret;
}
#endif

uintptr_t HAL_TLS_Connect(TLSConnectParams *pConnectParams, const char *host, int port)
{
    int ret = 0;

    TLSDataParams *pDataParams = (TLSDataParams *)HAL_Malloc(sizeof(TLSDataParams));

    if ((ret = _mbedtls_client_init(pDataParams, pConnectParams)) != QCLOUD_RET_SUCCESS) {
        goto error;
    }
    if ((80 != port) && (443 != port)) {
        HAL_Printf("[DBG] Setting up the SSL/TLS structure...\n");
    }
    if ((ret = mbedtls_ssl_config_defaults(&(pDataParams->ssl_conf), MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        HAL_Printf("[ERR] mbedtls_ssl_config_defaults failed returned 0x%04x\n", ret < 0 ? -ret : ret);
        goto error;
    }

    mbedtls_ssl_conf_verify(&(pDataParams->ssl_conf), _qcloud_server_certificate_verify, (void *)host);

    mbedtls_ssl_conf_authmode(&(pDataParams->ssl_conf), MBEDTLS_SSL_VERIFY_REQUIRED);

    mbedtls_ssl_conf_rng(&(pDataParams->ssl_conf), mbedtls_ctr_drbg_random, &(pDataParams->ctr_drbg));

    mbedtls_ssl_conf_ca_chain(&(pDataParams->ssl_conf), &(pDataParams->ca_cert), NULL);
    if ((ret = mbedtls_ssl_conf_own_cert(&(pDataParams->ssl_conf), &(pDataParams->client_cert),
                                         &(pDataParams->private_key))) != 0) {
        HAL_Printf("[ERR] mbedtls_ssl_conf_own_cert failed returned 0x%04x\n", ret < 0 ? -ret : ret);
        goto error;
    }

    mbedtls_ssl_conf_read_timeout(&(pDataParams->ssl_conf), pConnectParams->timeout_ms);
    if ((ret = mbedtls_ssl_setup(&(pDataParams->ssl), &(pDataParams->ssl_conf))) != 0) {
        HAL_Printf("[ERR] mbedtls_ssl_setup failed returned 0x%04x\n", ret < 0 ? -ret : ret);
        goto error;
    }

    // ciphersuites selection for PSK device
    if (pConnectParams->psk != NULL) {
        mbedtls_ssl_conf_ciphersuites(&(pDataParams->ssl_conf), ciphersuites);
    }

    // Set the hostname to check against the received server certificate and sni
    if ((ret = mbedtls_ssl_set_hostname(&(pDataParams->ssl), host)) != 0) {
        HAL_Printf("[ERR] mbedtls_ssl_set_hostname failed returned 0x%04x\n", ret < 0 ? -ret : ret);
        goto error;
    }

#ifndef USE_MBEDTLS_NET_FUNCTIONS
    mbedtls_ssl_set_bio(&(pDataParams->ssl), &(pDataParams->socket_fd), _linux_tcp_send, _linux_tcp_recv,
                        _linux_tcp_recv_timeout);
#else
#ifdef JIELI_791N_RTOS
    mbedtls_net_set_timeout(&(pDataParams->socket_fd), pConnectParams->timeout_ms, pConnectParams->timeout_ms);
#endif
    mbedtls_ssl_set_bio(&(pDataParams->ssl), &(pDataParams->socket_fd), mbedtls_net_send, mbedtls_net_recv,
                        mbedtls_net_recv_timeout);
#endif
    if (COMMON_HTTP_SERVER_PORT_TLS != port) {
        HAL_Printf("[DBG] Performing the SSL/TLS handshake...\n");
        HAL_Printf("[DBG] Connecting to /%s/%d...\n", STR_SAFE_PRINT(host), port);
    }

    int connect_timeout_s = 3 * (pConnectParams->timeout_ms / 1000);
    if (connect_timeout_s < 10)
        connect_timeout_s = 10;
    ret = _mbedtls_tcp_connect(&(pDataParams->socket_fd), host, port, connect_timeout_s);
    if (ret != QCLOUD_RET_SUCCESS) {
        goto error;
    }

    Timer timer;
    HAL_Timer_init(&timer);
    HAL_Timer_countdown_ms(&timer, 5000);
    while ((ret = mbedtls_ssl_handshake(&(pDataParams->ssl))) != 0) {
        if (HAL_Timer_expired(&timer)) {
            break;
        }
        HAL_SleepMs(2);
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            HAL_Printf("[ERR] mbedtls_ssl_handshake failed returned 0x%04x\n", ret < 0 ? -ret : ret);
            if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
                HAL_Printf("[ERR] Unable to verify the server's certificate\n");
            }
            goto error;
        }
    }

    if ((ret = mbedtls_ssl_get_verify_result(&(pDataParams->ssl))) != 0) {
        HAL_Printf("[ERR] mbedtls_ssl_get_verify_result failed returned 0x%04x\n", ret < 0 ? -ret : ret);
        goto error;
    }

    mbedtls_ssl_conf_read_timeout(&(pDataParams->ssl_conf), 10);
    if (COMMON_HTTP_SERVER_PORT_TLS != port) {
        HAL_Printf("[DBG] connected with /%s/%d... handle %p\n", STR_SAFE_PRINT(host), port, pDataParams);
    }
    return (uintptr_t)pDataParams;

error:
    _free_mbedtls(pDataParams);
    return 0;
}

void HAL_TLS_Disconnect(uintptr_t handle)
{
    if ((uintptr_t)NULL == handle) {
        HAL_Printf("[DBG] handle is NULL\n");
        return;
    }
    // HAL_Printf("[DBG] Disconnecting handle: %p\n", handle);
    TLSDataParams *pParams = (TLSDataParams *)handle;
    int ret                = 0;
    Timer timer;
    HAL_Timer_init(&timer);
    HAL_Timer_countdown_ms(&timer, 500);
    do {
        ret = mbedtls_ssl_close_notify(&(pParams->ssl));
        if (HAL_Timer_expired(&timer)) {
            break;
        }
        HAL_SleepMs(2);
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    // mbedtls_ssl_close_notify(&(pParams->ssl));
    mbedtls_net_free(&(pParams->socket_fd));
    mbedtls_x509_crt_free(&(pParams->client_cert));
    mbedtls_x509_crt_free(&(pParams->ca_cert));
    mbedtls_pk_free(&(pParams->private_key));
    mbedtls_ssl_free(&(pParams->ssl));
    mbedtls_ssl_config_free(&(pParams->ssl_conf));
    mbedtls_ctr_drbg_free(&(pParams->ctr_drbg));
    mbedtls_entropy_free(&(pParams->entropy));

    HAL_Free((void *)handle);
}

int HAL_TLS_Write(uintptr_t handle, unsigned char *msg, size_t totalLen, uint32_t timeout_ms, size_t *written_len)
{
    Timer timer;
    HAL_Timer_init(&timer);
    HAL_Timer_countdown_ms(&timer, (unsigned int)timeout_ms);
    size_t written_so_far;
    bool errorFlag = false;
    int write_rc   = 0;

    TLSDataParams *pParams = (TLSDataParams *)handle;

    for (written_so_far = 0; written_so_far < totalLen && !HAL_Timer_expired(&timer); written_so_far += write_rc) {
        while (!HAL_Timer_expired(&timer) &&
               (write_rc = mbedtls_ssl_write(&(pParams->ssl), msg + written_so_far, totalLen - written_so_far)) <= 0) {
            if (write_rc != MBEDTLS_ERR_SSL_WANT_READ && write_rc != MBEDTLS_ERR_SSL_WANT_WRITE) {
                HAL_Printf("[ERR] HAL_TLS_write failed 0x%04x, handle %p\n", write_rc < 0 ? -write_rc : write_rc,
                           handle);
                errorFlag = true;
                break;
            }
            // HAL_SleepMs(2);
        }

        if (errorFlag) {
            break;
        }
    }

    *written_len = written_so_far;

    if (errorFlag) {
        return QCLOUD_ERR_SSL_WRITE;
    } else if (HAL_Timer_expired(&timer) && written_so_far != totalLen) {
        return QCLOUD_ERR_SSL_WRITE_TIMEOUT;
    }

    return QCLOUD_RET_SUCCESS;
}

int HAL_TLS_Read(uintptr_t handle, unsigned char *msg, size_t totalLen, uint32_t timeout_ms, size_t *read_len)
{
    // mbedtls_ssl_conf_read_timeout(&(pParams->ssl_conf), timeout_ms); TODO:this
    // cause read blocking and no return even timeout
    // use non-blocking read
    Timer timer;
    HAL_Timer_init(&timer);
    HAL_Timer_countdown_ms(&timer, (unsigned int)timeout_ms);
    *read_len = 0;

    TLSDataParams *pParams = (TLSDataParams *)handle;

    do {
        int read_rc = 0;
        read_rc     = mbedtls_ssl_read(&(pParams->ssl), msg + *read_len, totalLen - *read_len);

        if (read_rc > 0) {
            *read_len += read_rc;
        } else if (read_rc == 0 || (read_rc != MBEDTLS_ERR_SSL_WANT_WRITE && read_rc != MBEDTLS_ERR_SSL_WANT_READ &&
                                    read_rc != MBEDTLS_ERR_SSL_TIMEOUT)) {
            HAL_Printf("[ERR] cloud_iot_network_tls_read failed: 0x%04x, handle %p\n", read_rc < 0 ? -read_rc : read_rc,
                       handle);
            return QCLOUD_ERR_SSL_READ;
        }

        if (HAL_Timer_expired(&timer)) {
            break;
        }
        // HAL_SleepMs(2);
    } while (*read_len < totalLen);

    if (totalLen == *read_len) {
        return QCLOUD_RET_SUCCESS;
    }

    if (*read_len == 0) {
        return QCLOUD_ERR_SSL_NOTHING_TO_READ;
    } else {
        return QCLOUD_ERR_SSL_READ_TIMEOUT;
    }
}
