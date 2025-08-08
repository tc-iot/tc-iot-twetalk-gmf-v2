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
 * @file HAL_OS_tencentos_tiny.c
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
#include <stdlib.h>
#include <stdarg.h>

#include "qcloud_iot_platform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/**
 * @brief Sleep for ms
 *
 * @param[in] ms ms to sleep
 */
void HAL_SleepMs(uint32_t ms)
{
    TickType_t ticks = ms / portTICK_PERIOD_MS;
    vTaskDelay(ticks ? ticks : 1); /* Minimum delay = 1 tick */
}

/**
 * @brief Printf with format.
 *
 * @param[in] fmt format
 */
void HAL_Printf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    fflush(stdout);
}

/**
 * @brief Snprintf with format.
 *
 * @param[out] str buffer to save
 * @param[in] len buffer len
 * @param[in] fmt format
 * @return length of formatted string, >0 for success.
 */
int HAL_Snprintf(char *str, const int len, const char *fmt, ...)
{
    va_list args;
    int     rc;

    va_start(args, fmt);
    rc = vsnprintf(str, len, fmt, args);
    va_end(args);

    return rc;
}

#ifdef WIFI_CONFIG_BLE_LLSYNC_USED  // for esp
#include "esp_heap_caps.h"
#endif

/**
 * @brief Malloc from heap.
 *
 * @param[in] size size to malloc
 * @return pointer to buffer, NULL for failed.
 */
void *HAL_Malloc(size_t size)
{
#ifdef WIFI_CONFIG_BLE_LLSYNC_USED  // for esp
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
    return pvPortMalloc(size);
#endif
}

/**
 * @brief Free buffer malloced by HAL_Malloc.
 *
 * @param[in] ptr
 */
void HAL_Free(void *ptr)
{
    if (ptr) {
#ifdef WIFI_CONFIG_BLE_LLSYNC_USED  // for esp
        heap_caps_free(ptr);
#else
        vPortFree(ptr);
#endif
    }
}

/**
 * @brief Mutex create.
 *
 * @return pointer to mutex
 */
void *HAL_MutexCreate(void)
{
    SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
    if (!mutex) {
        HAL_Printf("%s: xSemaphoreCreateRecursiveMutex failed\n", __FUNCTION__);
        return NULL;
    }
    return (void *)mutex;
}

/**
 * @brief Mutex destroy.
 *
 * @param[in,out] mutex pointer to mutex
 */
void HAL_MutexDestroy(void *mutex)
{
    if (xSemaphoreTakeRecursive(mutex, 0) != pdTRUE) {
        HAL_Printf("%s: xSemaphoreTakeRecursive failed\n", __FUNCTION__);
    }
    vSemaphoreDelete(mutex);
}

/**
 * @brief Mutex lock.
 *
 * @param[in,out] mutex pointer to mutex
 */
void HAL_MutexLock(void *mutex)
{
    if (!mutex) {
        HAL_Printf("%s: invalid mutex\n", __FUNCTION__);
        return;
    }

    if (xSemaphoreTakeRecursive(mutex, portMAX_DELAY) != pdTRUE) {
        HAL_Printf("%s: xSemaphoreTakeRecursive failed\n", __FUNCTION__);
        return;
    }
}

/**
 * @brief Mutex try lock.
 *
 * @param[in,out] mutex pointer to mutex
 * @return 0 for success
 */
int HAL_MutexTryLock(void *mutex)
{
    if (!mutex) {
        HAL_Printf("%s: invalid mutex\n", __FUNCTION__);
        return -1;
    }

    if (xSemaphoreTakeRecursive(mutex, 0) != pdTRUE) {
        HAL_Printf("%s: xSemaphoreTakeRecursive failed\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

/**
 * @brief Mutex unlock.
 *
 * @param[in,out] mutex pointer to mutex
 */
void HAL_MutexUnlock(void *mutex)
{
    if (!mutex) {
        HAL_Printf("%s: invalid mutex\n", __FUNCTION__);
        return;
    }

    if (xSemaphoreGiveRecursive(mutex) != pdTRUE) {
        HAL_Printf("%s: xSemaphoreGiveRecursive failed\n", __FUNCTION__);
        return;
    }
}

/**
 * @brief platform-dependent thread routine/entry function
 *
 * @param[in,out] ptr
 * @return NULL
 */
static void _HAL_thread_func_wrapper_(void *ptr)
{
    ThreadParams *params = (ThreadParams *)ptr;

    params->thread_func(params->user_arg);
    vTaskDelete(NULL);
}

/**
 * @brief platform-dependent thread create function
 *
 * @param[in,out] params params to create thread @see ThreadParams
 * @return @see IotReturnCode
 */
int HAL_ThreadCreate(ThreadParams *params)
{
    UBaseType_t uxPriority = 1;
    switch (params->priority) {
        case THREAD_PRIORITY_HIGH:
            uxPriority = configMAX_PRIORITIES - 1;
            break;
        case THREAD_PRIORITY_MIDDLE:
            uxPriority = configMAX_PRIORITIES / 2;
            break;
        case THREAD_PRIORITY_LOW:
            uxPriority = 1;
            break;
    }
    int rc = xTaskCreate(_HAL_thread_func_wrapper_, params->thread_name, params->stack_size, (void *)params, uxPriority,
                         (void *)&params->thread_id);
    if (rc != pdPASS) {
        HAL_Printf("%s: xTaskCreate failed: %d\n", __FUNCTION__, rc);
        return QCLOUD_ERR_FAILURE;
    }
    return QCLOUD_RET_SUCCESS;
}

/**
 * @brief platform-dependent thread destroy function.
 *
 */
void HAL_ThreadDestroy(void *thread_id)
{
    // no use in sdk
    vTaskDelete(thread_id);
}

/**
 * @brief platform-dependent semaphore create function.
 *
 * @return pointer to semaphore
 */
void *HAL_SemaphoreCreate(void)
{
    return (void *)xSemaphoreCreateBinary();
}

/**
 * @brief platform-dependent semaphore destory function.
 *
 * @param[in] sem pointer to semaphore
 */
void HAL_SemaphoreDestroy(void *sem)
{
    if (!sem) {
        return;
    }
    vSemaphoreDelete(sem);
}

/**
 * @brief platform-dependent semaphore post function.
 *
 * @param[in] sem pointer to semaphore
 */
void HAL_SemaphorePost(void *sem)
{
    if (!sem) {
        return;
    }
    xSemaphoreGive(sem);
}

/**
 * @brief platform-dependent semaphore wait function.
 *
 * @param[in] sem pointer to semaphore
 * @param[in] timeout_ms wait timeout
 * @return @see IotReturnCode
 */
int HAL_SemaphoreWait(void *sem, uint32_t timeout_ms)
{
    if (!sem) {
        return -1;
    }
    return pdTRUE != xSemaphoreTake(sem, timeout_ms);
}

/**
 * @brief platform-dependent mail queue init function.
 *
 * @param[in] pool pool using in mail queue
 * @param[in] mail_size mail size
 * @param[in] mail_count mail count
 * @return pointer to mail queue
 */
void *HAL_MailQueueInit(void *pool, size_t mail_size, int mail_count)
{
    if (pool) {
        Log_e("only dynamic create is supportted!");
    }
    return xQueueCreate(mail_count, mail_size);
}

/**
 * @brief platform-dependent mail queue deinit function.
 *
 * @param[in] mail_q pointer to mail queue
 */
void HAL_MailQueueDeinit(void *mail_q)
{
    if (!mail_q) {
        return;
    }
    vQueueDelete(mail_q);
    // HAL_Free(mail_q);
    return;
}

/**
 * @brief platform-dependent mail queue send function.
 *
 * @param[in] mail_q pointer to mail queue
 * @param[in] buf data buf
 * @param[in] size data size
 * @return 0 for success
 */
int HAL_MailQueueSend(void *mail_q, const void *buf, size_t size)
{
    if (!mail_q) {
        return -1;
    }
    if (pdTRUE != xQueueSend(mail_q, buf, portMAX_DELAY)) {
        return QCLOUD_ERR_FAILURE;
    }
    return 0;
}

/**
 * @brief platform-dependent mail queue send function.
 *
 * @param[in] mail_q pointer to mail queue
 * @param[out] buf data buf
 * @param[in] size data size
 * @param[in] timeout_ms
 * @return 0 for success
 */
int HAL_MailQueueRecv(void *mail_q, void *buf, size_t *size, uint32_t timeout_ms)
{
    if (!mail_q) {
        return -1;
    }
    return pdTRUE != xQueueReceive(mail_q, buf, timeout_ms);
}
