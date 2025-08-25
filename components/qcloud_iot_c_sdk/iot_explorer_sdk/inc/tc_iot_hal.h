/**
 * @file tc_iot_hal.h
 * @author hubertxxu (hubertxxu@tencent.com)
 * @brief 
 * @version 0.1
 * @date 2024-09-02
 * 
 * @copyright
 * Tencent is pleased to support the open source community by making IoT Hub available. 
 * Copyright(C) 2021 - 2026 THL A29 Limited, a Tencent company.All rights reserved.
 * Licensed under the MIT License(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 */

#ifndef __TC_IOT_HAL_H__
#define __TC_IOT_HAL_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include "tc_iot_libs_inc.h"

// ---------------------------------------------------------
// OS相关接口
// ---------------------------------------------------------


// 线程句柄类型定义
typedef unsigned long ThreadHandle_t;

// 线程运行函数
typedef void (*ThreadRunFunc)(void *arg);

// 线程优先级枚举类型定义
typedef enum {
    THREAD_PRIORITY_LOW     = -1,
    THREAD_PRIORITY_NORMAL  = 0,
    THREAD_PRIORITY_HIGH    = 1,
    THREAD_PRIORITY_HIGHER  = 2,
    THREAD_PRIORITY_HIGHEST = 3
} ThreadPriorityLevel;

// 创建线程参数结构体定义
typedef struct ThreadParams {
    char          *thread_name; // 线程名，必要参数，用户需要设置
    ThreadHandle_t thread_id;   // 线程句柄，必要参数
    ThreadRunFunc  thread_func; // 线程运行函数，必要参数，用户需要设置
    void          *user_arg;   // 用户自定义参数，可选参数
    uint32_t       stack_size; // 线程栈大小，单位字节。RTOS上为必要参数，用户需要设置
    void          *stack_ptr;  // 线程栈入口指针，某些平台如threadX需要该参数
    uint32_t       slice_tick; // 时间片大小，某些平台如RT-Thread需要指定该参数
    uint16_t       priority;  // 线程优先级，建议用ThreadPriorityLevel来描述级别
} ThreadParams;

/**
 * @brief 线程创建函数，对于RTOS设备来讲尽可能创建到外部psram中
 *
 * @param params  ThreadParams 线程创建的参数
 *                注意：该变量需要保证生命周期在线程启动后仍然有效，建议用static变量
 *
 * @return 0表示成功，非0表示失败
 */
int HAL_ThreadCreate(ThreadParams *params);

/**
 * @brief 线程销毁函数，SDK创建的线程执行完毕后，会自动调用线程销毁函数，用户不需要重复销毁
 *
 * @param thread_t thread handle 线程句柄
 *
 * @return 0表示成功，非0表示失败
 */
int HAL_ThreadDestroy(ThreadHandle_t *thread_t);

/**
 * @brief 获取当前线程的句柄
 *
 * @return 线程句柄
 */
unsigned long HAL_GetCurrentThreadHandle(void);

/**
 * @brief 创建互斥锁
 * 
 * 此函数用于创建一个新的互斥锁。互斥锁是一种同步机制，用于防止多个线程同时访问共享资源，
 * 从而避免竞态条件和数据损坏。
 *
 * @return 成功时返回一个有效的互斥锁句柄，失败时返回NULL。
 */
void *HAL_MutexCreate(void);

/**
 * @brief 销毁互斥锁
 *
 * @param mutex 锁句柄
 */
void HAL_MutexDestroy(void *mutex);

/**
 * @brief 获取锁，如果无法获取锁，则会阻塞当前线程直到锁可用。
 *
 * 此函数尝试获取一个互斥锁。如果锁已经被其他线程持有，
 * 调用此函数的线程将会进入阻塞状态，直到锁被释放。
 *
 * @param mutex 指向要获取的互斥锁的指针。
 * @return 成功获取锁返回0，失败返回错误码。
 */
int HAL_MutexLock(void *mutex);

/**
 * @brief 尝试获取锁，如果无法立即获取则立即返回
 *
 * 此函数尝试获取一个互斥锁。如果锁当前可用，则获取锁并返回成功状态码（0）。
 * 如果锁不可用，函数不会阻塞等待，而是立即返回一个错误代码。
 *
 * @param mutex     互斥锁句柄
 * @return 0 表示成功，或错误代码表示失败
 */
int HAL_MutexTryLock(void *mutex);

/**
 * @brief 解锁
 *
 * 此函数用于解锁一个之前已经被锁定的互斥量（mutex）。
 * 互斥量是一种同步机制，用于防止多个线程同时访问共享资源。
 * 
 * @param mutex     mutex handle
 *                  指向要解锁的互斥量的指针。
 */
int HAL_MutexUnlock(void *mutex);

/**
 * @brief 创建递归互斥锁
 *
 * 此函数用于创建一个递归互斥锁。递归互斥锁允许同一线程多次锁定同一个互斥锁，
 * 而不会导致死锁。当成功创建时，返回一个有效的递归互斥锁句柄；如果创建失败，则返回NULL。
 *
 * @return 成功时返回一个有效的互斥锁句柄，失败时返回NULL。
 */
void *HAL_RecursiveMutexCreate(void);

/**
 * @brief 销毁递归互斥锁
 *
 * @param mutex 指向要销毁的互斥量的指针。
 */
void HAL_RecursiveMutexDestroy(void *mutex);

/**
 * @brief 获取锁
 *
 * @param mutex     指向要上锁的互斥量的指针。
 * @param try_flag  0：阻塞获取锁，1：尝试获取锁
 */
int HAL_RecursiveMutexLock(void *mutex, int try_flag);

/**
 * @brief 释放锁
 *
 * @param mutex  指向要解锁的互斥量的指针。
 */
int HAL_RecursiveMutexUnLock(void *mutex);

/**
 * @brief 创建条件变量
 *
 * @return 成功时返回一个有效的条件变量句柄，失败时返回NULL。
 */
void *HAL_CondCreate(void);

/**
 * @brief 销毁条件变量
 *
 * @param cond 指向要释放的条件变量句柄
 */
void HAL_CondFree(void *cond_);

/**
 * @brief 通知或广播条件变量
 * 
 * 该函数用于通知等待在条件变量上的线程，或者如果指定了广播，则通知所有等待的线程。
 * 
 * @param cond_ 指向条件变量的指针。条件变量用于同步线程间的操作。
 * @param broadcast 如果设置为非零值，则进行广播通知；如果为零，则只通知一个等待的线程。
 * @return 返回值表示操作的成功与否，通常成功返回0，失败返回负数。
 */
int HAL_CondSignal(void *cond_, int broadcast);

/**
 * @brief 等待条件变量触发
 *
 * @param cond    条件变量的句柄
 * @param lock    锁变量的句柄。在调用条件变量之前，线程通常需要先获得锁，以确保对共享资源的访问是互斥的。HAL_CondWait
 * 函数在等待期间会释放这个锁，并在被唤醒后重新获得它。
 * @param timeout_ms 等待的超时时间，以毫秒为单位。如果设置为0，则表示无限期等待，直到条件变量被触发。
 */
int HAL_CondWait(void *cond, void *lock, unsigned long timeout_ms);

/**
 * @brief platform-dependent semaphore create function.
 *
 * @return pointer to semaphore
 */
void *HAL_SemaphoreCreate(void);
/**
 * @brief platform-dependent semaphore destory function.
 *
 * @param[in] sem pointer to semaphore
 */
void HAL_SemaphoreDestroy(void *sem);

/**
 * @brief platform-dependent semaphore post function.
 *
 * @param[in] sem pointer to semaphore
 */
void HAL_SemaphorePost(void *sem);

/**
 * @brief platform-dependent semaphore wait function.
 *
 * @param[in] sem pointer to semaphore
 * @param[in] timeout_ms wait timeout
 * @return @see IotReturnCode
 */
int HAL_SemaphoreWait(void *sem, uint32_t timeout_ms);

/**
 * @brief 平台相关的邮件队列初始化函数。
 *
 * @param[in] mail_count 邮件数量
 * @param[in] mail_size 邮件大小
 * @return 指向邮件队列的指针
 */
void *HAL_MailQueueInit(void *pool, size_t mail_size, int mail_count);

/**
 * @brief 平台相关的邮件队列反初始化函数。
 *
 * @param[in] mail_q 指向邮件队列的指针
 */
void HAL_MailQueueDeinit(void *mail_q);

/**
 * @brief 平台相关的邮件队列发送函数。
 *
 * @param[in] mail_q 指向邮件队列的指针
 * @param[in] buf 数据缓冲区
 * @param[in] size 数据大小
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return 成功返回 0
 */
int HAL_MailQueueSend(void *mail_q, const void *buf, size_t size, uint32_t timeout_ms);

/**
 * @brief 平台相关的邮件队列接收函数。
 *
 * @param[in] mail_q 指向邮件队列的指针
 * @param[out] buf 数据缓冲区
 * @param[in] size 数据大小
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return 成功返回 0
 */
int HAL_MailQueueRecv(void *mail_q, void *buf, size_t *size, uint32_t timeout_ms);

/**
 * @brief 申请内存，对于资源受限的设备请适配申请PSRAM内存
 *
 * @param size   需要申请的大小，单位：字节 申请完成后需memset为0
 * @return       申请到的内存指针，NULL表示申请失败
 */
void *HAL_Malloc(uint32_t size);

/**
 * @brief 调整已经申请的内存大小
 *
 * @param ptr   先前分配的内存指针
 * @param size  期望的大小，单位：字节
 * @return      调整后的内存指针，NULL表示申请失败
 */
void *HAL_Realloc(void *ptr, uint32_t size);

/**
 * @brief 释放内存
 *
 * @param ptr    内存指针
 */
void HAL_Free(void *ptr);

/**
 * @brief 打印日志到控制台
 *
 * @param fmt   格式化字符串指针
 * @param ...   可变参数
 */
void HAL_Printf(const char *fmt, ...);

/**
 * @brief 将数据按照指定格式打印到字符串中
 *
 * @param str   目标字符串，数据将被打印到这里
 * @param len   输出的最大尺寸，防止缓冲区溢出
 * @param fmt   打印格式，类似于printf的格式化字符串
 * @param ...   可变数量的参数，将根据fmt中的格式化指令进行打印
 * @return      成功打印的字节数
 */
int HAL_Snprintf(char *str, const int len, const char *fmt, ...);

/**
 * @brief 将数据按照格式打印到字符串中
 *
 * @param str   目标字符串
 * @param len   输出的最大大小
 * @param fmt   打印格式
 * @param ap    参数列表
 * @return      成功打印的字节数
 */
int HAL_Vsnprintf(char *str, const int len, const char *fmt, va_list ap);

/**
 * @brief 获取设备在网络中的MAC地址
 * 
 * @param mac 指向用于存储MAC地址的数组的指针
 * @param len MAC地址的长度
 */
void HAL_GetMAC(uint8_t *mac ,uint8_t len);

/**
 * @brief 获取内存大小，如果使用外部RAM，则这里指外部RAM的大小
 * 
 * @return uint32_t 内存大小，单位为Bytes
 */
uint32_t HAL_GetMemSize(void);

/**
 * @brief 获取平台类型，如Linux、Windows、ESP32等
 * 
 * @return char* 平台类型字符串
 */
char * HAL_GetPlatform(void);

/**
 * @brief 随机数生成（最好适配硬件随机数发生器）
 * 
 * @return long 
 */
long HAL_Random(void);

/**
 * @brief 设置信号处理函数
 * 
 * 该函数用于设置指定信号的处理函数。当操作系统发送指定信号时，会调用提供的处理函数。
 * 如果RTOS设备不支持信号处理，则直接不适配就好。
 * 
 * @param sginum 需要处理的信号
 * @param handler 信号处理函数
 */
void HAL_Signal(int sginum, void (*handler)(int));

// ----------------------------------------------------------------------
// 时间相关接口
// ----------------------------------------------------------------------

/**
 * @brief 休眠一段时间
 *
 * @param ms 休眠间隔时间，单位为毫秒
 */
void HAL_SleepMs(uint32_t ms);

//////////////////////////////////////////////////////////////////////////
// utc/local timestamp and timer functions
//////////////////////////////////////////////////////////////////////////
#define TIME_FORMAT_STR_LEN (24)

/**
 * @brief 获取本地时间，格式为：%y-%m-%d %H:%M:%S.%ms（例如：2023-01-11 18:01:12.973）
 * 
 * 此函数用于获取当前本地时间，并将其格式化为指定的字符串格式。
 * 用户需要提供一个足够大的缓冲区来存储格式化后的时间字符串。
 * 
 * @param time_str 用于存储格式化时间字符串的缓冲区，其大小应至少为 TIME_FORMAT_STR_LEN
 * @param time_str_len 缓冲区 time_str 的长度
 * @return 返回格式化后的时间字符串
 */
char *HAL_GetLocalTime(char *time_str, size_t time_str_len);

/**
 * @brief 获取当前时间戳（秒）
 *
 * 该函数用于获取当前的时间戳，以秒为单位。时间戳是从某个固定时间点（通常是1970年1月1日）到现在的总秒数。
 *
 * @return   当前时间戳（秒）
 */
long HAL_GetTimeSecond(void);

/**
 * @brief 获取当前时间戳（毫秒）
 *
 * 该函数用于获取当前系统时间的时间戳，单位为毫秒。
 *
 * @return 返回当前时间的时间戳，单位为毫秒
 */
uint64_t HAL_GetTimeMs(void);

/**
 * @brief 获取当前系统的滴答值，一般情况下是从上电开始一直累加，不可突变
 *
 * @return 返回当前系统的滴答值
 */
uint64_t HAL_GetTicksTimeMs(void);

/**
 * @brief 设置系统时间戳（毫秒）
 *
 * 此函数用于设置系统的当前时间戳，以毫秒为单位。
 * 时间戳是一个无符号整数，表示自某个固定时间点（通常是1970年1月1日）以来的毫秒数。
 * 如果不希望改变系统时间，可以直接return 0.
 *
 * @param timestamp_ms 时间戳值，单位为毫秒
 * @return 返回0表示设置成功
 */
int HAL_SetTimeMs(size_t timestamp_ms);

/**
 * @brief 设置系统时间戳（以秒为单位）
 *
 * 该函数用于设置系统的当前时间戳，时间戳以秒为单位。
 * 如果不希望设置系统时间，可以直接return 0.
 *
 * @param timestamp_sec 时间戳，单位为秒
 * @return 返回0表示成功设置时间戳
 */
int HAL_SetTimeSecond(size_t timestamp_sec);

/**
 * 定义定时器结构体，平台相关
 */
struct Timer {
#if defined(__linux__) && defined(__GLIBC__)
    // 在Linux和glibc环境下，使用timeval结构体来表示结束时间
    struct timeval end_time;
#else
    uint64_t end_time;
#endif
};

typedef struct Timer Timer;

/**
 * @brief 检查定时器是否到期
 *
 * @param timer     定时器的引用
 * @return          true = 已到期, false = 尚未到期
 */
bool HAL_Timer_expired(Timer *timer);

/**
 * @brief 设置计时器的倒计时/过期值
 * 
 * 此函数用于设置计时器的倒计时或过期时间，单位为毫秒。
 * 当计时器达到设定的时间后，可以触发某个事件或执行相应的操作。
 * 
 * @param timer         计时器对象的引用
 * @param timeout_ms    倒计时/过期值（单位：毫秒）
 */
void HAL_Timer_countdown_ms(Timer *timer, unsigned int timeout_ms);

/**
 * @brief 设置计时器的倒计时/过期值
 *
 * 此函数用于设置计时器的倒计时或过期时间。当计时器达到设定的时间后，可以触发某个事件或执行特定的操作。
 *
 * @param timer         计时器的引用，指向需要设置倒计时的计时器对象
 * @param timeout       倒计时/过期值，单位为秒。计时器将在该时间后触发事件或操作
 */
void HAL_Timer_countdown(Timer *timer, unsigned int timeout);

/**
 * @brief 检查定时器的剩余时间
 *
 * @param timer     定时器的引用
 * @return          如果定时器已到期返回0，否则返回剩余的毫秒数
 */
int HAL_Timer_remain(Timer *timer);

/**
 * @brief 初始化定时器
 *
 * @param timer reference to timer
 */
void HAL_Timer_init(Timer *timer);

// ----------------------------------------------------------------------
// TLS和TCP连接相关的接口
// ----------------------------------------------------------------------

#ifndef MAX_SIZE_OF_CLIENT_ID
#define MAX_SIZE_OF_CLIENT_ID (80)
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
    char         psk_id[MAX_SIZE_OF_CLIENT_ID + 1];  // PSK ID，最大长度为客户端ID长度加1
    size_t       psk_length;                     // PSK的长度
    unsigned int timeout_ms;                     // SSL握手超时时间，单位为毫秒
} SSLConnectParams;

typedef SSLConnectParams TLSConnectParams;

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
uintptr_t HAL_TLS_Connect(TLSConnectParams *pConnectParams, const char *host, int port);

/**
 * @brief 断开与TLS服务器的连接并释放资源
 *
 * @param handle TLS连接句柄，用于标识需要断开的TLS连接
 */
void HAL_TLS_Disconnect(uintptr_t handle);

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
int HAL_TLS_Write(uintptr_t handle, unsigned char *data, size_t totalLen, uint32_t timeout_ms, size_t *written_len);

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
int HAL_TLS_Read(uintptr_t handle, unsigned char *data, size_t totalLen, uint32_t timeout_ms, size_t *read_len);

/**
 * @brief 与服务器建立TCP连接
 *
 * 该函数用于初始化并建立一个到指定服务器的TCP连接。
 * 
 * @param host 服务器地址，字符串格式
 * @param port 服务器端口，16位无符号整数
 * @return 成功时返回TCP套接字句柄（值大于0），失败时返回0
 */
uintptr_t HAL_TCP_Connect(const char *host, uint16_t port);

/**
 * @brief 断开与服务器的连接并释放资源
 *
 * 此函数用于断开与服务器的TCP连接，并释放相关资源。
 * 调用此函数后，指定的TCP套接字句柄将不再有效。
 *
 * @param fd TCP Socket句柄
 * @return  成功时返回0
 */
int HAL_TCP_Disconnect(uintptr_t fd);

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
int HAL_TCP_Write(uintptr_t fd, const unsigned char *data, uint32_t len, uint32_t timeout_ms, size_t *written_len);

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
int HAL_TCP_Read(uintptr_t fd, unsigned char *data, uint32_t len, uint32_t timeout_ms, size_t *read_len);

/**************************************************************************************
 * network udp
 **************************************************************************************/

/**
 * @brief creat udp server
 *
 * @param[in] host host to bind
 * @param[in] port port to bind
 * @return socket fd
 */
int HAL_UDP_Bind(const char *ip, uint16_t port);

/**
 * @brief udp server read
 *
 * @param[in] fd socket fd
 * @param[out] p_data buffer to save read data
 * @param datalen[in] buffer len
 * @param timeout_ms[in] read timeout ms
 * @param recv_ip_addr[out] data recv from this ip
 * @param recv_addr_len[in] addr buffer len
 * @param recv_port[out] data recv from this port
 * @return @see IotReturnCode
 */
int HAL_UDP_Recv(int fd, uint8_t *p_data, uint32_t datalen, uint32_t timeout_ms, char *recv_ip_addr,
                 uint32_t recv_addr_len, uint16_t *recv_port);

/**
 * @brief udp write to
 *
 * @param[in] fd socket fd
 * @param[out] p_data buffer to write
 * @param datalen[in] buffer len
 * @param[in] host host to connect
 * @param[in] port port to connect
 * @return @see IotReturnCode
 */
int HAL_UDP_Send(int fd, const uint8_t *p_data, uint32_t datalen, const char *host, const char *port);

/**
 * @brief udp close
 *
 * @param[in] fd socket fd
 * @return 0 for success
 */
void HAL_UDP_Close(int fd);

// ----------------------------------------------------------------------
// 文件操作相关接口
// ----------------------------------------------------------------------

/**
 * @brief 获取文件系统根分区大小，单位: KBytes
 * 
 * @return uint32_t 根分区大小
 */
uint32_t HAL_FileGetDiskSize(void);

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
void *HAL_FileOpen(const char *filename, const char *mode);

/**
 * @brief 从给定的文件流中读取数据到由ptr指向的数组中。
 * 
 * @param ptr 指向用于存储读取数据的数组的指针。
 * @param size 每个数据元素的大小，以字节为单位。
 * @param nmemb 要读取的数据元素的数量。
 * @param fp 文件流的指针。
 * @return size_t 实际读取的字节数。
 */
size_t HAL_FileRead(void *ptr, size_t size, size_t nmemb, void *fp);

/**
 * @brief 将指针ptr指向的数组中的数据写入给定的文件流。
 * @param ptr 指向要写入数据的数组的指针。
 * @param size 数组中每个元素的大小。
 * @param nmemb 数组中元素的数量。
 * @param fp 文件流的指针。
 * @return 返回写入的字节数，如果发生错误则返回0。
 */
size_t HAL_FileWrite(const void *ptr, size_t size, size_t nmemb, void *fp);

#define HAL_SEEK_SET 0 // 文件指针定位到文件开头
#define HAL_SEEK_CUR 1 // 文件指针定位到当前位置
#define HAL_SEEK_END 2 // 文件指针定位到文件末尾

/**
 * @brief 将流的文件位置设置为给定的偏移量。参数offset表示从给定的whence位置开始的字节数。
 */
int HAL_FileSeek(void *fp, long int offset, int whence); // 函数声明：设置文件指针位置

/**
 * @brief 关闭文件流。所有缓冲区将被刷新。
 * @param fp 文件指针，指向需要关闭的文件流。
 * @return 返回值通常表示操作是否成功，例如返回0表示成功，非0值表示失败。
 */
int HAL_FileClose(void *fp);

/**
 * @brief 删除指定的文件名，使其不再可访问。
 * 
 * 此函数尝试删除传入的文件路径所指向的文件。如果文件成功删除，函数返回0；
 * 如果出现错误（例如文件不存在或没有足够的权限），则返回一个非零的错误代码。
 * 
 * @param filename 要删除的文件的路径和名称。
 * @return int 成功时返回0，失败时返回错误代码。
 */
int HAL_FileRemove(const char *filename);

/**
 * @brief 将给定流的文件位置设置到文件的开头。
 * 
 * 此函数用于将文件指针重置到文件的起始位置，以便重新读取或写入文件。
 * 
 * @param fp 指向要重置的文件流的指针。
 * @return int 成功时返回0，失败时返回错误代码。
 */
int HAL_FileRewind(void *fp);

/**
 * @brief 该函数用于将指定的旧文件名更改为新文件名。
 * 
 * @param old_filename 需要更改的旧文件名。
 * @param new_filename 更改后的新文件名。
 * @return int 如果文件重命名成功返回0，否则返回错误代码。
 */
int HAL_FileRename(const char *old_filename, const char *new_filename);

/**
 * @brief 测试给定流的文件结束指示器。
 * 
 * 此函数用于检查文件流是否已经到达文件末尾。
 * 
 * @param fp 指向文件流的指针。
 * @return 如果到达文件末尾返回非零值，否则返回零。
 */
int HAL_FileEof(void *fp);

/**
 * @brief 测试给定流的错误指示器。
 * 
 * 此函数用于检查与文件指针 `fp` 关联的流的错误状态。
 * 如果流中有错误发生，函数将返回一个非零值；否则返回零。
 *
 * @param fp 指向要测试的文件的指针。
 * @return int 如果流中有错误，返回非零值；否则返回0。
 */
int HAL_FileError(void *fp);

/**
 * @brief 获取流的当前位置。
 * 
 * 该函数用于获取文件指针当前指向的文件位置。在处理流数据时，了解当前的位置对于进行读取、写入或定位操作非常重要。
 *
 * @param fp 文件指针，指向需要查询位置的文件。
 * @return long 返回当前文件位置指示器，如果发生错误则返回-1。
 */
long HAL_FileTell(void *fp);

/**
 * @brief 获取文件流的大小。
 * 
 * 此函数用于获取给定文件指针所指向的文件的大小。
 * 
 * @param fp 文件指针，指向要获取大小的文件。
 * @return long 返回文件的大小，如果发生错误则返回-1。
 */
long HAL_FileSize(void *fp);

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
char *HAL_FileGets(char *str, int n, void *fp);

/**
 * @brief 刷新流的输出缓冲区。
 * 
 * 该函数用于刷新给定文件指针所指向的流的输出缓冲区。
 * 这意味着所有待写入的数据将被立即写入到文件或设备中。
 *
 * @param fp 要刷新的输出流的文件指针。
 * @return 返回0表示成功，返回非0值表示失败。
 */
int HAL_FileFlush(void *fp);

// TODO 以下API请按需实现！

/**************************************************************************************
 * wifi config soft ap
 **************************************************************************************/

/**
 * @brief start soft ap mode
 *
 * @param[in] ssid soft ap ssid
 * @param[in] password soft ap password
 * @param[in] ch soft ap channel
 * @return 0 for success
 */
int HAL_SoftAP_Start(const char *ssid, const char *password, uint8_t ch);

/**
 * @brief stop soft ap mode
 *
 * @return 0 for success
 */
int HAL_SoftAP_Stop(void);

/**************************************************************************************
 * wifi api
 ***************************************************************************************/

typedef enum {
    TC_IOT_WIFI_MODE_NULL  = 0,  // invalid mode
    TC_IOT_WIFI_MODE_STA   = 1,  // station
    TC_IOT_WIFI_MODE_AP    = 2,  // ap
    TC_IOT_WIFI_MODE_APSTA = 3,  // ap +sta
    TC_IOT_WIFI_MODE_MAX,
} TCIoTWifiMode;

/**
 * @brief init wifi stack
 *
 * @return 0 for success
 */
int HAL_Wifi_Init(void);

/**
 * @brief set wifi mode
 *
 * @param[in] mode @see TCIoTWifiMode
 * @return 0 for success
 */
int HAL_Wifi_ModeSet(TCIoTWifiMode mode);

/**
 * @brief set wifi sta info
 *
 * @param[in] ssid station ssid buffer
 * @param[in] ssid_len station ssid buffer len
 * @param[in] passwd station passwd buffer
 * @param[in] passwd_len station passwd buffer len
 * @return 0 for success
 */
int HAL_Wifi_StaInfoSet(const char *ssid, uint8_t ssid_len, const char *passwd, uint8_t passwd_len);

/**
 * @brief connect wifi router
 *
 * @param[in] timeout_ms max wait time. unit : ms
 * @return 0 for success
 */
int HAL_Wifi_StaConnect(uint32_t timeout_ms);
/**
 * @brief get error log
 *
 * @return 0 for success
 */
int HAL_Wifi_LogGet(void);

/**
 * @brief get device local ipv4 addr
 *
 * @return ipv4 addr
 */
uint32_t HAL_Wifi_Ipv4Get(void);

/**
 * @brief get wifi mac.
 *
 * @param[out] mac mac
 * @return mac length
 */
size_t HAL_Wifi_MacGet(uint8_t *mac);

/**************************************************************************************
 * AT module
 **************************************************************************************/

/**
 * @brief Urc handler.
 *
 */
typedef void (*OnUrcHandler)(const char *data, size_t data_len);

/**
 * @brief Init at module.
 *
 * @return 0 for success
 */
int HAL_Module_Init(void);

/**
 * @brief Deinit at module.
 *
 */
void HAL_Module_Deinit(void);

/**
 * @brief Send at cmd to at module and wait for resp.
 *
 * @param[in] at_cmd at cmd
 * @param[in] at_expect expect resp
 * @param[in] timeout_ms wait timeout
 * @return 0 for success
 */
int HAL_Module_SendAtCmdWaitResp(const char *at_cmd, const char *at_expect, uint32_t timeout_ms);

/**
 * @brief Send at cmd and waif for data.
 *
 * @param[in] at_cmd at cmd
 * @param[in] at_expect expect resp
 * @param[out] recv_buf recv data buffer
 * @param[out] recv_len recv data length
 * @param[in] timeout_ms wait timeout
 * @return 0 for success
 */
int HAL_Module_SendAtCmdWaitRespWithData(const char *at_cmd, const char *at_expect, void *recv_buf, uint32_t *recv_len,
                                         uint32_t timeout_ms);

/**
 * @brief Send date to at module.
 *
 * @param[in] data data to send
 * @param[in] data_len data length
 * @return 0 for success
 */
int HAL_Module_SendAtData(const void *data, int data_len);

/**
 * @brief Set urc.
 *
 * @param[in] urc irc string
 * @param[in] urc_handler urc handler
 * @return 0 for success
 */
int HAL_Module_SetUrc(const char *urc, OnUrcHandler urc_handler);

/**
 * @brief connect network
 *
 * @return int 0 for success
 */
int HAL_Module_ConnectNetwork(void);

// -----------------------------------------------------------------------------------------------------
// BLE OTA FUNC
// -----------------------------------------------------------------------------------------------------
uint32_t HAL_OTA_get_download_addr(void *usr_data);
int      HAL_OTA_read_flash(void *usr_data, uint32_t read_addr, uint8_t *read_data, uint32_t read_len);
int      HAL_OTA_write_flash(void *usr_data, uint32_t write_addr, uint8_t *write_data, uint32_t write_len);
void    *HAL_OTA_create_ota_timer(void *usr_data, void(ota_timer_callback)(void *timer));
int      HAL_OTA_start_ota_timer(void *usr_data, void *timer, uint32_t timeout_ms);
int      HAL_OTA_stop_ota_timer(void *usr_data, void *timer);
int      HAL_OTA_delete_ota_timer(void *usr_data, void *timer);


#if defined(__cplusplus)
}
#endif

#endif /* __TC_IOT_HAL_H__ */
