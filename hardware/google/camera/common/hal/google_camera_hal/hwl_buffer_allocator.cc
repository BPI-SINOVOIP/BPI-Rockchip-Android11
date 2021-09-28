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

//#define LOG_NDEBUG 0
#define LOG_TAG "GCH_HwlBufferAllocator"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include "hwl_buffer_allocator.h"

namespace android {
namespace google_camera_hal {

std::atomic<uint64_t> HwlBufferAllocator::global_instance_count_ = 0;

std::unique_ptr<IHalBufferAllocator> HwlBufferAllocator::Create(
    CameraBufferAllocatorHwl* camera_buffer_allocator_hwl) {
  ATRACE_CALL();
  auto hwl_buffer_allocator =
      std::unique_ptr<HwlBufferAllocator>(new HwlBufferAllocator());
  if (hwl_buffer_allocator == nullptr) {
    ALOGE("%s: Creating hwl_buffer_allocator failed.", __FUNCTION__);
    return nullptr;
  }

  status_t result =
      hwl_buffer_allocator->Initialize(camera_buffer_allocator_hwl);
  if (result != OK) {
    ALOGE("%s: HwlBuffer Initialize failed.", __FUNCTION__);
    return nullptr;
  }

  std::unique_ptr<IHalBufferAllocator> base_allocator;
  base_allocator.reset(hwl_buffer_allocator.release());

  return base_allocator;
}

status_t HwlBufferAllocator::Initialize(
    CameraBufferAllocatorHwl* camera_buffer_allocator_hwl) {
  ATRACE_CALL();
  if (camera_buffer_allocator_hwl == nullptr) {
    return BAD_VALUE;
  }
  camera_buffer_allocator_hwl_ = camera_buffer_allocator_hwl;
  id_ = ++global_instance_count_;
  return OK;
}

status_t HwlBufferAllocator::AllocateBuffers(
    const HalBufferDescriptor& buffer_descriptor,
    std::vector<buffer_handle_t>* buffers) {
  ATRACE_CALL();
  HalBufferDescriptor local_descriptor = buffer_descriptor;
  // some vendor allocator need to be aware of allocator instance id to manage
  // proper internal logic.
  local_descriptor.allocator_id_ = id_;
  status_t res =
      camera_buffer_allocator_hwl_->AllocateBuffers(local_descriptor, buffers);
  if (res != OK) {
    ALOGE("%s: HWL buffer allocation failed: %s (%d).", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  return OK;
}

void HwlBufferAllocator::FreeBuffers(std::vector<buffer_handle_t>* buffers) {
  ATRACE_CALL();
  camera_buffer_allocator_hwl_->FreeBuffers(buffers);
  buffers->clear();
}

}  // namespace google_camera_hal
}  // namespace android
