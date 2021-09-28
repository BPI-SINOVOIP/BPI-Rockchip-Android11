/*
 * Copyright 2018 Rockchip Electronics Co. LTD
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
 * author: rimon.xu@rock-chips.com
 *   date: 20181102
 */

#ifndef INCLUDE_RT_BASE_RT_TEST_HEADER_H_
#define INCLUDE_RT_BASE_RT_TEST_HEADER_H_

#include "rt_error.h" // NOLINT
#include "rt_type.h"  // NOLINT
#include "rt_array_list.h" // NOLINT

#define PRINT_TEST_BEGIN(index, total_index, content, ...) \
    do { \
        RT_LOGE("[%d/%d] %s begin\n", index + 1, total_index, content); \
    } while (0)

#define PRINT_TEST_END(index, total_index, content, result, ...) \
    do { \
        RT_LOGE("[%d/%d] %s [%s]\n", \
                index + 1, \
                total_index, \
                content, \
                result == RT_OK ? "succes" : "failed"); \
    } while (0)

typedef RT_RET (*TestFunc)(INT32 index, INT32 total_index);
typedef RT_RET (*TestFunc_uri)(int index, int total_index, char *uri);

typedef struct rt_test_context {
    char            name[128];
    UINT32          count;
    RtArrayList    *list;
} RtTestCtx;

typedef struct rt_test_node {
    char            name[128];
    TestFunc        func;
} RtTestNode;

typedef struct rt_test_node_uri {
    char            name[128];
    TestFunc_uri    func_uri;
} RtTestNodeUri;

RtTestCtx *     rt_tests_init(char *name);

RT_RET          rt_tests_add(RtTestCtx *ctx, TestFunc func, char *name);
RT_RET          rt_tests_run(RtTestCtx *ctx, RT_BOOL mem_dump);

RT_RET          rt_tests_add_uri(RtTestCtx *ctx, TestFunc_uri func, char *name);
RT_RET          rt_tests_run_uri(RtTestCtx *ctx, RT_BOOL mem_dump, char *uri);

RT_RET          rt_tests_deinit(RtTestCtx **ctx);

#endif  // INCLUDE_RT_BASE_RT_TEST_HEADER_H_
