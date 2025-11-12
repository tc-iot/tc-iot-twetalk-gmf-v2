/**
 * @file tci_hal_adapter.h
 * @brief 腾讯云物联网硬件抽象层适配器接口定义
 *
 * 本文件定义了腾讯云物联网(TCI)设备SDK的硬件抽象层(HAL)接口，
 * 提供了跨平台的统一API接口，包括线程管理、内存管理、时间管理、
 * 网络通信、文件操作等核心功能模块。
 *
 * 适配器设计用于屏蔽不同操作系统和硬件平台的差异，为上层应用
 * 提供一致的编程接口，支持Linux、FreeRTOS、RT-Thread等多种平台。
 *
 * @author hubertxxu (hubertxxu@tencent.com)
 * @version 1.0
 * @date 2025-10-14
 *
 * @copyright
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2021 - 2026 Tencent all rights reserved.
 * Licensed under the MIT License(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @par 修改记录:
 * <table>
 * <tr><th>日期       <th>版本 <th>作者      <th>描述
 * <tr><td>2025-10-14 <td>1.0  <td>hubertxxu <td>首次提交
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_PLATFORM_ADAPTER_TCI_HAL_ADAPTER_H_
#define IOT_HUB_DEVICE_C_SDK_PLATFORM_ADAPTER_TCI_HAL_ADAPTER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "stdint.h"
#include "stdarg.h"
#include "stdbool.h"

#if defined(__linux__) && defined(__GLIBC__)
#include <sys/time.h>
#endif

/**
 * @defgroup TCI_HAL_ADAPTER 腾讯云物联网硬件抽象层适配器
 * @brief 腾讯云物联网(TCI)硬件抽象层适配器接口
 *
 * TCI是Tencent Cloud IoT(腾讯云物联网)的缩写，本模块提供了
 * 统一的硬件抽象层接口，支持多种操作系统和硬件平台。
 * @{
 */

/**
 * @defgroup TCI_HAL_THREAD 线程管理接口
 * @brief 提供跨平台的线程创建、管理和同步功能
 * @{
 */

/** @brief 线程句柄类型定义 */
typedef unsigned long TCI_ThreadHandle_t;

/** @brief 线程运行函数类型定义 */
typedef void (*TCI_ThreadRunFunc)(void *arg);

/**
 * @brief 线程优先级枚举类型定义
 *
 * 定义了五个优先级等级，数值越大优先级越高。
 * 具体的优先级映射由各平台的实现决定。
 */
typedef enum {
    TCI_THREAD_PRIORITY_LOW     = -1, /**< 低优先级 */
    TCI_THREAD_PRIORITY_NORMAL  = 0,  /**< 普通优先级(默认) */
    TCI_THREAD_PRIORITY_HIGH    = 1,  /**< 高优先级 */
    TCI_THREAD_PRIORITY_HIGHER  = 2,  /**< 更高优先级 */
    TCI_THREAD_PRIORITY_HIGHEST = 3   /**< 最高优先级 */
} TCI_ThreadPriorityLevel;

/**
 * @brief 线程创建参数结构体
 *
 * 包含了创建线程所需的所有参数，不同平台可能需要不同的参数组合。
 * 该结构体必须在线程启动后保持有效，建议使用静态变量。
 */
typedef struct {
    char                *thread_name;  /**< 线程名称，必填参数 */
    TCI_ThreadHandle_t   thread_id;    /**< 线程句柄，必填参数 */
    TCI_ThreadRunFunc    thread_func;  /**< 线程运行函数，必填参数 */
    void                *user_arg;     /**< 用户自定义参数，可选 */
    uint32_t             stack_size;   /**< 线程栈大小(字节)，RTOS上为必填 */
    void                *stack_ptr;    /**< 线程栈指针，某些平台(如ThreadX)需要 */
    uint32_t             slice_tick;   /**< 时间片大小，某些平台(如RT-Thread)需要 */
    uint16_t             priority;     /**< 线程优先级，建议使用TCI_ThreadPriorityLevel */
} TCI_ThreadParams;

/**
 * @brief 创建线程
 *
 * 创建一个新的线程，对于RTOS设备建议创建到外部PSRAM中以节省内部RAM。
 * 线程参数结构体必须在线程启动后保持有效，建议使用静态变量。
 *
 * @param[in] params 线程创建参数，包含线程名、函数、栈大小等信息
 *
 * @return
 * - 0: 成功
 * - 非0: 失败，返回错误码
 *
 * @note 该函数是异步的，调用成功后线程会立即开始执行
 *
 * @par 示例代码:
 * @code
 * static TCI_ThreadParams thread_params = {
 *     .thread_name = "test_thread",
 *     .thread_func = test_thread_func,
 *     .stack_size = 4096,
 *     .priority = TCI_THREAD_PRIORITY_NORMAL
 * };
 * int ret = TCI_HAL_ThreadCreate(&thread_params);
 * @endcode
 */
int TCI_HAL_ThreadCreate(TCI_ThreadParams *params);

/**
 * @brief 销毁线程
 *
 * 销毁指定的线程并释放相关资源。SDK创建的线程执行完毕后会自动调用
 * 线程销毁函数，用户通常不需要手动调用此函数。
 *
 * @param[in,out] thread_t 要销毁的线程句柄指针，销毁后会被置为无效值
 *
 * @return
 * - 0: 成功
 * - 非0: 失败，返回错误码
 *
 * @warning 不要销毁正在运行的线程，应该先让线程正常退出
 */
int TCI_HAL_ThreadDestroy(TCI_ThreadHandle_t *thread_t);

/**
 * @brief 获取当前线程句柄
 *
 * 获取当前正在执行的线程的句柄，可用于线程间通信或调试。
 *
 * @return 当前线程的句柄值
 *
 * @note 在不同平台上，句柄的具体含义可能不同(线程ID、指针等)
 */
unsigned long TCI_HAL_GetCurrentThreadHandle(void);

/** @} */ // end of TCI_HAL_THREAD

/**
 * @defgroup TCI_HAL_SYNC 互斥锁和同步原语接口
 * @brief 提供跨平台的线程同步和互斥功能
 * @{
 */

/**
 * @brief 创建互斥锁
 *
 * 创建一个新的互斥锁(mutex)，用于防止多个线程同时访问共享资源，
 * 避免竞态条件和数据损坏。
 *
 * @return 成功时返回互斥锁句柄，失败时返回NULL
 *
 * @par 使用场景:
 * - 保护共享数据结构
 * - 临界区访问控制
 * - 资源独占访问
 */
void *TCI_HAL_MutexCreate(void);

/**
 * @brief 销毁互斥锁
 *
 * 销毁指定的互斥锁并释放相关资源。
 *
 * @param[in] mutex 要销毁的互斥锁句柄
 *
 * @warning 销毁前确保没有线程持有该锁，否则可能导致死锁
 */
void TCI_HAL_MutexDestroy(void *mutex);

/**
 * @brief 获取互斥锁(阻塞模式)
 *
 * 尝试获取互斥锁，如果锁已被其他线程持有，则阻塞当前线程直到
 * 锁变为可用状态。
 *
 * @param[in] mutex 要获取的互斥锁句柄
 *
 * @return
 * - 0: 成功获取锁
 * - 非0: 获取失败，返回错误码
 *
 * @note 必须与TCI_HAL_MutexUnlock配对使用
 */
int TCI_HAL_MutexLock(void *mutex);

/**
 * @brief 尝试获取互斥锁(非阻塞模式)
 *
 * 尝试获取互斥锁，如果锁当前不可用，函数立即返回而不阻塞。
 *
 * @param[in] mutex 要获取的互斥锁句柄
 *
 * @return
 * - 0: 成功获取锁
 * - 非0: 锁不可用或发生错误
 *
 * @par 使用场景:
 * 适用于不希望阻塞当前线程的情况，可以做其他工作后再次尝试。
 */
int TCI_HAL_MutexTryLock(void *mutex);

/**
 * @brief 释放互斥锁
 *
 * 释放之前获取的互斥锁，使其他等待的线程能够获取该锁。
 *
 * @param[in] mutex 要释放的互斥锁句柄
 *
 * @return
 * - 0: 成功释放锁
 * - 非0: 释放失败，返回错误码
 *
 * @warning 只能由持有锁的线程调用此函数
 */
int TCI_HAL_MutexUnlock(void *mutex);

/**
 * @brief 创建递归互斥锁
 *
 * 创建一个递归互斥锁，允许同一线程多次获取该锁而不会导致死锁。
 * 必须对应调用相同次数的unlock才能真正释放锁。
 *
 * @return 成功时返回递归互斥锁句柄，失败时返回NULL
 *
 * @par 使用场景:
 * - 递归函数中需要保护共享资源
 * - 同一线程需要多次进入临界区
 */
void *TCI_HAL_RecursiveMutexCreate(void);

/**
 * @brief 销毁递归互斥锁
 *
 * 销毁指定的递归互斥锁并释放相关资源。
 *
 * @param[in] mutex 要销毁的递归互斥锁句柄
 *
 * @warning 销毁前确保递归计数为0，即完全释放该锁
 */
void TCI_HAL_RecursiveMutexDestroy(void *mutex);

/**
 * @brief 获取递归互斥锁
 *
 * 获取递归互斥锁，支持阻塞和非阻塞两种模式。
 *
 * @param[in] mutex    要获取的递归互斥锁句柄
 * @param[in] try_flag 获取模式标志
 *                     - 0: 阻塞模式，等待直到获取成功
 *                     - 1: 非阻塞模式，立即返回结果
 *
 * @return
 * - 0: 成功获取锁
 * - 非0: 获取失败，返回错误码
 */
int TCI_HAL_RecursiveMutexLock(void *mutex, int try_flag);

/**
 * @brief 释放递归互斥锁
 *
 * 释放一次递归互斥锁。如果同一线程多次获取了该锁，
 * 必须调用相同次数的释放操作才能完全释放锁。
 *
 * @param[in] mutex 要释放的递归互斥锁句柄
 *
 * @return
 * - 0: 成功
 * - 非0: 失败，返回错误码
 */
int TCI_HAL_RecursiveMutexUnLock(void *mutex);

/**
 * @brief 创建条件变量
 *
 * 创建一个条件变量(condition variable)，用于线程间的条件同步。
 * 条件变量通常与互斥锁配合使用，实现线程的等待和唤醒机制。
 *
 * @return 成功时返回条件变量句柄，失败时返回NULL
 *
 * @par 使用场景:
 * - 生产者-消费者模式
 * - 线程池中的任务分发
 * - 事件驱动的线程同步
 */
void *TCI_HAL_CondCreate(void);

/**
 * @brief 销毁条件变量
 *
 * 销毁指定的条件变量并释放相关资源。
 *
 * @param[in] cond_ 要销毁的条件变量句柄
 *
 * @warning 销毁前确保没有线程在等待该条件变量
 */
void TCI_HAL_CondFree(void *cond_);

/**
 * @brief 通知条件变量
 *
 * 向等待在条件变量上的线程发送信号，可以选择唤醒一个或所有等待线程。
 *
 * @param[in] cond_     条件变量句柄
 * @param[in] broadcast 通知模式
 *                      - 0: 只唤醒一个等待线程
 *                      - 非0: 唤醒所有等待线程(广播模式)
 *
 * @return
 * - 0: 成功
 * - 非0: 失败，返回错误码
 */
int TCI_HAL_CondSignal(void *cond_, int broadcast);

/**
 * @brief 等待条件变量
 *
 * 使当前线程在条件变量上等待，直到被其他线程唤醒或超时。
 * 在等待期间会自动释放关联的互斥锁，被唤醒后重新获取该锁。
 *
 * @param[in] cond       条件变量句柄
 * @param[in] lock       关联的互斥锁句柄，调用前必须已获取该锁
 * @param[in] timeout_ms 等待超时时间(毫秒)
 *                       - 0: 无限期等待
 *                       - >0: 超时时间
 *
 * @return
 * - 0: 被正常唤醒
 * - 非0: 超时或发生错误
 *
 * @note 调用前必须持有互斥锁，函数返回后仍持有该锁
 */
int TCI_HAL_CondWait(void *cond, void *lock, unsigned long timeout_ms);

/**
 * @brief 创建信号量
 *
 * 创建一个信号量(semaphore)，用于控制对有限资源的访问。
 * 信号量维护一个计数值，表示可用资源的数量。
 *
 * @return 成功时返回信号量句柄，失败时返回NULL
 *
 * @par 使用场景:
 * - 资源池管理
 * - 限制并发访问数量
 * - 线程间的计数同步
 */
void *TCI_HAL_SemaphoreCreate(void);

/**
 * @brief 销毁信号量
 *
 * 销毁指定的信号量并释放相关资源。
 *
 * @param[in] sem 要销毁的信号量句柄
 *
 * @warning 销毁前确保没有线程在等待该信号量
 */
void TCI_HAL_SemaphoreDestroy(void *sem);

/**
 * @brief 释放信号量
 *
 * 增加信号量的计数值，如果有线程在等待该信号量，则唤醒其中一个。
 *
 * @param[in] sem 信号量句柄
 *
 * @note 通常在资源释放或任务完成后调用
 */
void TCI_HAL_SemaphorePost(void *sem);

/**
 * @brief 等待信号量
 *
 * 尝试获取信号量，如果当前计数值大于0则立即返回并将计数减1，
 * 否则阻塞等待直到有可用信号量或超时。
 *
 * @param[in] sem        信号量句柄
 * @param[in] timeout_ms 等待超时时间(毫秒)
 *                       - 0: 无限期等待
 *                       - >0: 超时时间
 *
 * @return
 * - 0: 成功获取信号量
 * - 非0: 超时或发生错误
 */
int TCI_HAL_SemaphoreWait(void *sem, uint32_t timeout_ms);

/**
 * @brief 创建邮件队列
 *
 * 初始化一个邮件队列，用于线程间的消息传递。邮件队列支持固定大小的消息，
 * 提供FIFO(先进先出)的消息传递机制。
 *
 * @param[in] pool       内存池指针，用于分配队列内存(某些平台需要)
 * @param[in] mail_size  单个邮件的大小(字节)
 * @param[in] mail_count 队列容量(最大邮件数量)
 *
 * @return 成功时返回邮件队列句柄，失败时返回NULL
 *
 * @par 使用场景:
 * - 任务间消息传递
 * - 事件通知机制
 * - 数据缓冲队列
 */
void *TCI_HAL_MailQueueInit(void *pool, size_t mail_size, int mail_count);

/**
 * @brief 销毁邮件队列
 *
 * 销毁指定的邮件队列并释放相关资源。
 *
 * @param[in] mail_q 要销毁的邮件队列句柄
 *
 * @warning 销毁前确保队列中没有重要数据，且没有线程在等待
 */
void TCI_HAL_MailQueueDeinit(void *mail_q);

/**
 * @brief 向邮件队列发送消息
 *
 * 向指定的邮件队列发送一个消息。如果队列已满，根据超时设置决定
 * 是阻塞等待还是立即返回失败。
 *
 * @param[in] mail_q     邮件队列句柄
 * @param[in] buf        要发送的数据缓冲区
 * @param[in] size       数据大小(字节)，必须与队列创建时的mail_size匹配
 * @param[in] timeout_ms 发送超时时间(毫秒)
 *                       - 0: 非阻塞，队列满时立即失败
 *                       - >0: 阻塞等待指定时间
 *
 * @return
 * - 0: 发送成功
 * - 非0: 发送失败，队列满或超时
 */
int TCI_HAL_MailQueueSend(void *mail_q, const void *buf, size_t size, uint32_t timeout_ms);

/**
 * @brief 从邮件队列接收消息
 *
 * 从指定的邮件队列接收一个消息。如果队列为空，根据超时设置决定
 * 是阻塞等待还是立即返回失败。
 *
 * @param[in]  mail_q     邮件队列句柄
 * @param[out] buf        接收数据的缓冲区
 * @param[in,out] size    输入时为缓冲区大小，输出时为实际接收的数据大小
 * @param[in]  timeout_ms 接收超时时间(毫秒)
 *                        - 0: 非阻塞，队列空时立即失败
 *                        - >0: 阻塞等待指定时间
 *
 * @return
 * - 0: 接收成功
 * - 非0: 接收失败，队列空或超时
 */
int TCI_HAL_MailQueueRecv(void *mail_q, void *buf, size_t *size, uint32_t timeout_ms);

/** @} */ // end of TCI_HAL_SYNC

/**
 * @defgroup TCI_HAL_MEMORY 内存管理接口
 * @brief 提供跨平台的内存分配、释放和信息获取功能
 * @{
 */

/**
 * @brief 分配内存
 *
 * 分配指定大小的内存块。对于资源受限的设备，建议适配为分配PSRAM内存。
 * 分配的内存会自动初始化为0。
 *
 * @param[in] size 要分配的内存大小(字节)
 *
 * @return 成功时返回分配的内存指针，失败时返回NULL
 *
 * @note 分配的内存已清零，相当于调用了memset(ptr, 0, size)
 * @warning 必须使用TCI_HAL_Free释放分配的内存，避免内存泄漏
 */
void *TCI_HAL_Malloc(size_t size);

/**
 * @brief 重新调整内存大小
 *
 * 调整之前分配的内存块大小。如果新大小更大，扩展部分的内容未定义；
 * 如果新大小更小，多余部分会被丢弃。
 *
 * @param[in] ptr  之前分配的内存指针，可以为NULL
 * @param[in] size 新的内存大小(字节)
 *
 * @return 成功时返回调整后的内存指针，失败时返回NULL
 *
 * @note 如果ptr为NULL，此函数等价于TCI_HAL_Malloc
 * @note 返回的指针可能与原指针不同，原指针在成功后失效
 */
void *TCI_HAL_Realloc(void *ptr, uint32_t size);

/**
 * @brief 释放内存
 *
 * 释放之前通过TCI_HAL_Malloc或TCI_HAL_Realloc分配的内存。
 *
 * @param[in] ptr 要释放的内存指针，可以为NULL
 *
 * @note 如果ptr为NULL，此函数不执行任何操作
 * @warning 不要重复释放同一块内存，释放后不要再使用该指针
 */
void TCI_HAL_Free(void *ptr);

/**
 * @brief 格式化输出到控制台
 *
 * 类似于标准库的printf函数，将格式化的字符串输出到控制台或调试终端。
 * 具体输出位置由平台实现决定(串口、终端、调试器等)。
 *
 * @param[in] fmt 格式化字符串，支持printf风格的格式说明符
 * @param[in] ... 可变参数列表，对应格式化字符串中的占位符
 *
 * @par 支持的格式说明符:
 * - %d, %i: 整数
 * - %u: 无符号整数
 * - %x, %X: 十六进制
 * - %s: 字符串
 * - %c: 字符
 * - %f: 浮点数(如果支持)
 */
void TCI_HAL_Printf(const char *fmt, ...);

/**
 * @brief 格式化字符串到缓冲区
 *
 * 将格式化的数据写入指定的字符串缓冲区，类似于标准库的snprintf函数。
 * 自动在结尾添加NULL终止符，并防止缓冲区溢出。
 *
 * @param[out] str 目标字符串缓冲区
 * @param[in]  len 缓冲区最大长度(包括NULL终止符)
 * @param[in]  fmt 格式化字符串
 * @param[in]  ... 可变参数列表
 *
 * @return 实际写入的字符数(不包括NULL终止符)
 *
 * @note 如果格式化结果超过缓冲区长度，会被截断但仍保证NULL终止
 */
int TCI_HAL_Snprintf(char *str, const int len, const char *fmt, ...);

/**
 * @brief 使用va_list格式化字符串到缓冲区
 *
 * 类似于TCI_HAL_Snprintf，但使用va_list参数列表而不是可变参数。
 * 通常用于包装函数中传递参数列表。
 *
 * @param[out] str 目标字符串缓冲区
 * @param[in]  len 缓冲区最大长度(包括NULL终止符)
 * @param[in]  fmt 格式化字符串
 * @param[in]  ap  参数列表(va_list类型)
 *
 * @return 实际写入的字符数(不包括NULL终止符)
 */
int TCI_HAL_Vsnprintf(char *str, const int len, const char *fmt, va_list ap);

/**
 * @brief 获取设备MAC地址
 *
 * 获取设备的网络接口MAC地址，通常是WiFi或以太网接口的MAC地址。
 *
 * @param[out] mac 存储MAC地址的缓冲区
 * @param[in]  len 缓冲区长度，通常为6字节
 *
 * @note 标准MAC地址长度为6字节(48位)
 * @note 某些设备可能有多个网络接口，此函数返回主要接口的MAC
 */
void TCI_HAL_GetMAC(uint8_t *mac ,uint8_t len);

/**
 * @brief 获取可用内存大小
 *
 * 获取系统可用的内存大小。对于使用外部RAM(如PSRAM)的设备，
 * 返回外部RAM的大小；否则返回系统内存大小。
 *
 * @return 可用内存大小(字节)
 *
 * @note 返回值可能是总内存或可用内存，具体取决于平台实现
 */
uint32_t TCI_HAL_GetMemSize(void);

/**
 * @brief 获取平台标识字符串
 *
 * 返回当前运行平台的标识字符串，用于调试和日志记录。
 *
 * @return 平台标识字符串指针(静态存储，不需要释放)
 *
 * @par 示例返回值:
 * - "Linux"
 * - "Windows"
 * - "FreeRTOS"
 * - "ESP32"
 * - "STM32"
 */
char * TCI_HAL_GetPlatform(void);

/**
 * @brief 生成随机数
 *
 * 生成一个随机数。建议使用硬件随机数发生器以获得更好的随机性。
 * 如果没有硬件支持，可以使用软件伪随机数生成器。
 *
 * @return 生成的随机数值
 *
 * @note 随机数的质量对加密和安全应用很重要
 * @note 可能需要在首次使用前进行种子初始化
 */
long TCI_HAL_Random(void);

/**
 * @brief 设置信号处理函数
 *
 * 为指定信号注册处理函数。当系统发送该信号时，会调用注册的处理函数。
 * RTOS或嵌入式系统如不支持信号机制，可以空实现。
 *
 * @param[in] sginum  信号编号(如SIGINT, SIGTERM等)
 * @param[in] handler 信号处理函数指针
 *
 * @note 仅适用于支持信号机制的操作系统(如Linux)
 * @note 嵌入式RTOS通常不支持信号，可以提供空实现
 */
void TCI_HAL_Signal(int sginum, void (*handler)(int));

/** @} */ // end of TCI_HAL_MEMORY

/**
 * @defgroup TCI_HAL_TIME 时间管理接口
 * @brief 提供跨平台的时间获取、设置和定时器功能
 * @{
 */

/**
 * @brief 线程休眠
 *
 * 使当前线程休眠指定的时间。在休眠期间，线程不会占用CPU资源，
 * 其他线程可以继续执行。
 *
 * @param[in] ms 休眠时间(毫秒)
 *
 * @note 实际休眠时间可能略大于指定时间，取决于系统调度精度
 * @note 在RTOS中通常基于系统滴答实现，精度受滴答频率影响
 */
void TCI_HAL_SleepMs(uint32_t ms);

/** @brief 时间格式字符串的最大长度 */
#define TCI_TIME_FORMAT_STR_LEN (24)

/**
 * @brief 获取本地时间字符串
 *
 * 获取当前本地时间并格式化为字符串，格式为：%Y-%m-%d %H:%M:%S.%ms
 * （例如：2023-01-11 18:01:12.973）
 *
 * @param[out] time_str     存储格式化时间字符串的缓冲区
 * @param[in]  time_str_len 缓冲区长度，建议至少为TCI_TIME_FORMAT_STR_LEN
 *
 * @return 成功时返回time_str指针，失败时返回NULL
 *
 * @note 时间格式依赖于系统时区设置
 * @note 某些嵌入式系统可能不支持毫秒精度
 */
char *TCI_HAL_GetLocalTime(char *time_str, size_t time_str_len);

/**
 * @brief 获取当前时间戳(秒)
 *
 * 获取当前UTC时间的时间戳，以秒为单位。时间戳是从Unix纪元
 * (1970年1月1日 00:00:00 UTC)到现在的总秒数。
 *
 * @return 当前时间戳(秒)，如果获取失败返回0
 *
 * @note 返回值为有符号长整型，支持到2038年(32位系统)
 * @note 嵌入式系统需要通过NTP或其他方式同步时间
 */
long TCI_HAL_GetTimeSecond(void);

/**
 * @brief 获取当前时间戳(毫秒)
 *
 * 获取当前UTC时间的时间戳，以毫秒为单位。提供比秒级更高的时间精度。
 *
 * @return 当前时间戳(毫秒)，如果获取失败返回0
 *
 * @note 使用64位无符号整型以避免溢出问题
 * @note 精度取决于系统时钟源的分辨率
 */
uint64_t TCI_HAL_GetTimeMs(void);

/**
 * @brief 获取系统滴答计数值
 *
 * 获取当前系统的滴答计数值，通常从系统启动开始累加。
 * 此值单调递增，不会因为系统时间调整而改变，适用于测量时间间隔。
 *
 * @return 当前系统滴答值(毫秒)
 *
 * @note 此值不受系统时间调整影响，适合用于计时和超时检测
 * @note 在某些系统中可能会溢出，使用时需要考虑回绕问题
 */
uint64_t TCI_HAL_GetTicksTimeMs(void);

/**
 * @brief 设置系统时间戳(毫秒)
 *
 * 设置系统的当前时间戳，以毫秒为单位。通常用于NTP同步后的时间设置。
 * 如果平台不支持或不需要设置系统时间，可以直接返回0。
 *
 * @param[in] timestamp_ms 时间戳值(毫秒)，从Unix纪元开始计算
 *
 * @return
 * - 0: 设置成功或不需要设置
 * - 非0: 设置失败
 *
 * @note 嵌入式设备可能需要特殊权限来设置系统时间
 */
int TCI_HAL_SetTimeMs(size_t timestamp_ms);

/**
 * @brief 设置系统时间戳(秒)
 *
 * 设置系统的当前时间戳，以秒为单位。通常用于NTP同步后的时间设置。
 * 如果平台不支持或不需要设置系统时间，可以直接返回0。
 *
 * @param[in] timestamp_sec 时间戳值(秒)，从Unix纪元开始计算
 *
 * @return
 * - 0: 设置成功或不需要设置
 * - 非0: 设置失败
 *
 * @note 嵌入式设备可能需要特殊权限来设置系统时间
 */
int TCI_HAL_SetTimeSecond(size_t timestamp_sec);

/**
 * @brief 定时器结构体
 *
 * 平台相关的定时器结构体定义，用于存储定时器的状态信息。
 * 不同平台使用不同的实现方式。
 */
struct TCI_Timer {
#if defined(__linux__) && defined(__GLIBC__)
    // 在Linux和glibc环境下，使用timeval结构体来表示结束时间
    struct timeval end_time;
#else
    uint64_t end_time;
#endif
};

typedef struct TCI_Timer TCI_Timer;

/**
 * @brief 检查定时器是否到期
 *
 * 检查指定的定时器是否已经超时。通常用于循环中的超时检测。
 *
 * @param[in] timer 定时器结构体指针
 *
 * @return
 * - true: 定时器已到期
 * - false: 定时器尚未到期
 *
 * @note 在使用前必须先调用TCI_HAL_Timer_countdown_ms或TCI_HAL_Timer_countdown
 */
bool TCI_HAL_TimerExpired(TCI_Timer *timer);

/**
 * @brief 设置定时器倒计时(毫秒)
 *
 * 设置定时器的倒计时时间，以毫秒为单位。定时器会在指定时间后达到到期状态。
 *
 * @param[in,out] timer      定时器结构体指针
 * @param[in]     timeout_ms 倒计时时间(毫秒)
 *
 * @note 调用此函数后，可以使用TCI_HAL_TimerExpired检查是否到期
 * @note 定时器精度取决于系统时钟源
 */
void TCI_HAL_TimerCountdownMs(TCI_Timer *timer, unsigned int timeout_ms);

/**
 * @brief 设置定时器倒计时(秒)
 *
 * 设置定时器的倒计时时间，以秒为单位。适用于较长的定时需求。
 *
 * @param[in,out] timer   定时器结构体指针
 * @param[in]     timeout 倒计时时间(秒)
 *
 * @note 调用此函数后，可以使用TCI_HAL_Timer_expired检查是否到期
 * @note 对于需要高精度的应用，建议使用TCI_HAL_Timer_countdown_ms
 */
void TCI_HAL_TimerCountdown(TCI_Timer *timer, unsigned int timeout);

/**
 * @brief 获取定时器剩余时间
 *
 * 检查定时器的剩余时间。如果定时器已到期，返回0。
 *
 * @param[in] timer 定时器结构体指针
 *
 * @return 剩余时间(毫秒)，如果已到期返回0
 *
 * @note 可用于显示倒计时或动态调整等待策略
 */
int TCI_HAL_TimerRemain(TCI_Timer *timer);

/**
 * @brief 初始化定时器
 *
 * 初始化定时器结构体，为后续使用做准备。
 *
 * @param[out] timer 要初始化的定时器结构体指针
 *
 * @note 在使用定时器相关函数之前必须先调用此函数
 */
/** @} */ // end of TCI_HAL_TIME

/**
 * @defgroup TCI_HAL_NETWORK 网络通信接口
 * @brief 提供跨平台的TCP/UDP/TLS网络通信功能
 *
 * @note TLS功能的启用受宏TCI_HAL_TLS_ENABLE控制：
 *       - 定义TCI_HAL_TLS_ENABLE：启用TLS功能，调用底层HAL_TLS_*接口
 *       - 未定义TCI_HAL_TLS_ENABLE：禁用TLS功能，提供空实现
 * @{
 */

#ifndef TCI_MAX_SIZE_OF_CLIENT_ID
#define TCI_MAX_SIZE_OF_CLIENT_ID (80)
#endif

/**
 * @brief 定义TLS连接参数的结构体
 *
 */
typedef struct {
    const char *ca_crt;                          // CA证书的字符串指针
    uint16_t    ca_crt_len;                      // CA证书的长度

    /**
     * 仅支持使用PSK的设备
     */
    const char  *psk;                            // PSK字符串
    char         psk_id[TCI_MAX_SIZE_OF_CLIENT_ID + 1];  // PSK ID，最大长度为客户端ID长度加1
    size_t       psk_length;                     // PSK的长度
    unsigned int timeout_ms;                     // SSL握手超时时间，单位为毫秒
} TCI_SSLConnectParams;

typedef TCI_SSLConnectParams TCI_TLSConnectParams;

/**
 * @brief 与服务器建立TLS连接
 *
 * 此函数用于设置与服务器的TLS连接。它需要TLS连接参数的引用、服务器地址和端口号。
 * 成功时返回TLS连接句柄，否则返回0。
 *
 * @param pConnectParams TLS连接参数的引用
 * @param host 服务器地址
 * @param port 服务器端口号
 * @return 成功时返回TLS连接句柄，失败时返回0
 */
uintptr_t TCI_HAL_TLS_Connect(TCI_TLSConnectParams *pConnectParams, const char *host, int port);

/**
 * @brief 断开与TLS服务器的连接并释放资源
 *
 * @param handle TLS连接句柄，用于标识需要断开的TLS连接
 */
void TCI_HAL_TLS_Disconnect(uintptr_t handle);

/**
 * @brief 通过TLS连接写入数据
 *
 * @param handle        TLS连接句柄，用于标识特定的TLS连接
 * @param data          源数据指针，指向需要写入的数据
 * @param totalLen      数据的总长度，单位为字节
 * @param timeout_ms    写操作的超时时间，单位为毫秒
 * @param written_len   成功写入的数据长度，通过指针返回
 * @return              QCLOUD_RET_SUCCESS表示成功，其他值表示失败并返回错误码
 */
int TCI_HAL_TLS_Write(uintptr_t handle, unsigned char *data, size_t totalLen, uint32_t timeout_ms, size_t *written_len);

/**
 * @brief 通过TLS连接读取数据
 *
 * @param handle        TLS连接句柄，用于标识特定的TLS连接
 * @param data          目标数据缓冲区，用于存放读取到的数据
 * @param totalLen      数据的总长度，即希望读取的数据量
 * @param timeout_ms    超时值，以毫秒为单位，指定等待数据的最长时间
 * @param read_len      成功读取的数据长度，函数执行后此参数会被更新为实际读取的数据长度
 * @return              QCLOUD_RET_SUCCESS表示成功，或其他错误代码表示失败
 */
int TCI_HAL_TLS_Read(uintptr_t handle, unsigned char *data, size_t totalLen, uint32_t timeout_ms, size_t *read_len);

/**
 * @brief 与服务器建立TCP连接
 *
 * 该函数用于初始化并建立一个到指定服务器的TCP连接。
 *
 * @param host 服务器地址，字符串格式
 * @param port 服务器端口，16位无符号整数
 * @return 成功时返回TCP套接字句柄（值大于0），失败时返回0
 */
uintptr_t TCI_HAL_TCP_Connect(const char *host, uint16_t port);

/**
 * @brief 断开与服务器的连接并释放资源
 *
 * 此函数用于断开与服务器的TCP连接，并释放相关资源。
 * 调用此函数后，指定的TCP套接字句柄将不再有效。
 *
 * @param fd TCP Socket句柄
 * @return  成功时返回0
 */
int TCI_HAL_TCP_Disconnect(uintptr_t fd);

/**
 * @brief 通过TCP连接写入数据
 *
 * @param fd            TCP套接字句柄
 * @param data          要写入的源数据
 * @param len           数据长度
 * @param timeout_ms    超时值，单位为毫秒
 * @param written_len   成功写入的数据长度
 * @return              QCLOUD_RET_SUCCESS表示成功，或错误代码表示失败
 */
int TCI_HAL_TCP_Write(uintptr_t fd, const unsigned char *data, uint32_t len, uint32_t timeout_ms, size_t *written_len);

/**
 * @brief 通过TCP连接读取数据
 *
 * @param fd            TCP套接字句柄
 * @param data          目标数据缓冲区，用于存放读取的数据
 * @param len           数据长度
 * @param timeout_ms    超时值，单位为毫秒
 * @param read_len      成功读取的数据长度
 * @return              QCLOUD_RET_SUCCESS表示成功，或其他错误代码表示失败
 */
int TCI_HAL_TCP_Read(uintptr_t fd, unsigned char *data, uint32_t len, uint32_t timeout_ms, size_t *read_len);

/**************************************************************************************
 * network udp
 **************************************************************************************/

/**
 * @brief 创建UDP服务器
 *
 * 创建一个UDP服务器并绑定到指定的IP地址和端口。
 *
 * @param[in] ip   要绑定的IP地址，可以是"0.0.0.0"表示所有接口
 * @param[in] port 要绑定的端口号
 *
 * @return 成功时返回socket文件描述符，失败时返回-1
 */
int TCI_HAL_UDP_Bind(const char *ip, uint16_t port);

/**
 * @brief UDP服务器接收数据
 *
 * 从UDP客户端接收数据，同时获取发送方的IP地址和端口信息。
 *
 * @param[in]  fd             socket文件描述符
 * @param[out] p_data         接收数据的缓冲区
 * @param[in]  datalen        缓冲区大小
 * @param[in]  timeout_ms     接收超时时间(毫秒)
 * @param[out] recv_ip_addr   发送方IP地址的存储缓冲区
 * @param[in]  recv_addr_len  IP地址缓冲区长度
 * @param[out] recv_port      发送方端口号
 *
 * @return 成功时返回接收到的字节数，失败或超时时返回负数
 */
int TCI_HAL_UDP_Recv(int fd, uint8_t *p_data, uint32_t datalen, uint32_t timeout_ms, char *recv_ip_addr,
                 uint32_t recv_addr_len, uint16_t *recv_port);

/**
 * @brief UDP数据发送
 *
 * 向指定的主机和端口发送UDP数据。
 *
 * @param[in] fd       socket文件描述符
 * @param[in] p_data   要发送的数据缓冲区
 * @param[in] datalen  数据长度
 * @param[in] host     目标主机的IP地址或域名
 * @param[in] port     目标端口号(字符串格式)
 *
 * @return 成功时返回发送的字节数，失败时返回负数
 */
int TCI_HAL_UDP_Send(int fd, const uint8_t *p_data, uint32_t datalen, const char *host, const char *port);

/**
 * @brief 关闭UDP连接
 *
 * 关闭指定UDP socket并释放相关资源。
 *
 * @param[in] fd socket文件描述符
 */
void TCI_HAL_UDP_Close(int fd);

/** @} */ // end of TCI_HAL_NETWORK

/**
 * @defgroup TCI_HAL_FILE 文件操作接口
 * @brief 提供跨平台的文件系统操作功能
 * @{
 */

/**
 * @brief 获取文件系统根分区大小，单位: KBytes
 *
 * @return uint32_t 根分区大小
 */
uint32_t TCI_HAL_FileGetDiskSize(void);

/**
 * @brief 打开由filename指向的文件，使用给定的模式。
 *
 * @param filename: 文件名及其长度，以及是相对路径还是绝对路径，
 *                  这是平台相关的
 * @param mode: 模式是平台相关的，以下信息来自Linux手册
 *            ┌─────────────┬───────────────────────────────┐
 *            │ fopen()模式 │ open()标志                    │
 *            ├─────────────┼───────────────────────────────┤
 *            │     r       │ O_RDONLY                      │
 *            ├─────────────┼───────────────────────────────┤
 *            │     w       │ O_WRONLY | O_CREAT | O_TRUNC  │
 *            ├─────────────┼───────────────────────────────┤
 *            │     a       │ O_WRONLY | O_CREAT | O_APPEND │
 *            ├─────────────┼───────────────────────────────┤
 *            │     r+      │ O_RDWR                        │
 *            ├─────────────┼───────────────────────────────┤
 *            │     w+      │ O_RDWR | O_CREAT | O_TRUNC    │
 *            ├─────────────┼───────────────────────────────┤
 *            │     a+      │ O_RDWR | O_CREAT | O_APPEND   │
 *            └─────────────┴───────────────────────────────┘
 * @return  一个有效的句柄(FILE *)或者在打开失败时返回NULL
 */
void *TCI_HAL_FileOpen(const char *filename, const char *mode);

/**
 * @brief 从给定的文件流中读取数据到由ptr指向的数组中。
 *
 * @param ptr 指向用于存储读取数据的数组的指针。
 * @param size 每个数据元素的大小，以字节为单位。
 * @param nmemb 要读取的数据元素的数量。
 * @param fp 文件流的指针。
 * @return size_t 实际读取的字节数。
 */
size_t TCI_HAL_FileRead(void *ptr, size_t size, size_t nmemb, void *fp);

/**
 * @brief 将指针ptr指向的数组中的数据写入给定的文件流。
 * @param ptr 指向要写入数据的数组的指针。
 * @param size 数组中每个元素的大小。
 * @param nmemb 数组中元素的数量。
 * @param fp 文件流的指针。
 * @return 返回写入的字节数，如果发生错误则返回0。
 */
size_t TCI_HAL_FileWrite(const void *ptr, size_t size, size_t nmemb, void *fp);

#define TCI_HAL_SEEK_SET 0 // 文件指针定位到文件开头
#define TCI_HAL_SEEK_CUR 1 // 文件指针定位到当前位置
#define TCI_HAL_SEEK_END 2 // 文件指针定位到文件末尾

/**
 * @brief 将流的文件位置设置为给定的偏移量。参数offset表示从给定的whence位置开始的字节数。
 */
int TCI_HAL_FileSeek(void *fp, long int offset, int whence); // 函数声明：设置文件指针位置

/**
 * @brief 关闭文件流。所有缓冲区将被刷新。
 * @param fp 文件指针，指向需要关闭的文件流。
 * @return 返回值通常表示操作是否成功，例如返回0表示成功，非0值表示失败。
 */
int TCI_HAL_FileClose(void *fp);

/**
 * @brief 删除指定的文件名，使其不再可访问。
 *
 * 此函数尝试删除传入的文件路径所指向的文件。如果文件成功删除，函数返回0；
 * 如果出现错误（例如文件不存在或没有足够的权限），则返回一个非零的错误代码。
 *
 * @param filename 要删除的文件的路径和名称。
 * @return int 成功时返回0，失败时返回错误代码。
 */
int TCI_HAL_FileRemove(const char *filename);

/**
 * @brief 将给定流的文件位置设置到文件的开头。
 *
 * 此函数用于将文件指针重置到文件的起始位置，以便重新读取或写入文件。
 *
 * @param fp 指向要重置的文件流的指针。
 * @return int 成功时返回0，失败时返回错误代码。
 */
int TCI_HAL_FileRewind(void *fp);

/**
 * @brief 该函数用于将指定的旧文件名更改为新文件名。
 *
 * @param old_filename 需要更改的旧文件名。
 * @param new_filename 更改后的新文件名。
 * @return int 如果文件重命名成功返回0，否则返回错误代码。
 */
int TCI_HAL_FileRename(const char *old_filename, const char *new_filename);

/**
 * @brief 测试给定流的文件结束指示器。
 *
 * 此函数用于检查文件流是否已经到达文件末尾。
 *
 * @param fp 指向文件流的指针。
 * @return 如果到达文件末尾返回非零值，否则返回零。
 */
int TCI_HAL_FileEof(void *fp);

/**
 * @brief 测试给定流的错误指示器。
 *
 * 此函数用于检查与文件指针 `fp` 关联的流的错误状态。
 * 如果流中有错误发生，函数将返回一个非零值；否则返回零。
 *
 * @param fp 指向要测试的文件的指针。
 * @return int 如果流中有错误，返回非零值；否则返回0。
 */
int TCI_HAL_FileError(void *fp);

/**
 * @brief 获取流的当前位置。
 *
 * 该函数用于获取文件指针当前指向的文件位置。在处理流数据时，了解当前的位置对于进行读取、写入或定位操作非常重要。
 *
 * @param fp 文件指针，指向需要查询位置的文件。
 * @return long 返回当前文件位置指示器，如果发生错误则返回-1。
 */
long TCI_HAL_FileTell(void *fp);

/**
 * @brief 获取文件流的大小。
 *
 * 此函数用于获取给定文件指针所指向的文件的大小。
 *
 * @param fp 文件指针，指向要获取大小的文件。
 * @return long 返回文件的大小，如果发生错误则返回-1。
 */
long TCI_HAL_FileSize(void *fp);

/**
 * @brief 从文件流中读取一行数据。
 *
 * 该函数尝试从给定的文件指针所指向的文件流中读取一行数据，并将其存储到提供的字符数组中。
 * 如果成功读取，返回指向该字符数组的指针；如果遇到文件结束或发生错误，返回NULL。
 *
 * @param str 用于存储读取行的字符数组。
 * @param n 字符数组的最大长度（包括终止的空字符）。
 * @param fp 指向要读取的文件流的指针。
 * @return char* 成功时返回指向str的指针，失败时返回NULL。
 */
char *TCI_HAL_FileGets(char *str, int n, void *fp);

/**
 * @brief 刷新流的输出缓冲区。
 *
 * 该函数用于刷新给定文件指针所指向的流的输出缓冲区。
 * 这意味着所有待写入的数据将被立即写入到文件或设备中。
 *
 * @param fp 要刷新的输出流的文件指针。
 * @return 返回0表示成功，返回非0值表示失败。
 */
int TCI_HAL_FileFlush(void *fp);

/**
 * @brief 设置设备三元组信息(持久化存储)
 * @param[in] device_info 设备三元组信息
 * @param[in] len 信息长度
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_SetDevInfo(uint8_t *device_info, int len);

/**
 * @brief 获取设备三元组信息
 * @param[in] device_info 设备三元组信息
 * @param[in] len 信息长度
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_GetDevInfo(uint8_t *device_info, int len);

/** @} */ // end of TCI_HAL_FILE

/**
 * @defgroup TCI_HAL_PLATFORM 平台相关接口
 * @brief WiFi、AT模块等平台相关功能接口
 * @{
 */

// TODO 以下API请按需实现！

/**************************************************************************************
 * wifi config soft ap
 **************************************************************************************/

/**
 * @brief 启动WiFi热点模式
 *
 * 启动设备的WiFi热点(SoftAP)模式，使其他设备可以连接到此设备。
 *
 * @param[in] ssid     热点的SSID(网络名称)
 * @param[in] password 热点的密码，可以为NULL表示开放网络
 * @param[in] ch       WiFi信道(1-13)
 *
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_SoftAP_Start(const char *ssid, const char *password, uint8_t ch);

/**
 * @brief 停止WiFi热点模式
 *
 * 停止设备的WiFi热点(SoftAP)模式并断开所有客户端连接。
 *
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_SoftAP_Stop(void);

/**************************************************************************************
 * wifi api
 ***************************************************************************************/

typedef enum {
    TCI_TCIOT_WIFI_MODE_NULL  = 0,  // invalid mode
    TCI_TCIOT_WIFI_MODE_STA   = 1,  // station
    TCI_TCIOT_WIFI_MODE_AP    = 2,  // ap
    TCI_TCIOT_WIFI_MODE_APSTA = 3,  // ap +sta
    TCI_TCIOT_WIFI_MODE_MAX,
} TCI_WifiMode;

/**
 * @brief 初始化WiFi系统
 *
 * 初始化WiFi协议栈和相关硬件资源。
 *
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_Wifi_Init(void);

/**
 * @brief 设置WiFi工作模式
 *
 * 设置WiFi的工作模式，支持站点模式、热点模式或同时开启两种模式。
 *
 * @param[in] mode WiFi工作模式，参见TCI_WifiMode枚举
 *
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_Wifi_ModeSet(TCI_WifiMode mode);

/**
 * @brief 设置WiFi站点信息
 *
 * 配置WiFi站点模式下要连接的路由器信息。
 *
 * @param[in] ssid       要连接的WiFi SSID
 * @param[in] ssid_len   SSID长度
 * @param[in] passwd     WiFi密码
 * @param[in] passwd_len 密码长度
 *
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_Wifi_StaInfoSet(const char *ssid, uint8_t ssid_len, const char *passwd, uint8_t passwd_len);

/**
 * @brief 连接WiFi路由器
 *
 * 使用之前设置的SSID和密码连接到WiFi路由器。
 *
 * @param[in] timeout_ms 最大等待时间(毫秒)
 *
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_Wifi_StaConnect(uint32_t timeout_ms);

/**
 * @brief 获取WiFi错误日志
 *
 * 获取WiFi模块的错误日志信息，用于故障诊断。
 *
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_Wifi_LogGet(void);

/**
 * @brief 获取设备IPv4地址
 *
 * 获取设备当前的IPv4地址(通常是由DHCP分配的)。
 *
 * @return IPv4地址(网络字节序)，0表示未获取到地址
 */
uint32_t TCI_HAL_Wifi_Ipv4Get(void);

/**
 * @brief 获取WiFi MAC地址
 *
 * 获取WiFi接口的MAC地址。
 *
 * @param[out] mac MAC地址存储缓冲区(6字节)
 *
 * @return 实际返回的MAC地址长度
 */
size_t TCI_HAL_Wifi_MacGet(uint8_t *mac);

/**************************************************************************************
 * AT module
 **************************************************************************************/

/**
 * @brief URC事件处理函数类型
 *
 * 用于处理AT模块上报的主动事件(Unsolicited Result Code)。
 *
 * @param[in] data     接收到的URC数据
 * @param[in] data_len 数据长度
 */
typedef void (*TCI_OnUrcHandler)(const char *data, size_t data_len);

/**
 * @brief 初始化AT模块
 *
 * 初始化AT模块的通信接口和相关资源。
 *
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_Module_Init(void);

/**
 * @brief 反初始化AT模块
 *
 * 关闭AT模块并释放相关资源。
 */
void TCI_HAL_Module_Deinit(void);

/**
 * @brief 发送AT命令并等待响应
 *
 * 向AT模块发送命令并等待指定的响应内容。
 *
 * @param[in] at_cmd    要发送的AT命令
 * @param[in] at_expect 期望的响应内容
 * @param[in] timeout_ms 等待超时时间(毫秒)
 *
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_Module_SendAtCmdWaitResp(const char *at_cmd, const char *at_expect, uint32_t timeout_ms);

/**
 * @brief 发送AT命令并接收数据响应
 *
 * 向AT模块发送命令并接收返回的数据。
 *
 * @param[in]  at_cmd     要发送的AT命令
 * @param[in]  at_expect  期望的响应内容
 * @param[out] recv_buf   接收数据缓冲区
 * @param[out] recv_len   接收到的数据长度
 * @param[in]  timeout_ms 等待超时时间(毫秒)
 *
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_Module_SendAtCmdWaitRespWithData(const char *at_cmd, const char *at_expect, void *recv_buf, uint32_t *recv_len,
                                         uint32_t timeout_ms);

/**
 * @brief 向AT模块发送原始数据
 *
 * 直接向AT模块发送原始数据，不加任何AT命令格式化。
 *
 * @param[in] data     要发送的数据
 * @param[in] data_len 数据长度
 *
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_Module_SendAtData(const void *data, int data_len);

/**
 * @brief 设置URC处理
 *
 * 为指定URC字符串注册URC处理函数。
 *
 * @param[in] urc         URC字符串标识
 * @param[in] urc_handler URC事件处理函数
 *
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_Module_SetUrc(const char *urc, TCI_OnUrcHandler urc_handler);

/**
 * @brief 连接网络
 *
 * 使用AT模块建立网络连接(如拨号、WiFi等)。
 *
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_Module_ConnectNetwork(void);

/**
 * @defgroup TCI_HAL_OTA OTA升级接口
 * @brief 蓝牙OTA(Over-The-Air)固件升级相关功能
 * @{
 */

/**
 * @brief 获取OTA下载地址
 * @param[in] usr_data 用户数据指针
 * @return OTA下载地址
 */
uint32_t TCI_HAL_OTA_get_download_addr(void *usr_data);

/**
 * @brief 从Flash读取数据
 * @param[in] usr_data 用户数据指针
 * @param[in] read_addr 读取地址
 * @param[out] read_data 读取数据缓冲区
 * @param[in] read_len 读取长度
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_OTA_read_flash(void *usr_data, uint32_t read_addr, uint8_t *read_data, uint32_t read_len);

/**
 * @brief 向Flash写入数据
 * @param[in] usr_data 用户数据指针
 * @param[in] write_addr 写入地址
 * @param[in] write_data 写入数据缓冲区
 * @param[in] write_len 写入长度
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_OTA_write_flash(void *usr_data, uint32_t write_addr, uint8_t *write_data, uint32_t write_len);

/**
 * @brief 创建OTA定时器
 * @param[in] usr_data 用户数据指针
 * @param[in] ota_timer_callback 定时器回调函数
 * @return 定时器句柄，失败时返回NULL
 */
void *TCI_HAL_OTA_create_ota_timer(void *usr_data, void(ota_timer_callback)(void *timer));

/**
 * @brief 启动OTA定时器
 * @param[in] usr_data 用户数据指针
 * @param[in] timer 定时器句柄
 * @param[in] timeout_ms 超时时间(毫秒)
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_OTA_start_ota_timer(void *usr_data, void *timer, uint32_t timeout_ms);

/**
 * @brief 停止OTA定时器
 * @param[in] usr_data 用户数据指针
 * @param[in] timer 定时器句柄
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_OTA_stop_ota_timer(void *usr_data, void *timer);

/**
 * @brief 删除OTA定时器
 * @param[in] usr_data 用户数据指针
 * @param[in] timer 定时器句柄
 * @return 0表示成功，非0表示失败
 */
int TCI_HAL_OTA_delete_ota_timer(void *usr_data, void *timer);

/** @} */ // end of TCI_HAL_OTA
/** @} */ // end of TCI_HAL_ADAPTER


#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_PLATFORM_ADAPTER_TCI_HAL_ADAPTER_H_
