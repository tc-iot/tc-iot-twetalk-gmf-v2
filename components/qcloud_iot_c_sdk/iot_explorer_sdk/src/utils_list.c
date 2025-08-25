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
 * @file utils_list.c
 * @brief utils list operation
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2021-05-25
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2021-05-28 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#include "utils_list.h"

/**
 * @brief Get container pointer.
 *
 */
#define list_container_of(ptr, type, member) ((type *)((char *)(ptr) - (size_t)(&((type *)0)->member)))

/**
 * @brief Define list node.
 *
 */
typedef struct ListNode {
    struct ListNode *prev;
    struct ListNode *next;
    uint8_t          val[0];
} ListNode;

/**
 * @brief  Double Linked List.
 *
 */
typedef struct {
    UtilsListFunc func;
    ListNode     *head;
    ListNode     *tail;
    void         *lock;
    int           len;
    int           max_len;
} List;

/**
 * @brief Lock list.
 *
 * @param[in] list pointer to list
 */
static inline void _list_lock(List *list, int try_flag)
{
    if (list->lock) {
        list->func.list_lock(list->lock, try_flag);
    }
}

/**
 * @brief Unlock list.
 *
 * @param[in] list pointer to list
 */
static inline void _list_unlock(List *list)
{
    if (list->lock) {
        list->func.list_unlock(list->lock);
    }
}

/**
 * @brief Delete the node in list and release the resource.
 *
 * @param[in] list pointer to list
 * @param[in] node pointer to node needed remove
 */
static void _list_remove(void *list, void *node)
{
    List     *self      = (List *)list;
    ListNode *list_node = (ListNode *)node;

    list_node->prev ? (list_node->prev->next = list_node->next) : (self->head = list_node->next);

    list_node->next ? (list_node->next->prev = list_node->prev) : (self->tail = list_node->prev);

    self->func.list_free(list_node);

    if (self->len) {
        --self->len;
    }
}

/**
 * @brief Process list using handle function.
 *
 * @param[in] list pointer to list
 * @param[in] direction direction to traverse
 * @param[in] handle process function @see OnNodeProcessHandle
 * @param[in,out] usr_data usr data to pass to OnNodeProcessHandle
 * @param[in] match usr function
 * @return 0 for matched
 */
static int _list_process(void *list, UtilsListDirection direction, OnNodeProcessHandle handle, void *usr_data,
                         int (*match)(void *val, void *usr_data))
{
    int       rc, matched_count = 0;
    ListNode *node = NULL;
    List     *self = (List *)list;
    if (!self) {
        return -1;
    }

    _list_lock(self, 0);

    if (!utils_list_len_get(list)) {
        _list_unlock(self);
        return -1;
    }

    ListNode *backup = direction == LIST_HEAD ? self->head : self->tail;
    // traverse list to process
    for (node = backup; node; node = backup) {
        backup = direction == LIST_HEAD ? node->next : node->prev;

        if (match) {
            if (!match(node->val, usr_data)) {
                continue;
            }
        }
        matched_count++;

        // process node and val
        if (!handle) {
            break;
        }

        rc = handle(list, node->val, usr_data);
        if (rc) {
            break;
        }
    }
    _list_unlock(list);
    return matched_count;
}

/**
 * @brief Delete the node in list and release the resource.
 *
 * @param[in] list pointer to list
 * @param[in] val pointer to val needed remove
 * @param[in] usr_data usr_data
 */
static UtilsListResult _list_remove_handler(void *list, void *val, void *usr_data)
{
    utils_list_remove(list, val);
    return LIST_TRAVERSE_BREAK;
}

/**
 * @brief Copy val.
 *
 * @param[in] val value to copy
 * @param[in] usr_data data to be copied
 * @param[in] size size to copy
 * @return 0 for success
 */
static int _list_val_construct_copy(void *val, void *usr_data, size_t size)
{
    memcpy(val, usr_data, size);
    return 0;
}

/**
 * @brief Create list with max len, return NULL if fail.
 *
 * @param[in] func function needed by list
 * @param[in] max_len max_len of list
 * @return pointer to list, NULL for failed
 */
void *utils_list_create(UtilsListFunc func, int max_len)
{
    List *self;

    if (max_len <= 0) {
        return NULL;
    }

    self = (List *)func.list_malloc(sizeof(List));
    if (!self) {
        return NULL;
    }

    memset(self, 0, sizeof(List));

    if (func.list_lock_init) {
        self->lock = func.list_lock_init();
        if (!self->lock) {
            func.list_free(self);
            return NULL;
        }
    }

    self->func    = func;
    self->max_len = max_len;
    return self;
}

/**
 * @brief Destroy list.
 *
 * @param[in] list pointer to list
 */
void utils_list_destroy(void *list)
{
    List *self = (List *)list;
    if (!self) {
        return;
    }

    utils_list_clear(self);
    if (self->lock) {
        self->func.list_lock_deinit(self->lock);
    }
    self->func.list_free(self);
}

/**
 * @brief Clear the list.
 *
 * @param[in] list pointer to list
 */
void utils_list_clear(void *list)
{
    List *self = (List *)list;

    if (!self) {
        return;
    }

    _list_lock(self, 0);

    ListNode *next;
    ListNode *curr = self->head;

    while (self->len) {
        next = curr->next;
        self->func.list_free(curr);
        curr = next;
        self->len--;
    }

    _list_unlock(self);
}

/**
 * @brief Get list len.
 *
 * @param[in] list pointer to list
 * @return len of list
 */
int utils_list_len_get(void *list)
{
    List *self = (List *)list;
    if (!self) {
        return 0;
    }
    return self->len;
}

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
                    int (*construct)(void *val, void *usr_data, size_t size))
{
    List *self = (List *)list;
    if (!self) {
        return -1;
    }

    _list_lock(self, 0);

    if (!val_size || self->len >= self->max_len) {
        _list_unlock(self);
        return -1;
    }

    ListNode *node;
    node = self->func.list_malloc(sizeof(ListNode) + val_size);
    if (!node) {
        _list_unlock(self);
        return -1;
    }

    node->prev = NULL;
    node->next = NULL;
    if (construct(node->val, usr_data, val_size)) {
        self->func.list_free(node);
        _list_unlock(self);
        return -1;
    }

    if (self->len) {
        node->prev       = self->tail;
        node->next       = NULL;
        self->tail->next = node;
        self->tail       = node;
    } else {
        self->head = self->tail = node;
        node->prev = node->next = NULL;
    }

    ++self->len;

    _list_unlock(self);
    return 0;
}

/**
 * @brief Copy value to list.
 *
 * @param[in] list pointer to list
 * @param[in] val value to copy
 * @param[in] val_size value size to malloc
 * @return 0 for success, -1 for fail
 */
int utils_list_push_copy(void *list, void *val, size_t val_size)
{
    return utils_list_push(list, val_size, val, _list_val_construct_copy);
}

/**
 * @brief Pop the value from list head, return NULL if list empty.
 *
 * @param[in] list pointer to list
 * @return value in the head node
 */
void *utils_list_pop(void *list)
{
    List     *self = (List *)list;
    ListNode *node = NULL;
    if (!self) {
        return NULL;
    }
    _list_lock(self, 0);

    if (!self->len) {
        _list_unlock(self);
        return NULL;
    }

    node = self->head;

    if (--self->len) {
        (self->head = node->next)->prev = NULL;
    } else {
        self->head = self->tail = NULL;
    }

    node->next = node->prev = NULL;

    _list_unlock(self);

    return node->val;
}

/**
 * @brief Free the val get from utils_list_pop.
 *
 * @param[in] list pointer to list
 * @param[in] val pointer to val needed free
 */
void utils_list_val_free(void *list, void *val)
{
    List *self = (List *)list;
    if (!self) {
        return;
    }
    self->func.list_free(list_container_of(val, ListNode, val));
}

/**
 * @brief Delete the node in list and release the resource.
 *
 * @param[in] list pointer to list
 * @param[in] val pointer to val needed remove
 */
void utils_list_remove(void *list, void *val)
{
    List *self = (List *)list;
    if (!self) {
        return;
    }
    _list_lock(self, 0);
    _list_remove(self, list_container_of(val, ListNode, val));
    _list_unlock(self);
}

/**
 * @brief Process list using handle function.
 *
 * @param[in] list pointer to list
 * @param[in] direction direction to traverse
 * @param[in] handle process function @see OnNodeProcessHandle
 * @param[in,out] usr_data usr data to pass to OnNodeProcessHandle
 */
void utils_list_process(void *list, UtilsListDirection direction, OnNodeProcessHandle handle, void *usr_data)
{
    _list_process(list, direction, handle, usr_data, NULL);
}

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
                             int (*match)(void *val, void *usr_data))
{
    return _list_process(list, direction, handle, usr_data, match);
}

/**
 * @brief Remove node when matched.
 *
 * @param[in] list pointer to list
 * @param[in] usr_data usr data to pass to match
 * @param[in] match usr function
 * @return  >= 0 for matched times, other for fail
 */
int utils_list_match_remove(void *list, void *usr_data, int (*match)(void *val, void *usr_data))
{
    return utils_list_process_match(list, LIST_HEAD, _list_remove_handler, usr_data, match);
}
