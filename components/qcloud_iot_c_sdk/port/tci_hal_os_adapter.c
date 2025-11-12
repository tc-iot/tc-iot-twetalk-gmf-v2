/**
 * @file tci_hal_os_adapter.c
 * @brief 腾讯云物联网硬件抽象层 - 操作系统适配器实现
 *
 * 本文件实现了操作系统相关的HAL接口，包括：
 * - 线程管理
 * - 互斥锁（普通和递归）
 * - 条件变量
 * - 信号量
 * - 邮箱队列
 *
 * @author hubertxxu (hubertxxu@tencent.com)
 * @version 1.0
 * @date 2025-10-15
 */

#include "tci_hal_adapter.h"
#include "tc_iot_hal.h"

// =============================================================================
// 线程管理接口实现
// =============================================================================

int TCI_HAL_ThreadCreate(TCI_ThreadParams *params)
{
    if (params == NULL) {
        return -1; // 参数错误
    }

    // 由于TCI_ThreadParams和ThreadParams结构体可能有差异，这里直接强制转换
    // 在实际使用中可能需要逐个字段转换以确保兼容性
    return HAL_ThreadCreate((ThreadParams *)params);
}

int TCI_HAL_ThreadDestroy(TCI_ThreadHandle_t *thread_t)
{
    if (thread_t == NULL) {
        return -1; // 参数错误
    }

    return HAL_ThreadDestroy((ThreadHandle_t *)thread_t);
}

unsigned long TCI_HAL_GetCurrentThreadHandle(void)
{
    return HAL_GetCurrentThreadHandle();
}

// =============================================================================
// 互斥锁和同步原语接口实现
// =============================================================================

void *TCI_HAL_MutexCreate(void)
{
    return HAL_MutexCreate();
}

void TCI_HAL_MutexDestroy(void *mutex)
{
    if (mutex != NULL) {
        HAL_MutexDestroy(mutex);
    }
}

int TCI_HAL_MutexLock(void *mutex)
{
    if (mutex == NULL) {
        return -1; // 参数错误
    }
    return HAL_MutexLock(mutex);
}

int TCI_HAL_MutexTryLock(void *mutex)
{
    if (mutex == NULL) {
        return -1; // 参数错误
    }
    return HAL_MutexTryLock(mutex);
}

int TCI_HAL_MutexUnlock(void *mutex)
{
    if (mutex == NULL) {
        return -1; // 参数错误
    }
    return HAL_MutexUnlock(mutex);
}

void *TCI_HAL_RecursiveMutexCreate(void)
{
    return HAL_RecursiveMutexCreate();
}

void TCI_HAL_RecursiveMutexDestroy(void *mutex)
{
    if (mutex != NULL) {
        HAL_RecursiveMutexDestroy(mutex);
    }
}

int TCI_HAL_RecursiveMutexLock(void *mutex, int try_flag)
{
    if (mutex == NULL) {
        return -1; // 参数错误
    }
    return HAL_RecursiveMutexLock(mutex, try_flag);
}

int TCI_HAL_RecursiveMutexUnLock(void *mutex)
{
    if (mutex == NULL) {
        return -1; // 参数错误
    }
    return HAL_RecursiveMutexUnLock(mutex);
}

void *TCI_HAL_CondCreate(void)
{
    return HAL_CondCreate();
}

void TCI_HAL_CondFree(void *cond_)
{
    if (cond_ != NULL) {
        HAL_CondFree(cond_);
    }
}

int TCI_HAL_CondSignal(void *cond_, int broadcast)
{
    if (cond_ == NULL) {
        return -1; // 参数错误
    }
    return HAL_CondSignal(cond_, broadcast);
}

int TCI_HAL_CondWait(void *cond, void *lock, unsigned long timeout_ms)
{
    if (cond == NULL || lock == NULL) {
        return -1; // 参数错误
    }
    return HAL_CondWait(cond, lock, timeout_ms);
}

void *TCI_HAL_SemaphoreCreate(void)
{
    return HAL_SemaphoreCreate();
}

void TCI_HAL_SemaphoreDestroy(void *sem)
{
    if (sem != NULL) {
        HAL_SemaphoreDestroy(sem);
    }
}

void TCI_HAL_SemaphorePost(void *sem)
{
    if (sem != NULL) {
        HAL_SemaphorePost(sem);
    }
}

int TCI_HAL_SemaphoreWait(void *sem, uint32_t timeout_ms)
{
    if (sem == NULL) {
        return -1; // 参数错误
    }
    return HAL_SemaphoreWait(sem, timeout_ms);
}

void *TCI_HAL_MailQueueInit(void *pool, size_t mail_size, int mail_count)
{
    return HAL_MailQueueInit(pool, mail_size, mail_count);
}

void TCI_HAL_MailQueueDeinit(void *mail_q)
{
    if (mail_q != NULL) {
        HAL_MailQueueDeinit(mail_q);
    }
}

int TCI_HAL_MailQueueSend(void *mail_q, const void *buf, size_t size, uint32_t timeout_ms)
{
    if (mail_q == NULL || buf == NULL) {
        return -1; // 参数错误
    }
    return HAL_MailQueueSend(mail_q, buf, size, timeout_ms);
}

int TCI_HAL_MailQueueRecv(void *mail_q, void *buf, size_t *size, uint32_t timeout_ms)
{
    if (mail_q == NULL || buf == NULL || size == NULL) {
        return -1; // 参数错误
    }
    return HAL_MailQueueRecv(mail_q, buf, size, timeout_ms);
}
