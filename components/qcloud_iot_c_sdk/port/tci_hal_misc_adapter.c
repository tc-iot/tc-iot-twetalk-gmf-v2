/**
 * @file tci_hal_misc_adapter.c
 * @brief 腾讯云物联网硬件抽象层 - 杂项功能适配器实现
 *
 * 本文件实现了其他杂项功能的HAL接口，包括：
 * - 内存管理
 * - 时间管理
 * - 定时器
 * - 平台信息
 * - WiFi和模块管理
 * - OTA相关接口
 *
 * @author hubertxxu (hubertxxu@tencent.com)
 * @version 1.0
 * @date 2025-10-15
 */

#include "tci_hal_adapter.h"
#include "tc_iot_hal.h"
#include <stdarg.h>
#include <stdio.h>

// =============================================================================
// 内存管理接口实现
// =============================================================================

void *TCI_HAL_Malloc(size_t size)
{
    return HAL_Malloc(size);
}

void *TCI_HAL_Realloc(void *ptr, uint32_t size)
{
    return HAL_Realloc(ptr, size);
}

void TCI_HAL_Free(void *ptr)
{
    HAL_Free(ptr);
}

void TCI_HAL_Printf(const char *fmt, ...)
{
    if (fmt != NULL) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
}

int TCI_HAL_Snprintf(char *str, const int len, const char *fmt, ...)
{
    if (str == NULL || fmt == NULL || len <= 0) {
        return 0; // 参数错误
    }

    va_list args;
    va_start(args, fmt);
    int result = HAL_Vsnprintf(str, len, fmt, args);
    va_end(args);
    return result;
}

int TCI_HAL_Vsnprintf(char *str, const int len, const char *fmt, va_list ap)
{
    if (str == NULL || fmt == NULL || len <= 0) {
        return 0; // 参数错误
    }
    return HAL_Vsnprintf(str, len, fmt, ap);
}

void TCI_HAL_GetMAC(uint8_t *mac, uint8_t len)
{
    if (mac != NULL) {
        HAL_GetMAC(mac, len);
    }
}

uint32_t TCI_HAL_GetMemSize(void)
{
    return HAL_GetMemSize();
}

char *TCI_HAL_GetPlatform(void)
{
    return HAL_GetPlatform();
}

int TCI_HAL_Random(void)
{
    return (int)HAL_Random();
}

void TCI_HAL_Signal(int sginum, void (*handler)(int))
{
    HAL_Signal(sginum, handler);
}

// =============================================================================
// 时间管理接口实现
// =============================================================================

void TCI_HAL_SleepMs(uint32_t ms)
{
    HAL_SleepMs(ms);
}

char *TCI_HAL_GetLocalTime(char *time_str, size_t time_str_len)
{
    if (time_str == NULL || time_str_len == 0) {
        return NULL; // 参数错误
    }
    return HAL_GetLocalTime(time_str, time_str_len);
}

long TCI_HAL_GetTimeSecond(void)
{
    return HAL_GetTimeSecond();
}

uint64_t TCI_HAL_GetTimeMs(void)
{
    return HAL_GetTimeMs();
}

uint64_t TCI_HAL_GetTicksTimeMs(void)
{
    return HAL_GetTicksTimeMs();
}

int TCI_HAL_SetTimeMs(size_t timestamp_ms)
{
    return HAL_SetTimeMs(timestamp_ms);
}

int TCI_HAL_SetTimeSecond(size_t timestamp_sec)
{
    return HAL_SetTimeSecond(timestamp_sec);
}

bool TCI_HAL_TimerExpired(TCI_Timer *timer)
{
    if (timer == NULL) {
        return true; // 参数错误，视为已过期
    }
    return HAL_Timer_expired((Timer *)timer);
}

void TCI_HAL_TimerCountdownMs(TCI_Timer *timer, unsigned int timeout_ms)
{
    if (timer != NULL) {
        HAL_Timer_countdown_ms((Timer *)timer, timeout_ms);
    }
}

void TCI_HAL_TimerCountdown(TCI_Timer *timer, unsigned int timeout)
{
    if (timer != NULL) {
        HAL_Timer_countdown((Timer *)timer, timeout);
    }
}

int TCI_HAL_TimerRemain(TCI_Timer *timer)
{
    if (timer == NULL) {
        return 0; // 参数错误，返回0表示已过期
    }
    return HAL_Timer_remain((Timer *)timer);
}

// =============================================================================
// WiFi和模块管理接口实现（按需实现的接口提供空实现）
// =============================================================================

int TCI_HAL_SoftAP_Start(const char *ssid, const char *password, uint8_t ch)
{
    return HAL_SoftAP_Start(ssid, password, ch);
}

int TCI_HAL_SoftAP_Stop(void)
{
    return HAL_SoftAP_Stop();
}

int TCI_HAL_Wifi_Init(void)
{
    return HAL_Wifi_Init();
}

int TCI_HAL_Wifi_ModeSet(TCI_WifiMode mode)
{
    return HAL_Wifi_ModeSet((TCIoTWifiMode)mode);
}

int TCI_HAL_Wifi_StaInfoSet(const char *ssid, uint8_t ssid_len, const char *passwd, uint8_t passwd_len)
{
    return HAL_Wifi_StaInfoSet(ssid, ssid_len, passwd, passwd_len);
}

int TCI_HAL_Wifi_StaConnect(uint32_t timeout_ms)
{
    return HAL_Wifi_StaConnect(timeout_ms);
}

int TCI_HAL_Wifi_LogGet(void)
{
    return HAL_Wifi_LogGet();
}

uint32_t TCI_HAL_Wifi_Ipv4Get(void)
{
    return HAL_Wifi_Ipv4Get();
}

size_t TCI_HAL_Wifi_MacGet(uint8_t *mac)
{
    return HAL_Wifi_MacGet(mac);
}

int TCI_HAL_Module_Init(void)
{
    // 按需实现的接口，提供空实现
    return -1; // 不支持
}

void TCI_HAL_Module_Deinit(void)
{
    // 按需实现的接口，提供空实现
}

int TCI_HAL_Module_SendAtCmdWaitResp(const char *at_cmd, const char *at_expect, uint32_t timeout_ms)
{
    // 按需实现的接口，提供空实现
    (void)at_cmd;
    (void)at_expect;
    (void)timeout_ms;
    return -1; // 不支持
}

int TCI_HAL_Module_SendAtCmdWaitRespWithData(const char *at_cmd, const char *at_expect, void *recv_buf, uint32_t *recv_len,
                                             uint32_t timeout_ms)
{
    // 按需实现的接口，提供空实现
    (void)at_cmd;
    (void)at_expect;
    (void)recv_buf;
    (void)recv_len;
    (void)timeout_ms;
    return -1; // 不支持
}

int TCI_HAL_Module_SendAtData(const void *data, int data_len)
{
    // 按需实现的接口，提供空实现
    (void)data;
    (void)data_len;
    return -1; // 不支持
}

int TCI_HAL_Module_SetUrc(const char *urc, TCI_OnUrcHandler urc_handler)
{
    // 按需实现的接口，提供空实现
    (void)urc;
    (void)urc_handler;
    return -1; // 不支持
}

int TCI_HAL_Module_ConnectNetwork(void)
{
    // 按需实现的接口，提供空实现
    return -1; // 不支持
}

// =============================================================================
// OTA相关接口实现（按需实现的接口提供空实现）
// =============================================================================

uint32_t TCI_HAL_OTA_get_download_addr(void *usr_data)
{
    return HAL_OTA_get_download_addr(usr_data);
}

int TCI_HAL_OTA_read_flash(void *usr_data, uint32_t read_addr, uint8_t *read_data, uint32_t read_len)
{
    return HAL_OTA_read_flash(usr_data, read_addr, read_data, read_len);
}

int TCI_HAL_OTA_write_flash(void *usr_data, uint32_t write_addr, uint8_t *write_data, uint32_t write_len)
{
    return HAL_OTA_write_flash(usr_data, write_addr, write_data, write_len);
}

void *TCI_HAL_OTA_create_ota_timer(void *usr_data, void(ota_timer_callback)(void *timer))
{
    return HAL_OTA_create_ota_timer(usr_data, ota_timer_callback);
}

int TCI_HAL_OTA_start_ota_timer(void *usr_data, void *timer, uint32_t timeout_ms)
{
    return HAL_OTA_start_ota_timer(usr_data, timer, timeout_ms);
}

int TCI_HAL_OTA_stop_ota_timer(void *usr_data, void *timer)
{
    return HAL_OTA_stop_ota_timer(usr_data, timer);
}

int TCI_HAL_OTA_delete_ota_timer(void *usr_data, void *timer)
{
    return HAL_OTA_delete_ota_timer(usr_data, timer);
}
