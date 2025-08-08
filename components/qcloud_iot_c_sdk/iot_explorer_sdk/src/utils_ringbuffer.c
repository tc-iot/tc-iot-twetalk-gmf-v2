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
 * @file utils_ringbuffer.c
 * @brief Ring buffer utility implementation
 * @author hubertxxu
 * @version 1.0
 * @date 2025-07-28
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2025-07-28 <td>1.0     <td>hubertxxu <td>Initial implementation
 * </table>
 */

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "utils_ringbuffer.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define RINGBUFFER_INDEX_TO_PTR(_index, _total_size) ((_index) % (_total_size))

/**
 * @brief Ring buffer structure
 */
typedef struct {
    UtilsRingBufferFunc func;        /**< Function pointers for memory and lock operations */
    uint32_t            total_size;  /**< Total size of the buffer */
    uint32_t            read_index;  /**< Current read index */
    uint32_t            write_index; /**< Current write index */
    uint32_t            used_count;  /**< Current used bytes count */
    uint8_t            *buffer;      /**< Pointer to the buffer memory */
    void               *lock;        /**< Pointer to the lock (if any) */
    int                 is_frame;    /**< Whether the ring buffer is used for frame */
} RingBuffer;

/**
 * @brief Acquire ring buffer lock
 *
 * @param[in] rb Pointer to ring buffer
 */
static inline void _ringbuffer_lock(RingBuffer *rb)
{
    if (rb->func.ringbuffer_lock) {
        rb->func.ringbuffer_lock(rb->lock);
    }
}

/**
 * @brief Release ring buffer lock
 *
 * @param[in] rb Pointer to ring buffer
 */
static inline void _ringbuffer_unlock(RingBuffer *rb)
{
    if (rb->func.ringbuffer_unlock) {
        rb->func.ringbuffer_unlock(rb->lock);
    }
}

/**
 * @brief Create a ring buffer
 *
 * @param[in] func Function pointers for memory and lock operations
 * @param[in] total_size Total size of the ring buffer
 * @return void* Pointer to created ring buffer, NULL on failure
 */
void *utils_ringbuffer_create(UtilsRingBufferFunc func, uint32_t total_size, int is_frame)
{
    RingBuffer *rb = (RingBuffer *)func.ringbuffer_malloc(sizeof(RingBuffer));
    if (!rb) {
        return NULL;
    }
    memset(rb, 0, sizeof(RingBuffer));
    rb->total_size  = total_size;
    rb->read_index  = 0;
    rb->write_index = 0;
    rb->used_count  = 0;
    rb->buffer      = (uint8_t *)func.ringbuffer_malloc(total_size);
    if (!rb->buffer) {
        func.ringbuffer_free(rb);
        return NULL;
    }

    if (func.ringbuffer_lock_init) {
        rb->lock = func.ringbuffer_lock_init();
        if (!rb->lock) {
            func.ringbuffer_free(rb->buffer);
            func.ringbuffer_free(rb);
            return NULL;
        }
    }
    rb->func     = func;
    rb->is_frame = is_frame;
    return rb;
}

/**
 * @brief Destroy a ring buffer
 *
 * @param[in] rb Pointer to ring buffer to destroy
 */
void utils_ringbuffer_destroy(void *rb)
{
    RingBuffer *ringbuffer = (RingBuffer *)rb;
    if (ringbuffer) {
        if (ringbuffer->buffer) {
            ringbuffer->func.ringbuffer_free(ringbuffer->buffer);
        }

        if (ringbuffer->lock) {
            ringbuffer->func.ringbuffer_lock_deinit(ringbuffer->lock);
        }
        ringbuffer->func.ringbuffer_free(ringbuffer);
    }
}

/**
 * @brief Get the total size of the ring buffer
 *
 * @param[in] ringbuffer Pointer to ring buffer
 * @return uint32_t Total size of the buffer
 */
uint32_t utils_ringbuffer_total_size(void *ringbuffer)
{
    RingBuffer *rb = (RingBuffer *)ringbuffer;
    if (rb == NULL) {
        return 0;
    }
    _ringbuffer_lock(rb);
    uint32_t total_size = rb->total_size;
    _ringbuffer_unlock(rb);
    return total_size;
}

/**
 * @brief Reset the ring buffer
 *
 * @param[in] ringbuffer Pointer to ring buffer
 */
static inline void utils_ringbuffer_reset(void *ringbuffer)
{
    RingBuffer *rb = (RingBuffer *)ringbuffer;
    if (rb == NULL) {
        return;
    }
    _ringbuffer_lock(rb);
    rb->write_index = 0;
    rb->read_index  = 0;
    rb->used_count  = 0;
    _ringbuffer_unlock(rb);
}

/**
 * @brief Check if ring buffer is empty
 *
 * @param[in] ringbuffer Pointer to ring buffer
 * @return int 1 if empty, 0 otherwise
 */
int utils_ringbuffer_is_empty(void *ringbuffer)
{
    RingBuffer *rb = (RingBuffer *)ringbuffer;
    if (rb == NULL) {
        return 1;  // Consider NULL as empty
    }
    _ringbuffer_lock(rb);
    int rc = rb->used_count == 0;
    _ringbuffer_unlock(rb);
    return rc;
}

/**
 * @brief Get used size of ring buffer
 *
 * @param[in] ringbuffer Pointer to ring buffer
 * @return uint32_t Used size in bytes
 */
uint32_t utils_ringbuffer_used_size(void *ringbuffer)
{
    RingBuffer *rb = (RingBuffer *)ringbuffer;
    if (rb == NULL) {
        return 0;
    }
    _ringbuffer_lock(rb);
    uint32_t used_size = rb->used_count;
    _ringbuffer_unlock(rb);
    return used_size;
}

/**
 * @brief Get free size of ring buffer
 *
 * @param[in] ringbuffer Pointer to ring buffer
 * @return uint32_t Free size in bytes
 */
uint32_t utils_ringbuffer_reserve_size(void *ringbuffer)
{
    RingBuffer *rb = (RingBuffer *)ringbuffer;
    if (rb == NULL) {
        return 0;
    }
    _ringbuffer_lock(rb);
    uint32_t reserve_size = rb->total_size - rb->used_count;
    _ringbuffer_unlock(rb);
    return reserve_size;
}

/**
 * @brief Check if ring buffer is full
 *
 * @param[in] ringbuffer Pointer to ring buffer
 * @return int 1 if full, 0 otherwise
 */
int utils_ringbuffer_is_full(void *ringbuffer)
{
    RingBuffer *rb = (RingBuffer *)ringbuffer;
    if (rb == NULL) {
        return 0;  // Consider NULL as not full
    }
    _ringbuffer_lock(rb);
    int is_full = rb->used_count == rb->total_size;
    _ringbuffer_unlock(rb);
    return is_full;
}

static uint32_t _ringbuffer_get(RingBuffer *rb, uint8_t *buffer, uint32_t len)
{
    uint32_t l;
    _ringbuffer_lock(rb);
    uint32_t rptr = RINGBUFFER_INDEX_TO_PTR(rb->read_index, rb->total_size);

    len = MIN(len, rb->used_count);

    /* First get the data from rb->read_index until the end of the buffer */
    l = MIN(len, rb->total_size - rptr);
    memcpy(buffer, rb->buffer + rptr, l);

    /* Then get the rest (if any) from the beginning of the buffer */
    memcpy(buffer + l, rb->buffer, len - l);

    rb->read_index = (rb->read_index + len) % rb->total_size;
    rb->used_count -= len;
    _ringbuffer_unlock(rb);
    return len;
}

static uint32_t _ringbuffer_put(RingBuffer *rb, uint8_t *buffer, uint32_t len)
{
    uint32_t l;
    _ringbuffer_lock(rb);
    uint32_t wptr = RINGBUFFER_INDEX_TO_PTR(rb->write_index, rb->total_size);

    len = MIN(len, rb->total_size - rb->used_count);

    /* First put the data starting from rb->write_index to buffer end */
    l = MIN(len, rb->total_size - wptr);
    memcpy(rb->buffer + wptr, buffer, l);

    /* Then put the rest (if any) at the beginning of the buffer */
    memcpy(rb->buffer, buffer + l, len - l);

    rb->write_index = (rb->write_index + len) % rb->total_size;
    rb->used_count += len;
    _ringbuffer_unlock(rb);
    return len;
}

/**
 * @brief Put data into ring buffer in frame mode
 *
 * @param[in] ringbuffer Pointer to ring buffer
 * @param[in] buffer Data to put
 * @param[in] len Length of data to put
 * @return uint32_t Actual number of bytes put (including frame header)
 */
static uint32_t utils_ringbuffer_put_frame(void *ringbuffer, uint8_t *buffer, uint32_t len)
{
    RingBuffer *rb = (RingBuffer *)ringbuffer;
    if (rb == NULL || buffer == NULL || len == 0 || !rb->is_frame) {
        return 0;
    }

    _ringbuffer_lock(rb);

    // 帧头: 2字节长度信息
    uint8_t header[2] = {
        (uint8_t)(len & 0xFF),        // 低字节
        (uint8_t)((len >> 8) & 0xFF)  // 高字节
    };

    // 检查空间是否足够 (数据 + 帧头)
    uint32_t required = len + sizeof(header);
    if (rb->total_size - rb->used_count < required) {
        _ringbuffer_unlock(rb);
        return 0;
    }

    // 写入帧头
    uint32_t header_written = _ringbuffer_put(rb, header, sizeof(header));
    if (header_written != sizeof(header)) {
        _ringbuffer_unlock(rb);
        return 0;
    }

    // 写入数据
    uint32_t data_written = _ringbuffer_put(rb, buffer, len);
    _ringbuffer_unlock(rb);
    return (data_written == len) ? (header_written + data_written) : 0;
}

/**
 * @brief Get data from ring buffer in frame mode
 *
 * @param[in] ringbuffer Pointer to ring buffer
 * @param[out] buffer Buffer to store data
 * @param[in] buffer_size Size of output buffer
 * @return uint32_t Actual number of data bytes retrieved (excluding frame header)
 */
static uint32_t utils_ringbuffer_get_frame(void *ringbuffer, uint8_t *buffer, uint32_t buffer_size)
{
    RingBuffer *rb = (RingBuffer *)ringbuffer;
    if (rb == NULL || buffer == NULL || buffer_size == 0 || !rb->is_frame) {
        return 0;
    }

    _ringbuffer_lock(rb);

    // 读取帧头
    uint8_t  header[2];
    uint32_t header_read = _ringbuffer_get(rb, header, sizeof(header));
    if (header_read != sizeof(header)) {
        _ringbuffer_unlock(rb);
        return 0;  // 帧头不完整
    }

    // 解析帧长度
    uint32_t frame_len = (uint32_t)header[0] | ((uint32_t)header[1] << 8);
    // 检查缓冲区是否足够
    if (frame_len > buffer_size) {
        // 缓冲区不足，放回帧头
        rb->read_index = (rb->read_index + rb->total_size - sizeof(header)) % rb->total_size;
        rb->used_count += sizeof(header);
        _ringbuffer_unlock(rb);
        return 0;
    }

    // 读取帧数据
    uint32_t data_read = _ringbuffer_get(rb, buffer, frame_len);
    _ringbuffer_unlock(rb);
    return (data_read == frame_len) ? data_read : 0;
}

/**
 * @brief Put data into ring buffer
 *
 * @param[in] ringbuffer Pointer to ring buffer
 * @param[in] buffer Data to put
 * @param[in] len Length of data to put
 * @return uint32_t Actual number of bytes put
 */
uint32_t utils_ringbuffer_put(void *ringbuffer, uint8_t *buffer, uint32_t len)
{
    RingBuffer *rb = (RingBuffer *)ringbuffer;
    if (rb == NULL || buffer == NULL || len == 0) {
        return 0;  // Invalid parameters
    }

    // 如果是帧模式，使用专用函数
    if (rb->is_frame) {
        return utils_ringbuffer_put_frame(ringbuffer, buffer, len);
    }
    return _ringbuffer_put(rb, buffer, len);
}

/**
 * @brief Get data from ring buffer
 *
 * @param[in] ringbuffer Pointer to ring buffer
 * @param[out] buffer Buffer to store data
 * @param[in] len Length of data to get
 * @return uint32_t Actual number of bytes retrieved
 */
uint32_t utils_ringbuffer_get(void *ringbuffer, uint8_t *buffer, uint32_t len)
{
    RingBuffer *rb = (RingBuffer *)ringbuffer;
    if (rb == NULL || buffer == NULL || len == 0) {
        return 0;  // Invalid parameters
    }
    // 如果是帧模式，使用专用函数
    if (rb->is_frame) {
        return utils_ringbuffer_get_frame(ringbuffer, buffer, len);
    }
    return _ringbuffer_get(rb, buffer, len);
}

/**
 * @brief Get idle rate of ring buffer
 *
 * @param[in] ringbuffer Pointer to ring buffer
 * @return int Idle rate (0-100), -1 on error
 */
int utils_ringbuffer_idle_rate(void *ringbuffer)
{
    RingBuffer *rb = (RingBuffer *)ringbuffer;
    if (rb == NULL || rb->total_size == 0) {
        return -1;  // Invalid ring buffer or size is 0
    }
    _ringbuffer_lock(rb);
    uint32_t reserve_size = rb->total_size - rb->used_count;
    int      idle_rate    = (int)((reserve_size * 100) / rb->total_size);
    _ringbuffer_unlock(rb);
    return idle_rate;
}
