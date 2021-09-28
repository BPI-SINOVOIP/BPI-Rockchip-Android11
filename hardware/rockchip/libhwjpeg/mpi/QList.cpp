/*
 * Copyright 2021 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * author: kevin.chen@rock-chips.com
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "QList"
#include <utils/Log.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "QList.h"

int QList::keys = 0;

typedef struct list_node {
    list_node*   prev;
    list_node*   next;
    int          key;
    int          size;
} list_node;

static inline void list_node_init(list_node *node)
{
    node->prev = node->next = node;
}

static inline void list_node_init_with_key_and_size(list_node *node,
                                                    int key,
                                                    int size)
{
    list_node_init(node);
    node->key   = key;
    node->size  = size;
}

static list_node* create_list(void *data, int size, int key)
{
    list_node *node = (list_node*)malloc(sizeof(list_node) + size);
    if (node) {
        void *dst = (void*)(node + 1);
        list_node_init_with_key_and_size(node, key, size);
        memcpy(dst, data, size);
    } else {
        ALOGE("failed to allocate list node");
    }
    return node;
}

static inline void _q_list_add(list_node * _new, list_node * prev, list_node * next)
{
    next->prev = _new;
    _new->next = next;
    _new->prev = prev;
    prev->next = _new;
}

static inline void q_list_add(list_node *_new, list_node *head)
{
    _q_list_add(_new, head, head->next);
}

static inline void q_list_add_tail(list_node *_new, list_node *head)
{
    _q_list_add(_new, head->prev, head);
}

static void release_list(list_node *node, void *data, int size)
{
    void *src = (void*)(node + 1);
    if (node->size == size) {
        if (data)
            memcpy(data, src, size);
    } else {
        ALOGE("node size check failed when release_list");
        size = (size < node->size) ? (size) : (node->size);
        if (data)
            memcpy(data, src, size);
    }
    free(node);
}

static inline void _q_list_del(list_node *prev, list_node *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void q_list_del_init(list_node *node)
{
    _q_list_del(node->prev, node->next);
    list_node_init(node);
}

static inline void _list_del_node_no_lock(list_node *node, void *data, int size)
{
    q_list_del_init(node);
    release_list(node, data, size);
}

QList::QList(node_destructor func)
    : destroy(NULL),
      head(NULL),
      count(0)
{
    destroy = func;
    head = (list_node*)malloc(sizeof(list_node));
    if (NULL == head) {
        ALOGE("failed to allocate list header");
    } else {
        list_node_init_with_key_and_size(head, 0, 0);
    }
}

QList::~QList()
{
    flush();
    if (head) free(head);
    head = NULL;
    destroy = NULL;
}

int QList::add_at_head(void *data, int size)
{
    int ret = -EINVAL;
    if (head) {
        list_node *node = create_list(data, size, 0);
        if (node) {
            q_list_add(node, head);
            count++;
            ret = 0;
        } else {
            ret = -ENOMEM;
        }
    }
    return ret;
}

int QList::add_at_tail(void *data, int size)
{
    int ret = -EINVAL;
    if (head) {
        list_node *node = create_list(data, size, 0);
        if (node) {
            q_list_add_tail(node, head);
            count++;
            ret = 0;
        } else {
            ret = -ENOMEM;
        }
    }
    return ret;
}

int QList::del_at_head(void *data, int size)
{
    int ret = -EINVAL;
    if (head && count) {
        _list_del_node_no_lock(head->next, data, size);
        count--;
        ret = 0;
    }
    return ret;
}

int QList::del_at_tail(void *data, int size)
{
    int ret = -EINVAL;
    if (head && count) {
        _list_del_node_no_lock(head->prev, data, size);
        count--;
        ret = 0;
    }
    return ret;
}

int QList::list_is_empty()
{
    int ret = (count == 0);
    return ret;
}

int QList::list_size()
{
    int ret = count;
    return ret;
}

int QList::add_by_key(void *data, int size, int *key)
{
    int ret = 0;
    if (head) {
        int list_key = get_key();
        *key = list_key;
        list_node *node = create_list(data, size, list_key);
        if (node) {
            q_list_add_tail(node, head);
            count++;
            ret = 0;
        } else {
            ret = -ENOMEM;
        }
    }
    return ret;
}

int QList::del_by_key(void *data, int size, int key)
{
    int ret = 0;
    if (head && count) {
        struct list_node *tmp = head->next;
        ret = -EINVAL;
        while (tmp->next != head) {
            if (tmp->key == key) {
                _list_del_node_no_lock(tmp, data, size);
                count--;
                break;
            }
        }
    }
    return ret;
}


int QList::show_by_key(void *data, int key)
{
    int ret = -EINVAL;
    (void)data;
    (void)key;
    return ret;
}

int QList::flush()
{
    if (head) {
        while (count) {
            list_node* node = head->next;
            q_list_del_init(node);
            if (destroy) {
                destroy((void*)(node + 1));
            }
            free(node);
            count--;
        }
    }
    mCondition.signal();
    return 0;
}

void QList::lock()
{
    mMutex.lock();
}

void QList::unlock()
{
    mMutex.unlock();
}

int QList::trylock()
{
    return mMutex.trylock();
}

Mutex *QList::mutex()
{
    return &mMutex;
}

int QList::get_key()
{
    return keys++;
}

void QList::wait()
{
    mCondition.wait(mMutex);
}

int QList::wait(int timeout)
{
    return mCondition.timedwait(mMutex, timeout);
}

void QList::signal()
{
    mCondition.signal();
}
