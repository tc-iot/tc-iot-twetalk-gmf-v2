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
 * @file utils_log.c
 * @brief different level log generator
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2021-05-28
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2021-05-28 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#include "utils_log.h"

#define HEX_DUMP_BYTE_PER_LINE 16

#define LOG_COLOR_ENABLED

#ifdef LOG_COLOR_ENABLED
static const char *LEVEL_STR[] = {
    "\033[34mDIS",  // 蓝色
    "\033[31mERR",  // 红色
    "\033[33mWRN",  // 黄色
    "\033[32mINF",  // 绿色
    "\033[0mDBG"    // 默认颜色
};
#else
static const char *LEVEL_STR[] = {"DIS", "ERR", "WRN", "INF", "DBG"};
#endif

/**
 * @brief Log handle.
 *
 */
typedef struct {
    LogHandleFunc log_handle_func;
    char         *log_buffer;
    int           log_max_size;
    void         *log_mutex;
    LogLevel      log_print_level;
} UtilsLogHandle;

/**
 * @brief Singleton log handle.
 *
 */
static UtilsLogHandle sg_log_handle;

/**
 * @brief Get file name form path.
 *
 * @param[in] path file path
 * @return file name
 */
static const char *_get_filename(const char *path)
{
#ifdef WIN32
    char ch = '\\';
#else
    char ch = '/';
#endif
    const char *q = strrchr(path, ch);
    if (!q) {
        q = path;
    } else {
        q++;
    }
    return q;
}

/**
 * @brief Init log with func, log level, max log size.
 *
 * @param[in] func function should be implement for utils log
 * @param[in] log_level @see LogLevel
 * @param[in] max_log_size max size of log to print
 * @return 0 for success
 */
int utils_log_init(LogHandleFunc func, LogLevel log_level, int max_log_size)
{
    sg_log_handle.log_handle_func = func;
    sg_log_handle.log_print_level = log_level;
    sg_log_handle.log_max_size    = max_log_size;
    if (func.log_mutex_create) {
        sg_log_handle.log_mutex = func.log_mutex_create();
        if (!sg_log_handle.log_mutex) {
            return -1;
        }
    }

    if (func.log_malloc) {
        sg_log_handle.log_buffer = func.log_malloc(max_log_size);
        if (!sg_log_handle.log_buffer) {
            if (sg_log_handle.log_mutex) {
                func.log_mutex_destroy(sg_log_handle.log_mutex);
            }
        }
    }
    return !sg_log_handle.log_buffer;
}

/**
 * @brief Deinit log.
 *
 */
void utils_log_deinit(void)
{
    sg_log_handle.log_handle_func.log_free(sg_log_handle.log_buffer);
    if (sg_log_handle.log_mutex) {
        sg_log_handle.log_handle_func.log_mutex_destroy(sg_log_handle.log_mutex);
    }
    sg_log_handle.log_mutex  = NULL;
    sg_log_handle.log_buffer = NULL;
}

/**
 * @brief Set log level.
 *
 * @param log_level @see LogLevel
 */
void utils_log_set_level(LogLevel log_level)
{
    sg_log_handle.log_print_level = log_level;
}

/**
 * @brief Get log level.
 *
 * @return @see LogLevel
 */
LogLevel utils_log_get_level(void)
{
    return sg_log_handle.log_print_level;
}

/**
 * @brief Generate log if level higher than set.
 *
 * @param[in] file file path
 * @param[in] func function where generate log
 * @param[in] line line of source file where generate log
 * @param[in] level @see LogLevel
 * @param[in] fmt format of log content
 */
void utils_log_gen(const char *file, const char *func, const int line, const LogLevel level, const char *fmt, ...)
{
    // check if log deinit
    if (!sg_log_handle.log_buffer) {
        return;
    }

    if (level > sg_log_handle.log_print_level) {
        return;
    }

    if (sg_log_handle.log_mutex) {
        sg_log_handle.log_handle_func.log_mutex_lock(sg_log_handle.log_mutex, 0);
    }

    /* format log content */
    const char *file_name = _get_filename(file);
    char       *o         = sg_log_handle.log_buffer;

    char time_str[30];

    memset(sg_log_handle.log_buffer, 0, sg_log_handle.log_max_size);

    o += snprintf(sg_log_handle.log_buffer, sg_log_handle.log_max_size, "%s|%s|%s|%s(%d): ", LEVEL_STR[level],
                  sg_log_handle.log_handle_func.log_get_current_time_str(time_str, sizeof(time_str)), file_name, func,
                  line);

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(o, sg_log_handle.log_max_size - 2 - strlen(sg_log_handle.log_buffer), fmt, ap);
    va_end(ap);

    strncat(sg_log_handle.log_buffer, "\r\n", sg_log_handle.log_max_size - strlen(sg_log_handle.log_buffer) - 1);

#ifdef LOG_COLOR_ENABLED
    strcat((char *)sg_log_handle.log_buffer, "\033[0m");
#endif

    if (sg_log_handle.log_handle_func.log_handle) {
        sg_log_handle.log_handle_func.log_handle(sg_log_handle.log_buffer);
    }
    sg_log_handle.log_handle_func.log_printf("%s", sg_log_handle.log_buffer);

    /* append to upload buffer */
    if (sg_log_handle.log_handle_func.log_upload) {
        sg_log_handle.log_handle_func.log_upload(level, sg_log_handle.log_buffer);
    }

    if (sg_log_handle.log_mutex) {
        sg_log_handle.log_handle_func.log_mutex_unlock(sg_log_handle.log_mutex);
    }

    return;
}

static void _hex_dump(const void *pdata, int len)
{
    int         i, j, k, l;
    const char *data = (const char *)pdata;
    char        buf[256], str[64], t[] = "0123456789ABCDEF";
    if (len <= 0) {
        return;
    }
    for (i = j = k = 0; i < len; i++) {
        if (0 == i % HEX_DUMP_BYTE_PER_LINE) {
            j += sg_log_handle.log_handle_func.log_snprintf(buf + j, sizeof(buf) - j, "%04xh: ", i);
        }
        buf[j++] = t[0x0f & (data[i] >> 4)];
        buf[j++] = t[0x0f & data[i]];
        buf[j++] = ' ';
        str[k++] = (data[i] >= ' ' && data[i] <= '~') ? data[i] : '.';
        if (0 == (i + 1) % HEX_DUMP_BYTE_PER_LINE) {
            str[k] = 0;
            j += sg_log_handle.log_handle_func.log_snprintf(buf + j, sizeof(buf) - j, "| %s\n", str);
            sg_log_handle.log_handle_func.log_printf("%s", buf);
            j = k = buf[0] = str[0] = 0;
        }
    }
    str[k] = 0;
    if (k) {
        for (l = 0; l < 3 * (HEX_DUMP_BYTE_PER_LINE - k); l++) {
            buf[j++] = ' ';
        }
        j += sg_log_handle.log_handle_func.log_snprintf(buf + j, sizeof(buf) - j, "| %s\n", str);
    }
    if (buf[0]) {
        sg_log_handle.log_handle_func.log_printf("%s", buf);
    }

    sg_log_handle.log_handle_func.log_printf("\r\n");
}

/**
 * @brief Print hex dump of array.
 *
 * @param[in] file file path
 * @param[in] func function where generate log
 * @param[in] line line of source file where generate log
 * @param[in] name name of array
 * @param[in] array pointer to array
 * @param[in] array_len array length
 */
void utils_log_hex_dump(const char *file, const char *func, const int line, const char *name, void *array,
                        size_t array_len)
{
    // check if log deinit
    if (!sg_log_handle.log_buffer) {
        return;
    }

    char time_str[30];

    if (sg_log_handle.log_mutex) {
        sg_log_handle.log_handle_func.log_mutex_lock(sg_log_handle.log_mutex, 0);
    }

    const char *file_name = _get_filename(file);
    sg_log_handle.log_handle_func.log_printf(
        "%s|%s|%s(%d) %s length : %ld, data : \r\n",
        sg_log_handle.log_handle_func.log_get_current_time_str(time_str, sizeof(time_str)), file_name, func, line, name,
        array_len);
    if (array && array_len) {
        sg_log_handle.log_handle_func.log_printf("offset: 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15\r\n");
        sg_log_handle.log_handle_func.log_printf("======================================================\r\n");

        _hex_dump(array, array_len);
    }

    if (sg_log_handle.log_mutex) {
        sg_log_handle.log_handle_func.log_mutex_unlock(sg_log_handle.log_mutex);
    }
}
