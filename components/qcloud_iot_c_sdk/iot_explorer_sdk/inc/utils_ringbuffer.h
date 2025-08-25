/**
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
 * @file utils_ringbuffer.h
 * @brief header file for ring buffer utility
 * @author hubertxxu
 * @version 1.0
 * @date 2025-07-28
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2025-07-28 <td>1.0     <td>hubertxxu <td>完善头文件声明
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_COMMON_UTILS_INC_UTILS_RINGBUFFER_H_
#define IOT_HUB_DEVICE_C_SDK_COMMON_UTILS_INC_UTILS_RINGBUFFER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#include "qcloud_iot_platform.h"

typedef struct {
    void *(*ringbuffer_malloc)(uint32_t len);
    void (*ringbuffer_free)(void *val);
    void *(*ringbuffer_lock_init)(void);
    int (*ringbuffer_lock)(void *lock, int try_flag);
    int (*ringbuffer_unlock)(void *lock);
    void (*ringbuffer_lock_deinit)(void *lock);
} UtilsRingBufferFunc;

/**
 * @brief Default ring buffer functions with lock support
 */
#define DEFAULT_RINGBUFFER_FUNCS \
    {HAL_Malloc,                 \
     HAL_Free,                   \
     HAL_RecursiveMutexCreate,   \
     HAL_RecursiveMutexLock,     \
     HAL_RecursiveMutexUnLock,   \
     HAL_RecursiveMutexDestroy}

/**
 * @brief Default ring buffer functions without lock support
 */
#define DEFAULT_UNLOCK_RINGBUFFER_FUNCS {HAL_Malloc, HAL_Free, NULL, NULL, NULL, NULL}

/**
 * @brief Create a ring buffer
 *
 * @param[in] func Function pointers for memory and lock operations
 * @param[in] total_size Total size of the ring buffer
 * @return void* Pointer to created ring buffer, NULL on failure
 */
void *utils_ringbuffer_create(UtilsRingBufferFunc func, uint32_t total_size, int is_frame);

/**
 * @brief Destroy a ring buffer
 *
 * @param[in] rb Pointer to ring buffer to destroy
 */
void utils_ringbuffer_destroy(void *rb);

/**
 * @brief Get the total size of the ring buffer
 *
 * @param[in] rb Pointer to ring buffer
 * @return uint32_t Total size of the buffer
 */
uint32_t utils_ringbuffer_total_size(void *rb);

/**
 * @brief Check if ring buffer is empty
 *
 * @param[in] rb Pointer to ring buffer
 * @return int 1 if empty, 0 otherwise
 */
int utils_ringbuffer_is_empty(void *rb);

/**
 * @brief Get used size of ring buffer
 *
 * @param[in] rb Pointer to ring buffer
 * @return uint32_t Used size in bytes
 */
uint32_t utils_ringbuffer_used_size(void *rb);

/**
 * @brief Get free size of ring buffer
 *
 * @param[in] rb Pointer to ring buffer
 * @return uint32_t Free size in bytes
 */
uint32_t utils_ringbuffer_reserve_size(void *rb);

/**
 * @brief Check if ring buffer is full
 *
 * @param[in] rb Pointer to ring buffer
 * @return int 1 if full, 0 otherwise
 */
int utils_ringbuffer_is_full(void *rb);

/**
 * @brief Put data into the ring buffer
 *
 * @param[in] rb Pointer to the ring buffer
 * @param[in] buffer Data to put into the buffer
 * @param[in] len Length of data to put
 * @return uint32_t Actual number of bytes put
 */
uint32_t utils_ringbuffer_put(void *rb, uint8_t *buffer, uint32_t len);

/**
 * @brief Get data from the ring buffer
 *
 * @param[in] rb Pointer to the ring buffer
 * @param[out] buffer Buffer to store the data
 * @param[in] len Length of data to get
 * @return uint32_t Actual number of bytes retrieved
 */
uint32_t utils_ringbuffer_get(void *rb, uint8_t *buffer, uint32_t len);

/**
 * @brief Get idle rate of the ring buffer
 *
 * @param[in] rb Pointer to the ring buffer
 * @return int Idle rate (0-100), -1 on error
 */
int utils_ringbuffer_idle_rate(void *rb);

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_COMMON_UTILS_INC_UTILS_RINGBUFFER_H_
