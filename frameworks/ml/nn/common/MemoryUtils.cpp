/*
 * Copyright (C) 2019 The Android Open Source Project
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

#define LOG_TAG "MemoryUtils"

#include "MemoryUtils.h"

#include <android-base/logging.h>
#include <android/hidl/allocator/1.0/IAllocator.h>

using ::android::hardware::hidl_memory;
using ::android::hidl::allocator::V1_0::IAllocator;

namespace android {
namespace nn {

hidl_memory allocateSharedMemory(int64_t size) {
    static const std::string type = "ashmem";
    static sp<IAllocator> allocator = IAllocator::getService(type);

    hidl_memory memory;

    // TODO: should we align memory size to nearest page? doesn't seem necessary...
    allocator->allocate(size, [&](bool success, const hidl_memory& mem) {
        if (!success) {
            LOG(ERROR) << "unable to allocate " << size << " bytes of " << type;
        } else {
            memory = mem;
        }
    });

    return memory;
}

}  // namespace nn
}  // namespace android
