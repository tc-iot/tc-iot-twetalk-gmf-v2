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
 * @file utils_min_heap.c
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

#include "utils_min_heap.h"

typedef struct {
    void** elem_pointer;
    size_t num;
    size_t max;
    int (*elem_greater)(void* elem_a, void* elem_b);
    void (*elem_set_index)(void* elem, int index);
    int (*elem_get_index)(void* elem);
    void* (*malloc)(size_t len);
    void (*free)(void* val);
} UtilsMinHeap;

static inline void _min_heap_set_index(UtilsMinHeap* s, size_t index, void* elem)
{
    s->elem_pointer[index] = elem;
    s->elem_set_index(elem, index);
}

static void _min_heap_shift_up_unconditional(UtilsMinHeap* s, size_t hole_index, void* elem)
{
    size_t parent = (hole_index - 1) >> 1;
    do {
        _min_heap_set_index(s, hole_index, s->elem_pointer[parent]);
        hole_index = parent;
        parent     = (hole_index - 1) >> 1;
    } while (hole_index && s->elem_greater(s->elem_pointer[parent], elem));
    _min_heap_set_index(s, hole_index, elem);
}

static void _min_heap_shift_up(UtilsMinHeap* s, size_t hole_index, void* elem)
{
    size_t parent = (hole_index - 1) >> 1;
    while (hole_index && s->elem_greater(s->elem_pointer[parent], elem)) {
        _min_heap_set_index(s, hole_index, s->elem_pointer[parent]);
        hole_index = parent;
        parent     = (hole_index - 1) >> 1;
    }
    _min_heap_set_index(s, hole_index, elem);
}

static void _min_heap_shift_down_(UtilsMinHeap* s, size_t hole_index, void* elem)
{
    size_t min_child = (hole_index + 1) << 1;
    while (min_child <= s->num) {
        min_child -=
            (min_child == s->num) || s->elem_greater(s->elem_pointer[min_child], s->elem_pointer[min_child - 1]);
        if (!(s->elem_greater(elem, s->elem_pointer[min_child]))) {
            break;
        }
        _min_heap_set_index(s, hole_index, s->elem_pointer[min_child]);
        hole_index = min_child;
        min_child  = (hole_index + 1) << 1;
    }
    _min_heap_set_index(s, hole_index, elem);
}

// -------------------------------------------------------------------------------------------------
// api
// -------------------------------------------------------------------------------------------------

void* utils_min_heap_create(UtilsMinHeapInitParams* params)
{
    UtilsMinHeap* s = params->malloc(sizeof(UtilsMinHeap));
    if (!s) {
        return NULL;
    }
    s->elem_pointer = params->malloc(sizeof(void*) * params->num);
    if (!s->elem_pointer) {
        params->free(s);
        return NULL;
    }
    memset(s->elem_pointer, 0, sizeof(void*) * params->num);
    s->num            = 0;
    s->max            = params->num;
    s->elem_greater   = params->elem_greater;
    s->elem_set_index = params->elem_set_index;
    s->elem_get_index = params->elem_get_index;
    s->malloc         = params->malloc;
    s->free           = params->free;
    return s;
}

void utils_min_heap_destroy(void* min_heap)
{
    if (!min_heap) {
        return;
    }
    UtilsMinHeap* s = min_heap;
    s->free(s->elem_pointer);
    s->free(s);
}

void utils_min_heap_clear(void* min_heap)
{
    if (!min_heap) {
        return;
    }
    UtilsMinHeap* s = min_heap;
    s->num          = 0;
}

int utils_min_heap_empty(void* min_heap)
{
    if (!min_heap) {
        return -1;
    }
    UtilsMinHeap* s = min_heap;
    return 0u == s->num;
}

size_t utils_min_heap_size(void* min_heap)
{
    if (!min_heap) {
        return 0;
    }
    UtilsMinHeap* s = min_heap;
    return s->num;
}

void* utils_min_heap_top(void* min_heap)
{
    if (!min_heap) {
        return NULL;
    }
    UtilsMinHeap* s = min_heap;
    return s->num ? *s->elem_pointer : NULL;
}

int utils_min_heap_push(void* min_heap, void* elem)
{
    if (!min_heap) {
        return -1;
    }
    UtilsMinHeap* s = min_heap;
    if (s->num == s->max) {
        return -1;
    }
    _min_heap_shift_up(s, s->num++, elem);
    return 0;
}

void* utils_min_heap_pop(void* min_heap)
{
    if (!min_heap) {
        return NULL;
    }
    UtilsMinHeap* s = min_heap;
    if (s->num) {
        void* elem = *s->elem_pointer;
        _min_heap_shift_down_(s, 0u, s->elem_pointer[--s->num]);
        s->elem_set_index(elem, -1);
        return elem;
    }
    return NULL;
}

int utils_min_heap_adjust(void* min_heap, void* elem)
{
    if (!min_heap) {
        return -1;
    }
    UtilsMinHeap* s = min_heap;

    int index = s->elem_get_index(elem);
    if (-1 == index) {
        return utils_min_heap_push(min_heap, elem);
    }

    size_t parent = (index - 1) >> 1;
    /* The position of e has changed; we shift it up or down
     * as needed.  We can't need to do both. */
    if (index > 0 && s->elem_greater(s->elem_pointer[parent], elem)) {
        _min_heap_shift_up_unconditional(s, index, elem);
        return 0;
    }

    _min_heap_shift_down_(s, index, elem);
    return 0;
}

int utils_min_heap_erase(void* min_heap, void* elem)
{
    if (!min_heap) {
        return -1;
    }
    UtilsMinHeap* s = min_heap;

    int index = s->elem_get_index(elem);
    if (index == -1) {
        return -1;
    }

    void*  last   = s->elem_pointer[--s->num];
    size_t parent = (index - 1) >> 1;
    /* we replace e with the last element in the heap.  We might need to
       shift it upward if it is less than its parent, or downward if it is
       greater than one or both its children. Since the children are known
       to be less than the parent, it can't need to shift both up and
       down. */
    if (index > 0 && s->elem_greater(s->elem_pointer[parent], last)) {
        _min_heap_shift_up_unconditional(s, index, last);
    } else {
        _min_heap_shift_down_(s, index, last);
    }
    s->elem_set_index(elem, -1);
    return 0;
}
