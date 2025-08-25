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

#ifdef __cplusplus
extern "C" {
#endif

#include "tc_iot_hal.h"

#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/semphr.h"
#include "FreeRTOS/task.h"
#include "FreeRTOS/queue.h"

// #define PLATFORM_HAS_TIME_FUNCS

#ifdef PLATFORM_HAS_TIME_FUNCS
#include <sys/time.h>
#include <time.h>
#endif

uint64_t HAL_GetTicksTimeMs(void)
{
    // #define portTICK_PERIOD_MS ( ( TickType_t ) 1000 / configTICK_RATE_HZ )
    return (xTaskGetTickCount() * portTICK_PERIOD_MS);
}

////////////////////////////////////////////////////////////////
// utc timestamp and local time and timer functions
////////////////////////////////////////////////////////////////

uint64_t HAL_GetTimeMs(void)
{
#if defined PLATFORM_HAS_TIME_FUNCS
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#else
    return HAL_GetTicksTimeMs();
#endif
}

long HAL_GetTimeSecond(void)
{
    return HAL_GetTimeMs() / 1000;
}

char *HAL_GetLocalTime(char *time_str, size_t time_str_len)
{
    if (time_str == NULL || time_str_len < TIME_FORMAT_STR_LEN)
        return " ";

#if defined PLATFORM_HAS_TIME_FUNCS
    struct timeval tv = {0, 0};
    time_t nowtime;
    struct tm nowtm;  // NOLINT
    char tmbuf[TIME_FORMAT_STR_LEN] = {0};

    gettimeofday(&tv, NULL);
#ifdef JIELI_RTOS
    nowtime = time(NULL) + 28800;
#else
    nowtime = tv.tv_sec;
#endif
    localtime_r(&nowtime, &nowtm);
    strftime(tmbuf, sizeof(tmbuf), "%Y-%m-%d %H:%M:%S", &nowtm);

    memset(time_str, 0, time_str_len);
    HAL_Snprintf(time_str, time_str_len, "%s.%03u (%ld)", tmbuf, tv.tv_usec / 1000, HAL_GetTicksTimeMs());

#else
    long time_ms = HAL_GetTimeMs();
	 memset(time_str, 0, time_str_len);
    HAL_Snprintf(time_str, time_str_len, "%ld.%03ld", time_ms / 1000, time_ms % 1000);
#endif
    return time_str;
}

int HAL_SetTimeSecond(size_t timestamp_sec)
{
    // struct timeval stime;
    // stime.tv_sec  = timestamp_sec;
    // stime.tv_usec = 0;

    // if (0 != settimeofday(&stime, NULL)) {
    //     return -1;
    // }
    return 0;
}

int HAL_SetTimeMs(size_t timestamp_ms)
{
    // struct timeval stime;
    // stime.tv_sec  = (timestamp_ms / 1000);
    // stime.tv_usec = ((timestamp_ms % 1000) * 1000);

    // if (0 != settimeofday(&stime, NULL)) {
    //     return -1;
    // }
    return 0;
}

/****************************************************************
 * 注意：对于timer操作，需要使用单调递增的时间戳，避免时间戳出现跳变
 *       如在Linux平台，应使用CLOCK_MONOTONIC而非gettimeofday
 ****************************************************************/

bool HAL_Timer_expired(Timer *timer)
{
    uint64_t now_ts;

    now_ts = HAL_GetTimeMs();

    return (now_ts > timer->end_time) ? true : false;
}

void HAL_Timer_countdown_ms(Timer *timer, unsigned int timeout_ms)
{
    timer->end_time = HAL_GetTimeMs();
    timer->end_time += timeout_ms;
}

void HAL_Timer_countdown(Timer *timer, unsigned int timeout)
{
    timer->end_time = HAL_GetTimeMs();
    timer->end_time += timeout * 1000;
}

int HAL_Timer_remain(Timer *timer)
{
    uint64_t time_now = HAL_GetTimeMs();
    if (time_now >= timer->end_time) {
        return 0;
    }
    return (int)(timer->end_time - time_now);
}

void HAL_Timer_init(Timer *timer)
{
    timer->end_time = 0;
}

#ifdef __cplusplus
}
#endif
