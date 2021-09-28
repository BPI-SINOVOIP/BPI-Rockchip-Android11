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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_UTILS_HAL_BUFFER_ALLOCATOR_H
#define HARDWARE_GOOGLE_CAMERA_HAL_UTILS_HAL_BUFFER_ALLOCATOR_H

#include "hal_types.h"

#include <hardware/gralloc1.h>
#include <utils/Errors.h>
#include <vector>

namespace android {
namespace google_camera_hal {

// IHalBufferAllocator defines an interface for HAL to allocate HW accessible
// buffers
class IHalBufferAllocator {
 public:
  virtual ~IHalBufferAllocator(){};

  // Allocate buffers and return buffer via buffers.
  // The buffers is owned by caller
  virtual status_t AllocateBuffers(const HalBufferDescriptor& buffer_descriptor,
                                   std::vector<buffer_handle_t>* buffers) = 0;

  // free the list of buffers
  virtual void FreeBuffers(std::vector<buffer_handle_t>* buffers) = 0;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_UTILS_HAL_BUFFER_ALLOCATOR_H
