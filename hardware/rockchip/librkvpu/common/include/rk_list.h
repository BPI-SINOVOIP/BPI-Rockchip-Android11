/*
 * Copyright(C) 2010 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved
 * BY DOWNLOADING, INSTALLING, COPYING, SAVING OR OTHERWISE USING THIS
 * SOFTWARE, YOU ACKNOWLEDGE THAT YOU AGREE THE SOFTWARE RECEIVED FORM ROCKCHIP
 * IS PROVIDED TO YOU ON AN "AS IS" BASIS and ROCKCHIP DISCLAIMS ANY AND ALL
 * WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH FILE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED
 * WARRANTIES OF TITLE, NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY,
 * ACCURACY OR FITNESS FOR A PARTICULAR PURPOSE
 * Rockchip hereby grants to you a limited, non-exclusive, non-sublicensable and
 * non-transferable license
 *   (a) to install, save and use the Software;
 *   (b) to * copy and distribute the Software in binary code format only
 * Except as expressively authorized by Rockchip in writing, you may NOT:
 *   (a) distribute the Software in source code;
 *   (b) distribute on a standalone basis but you may distribute the Software in
 *   conjunction with platforms incorporating Rockchip integrated circuits;
 *   (c) modify the Software in whole or part;
 *   (d) decompile, reverse-engineer, dissemble, or attempt to derive any source
 *   code from the Software;
 *   (e) remove or obscure any copyright, patent, or trademark statement or
 *   notices contained in the Software
 */

#ifndef __RK_LIST_H__
#define __RK_LIST_H__

#include <pthread.h>

// desctructor of list node
typedef void *(*node_destructor)(void *);

struct rk_list_node;
class rk_list {
public:
    rk_list(node_destructor func);
    ~rk_list();

    // for FIFO or FILO implement
    // adding functions support simple structure like C struct or C++ class pointer,
    // do not support C++ object
    int32_t add_at_head(void *data, int32_t size);
    int32_t add_at_tail(void *data, int32_t size);
    // deleting function will copy the stored data to input pointer with size as size
    // if NULL is passed to deleting functions, the node will be delete directly
    int32_t del_at_head(void *data, int32_t size);
    int32_t del_at_tail(void *data, int32_t size);

    // for status check
    int32_t list_is_empty();
    int32_t list_size();

    // for vector implement - not implemented yet
    // adding function will return a key
    int32_t add_by_key(void *data, int32_t size, uint32_t *key);
    int32_t del_by_key(void *data, int32_t size, uint32_t key);
    int32_t show_by_key(void *data, uint32_t key);

    int32_t flush();

private:
    pthread_mutex_t         mutex;
    node_destructor         destroy;
    struct rk_list_node    *head;
    int32_t                 count;

    rk_list();
    rk_list(const rk_list &);
    rk_list &operator=(const rk_list &);
};

#endif /*__RK_LIST_H__*/
