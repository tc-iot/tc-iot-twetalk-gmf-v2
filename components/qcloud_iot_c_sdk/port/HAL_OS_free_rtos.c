/*****************************************************************************
 * Copyright (C) 2022 Tencent. All rights reserved.
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

#ifdef PLATFORM_ESP_RTOS

#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/semphr.h"
#include "FreeRTOS/task.h"
#include "FreeRTOS/queue.h"

#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_mac.h"


typedef struct {
    StackType_t  *taskStack;
    StaticTask_t *taskTCB;
} FreeRTOSStackInfo;

#endif

#ifndef MS_TO_TICKS
#define MS_TO_TICKS(n) ((n) / portTICK_PERIOD_MS ? (n) / portTICK_PERIOD_MS : 1)
#endif

void HAL_SleepMs(uint32_t ms)
{
#ifdef JIELI_RTOS
    mdelay(ms);
#else
    vTaskDelay(MS_TO_TICKS(ms)); /* Minimum delay = 1 tick */
#endif

    return;
}

void *HAL_Malloc(uint32_t size)
{
    void *ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
        return ptr;
    }
    return NULL;
}

void *HAL_Realloc(void *ptr, uint32_t size)
{
    return realloc(ptr, size);
}

void HAL_Free(void *ptr)
{
    if (ptr) {
        free(ptr);
    }
    ptr = NULL;
}

void HAL_Printf(const char *fmt, ...)
{
    char    buf[512] = {0};
    va_list args;
    int     len = 0;

    va_start(args, fmt);
    len = vsnprintf(buf, 511, fmt, args);
    va_end(args);

    printf("%s\n", buf);
}

int HAL_Snprintf(char *str, const int len, const char *fmt, ...)
{
    va_list args;
    int     rc;

    va_start(args, fmt);
    rc = vsnprintf(str, len, fmt, args);
    va_end(args);

    return rc;
}

int HAL_Vsnprintf(char *str, const int len, const char *format, va_list ap)
{
    return vsnprintf(str, len, format, ap);
}

void HAL_GetMAC(uint8_t *mac, uint8_t len)
{
    // !获取MAC地址
#ifdef PLATFORM_ESP_RTOS
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_BASE);
    if (ret == ESP_OK) {
        Log_d("MAC address: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        Log_e("Failed to get MAC address");
    }
#else
    Log_e("HAL_GetMAC not implement!!!");
    return;
#endif
}

uint32_t HAL_GetMemSize(void)
{
    // !获取剩余内存大小
#ifdef PLATFORM_ESP_RTOS
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
#else
    Log_e("HAL_GetMemSize not implement!!!");
    return 0;
#endif
}

long HAL_Random(void)
{
    // !获取随机数
#ifdef PLATFORM_ESP_RTOS
    return esp_random();
#else
    Log_e("HAL_Random not implemented!!!");
    return 0;
#endif
}

char *HAL_GetPlatform(void)
{
    return "freertos";
}

void HAL_Signal(int sginum, void (*handler)(int))
{
    return;
}

// platform-dependant thread routine/entry function
static void _HAL_thread_func_wrapper_(void *ptr)
{
    ThreadParams *params = (ThreadParams *)ptr;

    params->thread_func(params->user_arg);

#ifndef JIELI_RTOS
    vTaskDelete(NULL);

#ifdef PLATFORM_ESP_RTOS
    FreeRTOSStackInfo *stack_info = params->stack_ptr;
    HAL_Free(stack_info->taskStack);
    HAL_Free(stack_info->taskTCB);
    HAL_Free(stack_info);
#endif  // PLATFORM_ESP_RTOS

#endif  // JIELI_RTOS
}

// platform-dependant thread create function
int HAL_ThreadCreate(ThreadParams *params)
{
    if (params == NULL) {
        Log_e("null ThreadParams!");
        return QCLOUD_ERR_INVAL;
    }

    if (params->thread_name == NULL || params->thread_func == NULL || params->stack_size == 0) {
        Log_e("thread params (%p %p %u) invalid", params->thread_name, params->thread_func, params->stack_size);
        return QCLOUD_ERR_INVAL;
    }

    // the specific thread priority is platform dependent
    int priority = params->priority + 3;

#ifdef JIELI_RTOS
    priority = params->priority + 26;
    int ret = thread_fork(params->thread_name, priority, params->stack_size, 1024, (int *)&params->thread_id,
                          _HAL_thread_func_wrapper_, params);
    if (ret) {
        Log_e("thread_fork failed: %d", ret);
        return QCLOUD_ERR_FAILURE;
    }
#else

#ifdef PLATFORM_ESP_RTOS
    // ESP32 都使用外部RAM CPU1来创建任务
    // priority = configMAX_PRIORITIES - 4 + params->priority;
    params->stack_ptr             = (FreeRTOSStackInfo *)HAL_Malloc(sizeof(FreeRTOSStackInfo));
    FreeRTOSStackInfo *stack_info = (FreeRTOSStackInfo *)params->stack_ptr;
    if (!stack_info) {
        Log_e("Error stack info malloc fail. create %s stack fail.\n", params->thread_name);
        return QCLOUD_ERR_FAILURE;
    }

    stack_info->taskStack = (StackType_t *)HAL_Malloc(params->stack_size);

    if (stack_info->taskStack) {
        memset(stack_info->taskStack, 0, params->stack_size);
    } else {
        Log_e("Error malloc %s task stack %d bytes.\n", params->thread_name, params->stack_size);
        return QCLOUD_ERR_FAILURE;
    }

    stack_info->taskTCB = heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);  // 内部sram
    if (!stack_info->taskTCB) {
        Log_e("%s task. HAL_Malloc taskTCB failed\n", params->thread_name);
        HAL_Free(stack_info->taskStack);
        HAL_Free(stack_info);
        return QCLOUD_ERR_FAILURE;
    }

    memset(stack_info->taskTCB, 0, sizeof(StaticTask_t));
    params->thread_id = (ThreadHandle_t)xTaskCreateStaticPinnedToCore(_HAL_thread_func_wrapper_, params->thread_name,
                                                      params->stack_size, (void *)params, priority | portPRIVILEGE_BIT,
                                                      stack_info->taskStack, stack_info->taskTCB, 0);
    if (params->thread_id == NULL) {
        Log_e("Error creating xTaskCreateStaticPinnedToCore %s\n", params->thread_name);
        HAL_Free(stack_info->taskStack);
        HAL_Free(stack_info->taskTCB);
        HAL_Free(stack_info);
        return QCLOUD_ERR_FAILURE;
    }

#else
    int ret = xTaskCreate(_HAL_thread_func_wrapper_, params->thread_name,
                          params->stack_size / sizeof(StackType_t),  // byte --> world
                          (void *)params, priority, (void *)&params->thread_id);
    if (ret != pdPASS) {
        Log_e("xTaskCreate failed: %d", ret);
        return QCLOUD_ERR_FAILURE;
    }
#endif  // PLATFORM_ESP_RTOS

#endif  // JIELI_RTOS

    Log_i("created task %s %p stack size : %d Kbytes priority : %d", params->thread_name, params->thread_id,
          ((params->stack_size) >> 10), priority);
    return QCLOUD_RET_SUCCESS;
}

int HAL_ThreadDestroy(ThreadHandle_t *thread_handle)
{
    if (NULL == thread_handle) {
        return QCLOUD_ERR_FAILURE;
    }

#ifdef JIELI_RTOS
    // thread_kill(thread_handle, KILL_FORCE);
#else
    vTaskDelete((TaskHandle_t)(*thread_handle));
#endif

    ThreadParams *params = container_of(thread_handle, ThreadParams, thread_id);
    if (params) {
        Log_i("destroy task %s %p %p", params->thread_name, params, params->thread_id);
    }
    return 0;
}

unsigned long HAL_GetCurrentThreadHandle(void)
{
    unsigned long handle = (unsigned long)xTaskGetCurrentTaskHandle();
    return handle;
}


/**
 * @brief platform-dependent semaphore create function.
 *
 * @return pointer to semaphore
 */
void *HAL_SemaphoreCreate(void)
{
    SemaphoreHandle_t sem = xSemaphoreCreateCounting(10, 0);
    if (NULL == sem) {
        Log_e("xSemaphoreCreate failed");
        return NULL;
    }

    return sem;
}

/**
 * @brief platform-dependent semaphore destory function.
 *
 * @param[in] sem pointer to semaphore
 */
void HAL_SemaphoreDestroy(void *sem)
{
    if (!sem) {
        Log_e("invalid sem");
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
        Log_e("invalid sem");
        return;
    }

    if (xSemaphoreGive(sem) != pdTRUE) {
        Log_e("xSemaphoreGive failed %p", sem);
    }
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
        Log_e("invalid sem");
        return QCLOUD_ERR_FAILURE;
    }

    if (xSemaphoreTake(sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        Log_e("xSemaphoreTake failed %p", sem);
        return QCLOUD_ERR_FAILURE;
    }

    return QCLOUD_RET_SUCCESS;
}

void *HAL_MutexCreate(void)
{
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    if (NULL == mutex) {
        Log_e("xSemaphoreCreateMutex failed");
        return NULL;
    }

    return mutex;
}

void HAL_MutexDestroy(void *mutex)
{
    if (!mutex) {
        Log_e("invalid mutex");
        return;
    }

    if (xSemaphoreTake(mutex, 0) != pdTRUE) {
        Log_e("xSemaphoreTake failed");
    }

    vSemaphoreDelete(mutex);
}

int HAL_MutexLock(void *mutex)
{
    if (!mutex) {
        Log_e("invalid mutex");
        return -1;
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        Log_e("xSemaphoreTake failed");
        return -1;
    }

    return 0;
}

int HAL_MutexTryLock(void *mutex)
{
    if (!mutex) {
        Log_e("invalid mutex");
        return -1;
    }

    if (xSemaphoreTake(mutex, 0) != pdTRUE) {
        Log_e("xSemaphoreTake failed");
        return -1;
    }

    return 0;
}

int HAL_MutexUnlock(void *mutex)
{
    if (!mutex) {
        Log_e("invalid mutex");
        return -1;
    }

    if (xSemaphoreGive(mutex) != pdTRUE) {
        Log_e("xSemaphoreGive failed");
        return -1;
    }
    return 0;
}

void *HAL_RecursiveMutexCreate(void)
{
    SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
    if (NULL == mutex) {
        Log_e("xSemaphoreCreateRecursiveMutex failed");
        return NULL;
    }

    return mutex;
}

void HAL_RecursiveMutexDestroy(void *mutex)
{
    if (!mutex) {
        Log_e("invalid mutex");
        return;
    }

    if (xSemaphoreTakeRecursive(mutex, 0) != pdTRUE) {
        Log_e("xSemaphoreTakeRecursive failed");
    }

    vSemaphoreDelete(mutex);
}

int HAL_RecursiveMutexLock(void *mutex, int try_flag)
{
    if (!mutex) {
        Log_e("invalid mutex");
        return -1;
    }

    int32_t xBlockTime = portMAX_DELAY;
    if (try_flag)
        xBlockTime = 0;

    if (xSemaphoreTakeRecursive(mutex, xBlockTime) != pdTRUE) {
        Log_e("xSemaphoreTakeRecursive failed");
        return -1;
    }
    return 0;
}

int HAL_RecursiveMutexUnLock(void *mutex)
{
    if (!mutex) {
        Log_e("invalid mutex");
        return -1;
    }

    if (xSemaphoreGiveRecursive(mutex) != pdTRUE) {
        Log_e("xSemaphoreGiveRecursive failed");
        return -1;
    }

    return 0;
}

void *HAL_CondCreate()
{
    SemaphoreHandle_t xSemaphore = xSemaphoreCreateBinary();
    if (NULL == xSemaphore) {
        Log_e("xSemaphoreCreateBinary failed");
        return NULL;
    }

    return xSemaphore;
}

void HAL_CondFree(void *cond_)
{
    if (!cond_) {
        Log_e("invalid cond handle");
        return;
    }

    vSemaphoreDelete(cond_);
    return;
}

int HAL_CondSignal(void *cond_, int broadcast)
{
    if (!cond_) {
        Log_e("invalid cond handle");
        return -1;
    }

    if (broadcast) {
        Log_w("broadcast not supported");
    }

    if (xSemaphoreGive(cond_) != pdTRUE) {
        Log_e("xSemaphoreGive failed %p", cond_);
        return -1;
    }

    return 0;
}

static uint32_t HAL_msToOSTick(uint32_t ms)
{
    if (ms == 0)
        return 0;

    // Tick will be round up. With the checker, 32bits divide
    // will be used in most cases.
    if (ms <= ((0xffffffff - 999) / configTICK_RATE_HZ))
        return (ms * configTICK_RATE_HZ + 999) / 1000;

    return ((uint64_t)ms * configTICK_RATE_HZ + 999) / 1000;
}

int HAL_CondWait(void *cond_, void *lock_, unsigned long timeout_ms)
{
    if (!cond_ || !lock_) {
        Log_e("invalid handle");
        return -1;
    }

    TickType_t xBlockTime = portMAX_DELAY;
    if (timeout_ms) {
        xBlockTime = MS_TO_TICKS(timeout_ms);
        // xBlockTime = HAL_msToOSTick(timeout_ms);
    }

    HAL_RecursiveMutexUnLock(lock_);

    if (xSemaphoreTake(cond_, xBlockTime) != pdTRUE) {
        Log_e("xSemaphoreTake failed %p %u", cond_, xBlockTime);
        return -1;
    }

    HAL_RecursiveMutexLock(lock_, 0);

    return 0;
}

/**
 * @brief platform-dependent mail queue init function.
 *
 * @param[in] pool pool using in mail queue
 * @param[in] mail_size mail size
 * @param[in] mail_count mail count
 * @return pointer to mail queue
 */
void *HAL_MailQueueInit(void *pool,size_t mail_size, int mail_count)
{
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
int HAL_MailQueueSend(void *mail_q, const void *buf, size_t size, uint32_t timeout_ms)
{
    if (!mail_q) {
        return -1;
    }
    if (pdTRUE != xQueueSend(mail_q, buf, MS_TO_TICKS(timeout_ms))) {
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
    return xQueueReceive(mail_q, buf, MS_TO_TICKS(timeout_ms)) == pdTRUE ? 0 : QCLOUD_ERR_FAILURE;
}

