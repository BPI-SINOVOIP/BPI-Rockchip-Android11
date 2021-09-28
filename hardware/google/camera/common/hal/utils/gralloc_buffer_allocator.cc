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
#define LOG_TAG "GCH_GrallocBufferAllocator"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include "gralloc_buffer_allocator.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<IHalBufferAllocator> GrallocBufferAllocator::Create() {
  ATRACE_CALL();
  auto gralloc_buffer =
      std::unique_ptr<GrallocBufferAllocator>(new GrallocBufferAllocator());
  if (gralloc_buffer == nullptr) {
    ALOGE("%s: Creating gralloc_buffer failed.", __FUNCTION__);
    return nullptr;
  }

  status_t result = gralloc_buffer->Initialize();
  if (result != OK) {
    ALOGE("%s: GrallocBuffer Initialize failed.", __FUNCTION__);
    return nullptr;
  }

  std::unique_ptr<IHalBufferAllocator> base_allocator;
  base_allocator.reset(gralloc_buffer.release());

  return base_allocator;
}

GrallocBufferAllocator::~GrallocBufferAllocator() {
  if (device_ != nullptr) {
    gralloc1_close(device_);
  }
}

status_t GrallocBufferAllocator::Initialize() {
  ATRACE_CALL();
  int32_t error =
      hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t**)&module_);

  if (error < 0) {
    ALOGE("%s: Could not load GRALLOC HAL module: %d (%s)", __FUNCTION__, error,
          strerror(-error));
    return INVALID_OPERATION;
  }

  gralloc1_open(module_, &device_);
  if (device_ == nullptr) {
    ALOGE("%s: gralloc1 open failed", __FUNCTION__);
    return INVALID_OPERATION;
  }

  InitGrallocInterface(GRALLOC1_FUNCTION_CREATE_DESCRIPTOR, &create_descriptor_);
  InitGrallocInterface(GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR,
                       &destroy_descriptor_);
  InitGrallocInterface(GRALLOC1_FUNCTION_SET_DIMENSIONS, &set_dimensions_);
  InitGrallocInterface(GRALLOC1_FUNCTION_SET_FORMAT, &set_format_);
  InitGrallocInterface(GRALLOC1_FUNCTION_SET_CONSUMER_USAGE,
                       &set_consumer_usage_);
  InitGrallocInterface(GRALLOC1_FUNCTION_SET_PRODUCER_USAGE,
                       &set_producer_usage_);
  InitGrallocInterface(GRALLOC1_FUNCTION_GET_STRIDE, &get_stride_);
  InitGrallocInterface(GRALLOC1_FUNCTION_ALLOCATE, &allocate_);
  InitGrallocInterface(GRALLOC1_FUNCTION_RELEASE, &release_);
  return OK;
}

template <typename T>
void GrallocBufferAllocator::InitGrallocInterface(
    gralloc1_function_descriptor_t desc, T* output_function) {
  ATRACE_CALL();
  auto function = device_->getFunction(device_, desc);
  if (!function) {
    ALOGE("%s: failed to get gralloc1 function %d", __FUNCTION__, desc);
  }
  *output_function = reinterpret_cast<T>(function);
}

status_t GrallocBufferAllocator::SetupDescriptor(
    const BufferDescriptor& buffer_descriptor,
    gralloc1_buffer_descriptor_t* output_descriptor) {
  ATRACE_CALL();
  int32_t error =
      set_dimensions_(device_, *output_descriptor, buffer_descriptor.width,
                      buffer_descriptor.height);
  if (error != GRALLOC1_ERROR_NONE) {
    ALOGE("%s: set_dimensions failed", __FUNCTION__);
    return INVALID_OPERATION;
  }

  error = set_format_(device_, *output_descriptor, buffer_descriptor.format);
  if (error != GRALLOC1_ERROR_NONE) {
    ALOGE("%s: set_format failed", __FUNCTION__);
    return INVALID_OPERATION;
  }

  error = set_producer_usage_(device_, *output_descriptor,
                              buffer_descriptor.producer_flags);
  if (error != GRALLOC1_ERROR_NONE) {
    ALOGE("%s: set_producer_usage failed", __FUNCTION__);
    return INVALID_OPERATION;
  }

  error = set_consumer_usage_(device_, *output_descriptor,
                              buffer_descriptor.consumer_flags);
  if (error != GRALLOC1_ERROR_NONE) {
    ALOGE("%s: set_consumer_usage_ failed", __FUNCTION__);
    return INVALID_OPERATION;
  }

  return OK;
}

void GrallocBufferAllocator::ConvertHalBufferDescriptor(
    const HalBufferDescriptor& hal_buffer_descriptor,
    BufferDescriptor* gralloc_buffer_descriptor) {
  ATRACE_CALL();
  // For BLOB format, the gralloc buffer width should be the actual size, and
  // height should be 1.
  if (hal_buffer_descriptor.format == HAL_PIXEL_FORMAT_BLOB) {
    gralloc_buffer_descriptor->width =
        hal_buffer_descriptor.width * hal_buffer_descriptor.height;
    gralloc_buffer_descriptor->height = 1;
  } else {
    gralloc_buffer_descriptor->width = hal_buffer_descriptor.width;
    gralloc_buffer_descriptor->height = hal_buffer_descriptor.height;
  }
  gralloc_buffer_descriptor->format = hal_buffer_descriptor.format;
  gralloc_buffer_descriptor->producer_flags =
      hal_buffer_descriptor.producer_flags;
  gralloc_buffer_descriptor->consumer_flags =
      hal_buffer_descriptor.consumer_flags;
  gralloc_buffer_descriptor->num_buffers =
      hal_buffer_descriptor.immediate_num_buffers;
}

status_t GrallocBufferAllocator::AllocateBuffers(
    const HalBufferDescriptor& buffer_descriptor,
    std::vector<buffer_handle_t>* buffers) {
  ATRACE_CALL();
  // This function runs four operations.
  // 1. create descriptor
  // 2. setup descriptor
  // 3. allocate buffer via descriptor
  // 4. destroy descriptor_
  gralloc1_buffer_descriptor_t descriptor;
  int32_t error = create_descriptor_(device_, &descriptor);
  if (error != GRALLOC1_ERROR_NONE) {
    ALOGE("%s: create descriptor failed", __FUNCTION__);
    return INVALID_OPERATION;
  }

  BufferDescriptor gralloc_buffer_descriptor{0};
  ConvertHalBufferDescriptor(buffer_descriptor, &gralloc_buffer_descriptor);
  status_t result = SetupDescriptor(gralloc_buffer_descriptor, &descriptor);
  if (result != OK) {
    ALOGE("%s: SetupDescriptor failed", __FUNCTION__);
    destroy_descriptor_(device_, descriptor);
    return INVALID_OPERATION;
  }

  uint32_t stride = 0, temp_stride = 0;
  for (uint32_t i = 0; i < gralloc_buffer_descriptor.num_buffers; i++) {
    buffer_handle_t buffer;
    error = allocate_(device_, 1, &descriptor, &buffer);
    if (error != GRALLOC1_ERROR_NONE) {
      ALOGE("%s: buffer(%d) allocate failed", __FUNCTION__, i);
      break;
    }
    buffers->push_back(buffer);

    error = get_stride_(device_, buffer, &temp_stride);
    if (error != GRALLOC1_ERROR_NONE) {
      ALOGE("%s: buffer(%d) get_stride failed", __FUNCTION__, i);
      break;
    }
    // Check non-uniform strides
    if (stride == 0) {
      stride = temp_stride;
    } else if (stride != temp_stride) {
      ALOGE("%s: non-uniform strides (%d) != (%d)", __FUNCTION__, stride,
            temp_stride);
      error = GRALLOC1_ERROR_UNSUPPORTED;
      break;
    }
  }

  destroy_descriptor_(device_, descriptor);
  if (error != GRALLOC1_ERROR_NONE) {
    FreeBuffers(buffers);
    return INVALID_OPERATION;
  }
  return OK;
}

void GrallocBufferAllocator::FreeBuffers(std::vector<buffer_handle_t>* buffers) {
  ATRACE_CALL();
  for (auto buffer : *buffers) {
    if (buffer != nullptr) {
      release_(device_, buffer);
    }
  }
  buffers->clear();
}

}  // namespace google_camera_hal
}  // namespace android
