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

#define LOG_TAG "GrallocBufferAllocatorTests"
#include <log/log.h>

#include <gralloc_buffer_allocator.h>
#include <gtest/gtest.h>

namespace android {
namespace google_camera_hal {

static const uint32_t kBufferWidth = 4032;
static const uint32_t kBufferHeight = 3024;
static const uint32_t kMaxBufferDepth = 10;

// Test GrallocBufferAllocator Create.
TEST(GrallocBufferAllocatorTests, Create) {
  auto allocator = GrallocBufferAllocator::Create();
  ASSERT_NE(allocator, nullptr) << "Create GrallocBufferAllocator failed.";
}

// Test GrallocBufferAllocator AllocateFreeBuffers.
TEST(GrallocBufferAllocatorTests, AllocateFreeBuffers) {
  auto allocator = GrallocBufferAllocator::Create();
  ASSERT_NE(allocator, nullptr) << "Create GrallocBufferAllocator failed.";

  HalBufferDescriptor buffer_descriptor = {};
  buffer_descriptor.width = kBufferWidth;
  buffer_descriptor.height = kBufferHeight;
  buffer_descriptor.format = HAL_PIXEL_FORMAT_RAW10;
  buffer_descriptor.producer_flags = GRALLOC1_PRODUCER_USAGE_CAMERA;
  buffer_descriptor.consumer_flags = GRALLOC1_CONSUMER_USAGE_CAMERA;
  buffer_descriptor.immediate_num_buffers = kMaxBufferDepth;
  buffer_descriptor.max_num_buffers = kMaxBufferDepth;

  std::vector<buffer_handle_t> buffers_;
  status_t res = allocator->AllocateBuffers(buffer_descriptor, &buffers_);
  ASSERT_EQ(res, OK) << "AllocateBuffers failed: " << strerror(res);
  ASSERT_EQ(buffers_.size(), (uint32_t)kMaxBufferDepth)
      << "AllocateBuffers failed with wrong buffer number " << buffers_.size();

  allocator->FreeBuffers(&buffers_);
  ASSERT_EQ(buffers_.size(), (uint32_t)0)
      << "AllocateBuffers failed with wrong buffer number " << buffers_.size();
}

// Test GrallocBufferAllocator MultipleAllocateBuffers.
TEST(GrallocBufferAllocatorTests, MultipleAllocateBuffers) {
  auto allocator = GrallocBufferAllocator::Create();
  ASSERT_NE(allocator, nullptr) << "Create GrallocBufferAllocator failed.";

  HalBufferDescriptor buffer_descriptor = {};
  buffer_descriptor.width = kBufferWidth;
  buffer_descriptor.height = kBufferHeight;
  buffer_descriptor.format = HAL_PIXEL_FORMAT_RAW10;
  buffer_descriptor.producer_flags = GRALLOC1_PRODUCER_USAGE_CAMERA;
  buffer_descriptor.consumer_flags = GRALLOC1_CONSUMER_USAGE_CAMERA;
  buffer_descriptor.immediate_num_buffers = kMaxBufferDepth;
  buffer_descriptor.max_num_buffers = kMaxBufferDepth;

  std::vector<buffer_handle_t> buffers_;
  status_t res = allocator->AllocateBuffers(buffer_descriptor, &buffers_);
  ASSERT_EQ(res, OK) << "AllocateBuffers failed: " << strerror(res);
  ASSERT_EQ(buffers_.size(), (uint32_t)kMaxBufferDepth)
      << "AllocateBuffers failed with wrong buffer number " << buffers_.size();

  res = allocator->AllocateBuffers(buffer_descriptor, &buffers_);
  ASSERT_EQ(res, OK) << "Second AllocateBuffers failed: " << strerror(res);
  ASSERT_EQ(buffers_.size(), (uint32_t)(kMaxBufferDepth + kMaxBufferDepth))
      << "Second AllocateBuffers failed with wrong buffer number "
      << buffers_.size();

  allocator->FreeBuffers(&buffers_);
  ASSERT_EQ(buffers_.size(), (uint32_t)0)
      << "AllocateBuffers failed with wrong buffer number " << buffers_.size();
}

}  // namespace google_camera_hal
}  // namespace android
