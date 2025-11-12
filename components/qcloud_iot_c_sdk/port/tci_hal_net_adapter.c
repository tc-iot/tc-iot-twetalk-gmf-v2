/**
 * @file tci_hal_net_adapter.c
 * @brief 腾讯云物联网硬件抽象层 - 网络通信适配器实现
 *
 * 本文件实现了网络通信相关的HAL接口，包括：
 * - TLS安全连接
 * - TCP连接
 * - UDP通信
 *
 * @author hubertxxu (hubertxxu@tencent.com)
 * @version 1.0
 * @date 2025-10-15
 */

#include "tci_hal_adapter.h"
#include "tc_iot_hal.h"
#include "qcloud_iot_config.h"

// =============================================================================
// 网络通信接口实现
// =============================================================================

#ifndef ENABLE_AUTH_NO_TLS
#define TCI_HAL_TLS_ENABLE
#endif

uintptr_t TCI_HAL_TLS_Connect(TCI_TLSConnectParams *pConnectParams, const char *host, int port)
{
#ifdef TCI_HAL_TLS_ENABLE
    if (pConnectParams == NULL || host == NULL) {
        return 0; // 参数错误
    }
    // TCI_TLSConnectParams和TLSConnectParams结构体兼容，直接转换
    return HAL_TLS_Connect((TLSConnectParams *)pConnectParams, host, port);
#else
    // TLS功能未启用，提供空实现
    (void)pConnectParams;
    (void)host;
    (void)port;
    return 0; // 不支持TLS
#endif
}

void TCI_HAL_TLS_Disconnect(uintptr_t handle)
{
#ifdef TCI_HAL_TLS_ENABLE
    if (handle != 0) {
        HAL_TLS_Disconnect(handle);
    }
#else
    // TLS功能未启用，提供空实现
    (void)handle;
#endif
}

int TCI_HAL_TLS_Write(uintptr_t handle, unsigned char *data, size_t totalLen, uint32_t timeout_ms, size_t *written_len)
{
#ifdef TCI_HAL_TLS_ENABLE
    if (handle == 0 || data == NULL || written_len == NULL) {
        return -1; // 参数错误
    }
    return HAL_TLS_Write(handle, data, totalLen, timeout_ms, written_len);
#else
    // TLS功能未启用，提供空实现
    (void)handle;
    (void)data;
    (void)totalLen;
    (void)timeout_ms;
    (void)written_len;
    return -1; // 不支持TLS
#endif
}

int TCI_HAL_TLS_Read(uintptr_t handle, unsigned char *data, size_t totalLen, uint32_t timeout_ms, size_t *read_len)
{
#ifdef TCI_HAL_TLS_ENABLE
    if (handle == 0 || data == NULL || read_len == NULL) {
        return -1; // 参数错误
    }
    return HAL_TLS_Read(handle, data, totalLen, timeout_ms, read_len);
#else
    // TLS功能未启用，提供空实现
    (void)handle;
    (void)data;
    (void)totalLen;
    (void)timeout_ms;
    (void)read_len;
    return -1; // 不支持TLS
#endif
}

uintptr_t TCI_HAL_TCP_Connect(const char *host, uint16_t port)
{
    if (host == NULL) {
        return 0; // 参数错误
    }
    return HAL_TCP_Connect(host, port);
}

int TCI_HAL_TCP_Disconnect(uintptr_t fd)
{
    if (fd == 0) {
        return -1; // 参数错误
    }
    return HAL_TCP_Disconnect(fd);
}

int TCI_HAL_TCP_Write(uintptr_t fd, const unsigned char *data, uint32_t len, uint32_t timeout_ms, size_t *written_len)
{
    if (fd == 0 || data == NULL || written_len == NULL) {
        return -1; // 参数错误
    }
    return HAL_TCP_Write(fd, data, len, timeout_ms, written_len);
}

int TCI_HAL_TCP_Read(uintptr_t fd, unsigned char *data, uint32_t len, uint32_t timeout_ms, size_t *read_len)
{
    if (fd == 0 || data == NULL || read_len == NULL) {
        return -1; // 参数错误
    }
    return HAL_TCP_Read(fd, data, len, timeout_ms, read_len);
}

int TCI_HAL_UDP_Bind(const char *ip, uint16_t port)
{
    // 按需实现的接口，提供空实现
    (void)ip;
    (void)port;
    return 0;
}

int TCI_HAL_UDP_Recv(int fd, uint8_t *p_data, uint32_t datalen, uint32_t timeout_ms, char *recv_ip_addr,
                     uint32_t recv_addr_len, uint16_t *recv_port)
{
    // 按需实现的接口，提供空实现
    (void)fd;
    (void)p_data;
    (void)datalen;
    (void)timeout_ms;
    (void)recv_ip_addr;
    (void)recv_addr_len;
    (void)recv_port;
    return 0;
}

int TCI_HAL_UDP_Send(int fd, const uint8_t *p_data, uint32_t datalen, const char *host, const char *port)
{
    // 按需实现的接口，提供空实现
    (void)fd;
    (void)p_data;
    (void)datalen;
    (void)host;
    (void)port;
    return 0;
}

void TCI_HAL_UDP_Close(int fd)
{
    // 按需实现的接口，提供空实现
    (void)fd;
    return;
}
