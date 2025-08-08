/**
 * @copyright
 *
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2018 - 2023 THL A29 Limited, a Tencent company.All rights reserved.
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
 * @file utils_min_heap.h
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2023-02-13
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2023-02-13 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_COMMON_UTILS_INC_UTILS_MIN_HEAP_H_
#define IOT_HUB_DEVICE_C_SDK_COMMON_UTILS_INC_UTILS_MIN_HEAP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    size_t num;
    int (*elem_greater)(void* elem_a, void* elem_b);
    void (*elem_set_index)(void* elem, int index);
    int (*elem_get_index)(void* elem);
    void* (*malloc)(size_t len);
    void (*free)(void* val);
} UtilsMinHeapInitParams;

void*  utils_min_heap_create(UtilsMinHeapInitParams* params);
void   utils_min_heap_destroy(void* min_heap);
void   utils_min_heap_clear(void* min_heap);
int    utils_min_heap_empty(void* min_heap);
size_t utils_min_heap_size(void* min_heap);
void*  utils_min_heap_top(void* min_heap);
int    utils_min_heap_push(void* min_heap, void* elem);
void*  utils_min_heap_pop(void* min_heap);
int    utils_min_heap_adjust(void* min_heap, void* elem);
int    utils_min_heap_erase(void* min_heap, void* elem);

#ifdef __cplusplus
}

#endif

#endif  // IOT_HUB_DEVICE_C_SDK_COMMON_UTILS_INC_UTILS_MIN_HEAP_H_
