/// Copyright (C) 2019 The Android Open Source Project
// Copyright (C) 2019 Google Inc.
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
#pragma once

#include "HostVisibleMemoryVirtualization.h"

#include <vulkan/vulkan.h>
#include <vndk/hardware_buffer.h>

// Structure similar to
// https://github.com/mesa3d/mesa/blob/master/src/intel/vulkan/anv_android.c

class Gralloc;

namespace goldfish_vk {

uint64_t
getAndroidHardwareBufferUsageFromVkUsage(
    const VkImageCreateFlags vk_create,
    const VkImageUsageFlags vk_usage);

VkResult getAndroidHardwareBufferPropertiesANDROID(
    Gralloc* grallocHelper,
    const HostVisibleMemoryVirtualizationInfo* hostMemVirtInfo,
    VkDevice device,
    const AHardwareBuffer* buffer,
    VkAndroidHardwareBufferPropertiesANDROID* pProperties);

VkResult getMemoryAndroidHardwareBufferANDROID(
    struct AHardwareBuffer **pBuffer);

VkResult importAndroidHardwareBuffer(
    Gralloc* grallocHelper,
    const VkImportAndroidHardwareBufferInfoANDROID* info,
    struct AHardwareBuffer **importOut);

VkResult createAndroidHardwareBuffer(
    bool hasDedicatedImage,
    bool hasDedicatedBuffer,
    const VkExtent3D& imageExtent,
    uint32_t imageLayers,
    VkFormat imageFormat,
    VkImageUsageFlags imageUsage,
    VkImageCreateFlags imageCreateFlags,
    VkDeviceSize bufferSize,
    VkDeviceSize allocationInfoAllocSize,
    struct AHardwareBuffer **out);

} // namespace goldfish_vk
