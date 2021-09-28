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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_DEVICE_HWL_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_DEVICE_HWL_H_

#include <camera_buffer_allocator_hwl.h>
#include <camera_device_session_hwl.h>
#include <unordered_map>

#include "mock_device_session_hwl.h"

namespace android {
namespace google_camera_hal {

class MockBufferAllocatorHwl : public CameraBufferAllocatorHwl {
 public:
  static std::unique_ptr<CameraBufferAllocatorHwl> Create() {
    auto hwl_buffer_allocator =
        std::unique_ptr<MockBufferAllocatorHwl>(new MockBufferAllocatorHwl());

    std::unique_ptr<CameraBufferAllocatorHwl> base_allocator;
    base_allocator.reset(dynamic_cast<CameraBufferAllocatorHwl*>(
        hwl_buffer_allocator.release()));

    return base_allocator;
  }

  // Allocate HWL buffers
  status_t AllocateBuffers(const HalBufferDescriptor& buffer_descriptor,
                           std::vector<buffer_handle_t>* buffers) {
    buffers->resize(buffer_descriptor.max_num_buffers);
    return OK;
  }

  // Free HWL buffers
  status_t FreeBuffers(std::vector<buffer_handle_t>* buffers) {
    buffers->clear();
    return OK;
  }

  bool IsHwlAllocatedBuffer(buffer_handle_t /*buffer*/) {
    return false;
  }

 protected:
  MockBufferAllocatorHwl() = default;
};
}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_DEVICE_HWL_H_
