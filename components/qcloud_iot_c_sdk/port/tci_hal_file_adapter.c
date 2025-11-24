/**
 * @file tci_hal_file_adapter.c
 * @brief 腾讯云物联网硬件抽象层 - 文件操作适配器实现
 *
 * 本文件实现了文件操作相关的HAL接口，包括：
 * - 文件打开/关闭
 * - 文件读写
 * - 文件定位
 * - 文件信息查询
 * - 设备信息存储
 *
 * @author hubertxxu (hubertxxu@tencent.com)
 * @version 1.0
 * @date 2025-10-15
 */

#include "tci_hal_adapter.h"
#include "tc_iot_hal.h"
#include <string.h>

// =============================================================================
// 文件操作接口实现
// =============================================================================

uint32_t TCI_HAL_FileGetDiskSize(void)
{
    return HAL_FileGetDiskSize();
}

void *TCI_HAL_FileOpen(const char *filename, const char *mode)
{
    if (filename == NULL || mode == NULL) {
        return NULL; // 参数错误
    }
    return HAL_FileOpen(filename, mode);
}

size_t TCI_HAL_FileRead(void *ptr, size_t size, size_t nmemb, void *fp)
{
    if (ptr == NULL || fp == NULL || size == 0) {
        return 0; // 参数错误
    }
    return HAL_FileRead(ptr, size, nmemb, fp);
}

size_t TCI_HAL_FileWrite(const void *ptr, size_t size, size_t nmemb, void *fp)
{
    if (ptr == NULL || fp == NULL || size == 0) {
        return 0; // 参数错误
    }
    return HAL_FileWrite(ptr, size, nmemb, fp);
}

int TCI_HAL_FileSeek(void *fp, long int offset, int whence)
{
    if (fp == NULL) {
        return -1; // 参数错误
    }
    return HAL_FileSeek(fp, offset, whence);
}

int TCI_HAL_FileClose(void *fp)
{
    if (fp == NULL) {
        return -1; // 参数错误
    }
    return HAL_FileClose(fp);
}

int TCI_HAL_FileRemove(const char *filename)
{
    if (filename == NULL) {
        return -1; // 参数错误
    }
    return HAL_FileRemove(filename);
}

int TCI_HAL_FileRewind(void *fp)
{
    if (fp == NULL) {
        return -1; // 参数错误
    }
    return HAL_FileRewind(fp);
}

int TCI_HAL_FileRename(const char *old_filename, const char *new_filename)
{
    if (old_filename == NULL || new_filename == NULL) {
        return -1; // 参数错误
    }
    return HAL_FileRename(old_filename, new_filename);
}

int TCI_HAL_FileEof(void *fp)
{
    if (fp == NULL) {
        return 1; // 参数错误，返回EOF状态
    }
    return HAL_FileEof(fp);
}

int TCI_HAL_FileError(void *fp)
{
    if (fp == NULL) {
        return 1; // 参数错误，返回错误状态
    }
    return HAL_FileError(fp);
}

long TCI_HAL_FileTell(void *fp)
{
    if (fp == NULL) {
        return -1; // 参数错误
    }
    return HAL_FileTell(fp);
}

long TCI_HAL_FileSize(void *fp)
{
    if (fp == NULL) {
        return -1; // 参数错误
    }
    return HAL_FileSize(fp);
}

char *TCI_HAL_FileGets(char *str, int n, void *fp)
{
    if (str == NULL || fp == NULL || n <= 0) {
        return NULL; // 参数错误
    }
    return HAL_FileGets(str, n, fp);
}

int TCI_HAL_FileFlush(void *fp)
{
    if (fp == NULL) {
        return -1; // 参数错误
    }
    return HAL_FileFlush(fp);
}

// =============================================================================
// 设备信息存储接口实现
// =============================================================================

static uint8_t sg_device_info[256] = {0}; // 假设设备信息最大长度为256字节

/**
 * @brief 三元组持久化存储接口
 *
 * @param[in] device_info 设备三元组信息
 * @param[in] len 信息长度
 * @return @see IotReturnCode
 */
int TCI_HAL_SetDevInfo(uint8_t *device_info, int len)
{
    memset(sg_device_info, 0, sizeof(sg_device_info));
    memcpy(sg_device_info, device_info, len);
    return 0;
}

/**
 * @brief 获取设备三元组信息
 *
 * @param[out] device_info 设备三元组信息
 * @param[in] len 信息长度
 * @return @see IotReturnCode
 */
int TCI_HAL_GetDevInfo(uint8_t *device_info, int len)
{
    memcpy(device_info, sg_device_info, len);
    return 0;
}

