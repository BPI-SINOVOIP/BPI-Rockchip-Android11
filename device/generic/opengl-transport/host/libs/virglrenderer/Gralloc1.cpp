/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdint>

extern "C" {
#include "virgl_hw.h"
}

#include "Resource.h"

#include <android/sync.h>
#include <hardware/gralloc1.h>

#include <cerrno>

#define ALIGN(A, B) (((A) + (B)-1) / (B) * (B))

static int gralloc1_device_open(const hw_module_t*, const char*, hw_device_t**);

static hw_module_methods_t g_gralloc1_methods = {
    .open = gralloc1_device_open,
};

static hw_module_t g_gralloc1_module = {
    .tag = HARDWARE_MODULE_TAG,
    .module_api_version = GRALLOC_MODULE_API_VERSION_1_0,
    .hal_api_version = HARDWARE_HAL_API_VERSION,
    .id = GRALLOC_HARDWARE_MODULE_ID,
    .name = "AVDVirglRenderer",
    .author = "Google",
    .methods = &g_gralloc1_methods,
};

static int gralloc1_device_close(hw_device_t*) {
    // No-op
    return 0;
}

static void gralloc1_getCapabilities(gralloc1_device_t*, uint32_t* outCount, int32_t*) {
    *outCount = 0U;
}

static int32_t gralloc1_lock(gralloc1_device_t*, buffer_handle_t buffer, uint64_t producerUsage,
                             uint64_t consumerUsage, const gralloc1_rect_t* rect, void** outData,
                             int32_t) {
    uint32_t resource_id = (uint32_t) reinterpret_cast<uintptr_t>(buffer);
    std::map<uint32_t, Resource*>::iterator it;
    it = Resource::map.find(resource_id);
    if (it == Resource::map.end())
        return GRALLOC1_ERROR_BAD_HANDLE;

    Resource* res = it->second;

    // validate the lock rectangle
    if (rect->width < 0 || rect->height < 0 || rect->left < 0 || rect->top < 0 ||
        (uint32_t)rect->width + (uint32_t)rect->left > res->args.width ||
        (uint32_t)rect->height + (uint32_t)rect->top > res->args.height) {
        return GRALLOC1_ERROR_BAD_VALUE;
    }

    uint32_t bpp;
    switch (res->args.format) {
        case VIRGL_FORMAT_R8_UNORM:
            bpp = 1U;
            break;
        case VIRGL_FORMAT_B5G6R5_UNORM:
            bpp = 2U;
            break;
        default:
            bpp = 4U;
            break;
    }
    uint32_t stride = ALIGN(res->args.width * bpp, 16U);
    *outData = (char*)res->linear + rect->top * stride + rect->left * bpp;
    return GRALLOC1_ERROR_NONE;
}

static int32_t gralloc1_unlock(gralloc1_device_t*, buffer_handle_t buffer,
                               int32_t* outReleaseFence) {
    uint32_t resource_id = (uint32_t) reinterpret_cast<uintptr_t>(buffer);
    std::map<uint32_t, Resource*>::iterator it;
    it = Resource::map.find(resource_id);
    if (it == Resource::map.end())
        return GRALLOC1_ERROR_BAD_HANDLE;
    if (outReleaseFence)
        *outReleaseFence = -1;

    return GRALLOC1_ERROR_NONE;
}

static gralloc1_function_pointer_t gralloc1_getFunction(gralloc1_device_t*, int32_t descriptor) {
    switch (descriptor) {
        case GRALLOC1_FUNCTION_LOCK:
            return reinterpret_cast<gralloc1_function_pointer_t>(&gralloc1_lock);
        case GRALLOC1_FUNCTION_UNLOCK:
            return reinterpret_cast<gralloc1_function_pointer_t>(&gralloc1_unlock);
        default:
            return nullptr;
    }
}

static gralloc1_device_t g_gralloc1_device = {
    .common =
        {
            .tag = HARDWARE_DEVICE_TAG,
            .module = &g_gralloc1_module,
            .close = gralloc1_device_close,
        },
    .getCapabilities = gralloc1_getCapabilities,
    .getFunction = gralloc1_getFunction,
};

static int gralloc1_device_open(const hw_module_t* module, const char* id, hw_device_t** device) {
    if (module != &g_gralloc1_module)
        return -EINVAL;
    if (strcmp(id, g_gralloc1_module.id))
        return -EINVAL;
    *device = &g_gralloc1_device.common;
    return 0;
}

int hw_get_module(const char* id, const hw_module_t** module) {
    if (strcmp(id, g_gralloc1_module.id))
        return -EINVAL;
    *module = const_cast<const hw_module_t*>(&g_gralloc1_module);
    return 0;
}

int sync_wait(int, int) {
    return 0;
}
