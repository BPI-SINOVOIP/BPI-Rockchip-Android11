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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_UTILS_HWL_BUFFER_ALLOCATOR_H
#define HARDWARE_GOOGLE_CAMERA_HAL_UTILS_HWL_BUFFER_ALLOCATOR_H

#include "camera_buffer_allocator_hwl.h"
#include "camera_device_session_hwl.h"
#include "hal_buffer_allocator.h"

#include <hardware/gralloc1.h>
#include <utils/Errors.h>
#include <vector>

namespace android {
namespace google_camera_hal {

// Implement IHalBufferAllocator to allocate/free buffers from HWL allocator
// This allocator can be created from device session where it knows the instance
// of HWL allocator implementation
class HwlBufferAllocator : IHalBufferAllocator {
 public:
  // Creates HwlBuffer and allocate buffers
  static std::unique_ptr<IHalBufferAllocator> Create(
      CameraBufferAllocatorHwl* camera_buffer_allocator_hwl);

  virtual ~HwlBufferAllocator() = default;

  // Allocate buffers and return buffer via buffers.
  // The buffers is owned by caller
  status_t AllocateBuffers(const HalBufferDescriptor& buffer_descriptor,
                           std::vector<buffer_handle_t>* buffers) override;

  void FreeBuffers(std::vector<buffer_handle_t>* buffers) override;

 protected:
  HwlBufferAllocator() = default;

 private:
  status_t Initialize(CameraBufferAllocatorHwl* camera_buffer_allocator_hwl);

  // Do not support the copy constructor or assignment operator
  HwlBufferAllocator(const HwlBufferAllocator&) = delete;
  HwlBufferAllocator& operator=(const HwlBufferAllocator&) = delete;

  CameraBufferAllocatorHwl* camera_buffer_allocator_hwl_ = nullptr;

  uint64_t id_ = 0;
  static std::atomic<uint64_t> global_instance_count_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_UTILS_HWL_BUFFER_ALLOCATOR_H
