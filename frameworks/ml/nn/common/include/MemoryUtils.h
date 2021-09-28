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

#ifndef ANDROID_FRAMEWORKS_ML_NN_COMMON_MEMORY_UTILS_H
#define ANDROID_FRAMEWORKS_ML_NN_COMMON_MEMORY_UTILS_H

#include <hidlmemory/mapping.h>

namespace android {
namespace nn {

// Memory is unmapped.
// Memory is reference counted by hidl_memory instances, and is deallocated
// once there are no more references.
hardware::hidl_memory allocateSharedMemory(int64_t size);

}  // namespace nn
}  // namespace android

#endif  // ANDROID_FRAMEWORKS_ML_NN_COMMON_MEMORY_UTILS_H
