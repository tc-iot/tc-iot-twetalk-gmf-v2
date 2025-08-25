/*****************************************************************************
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef __TC_IOT_LIBS_INC_H__
#define __TC_IOT_LIBS_INC_H__

// standard C library headers
#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef JIELI_RTOS
#include <stdio.h>
#include "errno.h"
#endif

// C99 only
#include <inttypes.h>
#include <stdint.h>

// bool define
#ifdef JIELI_RTOS
// JIELI AC79XX/AC57XX FreeRTOS platform specific
#include "generic\typedef.h"
#include "fs/fs.h"
#else
#include <stdbool.h>
#endif

/**********************************************************
 * Common define
 * ********************************************************/
/* Max size of a file full path */
#define FILE_PATH_MAX_LEN 256

#ifndef Max
#define Max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef Min
#define Min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef container_of
#ifdef offsetof
#define _my_offsetof(type, field) offsetof(type, field)
#else
#define _my_offsetof(type, field) ((off_t)(&((type*)0)->field))
#endif
#define container_of(ptr, type, field) ((type*)(((char*)(ptr)) - _my_offsetof(type, field)))
#endif

#define DO_LOG_EVERY_N(n, what)                \
    {                                          \
        static unsigned int _log_counter_ = 0; \
        if ((_log_counter_++) % (n) == 0)      \
            what                               \
    }

#ifndef STR_SAFE_PRINT
#define STR_SAFE_PRINT(ptr) ((ptr) ? (ptr) : "null")
#endif

#define COMMON_HTTP_SERVER_PORT     80
#define COMMON_HTTP_SERVER_PORT_TLS 443

/**********************************************************
 * Platform define default use linux
 * ********************************************************/
// #define PLATFORM_FREERTOS
// #define PLATFORM_RTTHREAD
// #define PLAT_USE_THREADX
// #define PLAT_USE_LITEOS

#endif /* __TC_IOT_LIBS_INC_H__ */
