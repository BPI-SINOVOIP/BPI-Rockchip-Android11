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

#ifndef LOG_TAG
#define LOG_TAG "IA_TRACE"
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "IaAtrace.h"

NAMESPACE_DECLARATION {

volatile int32_t ia_trace_is_ready  = 0;
uint64_t ia_trace_enabled_tags = IA_TRACE_TAG_NOT_READY;
int ia_trace_marker_fd  = -1;
static pthread_once_t ia_trace_once_control = PTHREAD_ONCE_INIT;

static inline __attribute__((always_inline)) void
ia_trace_atomic_release_store(int32_t value, volatile int32_t *ptr)
{
    __asm__ __volatile__ ("" : : : "memory");
    *ptr = value;
}

static void ia_trace_init_once(void)
{
    ia_trace_marker_fd = open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY);
    if (ia_trace_marker_fd == -1) {
        LOGE("open error!");
        return;
    }
    ia_trace_enabled_tags = IA_TRACE_TAG_ALWAYS;
    ia_trace_atomic_release_store(1, &ia_trace_is_ready);
}

void ia_trace_setup(void)
{
    pthread_once(&ia_trace_once_control, ia_trace_init_once);
}

} NAMESPACE_DECLARATION_END
