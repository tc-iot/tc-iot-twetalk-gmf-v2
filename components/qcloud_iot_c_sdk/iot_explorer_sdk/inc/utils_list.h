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
 * @file utils_list.h
 * @brief header file for utils list
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2021-05-25
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2021-05-25 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_COMMON_UTILS_INC_UTILS_LIST_H_
#define IOT_HUB_DEVICE_C_SDK_COMMON_UTILS_INC_UTILS_LIST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>

#include "qcloud_iot_platform.h"

/**
 * @brief ListNode iterator direction.
 *
 */
typedef enum {
    LIST_HEAD,
    LIST_TAIL,
} UtilsListDirection;

/**
 * @brief ListNode process result of OnNodeProcessHandle.
 *
 */
typedef enum {
    LIST_TRAVERSE_CONTINUE,
    LIST_TRAVERSE_BREAK,
} UtilsListResult;

/**
 * @brief Utils list function.
 *
 */
typedef struct {
    void *(*list_malloc)(size_t len);
    void (*list_free)(void *val);

    void *(*list_lock_init)(void);
    int (*list_lock)(void *lock, int try_flag);
    int (*list_unlock)(void *lock);
    void (*list_lock_deinit)(void *lock);
} UtilsListFunc;

/**
 * @brief Default list func
 *
 */
#define DEFAULT_LIST_FUNCS     \
    {TCI_HAL_Malloc,               \
     TCI_HAL_Free,                 \
     TCI_HAL_RecursiveMutexCreate, \
     TCI_HAL_RecursiveMutexLock,   \
     TCI_HAL_RecursiveMutexUnLock, \
     TCI_HAL_RecursiveMutexDestroy}

/**
 * @brief Default list func
 *
 */
#define DEFAULT_UNLOCK_LIST_FUNCS {TCI_HAL_Malloc, TCI_HAL_Free, NULL, NULL, NULL, NULL}

/**
 * @brief Node process handle called by utils_list_process.
 *
 */
typedef UtilsListResult (*OnNodeProcessHandle)(void *list, void *val, void *usr_data);

/**
 * @brief Create list with max len, return NULL if fail.
 *
 * @param[in] func function needed by list
 * @param[in] max_len max_len of list
 * @return pointer to list, NULL for failed
 */
void *utils_list_create(UtilsListFunc func, int max_len);

/**
 * @brief Destroy list.
 *
 * @param[in] list pointer to list
 */
void utils_list_destroy(void *list);

/**
 * @brief Clear the list
 *
 * @param list
 */
void utils_list_clear(void *list);

/**
 * @brief Get list len.
 *
 * @param[in] list pointer to list
 * @return len of list
 */
int utils_list_len_get(void *list);

/**
 * @brief Push the node to list tail, return NULL if node invalid.
 *
 * @param[in] list pointer to list
 * @param[in] val_size value size to malloc
 * @param[in] usr_data usr data to construct
 * @param[in] construct construct value callback
 * @return 0 for success, -1 for fail
 */
int utils_list_push(void *list, size_t val_size, void *usr_data,
                    int (*construct)(void *val, void *usr_data, size_t size));

/**
 * @brief Copy value to list.
 *
 * @param[in] list pointer to list
 * @param[in] val value to copy
 * @param[in] val_size value size to malloc
 * @return 0 for success, -1 for fail
 */
int utils_list_push_copy(void *list, void *val, size_t val_size);

/**
 * @brief Pop the val from list head, return NULL if list empty.
 *
 * @param[in] list pointer to list
 * @return val in the head node
 */
void *utils_list_pop(void *list);

/**
 * @brief Free the val get from utils_list_pop.
 *
 * @param[in] list pointer to list
 * @param[in] val pointer to val needed free
 */
void utils_list_val_free(void *list, void *val);

/**
 * @brief Delete the node in list and release the resource.
 *
 * @param[in] list pointer to list
 * @param[in] val pointer to val needed remove
 */
void utils_list_remove(void *list, void *val);

/**
 * @brief Process list using handle function.
 *
 * @param[in] list pointer to list
 * @param[in] direction direction to traverse @see UtilsListDirection
 * @param[in] handle process function @see OnNodeProcessHandle
 * @param[in,out] usr_data usr data to pass to OnNodeProcessHandle
 */
void utils_list_process(void *list, UtilsListDirection direction, OnNodeProcessHandle handle, void *usr_data);

/**
 * @brief Process list using handle function.
 *
 * @param[in] list pointer to list
 * @param[in] direction direction to traverse
 * @param[in] handle process function @see OnNodeProcessHandle
 * @param[in,out] usr_data usr data to pass to OnNodeProcessHandle
 * @param[in] match usr function
 * @return  >= 0 for matched times, other for fail
 */
int utils_list_process_match(void *list, UtilsListDirection direction, OnNodeProcessHandle handle, void *usr_data,
                             int (*match)(void *val, void *usr_data));

/**
 * @brief Remove node when matched.
 *
 * @param[in] list pointer to list
 * @param[in] usr_data usr data to pass to match
 * @param[in] match usr function
 * @return  >= 0 for matched times, other for fail
 */
int utils_list_match_remove(void *list, void *usr_data, int (*match)(void *val, void *usr_data));

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_COMMON_UTILS_INC_UTILS_LIST_H_
