/*
 * Copyright (C) 2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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
 */
#ifndef _IA_TRACE_H
#define _IA_TRACE_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <unistd.h>
#include "LogHelper.h"

NAMESPACE_DECLARATION {

#define IA_TRACE_MESSAGE_LENGTH       256
#define IA_TRACE_TAG_ALWAYS           (1 << 0)
#define IA_TRACE_TAG_NOT_READY        (1LL << 63)
#define IA_TRACE_TAG                  IA_TRACE_TAG_ALWAYS

#define IA_TRACE_DECLSPEC static inline

void ia_trace_setup(void);
extern volatile int32_t ia_trace_is_ready;
extern uint64_t ia_trace_enabled_tags;
extern int ia_trace_marker_fd;

IA_TRACE_DECLSPEC __attribute__((always_inline)) int32_t
ia_trace_atomic_acquire_load(volatile const int32_t *ptr)
{
    int32_t value = *ptr;
    __asm__ __volatile__ ("" : : : "memory");
    return value;
}

IA_TRACE_DECLSPEC void ia_trace_init(void)
{
    if (!ia_trace_atomic_acquire_load(&ia_trace_is_ready)) {
        ia_trace_setup();
    }
}

IA_TRACE_DECLSPEC uint64_t ia_trace_is_tag_enabled(uint64_t tag)
{
    ia_trace_init();
    return ia_trace_enabled_tags & tag;
}

IA_TRACE_DECLSPEC void ia_trace_begin(uint64_t tag, const char *name)
{
    if (ia_trace_is_tag_enabled(tag)) {
        char buf[IA_TRACE_MESSAGE_LENGTH];
        ssize_t len;
        len = snprintf(buf, IA_TRACE_MESSAGE_LENGTH, "B|%d|%s", getpid(), name);
        if (write(ia_trace_marker_fd, buf, len) != len)
            LOGE("write error!");
    }
}

IA_TRACE_DECLSPEC void ia_trace_end(uint64_t tag)
{
    if (ia_trace_is_tag_enabled(tag)) {
        char c = 'E';
        if (write(ia_trace_marker_fd, &c, 1) !=1)
            LOGE("write error!");
    }
}

} NAMESPACE_DECLARATION_END
#endif //_IA_TRACE_H
