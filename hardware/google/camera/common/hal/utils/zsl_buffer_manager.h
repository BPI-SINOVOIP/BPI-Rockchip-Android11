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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_UTILS_ZSL_BUFFER_MANAGER_H
#define HARDWARE_GOOGLE_CAMERA_HAL_UTILS_ZSL_BUFFER_MANAGER_H

#include <utils/Errors.h>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "gralloc_buffer_allocator.h"
#include "hal_buffer_allocator.h"

#include "hal_types.h"

namespace android {
namespace google_camera_hal {

// ZslBufferManager creates and manages ZSL buffers.
class ZslBufferManager {
 public:
  // allocator will be used to allocate buffers. If allocator is nullptr,
  // GrallocBufferAllocator will be used to allocate buffers.
  ZslBufferManager(IHalBufferAllocator* allocator = nullptr);
  virtual ~ZslBufferManager();

  // Defines a ZSL buffer.
  struct ZslBuffer {
    // Original frame number of this ZSL buffer captured by HAL.
    uint32_t frame_number = 0;
    // Buffer
    StreamBuffer buffer;
    // Original result metadata of this ZSL buffer captured by HAL.
    std::unique_ptr<HalCameraMetadata> metadata;
  };

  // Allocate buffers. This can only be called once.
  // The second call will return ALREADY_EXISTS.
  status_t AllocateBuffers(const HalBufferDescriptor& buffer_descriptor);

  // Get an empty buffer for capture. The caller can modify the buffer data
  // but the buffer is owned by ZslBufferManager and
  // must not be freed by the caller.
  buffer_handle_t GetEmptyBuffer();

  // Return an empty buffer that was previously obtained by GetEmptyBuffer().
  status_t ReturnEmptyBuffer(buffer_handle_t buffer);

  // Return the buffer part of a filled buffer
  // that was previously obtained by GetEmptyBuffer().
  status_t ReturnFilledBuffer(uint32_t frame_number, const StreamBuffer& buffer);

  // Return the metadata part of a filled buffer
  // that was previously obtained by GetEmptyBuffer().
  // ZSL buffer manager will make a copy of metadata.
  // The caller still owns metadata.
  status_t ReturnMetadata(uint32_t frame_number,
                          const HalCameraMetadata* metadata);

  // Get a number of the most recent ZSL buffers.
  // If numBuffers is larger than available ZSL buffers,
  // zslBuffers will contain all available ZSL buffers,
  // i.e. zslBuffers.size() may be smaller than numBuffers.
  // The buffer and metadata are owned by ZslBufferManager and
  // must not be freed by the caller.
  // minBuffers is the minimum number of buffers the
  // zsl buffer manager should return. If this can not be satisfied
  // (i.e. not enough ZSL buffers exist),
  // this GetMostRecentZslBuffers returns an empty vector.
  void GetMostRecentZslBuffers(std::vector<ZslBuffer>* zsl_buffers,
                               uint32_t num_buffers, uint32_t min_buffers);

  // Return a ZSL buffer that was previously obtained by
  // GetMostRecentZslBuffers().
  void ReturnZslBuffer(ZslBuffer zsl_buffer);

  // Return a vector of ZSL buffers that were previously obtained by
  // GetMostRecentZslBuffers().
  void ReturnZslBuffers(std::vector<ZslBuffer> zsl_buffers);

  // Check ZslBuffers are allocated or not.
  bool IsBufferAllocated() {
    return allocated_;
  };

  // Check pending_zsl_buffers_ is empty or not.
  bool IsPendingBufferEmpty();

  // Add buffer map to pending_zsl_buffers_
  void AddPendingBuffers(const std::vector<ZslBuffer>& buffers);

  // Clean buffer map from pending_zsl_buffers_
  status_t CleanPendingBuffers(std::vector<ZslBuffer>* buffers);

 private:
  static const uint32_t kMaxPartialZslBuffers = 100;

  // Max timestamp difference of the ZSL buffer and current time. Used
  // to discard old ZSL buffers.
  static const int64_t kMaxBufferTimestampDiff = 1000000000;  // 1 second

  // Maximum number of unused buffers. When the number of unused buffers >
  // kMaxUnusedBuffers, it will try to free excessive buffers.
  static const uint32_t kMaxUnusedBuffers = 2;

  // Maximum number of frames with enough unused buffers. When the number of
  // counter > kMaxIdelBufferFrameCounter, it will try to free excessive
  // buffers.
  static const uint32_t kMaxIdelBufferFrameCounter = 300;

  const bool kMemoryProfilingEnabled;

  // Remove the oldest metadata.
  status_t RemoveOldestMetadataLocked();

  // Get current BOOT_TIME timestamp in nanoseconds
  status_t GetCurrentTimestampNs(int64_t* current_timestamp);

  // Allocate a number of buffers. Must be protected by zsl_buffers_lock_.
  status_t AllocateBuffersLocked(uint32_t buffer_number);

  // Get an empty buffer. Must be protected by zsl_buffers_lock_.
  buffer_handle_t GetEmptyBufferLocked();

  // Try to free unused buffers. Must be protected by zsl_buffers_lock_.
  void FreeUnusedBuffersLocked();

  bool allocated_ = false;
  std::mutex zsl_buffers_lock_;

  // Buffer manager for allocating the buffers. Protected by mZslBuffersLock.
  std::unique_ptr<IHalBufferAllocator> internal_buffer_allocator_;

  // external buffer allocator
  IHalBufferAllocator* buffer_allocator_ = nullptr;

  // Empty ZSL buffer queue. Protected by mZslBuffersLock.
  std::deque<buffer_handle_t> empty_zsl_buffers_;

  // Filled ZSL buffers. Map from frameNumber to ZslBuffer.
  // Ordered from the oldest to the newest buffers.
  // Protected by mZslBuffersLock.
  std::map<uint32_t, ZslBuffer> filled_zsl_buffers_;

  // Partially filled ZSL buffers. Either the metadata or
  // the buffer is returned. Once the metadata and the buffer are both ready,
  // the ZslBuffer will be moved to the filled_zsl_buffers_.
  // Map from frameNumber to ZslBuffer. Ordered from the oldest to the newest
  // buffers. filled_zsl_buffers_ protected by zsl_buffers_lock_.
  std::map<uint32_t, ZslBuffer> partially_filled_zsl_buffers_;

  // Store all allocated buffers return from GrallocBufferAllocator
  std::vector<buffer_handle_t> buffers_;

  std::mutex pending_zsl_buffers_mutex;

  // Map from buffer handle to ZSL buffer. Protected by pending_zsl_buffers_mutex.
  std::unordered_map<buffer_handle_t, ZslBuffer> pending_zsl_buffers_;

  // Store the buffer descriptor when call AllocateBuffers()
  // Use it for AllocateExtraBuffers()
  HalBufferDescriptor buffer_descriptor_;

  // Count the number when there are enough unused buffers.
  uint32_t idle_buffer_frame_counter_ = 0;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_UTILS_ZSL_BUFFER_MANAGER_H
