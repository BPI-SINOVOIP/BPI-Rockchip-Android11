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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_UTILS_GRALLOC_BUFFER_ALLOCATOR_H
#define HARDWARE_GOOGLE_CAMERA_HAL_UTILS_GRALLOC_BUFFER_ALLOCATOR_H

#include "hal_buffer_allocator.h"

#include <hardware/gralloc1.h>
#include <utils/Errors.h>
#include <vector>

namespace android {
namespace google_camera_hal {

// Gralloc1 buffer data structure
struct BufferDescriptor {
  uint32_t width = 0;
  uint32_t height = 0;
  int32_t format = -1;
  uint64_t producer_flags = 0;
  uint64_t consumer_flags = 0;
  uint32_t num_buffers = 0;
};

class GrallocBufferAllocator : IHalBufferAllocator {
 public:
  // Creates GrallocBuffer and allocate buffers
  static std::unique_ptr<IHalBufferAllocator> Create();

  virtual ~GrallocBufferAllocator();

  // Allocate buffers and return buffer via buffers.
  // The buffers is owned by caller
  status_t AllocateBuffers(const HalBufferDescriptor& buffer_descriptor,
                           std::vector<buffer_handle_t>* buffers);

  void FreeBuffers(std::vector<buffer_handle_t>* buffers);

 protected:
  GrallocBufferAllocator() = default;

 private:
  status_t Initialize();

  // Do not support the copy constructor or assignment operator
  GrallocBufferAllocator(const GrallocBufferAllocator&) = delete;
  GrallocBufferAllocator& operator=(const GrallocBufferAllocator&) = delete;

  // Init gralloc1 interface function pointer
  template <typename T>
  void InitGrallocInterface(gralloc1_function_descriptor_t desc,
                            T* output_function);

  // Setup descriptor before allocate buffer via gralloc1 interface
  status_t SetupDescriptor(const BufferDescriptor& buffer_descriptor,
                           gralloc1_buffer_descriptor_t* output_descriptor);

  // Convert HAL buffer descriptor to gralloc descriptor
  void ConvertHalBufferDescriptor(
      const HalBufferDescriptor& hal_buffer_descriptor,
      BufferDescriptor* gralloc_buffer_descriptor);

  hw_module_t* module_ = nullptr;
  gralloc1_device_t* device_ = nullptr;

  // Gralloc1 Interface
  GRALLOC1_PFN_CREATE_DESCRIPTOR create_descriptor_ = nullptr;
  GRALLOC1_PFN_DESTROY_DESCRIPTOR destroy_descriptor_ = nullptr;
  GRALLOC1_PFN_SET_DIMENSIONS set_dimensions_ = nullptr;
  GRALLOC1_PFN_SET_FORMAT set_format_ = nullptr;
  GRALLOC1_PFN_SET_CONSUMER_USAGE set_consumer_usage_ = nullptr;
  GRALLOC1_PFN_SET_PRODUCER_USAGE set_producer_usage_ = nullptr;
  GRALLOC1_PFN_GET_STRIDE get_stride_ = nullptr;
  GRALLOC1_PFN_ALLOCATE allocate_ = nullptr;
  GRALLOC1_PFN_RELEASE release_ = nullptr;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_UTILS_GRALLOC_BUFFER_ALLOCATOR_H
