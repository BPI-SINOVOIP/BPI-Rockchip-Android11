// Copyright (C) 2018 The Android Open Source Project
// Copyright (C) 2018 Google Inc.
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

#include <vulkan/vulkan.h>

#define VIRTUAL_HOST_VISIBLE_HEAP_SIZE 512ULL * (1048576ULL)

struct EmulatorFeatureInfo;

namespace android {
namespace base {
namespace guest {

class SubAllocator;

} // namespace guest
} // namespace base
} // namespace android

namespace goldfish_vk {

class VkEncoder;

struct HostVisibleMemoryVirtualizationInfo {
    bool initialized = false;
    bool memoryPropertiesSupported;
    bool directMemSupported;
    bool virtualizationSupported;
    bool virtioGpuNextSupported;

    VkPhysicalDevice physicalDevice;

    VkPhysicalDeviceMemoryProperties hostMemoryProperties;
    VkPhysicalDeviceMemoryProperties guestMemoryProperties;

    uint32_t memoryTypeIndexMappingToHost[VK_MAX_MEMORY_TYPES];
    uint32_t memoryHeapIndexMappingToHost[VK_MAX_MEMORY_TYPES];

    uint32_t memoryTypeIndexMappingFromHost[VK_MAX_MEMORY_TYPES];
    uint32_t memoryHeapIndexMappingFromHost[VK_MAX_MEMORY_TYPES];

    bool memoryTypeBitsShouldAdvertiseBoth[VK_MAX_MEMORY_TYPES];
};

bool canFitVirtualHostVisibleMemoryInfo(
    const VkPhysicalDeviceMemoryProperties* memoryProperties);

void initHostVisibleMemoryVirtualizationInfo(
    VkPhysicalDevice physicalDevice,
    const VkPhysicalDeviceMemoryProperties* memoryProperties,
    const EmulatorFeatureInfo* featureInfo,
    HostVisibleMemoryVirtualizationInfo* info_out);

bool isHostVisibleMemoryTypeIndexForGuest(
    const HostVisibleMemoryVirtualizationInfo* info,
    uint32_t index);

bool isDeviceLocalMemoryTypeIndexForGuest(
    const HostVisibleMemoryVirtualizationInfo* info,
    uint32_t index);

struct HostMemAlloc {
    bool initialized = false;
    VkResult initResult = VK_SUCCESS;
    VkDevice device = nullptr;
    uint32_t memoryTypeIndex = 0;
    VkDeviceSize nonCoherentAtomSize = 0;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize allocSize = 0;
    VkDeviceSize mappedSize = 0;
    uint8_t* mappedPtr = nullptr;
    android::base::guest::SubAllocator* subAlloc = nullptr;
};

VkResult finishHostMemAllocInit(
    VkEncoder* enc,
    VkDevice device,
    uint32_t memoryTypeIndex,
    VkDeviceSize nonCoherentAtomSize,
    VkDeviceSize allocSize,
    VkDeviceSize mappedSize,
    uint8_t* mappedPtr,
    HostMemAlloc* out);

void destroyHostMemAlloc(
    bool freeMemorySyncSupported,
    VkEncoder* enc,
    VkDevice device,
    HostMemAlloc* toDestroy);

struct SubAlloc {
    uint8_t* mappedPtr = nullptr;
    VkDeviceSize subAllocSize = 0;
    VkDeviceSize subMappedSize = 0;

    VkDeviceMemory baseMemory = VK_NULL_HANDLE;
    VkDeviceSize baseOffset = 0;
    android::base::guest::SubAllocator* subAlloc = nullptr;
    VkDeviceMemory subMemory = VK_NULL_HANDLE;
};

void subAllocHostMemory(
    HostMemAlloc* alloc,
    const VkMemoryAllocateInfo* pAllocateInfo,
    SubAlloc* out);

void subFreeHostMemory(SubAlloc* toFree);

bool canSubAlloc(android::base::guest::SubAllocator* subAlloc, VkDeviceSize size);

bool isNoFlagsMemoryTypeIndexForGuest(
    const HostVisibleMemoryVirtualizationInfo* info,
    uint32_t index);

} // namespace goldfish_vk
