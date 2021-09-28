// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "Resources.h"

#include <log/log.h>
#include <stdlib.h>

#define GOLDFISH_VK_OBJECT_DEBUG 0

#if GOLDFISH_VK_OBJECT_DEBUG
#define D(fmt,...) ALOGD("%s: " fmt, __func__, ##__VA_ARGS__);
#else
#ifndef D
#define D(fmt,...)
#endif
#endif

extern "C" {

#define GOLDFISH_VK_NEW_DISPATCHABLE_FROM_HOST_IMPL(type) \
    type new_from_host_##type(type underlying) { \
        struct goldfish_##type* res = \
            static_cast<goldfish_##type*>(malloc(sizeof(goldfish_##type))); \
        if (!res) { \
            ALOGE("FATAL: Failed to alloc " #type " handle"); \
            abort(); \
        } \
        res->dispatch.magic = HWVULKAN_DISPATCH_MAGIC; \
        res->underlying = (uint64_t)underlying; \
        return reinterpret_cast<type>(res); \
    } \

#define GOLDFISH_VK_NEW_TRIVIAL_NON_DISPATCHABLE_FROM_HOST_IMPL(type) \
    type new_from_host_##type(type underlying) { \
        struct goldfish_##type* res = \
            static_cast<goldfish_##type*>(malloc(sizeof(goldfish_##type))); \
        res->underlying = (uint64_t)underlying; \
        return reinterpret_cast<type>(res); \
    } \

#define GOLDFISH_VK_AS_GOLDFISH_IMPL(type) \
    struct goldfish_##type* as_goldfish_##type(type toCast) { \
        return reinterpret_cast<goldfish_##type*>(toCast); \
    } \

#define GOLDFISH_VK_GET_HOST_IMPL(type) \
    type get_host_##type(type toUnwrap) { \
        if (!toUnwrap) return VK_NULL_HANDLE; \
        auto as_goldfish = as_goldfish_##type(toUnwrap); \
        return (type)(as_goldfish->underlying); \
    } \

#define GOLDFISH_VK_DELETE_GOLDFISH_IMPL(type) \
    void delete_goldfish_##type(type toDelete) { \
        D("guest %p", toDelete); \
        free(as_goldfish_##type(toDelete)); \
    } \

#define GOLDFISH_VK_IDENTITY_IMPL(type) \
    type vk_handle_identity_##type(type handle) { \
        return handle; \
    } \

#define GOLDFISH_VK_NEW_DISPATCHABLE_FROM_HOST_U64_IMPL(type) \
    type new_from_host_u64_##type(uint64_t underlying) { \
        struct goldfish_##type* res = \
            static_cast<goldfish_##type*>(malloc(sizeof(goldfish_##type))); \
        if (!res) { \
            ALOGE("FATAL: Failed to alloc " #type " handle"); \
            abort(); \
        } \
        res->dispatch.magic = HWVULKAN_DISPATCH_MAGIC; \
        res->underlying = underlying; \
        return reinterpret_cast<type>(res); \
    } \

#define GOLDFISH_VK_NEW_TRIVIAL_NON_DISPATCHABLE_FROM_HOST_U64_IMPL(type) \
    type new_from_host_u64_##type(uint64_t underlying) { \
        struct goldfish_##type* res = \
            static_cast<goldfish_##type*>(malloc(sizeof(goldfish_##type))); \
        res->underlying = underlying; \
        D("guest %p: host u64: 0x%llx", res, (unsigned long long)res->underlying); \
        return reinterpret_cast<type>(res); \
    } \

#define GOLDFISH_VK_GET_HOST_U64_IMPL(type) \
    uint64_t get_host_u64_##type(type toUnwrap) { \
        if (!toUnwrap) return 0; \
        auto as_goldfish = as_goldfish_##type(toUnwrap); \
        D("guest %p: host u64: 0x%llx", toUnwrap, (unsigned long long)as_goldfish->underlying); \
        return as_goldfish->underlying; \
    } \

GOLDFISH_VK_LIST_DISPATCHABLE_HANDLE_TYPES(GOLDFISH_VK_NEW_DISPATCHABLE_FROM_HOST_IMPL)
GOLDFISH_VK_LIST_DISPATCHABLE_HANDLE_TYPES(GOLDFISH_VK_AS_GOLDFISH_IMPL)
GOLDFISH_VK_LIST_DISPATCHABLE_HANDLE_TYPES(GOLDFISH_VK_GET_HOST_IMPL)
GOLDFISH_VK_LIST_DISPATCHABLE_HANDLE_TYPES(GOLDFISH_VK_DELETE_GOLDFISH_IMPL)
GOLDFISH_VK_LIST_DISPATCHABLE_HANDLE_TYPES(GOLDFISH_VK_IDENTITY_IMPL)
GOLDFISH_VK_LIST_DISPATCHABLE_HANDLE_TYPES(GOLDFISH_VK_NEW_DISPATCHABLE_FROM_HOST_U64_IMPL)
GOLDFISH_VK_LIST_DISPATCHABLE_HANDLE_TYPES(GOLDFISH_VK_GET_HOST_U64_IMPL)

GOLDFISH_VK_LIST_NON_DISPATCHABLE_HANDLE_TYPES(GOLDFISH_VK_AS_GOLDFISH_IMPL)
GOLDFISH_VK_LIST_NON_DISPATCHABLE_HANDLE_TYPES(GOLDFISH_VK_GET_HOST_IMPL)
GOLDFISH_VK_LIST_NON_DISPATCHABLE_HANDLE_TYPES(GOLDFISH_VK_IDENTITY_IMPL)
GOLDFISH_VK_LIST_NON_DISPATCHABLE_HANDLE_TYPES(GOLDFISH_VK_GET_HOST_U64_IMPL)
GOLDFISH_VK_LIST_NON_DISPATCHABLE_HANDLE_TYPES(GOLDFISH_VK_NEW_TRIVIAL_NON_DISPATCHABLE_FROM_HOST_IMPL)
GOLDFISH_VK_LIST_NON_DISPATCHABLE_HANDLE_TYPES(GOLDFISH_VK_NEW_TRIVIAL_NON_DISPATCHABLE_FROM_HOST_U64_IMPL)
GOLDFISH_VK_LIST_NON_DISPATCHABLE_HANDLE_TYPES(GOLDFISH_VK_DELETE_GOLDFISH_IMPL)

} // extern "C"
