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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_CAMERA_BUFFER_ALLOCATOR_HWL_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_CAMERA_BUFFER_ALLOCATOR_HWL_H_

#include <utils/Errors.h>

#include "hwl_types.h"

namespace android {
namespace google_camera_hal {

// HAL internal stream ids start from (1 << 16).
constexpr int32_t kHalInternalStreamStart = (1 << 16);

// HWL internal stream ids start from (1 << 24). This is reserved for HWL
// for vendor-specific usages.
constexpr int32_t kImplementationDefinedInternalStreamStart = (1 << 24);

// CameraBufferAllocatorHwl provides methods to register, allocate and
// free buffers
class CameraBufferAllocatorHwl {
 public:
  virtual ~CameraBufferAllocatorHwl() = default;

  // Allocate HWL buffers
  virtual status_t AllocateBuffers(const HalBufferDescriptor& buffer_descriptor,
                                   std::vector<buffer_handle_t>* buffers) = 0;

  // Free HWL buffers
  virtual status_t FreeBuffers(std::vector<buffer_handle_t>* buffers) = 0;

  // Check if buffer is allocated by HWL allocator
  virtual bool IsHwlAllocatedBuffer(buffer_handle_t buffer) = 0;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_CAMERA_BUFFER_ALLOCATOR_HWL_H_
