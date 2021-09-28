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
#include "HostVisibleMemoryVirtualization.h"

#include "android/base/AndroidSubAllocator.h"

#include "Resources.h"
#include "VkEncoder.h"

#include "../OpenglSystemCommon/EmulatorFeatureInfo.h"

#include <log/log.h>

#include <set>

using android::base::guest::SubAllocator;

namespace goldfish_vk {

bool canFitVirtualHostVisibleMemoryInfo(
    const VkPhysicalDeviceMemoryProperties* memoryProperties) {
    uint32_t typeCount =
        memoryProperties->memoryTypeCount;
    uint32_t heapCount =
        memoryProperties->memoryHeapCount;

    bool canFit = true;

    if (typeCount == VK_MAX_MEMORY_TYPES) {
        canFit = false;
        ALOGE("Underlying device has no free memory types");
    }

    if (heapCount == VK_MAX_MEMORY_HEAPS) {
        canFit = false;
        ALOGE("Underlying device has no free memory heaps");
    }

    uint32_t numFreeMemoryTypes = VK_MAX_MEMORY_TYPES - typeCount;
    uint32_t hostVisibleMemoryTypeCount = 0;

    if (hostVisibleMemoryTypeCount > numFreeMemoryTypes) {
        ALOGE("Underlying device has too many host visible memory types (%u)"
              "and not enough free types (%u)",
              hostVisibleMemoryTypeCount, numFreeMemoryTypes);
        canFit = false;
    }

    return canFit;
}

void initHostVisibleMemoryVirtualizationInfo(
    VkPhysicalDevice physicalDevice,
    const VkPhysicalDeviceMemoryProperties* memoryProperties,
    const EmulatorFeatureInfo* featureInfo,
    HostVisibleMemoryVirtualizationInfo* info_out) {

    if (info_out->initialized) return;

    info_out->hostMemoryProperties = *memoryProperties;
    info_out->initialized = true;

    info_out->memoryPropertiesSupported =
        canFitVirtualHostVisibleMemoryInfo(memoryProperties);

    info_out->directMemSupported = featureInfo->hasDirectMem;
    info_out->virtioGpuNextSupported = featureInfo->hasVirtioGpuNext;

    if (!info_out->memoryPropertiesSupported ||
        (!info_out->directMemSupported &&
         !info_out->virtioGpuNextSupported)) {
        info_out->virtualizationSupported = false;
        return;
    }

    info_out->virtualizationSupported = true;

    info_out->physicalDevice = physicalDevice;
    info_out->guestMemoryProperties = *memoryProperties;

    uint32_t typeCount =
        memoryProperties->memoryTypeCount;
    uint32_t heapCount =
        memoryProperties->memoryHeapCount;

    uint32_t firstFreeTypeIndex = typeCount;
    uint32_t firstFreeHeapIndex = heapCount;

    for (uint32_t i = 0; i < typeCount; ++i) {

        // Set up identity mapping and not-both
        // by default, to be edited later.
        info_out->memoryTypeIndexMappingToHost[i] = i;
        info_out->memoryHeapIndexMappingToHost[i] = i;

        info_out->memoryTypeIndexMappingFromHost[i] = i;
        info_out->memoryHeapIndexMappingFromHost[i] = i;

        info_out->memoryTypeBitsShouldAdvertiseBoth[i] = false;

        const auto& type = memoryProperties->memoryTypes[i];

        if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            uint32_t heapIndex = type.heapIndex;

            auto& guestMemoryType =
                info_out->guestMemoryProperties.memoryTypes[i];

            auto& newVirtualMemoryType =
                info_out->guestMemoryProperties.memoryTypes[firstFreeTypeIndex];

            auto& newVirtualMemoryHeap =
                info_out->guestMemoryProperties.memoryHeaps[firstFreeHeapIndex];

            // Remove all references to host visible in the guest memory type at
            // index i, while transferring them to the new virtual memory type.
            newVirtualMemoryType = type;

            // Set this memory type to have a separate heap.
            newVirtualMemoryType.heapIndex = firstFreeHeapIndex;

            newVirtualMemoryType.propertyFlags =
                type.propertyFlags &
                ~(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            guestMemoryType.propertyFlags =
                type.propertyFlags & \
                ~(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                  VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

            // In the corresponding new memory heap, copy the information over,
            // remove device local flags, and resize it based on what is
            // supported by the PCI device.
            newVirtualMemoryHeap =
                memoryProperties->memoryHeaps[heapIndex];
            newVirtualMemoryHeap.flags =
                newVirtualMemoryHeap.flags &
                ~(VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);

            // TODO: Figure out how to support bigger sizes
            newVirtualMemoryHeap.size = VIRTUAL_HOST_VISIBLE_HEAP_SIZE;

            info_out->memoryTypeIndexMappingToHost[firstFreeTypeIndex] = i;
            info_out->memoryHeapIndexMappingToHost[firstFreeHeapIndex] = i;

            info_out->memoryTypeIndexMappingFromHost[i] = firstFreeTypeIndex;
            info_out->memoryHeapIndexMappingFromHost[i] = firstFreeHeapIndex;

            // Was the original memory type also a device local type? If so,
            // advertise both types in resulting type bits.
            info_out->memoryTypeBitsShouldAdvertiseBoth[i] =
                type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ||
                type.propertyFlags == 0;

            ++firstFreeTypeIndex;

            // Explicitly only create one new heap.
            // ++firstFreeHeapIndex;
        }
    }

    info_out->guestMemoryProperties.memoryTypeCount = firstFreeTypeIndex;
    info_out->guestMemoryProperties.memoryHeapCount = firstFreeHeapIndex + 1;

    for (uint32_t i = info_out->guestMemoryProperties.memoryTypeCount; i < VK_MAX_MEMORY_TYPES; ++i) {
        memset(&info_out->guestMemoryProperties.memoryTypes[i],
               0x0, sizeof(VkMemoryType));
    }
}

bool isHostVisibleMemoryTypeIndexForGuest(
    const HostVisibleMemoryVirtualizationInfo* info,
    uint32_t index) {

    const auto& props =
        info->virtualizationSupported ?
        info->guestMemoryProperties :
        info->hostMemoryProperties;

    return props.memoryTypes[index].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
}

bool isDeviceLocalMemoryTypeIndexForGuest(
    const HostVisibleMemoryVirtualizationInfo* info,
    uint32_t index) {

    const auto& props =
        info->virtualizationSupported ?
        info->guestMemoryProperties :
        info->hostMemoryProperties;

    return props.memoryTypes[index].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}

VkResult finishHostMemAllocInit(
    VkEncoder*,
    VkDevice device,
    uint32_t memoryTypeIndex,
    VkDeviceSize nonCoherentAtomSize,
    VkDeviceSize allocSize,
    VkDeviceSize mappedSize,
    uint8_t* mappedPtr,
    HostMemAlloc* out) {

    out->device = device;
    out->memoryTypeIndex = memoryTypeIndex;
    out->nonCoherentAtomSize = nonCoherentAtomSize;
    out->allocSize = allocSize;
    out->mappedSize = mappedSize;
    out->mappedPtr = mappedPtr;

    // because it's not just nonCoherentAtomSize granularity,
    // people will also use it for uniform buffers, images, etc.
    // that need some bigger alignment
// #define HIGHEST_BUFFER_OR_IMAGE_ALIGNMENT 1024
// bug: 145153816
// HACK: Make it 65k so yuv images are happy on vk cts 1.2.1
// TODO: Use a munmap/mmap MAP_FIXED scheme to realign memories
// if it's found that the buffer or image bind alignment will be violated
#define HIGHEST_BUFFER_OR_IMAGE_ALIGNMENT 65536

    uint64_t neededPageSize = out->nonCoherentAtomSize;
    if (HIGHEST_BUFFER_OR_IMAGE_ALIGNMENT >
        neededPageSize) {
        neededPageSize = HIGHEST_BUFFER_OR_IMAGE_ALIGNMENT;
    }

    out->subAlloc = new
        SubAllocator(
            out->mappedPtr,
            out->mappedSize,
            neededPageSize);

    out->initialized = true;
    out->initResult = VK_SUCCESS;
    return VK_SUCCESS;
}

void destroyHostMemAlloc(
    bool freeMemorySyncSupported,
    VkEncoder* enc,
    VkDevice device,
    HostMemAlloc* toDestroy) {

    if (toDestroy->initResult != VK_SUCCESS) return;
    if (!toDestroy->initialized) return;

    if (freeMemorySyncSupported) {
        enc->vkFreeMemorySyncGOOGLE(device, toDestroy->memory, nullptr);
    } else {
        enc->vkFreeMemory(device, toDestroy->memory, nullptr);
    }

    delete toDestroy->subAlloc;
}

void subAllocHostMemory(
    HostMemAlloc* alloc,
    const VkMemoryAllocateInfo* pAllocateInfo,
    SubAlloc* out) {

    VkDeviceSize mappedSize =
        alloc->nonCoherentAtomSize * (
            (pAllocateInfo->allocationSize +
             alloc->nonCoherentAtomSize - 1) /
            alloc->nonCoherentAtomSize);

    ALOGV("%s: alloc size %u mapped size %u ncaSize %u\n", __func__,
            (unsigned int)pAllocateInfo->allocationSize,
            (unsigned int)mappedSize,
            (unsigned int)alloc->nonCoherentAtomSize);

    void* subMapped = alloc->subAlloc->alloc(mappedSize);
    out->mappedPtr = (uint8_t*)subMapped;

    out->subAllocSize = pAllocateInfo->allocationSize;
    out->subMappedSize = mappedSize;

    out->baseMemory = alloc->memory;
    out->baseOffset = alloc->subAlloc->getOffset(subMapped);

    out->subMemory = new_from_host_VkDeviceMemory(VK_NULL_HANDLE);
    out->subAlloc = alloc->subAlloc;
}

void subFreeHostMemory(SubAlloc* toFree) {
    delete_goldfish_VkDeviceMemory(toFree->subMemory);
    toFree->subAlloc->free(toFree->mappedPtr);
    memset(toFree, 0x0, sizeof(SubAlloc));
}

bool canSubAlloc(android::base::guest::SubAllocator* subAlloc, VkDeviceSize size) {
    auto ptr = subAlloc->alloc(size);
    if (!ptr) return false;
    subAlloc->free(ptr);
    return true;
}

bool isNoFlagsMemoryTypeIndexForGuest(
    const HostVisibleMemoryVirtualizationInfo* info,
    uint32_t index) {
    const auto& props =
        info->virtualizationSupported ?
        info->guestMemoryProperties :
        info->hostMemoryProperties;
    return props.memoryTypes[index].propertyFlags == 0;
}


} // namespace goldfish_vk
