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
#define LOG_TAG "GCH_ZslBufferManager"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <cutils/properties.h>
#include <log/log.h>
#include <utils/Trace.h>

#include <time.h>

#include "zsl_buffer_manager.h"

namespace android {
namespace google_camera_hal {

ZslBufferManager::ZslBufferManager(IHalBufferAllocator* allocator)
    : kMemoryProfilingEnabled(
          property_get_bool("persist.camera.hal.memoryprofile", false)),
      buffer_allocator_(allocator) {
}

ZslBufferManager::~ZslBufferManager() {
  ATRACE_CALL();
  std::unique_lock<std::mutex> lock(zsl_buffers_lock_);
  if (buffer_allocator_ != nullptr) {
    buffer_allocator_->FreeBuffers(&buffers_);
  }
}

status_t ZslBufferManager::AllocateBuffers(
    const HalBufferDescriptor& buffer_descriptor) {
  ATRACE_CALL();
  std::unique_lock<std::mutex> lock(zsl_buffers_lock_);

  if (allocated_) {
    ALOGE("%s: Buffer is already allocated.", __FUNCTION__);
    return ALREADY_EXISTS;
  }

  // Create a buffer allocator if the client doesn't specify one.
  if (buffer_allocator_ == nullptr) {
    // Create a buffer manager.
    internal_buffer_allocator_ = GrallocBufferAllocator::Create();
    if (internal_buffer_allocator_ == nullptr) {
      ALOGE("%s: Creating a buffer manager failed.", __FUNCTION__);
      return NO_MEMORY;
    }

    buffer_allocator_ = internal_buffer_allocator_.get();
  }

  uint32_t num_buffers = buffer_descriptor.immediate_num_buffers;
  buffer_descriptor_ = buffer_descriptor;
  status_t res = AllocateBuffersLocked(num_buffers);
  if (res != OK) {
    ALOGE("%s: Allocating %d buffers failed.", __FUNCTION__, num_buffers);
    return res;
  }

  allocated_ = true;
  return OK;
}

status_t ZslBufferManager::AllocateBuffersLocked(uint32_t buffer_number) {
  if (buffer_number + buffers_.size() > buffer_descriptor_.max_num_buffers) {
    ALOGE("%s: allocate %d + exist %zu > max buffer number %d", __FUNCTION__,
          buffer_number, buffers_.size(), buffer_descriptor_.max_num_buffers);
    return NO_MEMORY;
  }

  HalBufferDescriptor buffer_descriptor = buffer_descriptor_;
  buffer_descriptor.immediate_num_buffers = buffer_number;
  std::vector<buffer_handle_t> buffers;
  status_t res = buffer_allocator_->AllocateBuffers(buffer_descriptor, &buffers);
  if (res != OK) {
    ALOGE("%s: AllocateBuffers fail.", __FUNCTION__);
    return res;
  }

  for (auto& buffer : buffers) {
    if (buffer != kInvalidBufferHandle) {
      buffers_.push_back(buffer);
      empty_zsl_buffers_.push_back(buffer);
    }
  }

  if (buffers.size() != buffer_number) {
    ALOGE("%s: allocate buffer failed. request %u, get %zu", __FUNCTION__,
          buffer_number, buffers.size());
    return NO_MEMORY;
  }

  if (kMemoryProfilingEnabled) {
    ALOGI(
        "%s: Allocated %u buffers, res %ux%u, format %d, overall allocated "
        "%zu buffers",
        __FUNCTION__, buffer_number, buffer_descriptor_.width,
        buffer_descriptor_.height, buffer_descriptor_.format, buffers_.size());
  }

  return OK;
}

buffer_handle_t ZslBufferManager::GetEmptyBuffer() {
  ATRACE_CALL();
  std::unique_lock<std::mutex> lock(zsl_buffers_lock_);
  if (!allocated_) {
    ALOGE("%s: Buffers are not allocated.", __FUNCTION__);
    return kInvalidBufferHandle;
  }

  buffer_handle_t buffer = GetEmptyBufferLocked();
  if (buffer == kInvalidBufferHandle) {
    // Try to allocate one more buffer if there is no empty buffer.
    status_t res = AllocateBuffersLocked(/*buffer_number=*/1);
    if (res != OK) {
      ALOGE("%s: Allocating one more buffer failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return kInvalidBufferHandle;
    }

    buffer = GetEmptyBufferLocked();
  }

  return buffer;
}

buffer_handle_t ZslBufferManager::GetEmptyBufferLocked() {
  ATRACE_CALL();
  buffer_handle_t buffer = kInvalidBufferHandle;

  // Get an empty buffer from empty ZSL buffer queue.
  // If empty ZSL buffer queue is empty,
  // Get the oldest buffer from filled ZSL buffer queue.
  // If filled ZSL buffer queue is empty,
  // Get the oldest buffer from the partially filled ZSL buffer queue.
  if (empty_zsl_buffers_.size() > 0) {
    buffer = empty_zsl_buffers_[0];
    empty_zsl_buffers_.pop_front();
  } else if (filled_zsl_buffers_.size() > 0) {
    auto buffer_iter = filled_zsl_buffers_.begin();
    buffer = buffer_iter->second.buffer.buffer;
    filled_zsl_buffers_.erase(buffer_iter);
  } else if (partially_filled_zsl_buffers_.size() > 0) {
    auto buffer_iter = partially_filled_zsl_buffers_.begin();
    while (buffer_iter != partially_filled_zsl_buffers_.end()) {
      if (buffer_iter->second.metadata != nullptr) {
        if (buffer_iter->second.buffer.buffer != kInvalidBufferHandle) {
          ALOGE("%s: Invalid: both are ready in partial zsl queue.",
                __FUNCTION__);
          // TODO: clean up resources in this invalid situation.
          return kInvalidBufferHandle;
        }
        ALOGI(
            "%s: Remove metadata-only buffer in partially filled zsl "
            "buffer queue. Releasing the metadata resource.",
            __FUNCTION__);
      } else if (buffer_iter->second.buffer.buffer == kInvalidBufferHandle) {
        ALOGE(
            "%s: Invalid: both buffer and metadata are empty in partial "
            "zsl queue.",
            __FUNCTION__);
        // TODO: clean up resources in this invalid situation.
        return kInvalidBufferHandle;
      } else {
        ALOGI(
            "%s: Get buffer-only empty buffer from partially filled zsl "
            "buffer queue.",
            __FUNCTION__);
        buffer = buffer_iter->second.buffer.buffer;
      }

      // remove whatever visited
      buffer_iter = partially_filled_zsl_buffers_.erase(buffer_iter);

      if (buffer != kInvalidBufferHandle) {
        break;
      }
    }

    if (buffer_iter == partially_filled_zsl_buffers_.end()) {
      ALOGE("%s: No empty buffer available.", __FUNCTION__);
    }
  } else {
    ALOGW("%s: No empty buffer available.", __FUNCTION__);
  }

  return buffer;
}

void ZslBufferManager::FreeUnusedBuffersLocked() {
  ATRACE_CALL();
  if (empty_zsl_buffers_.size() <= kMaxUnusedBuffers ||
      buffers_.size() <= buffer_descriptor_.immediate_num_buffers) {
    idle_buffer_frame_counter_ = 0;
    return;
  }

  idle_buffer_frame_counter_++;
  if (idle_buffer_frame_counter_ <= kMaxIdelBufferFrameCounter) {
    return;
  }

  std::vector<buffer_handle_t> unused_buffers;

  // When there are at least kMaxUnusedBuffers unused buffers, try to reduce
  // the number of buffers to buffer_descriptor_.immediate_num_buffers.
  while (buffers_.size() > buffer_descriptor_.immediate_num_buffers &&
         empty_zsl_buffers_.size() > 0) {
    buffer_handle_t buffer = empty_zsl_buffers_.back();
    unused_buffers.push_back(buffer);
    empty_zsl_buffers_.pop_back();
    buffers_.erase(std::find(buffers_.begin(), buffers_.end(), buffer));
  }

  if (kMemoryProfilingEnabled) {
    ALOGI(
        "%s: Freeing %zu buffers, res %ux%u, format %d, overall allocated "
        "%zu buffers",
        __FUNCTION__, unused_buffers.size(), buffer_descriptor_.width,
        buffer_descriptor_.height, buffer_descriptor_.format, buffers_.size());
  }

  buffer_allocator_->FreeBuffers(&unused_buffers);
}

status_t ZslBufferManager::ReturnEmptyBuffer(buffer_handle_t buffer) {
  ATRACE_CALL();
  if (buffer == kInvalidBufferHandle) {
    ALOGE("%s: buffer is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  std::unique_lock<std::mutex> lock(zsl_buffers_lock_);
  // Check whether the returned buffer is freed or not
  auto exist_buffer = std::find(buffers_.begin(), buffers_.end(), buffer);
  if (exist_buffer == buffers_.end()) {
    ALOGE("%s: Buffer %p is invalid", __FUNCTION__, buffer);
    return BAD_VALUE;
  }

  auto empty_buffer = std::find(std::begin(empty_zsl_buffers_),
                                std::end(empty_zsl_buffers_), buffer);
  if (empty_buffer != std::end(empty_zsl_buffers_)) {
    ALOGE("%s: Buffer %p was already returned.", __FUNCTION__, buffer);
    return ALREADY_EXISTS;
  }

  empty_zsl_buffers_.push_back(buffer);
  FreeUnusedBuffersLocked();
  return OK;
}

status_t ZslBufferManager::ReturnFilledBuffer(uint32_t frame_number,
                                              const StreamBuffer& buffer) {
  ATRACE_CALL();
  ZslBuffer zsl_buffer = {};
  zsl_buffer.frame_number = frame_number;
  zsl_buffer.buffer = buffer;

  std::unique_lock<std::mutex> lock(zsl_buffers_lock_);
  if (partially_filled_zsl_buffers_.empty() ||
      partially_filled_zsl_buffers_.find(frame_number) ==
          partially_filled_zsl_buffers_.end()) {
    // not able to distinguish these two cases through the current status
    // of the partial buffer
    ALOGV(
        "%s: no entry for frame[%u] in ZslBufferManager. Not created or "
        "has been removed",
        __FUNCTION__, frame_number);

    zsl_buffer.metadata = nullptr;
    partially_filled_zsl_buffers_[frame_number] = std::move(zsl_buffer);
  } else if (partially_filled_zsl_buffers_[frame_number].buffer.buffer ==
                 kInvalidBufferHandle &&
             partially_filled_zsl_buffers_[frame_number].metadata != nullptr) {
    ALOGV(
        "%s: both buffer and metadata for frame[%u] are ready. Move to "
        "filled_zsl_buffers_.",
        __FUNCTION__, frame_number);

    zsl_buffer.metadata =
        std::move(partially_filled_zsl_buffers_[frame_number].metadata);
    filled_zsl_buffers_[frame_number] = std::move(zsl_buffer);
    partially_filled_zsl_buffers_.erase(frame_number);
  } else {
    ALOGE(
        "%s: the buffer for frame[%u] already returned or the metadata is "
        "missing.",
        __FUNCTION__, frame_number);
    return INVALID_OPERATION;
  }

  return OK;
}

status_t ZslBufferManager::ReturnMetadata(uint32_t frame_number,
                                          const HalCameraMetadata* metadata) {
  ATRACE_CALL();
  std::unique_lock<std::mutex> lock(zsl_buffers_lock_);

  ZslBuffer zsl_buffer = {};
  zsl_buffer.frame_number = frame_number;
  zsl_buffer.metadata = HalCameraMetadata::Clone(metadata);
  if (zsl_buffer.metadata == nullptr) {
    ALOGE("%s: Failed to Clone camera metadata.", __FUNCTION__);
    return NO_MEMORY;
  }

  if (partially_filled_zsl_buffers_.empty() ||
      partially_filled_zsl_buffers_.find(frame_number) ==
          partially_filled_zsl_buffers_.end()) {
    // not able to distinguish these two cases through the current status of
    // the partial buffer
    ALOGV(
        "%s: no entry for frame[%u] in ZslBufferManager. Not created or "
        "has been removed",
        __FUNCTION__, frame_number);

    zsl_buffer.buffer = {};
    partially_filled_zsl_buffers_[frame_number] = std::move(zsl_buffer);
  } else if (partially_filled_zsl_buffers_[frame_number].metadata == nullptr &&
             partially_filled_zsl_buffers_[frame_number].buffer.buffer !=
                 kInvalidBufferHandle) {
    ALOGV(
        "%s: both buffer and metadata for frame[%u] are ready. Move to "
        "filled_zsl_buffers_.",
        __FUNCTION__, frame_number);

    zsl_buffer.buffer = partially_filled_zsl_buffers_[frame_number].buffer;
    filled_zsl_buffers_[frame_number] = std::move(zsl_buffer);
    partially_filled_zsl_buffers_.erase(frame_number);
  } else {
    ALOGE(
        "%s: the metadata for frame[%u] already returned or the buffer is "
        "missing.",
        __FUNCTION__, frame_number);
    return INVALID_OPERATION;
  }

  if (partially_filled_zsl_buffers_.size() > kMaxPartialZslBuffers) {
    // Remove the oldest one if it exceeds the maximum number of partial ZSL
    // buffers.
    partially_filled_zsl_buffers_.erase(partially_filled_zsl_buffers_.begin());
  }

  return OK;
}

status_t ZslBufferManager::GetCurrentTimestampNs(int64_t* current_timestamp) {
  if (current_timestamp == nullptr) {
    ALOGE("%s: current_timestamp is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  struct timespec ts;
  if (clock_gettime(CLOCK_BOOTTIME, &ts)) {
    ALOGE("%s: Getting boot time failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  static const int64_t kNsPerSec = 1000000000;
  *current_timestamp = ts.tv_sec * kNsPerSec + ts.tv_nsec;
  return OK;
}

void ZslBufferManager::GetMostRecentZslBuffers(
    std::vector<ZslBuffer>* zsl_buffers, uint32_t num_buffers,
    uint32_t min_buffers) {
  ATRACE_CALL();
  if (zsl_buffers == nullptr) {
    return;
  }

  int64_t current_timestamp;
  status_t res = GetCurrentTimestampNs(&current_timestamp);
  if (res != OK) {
    ALOGE("%s: Getting current timestamp failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return;
  }

  std::unique_lock<std::mutex> lock(zsl_buffers_lock_);
  if (filled_zsl_buffers_.size() < min_buffers) {
    ALOGD("%s: Requested min_buffers = %u, ZslBufferManager only has %zu",
          __FUNCTION__, min_buffers, filled_zsl_buffers_.size());
    ALOGD("%s: Not enough ZSL buffers to get, returns empty zsl_buffers.",
          __FUNCTION__);
    return;
  }

  num_buffers =
      std::min(static_cast<uint32_t>(filled_zsl_buffers_.size()), num_buffers);
  auto zsl_buffer_iter = filled_zsl_buffers_.begin();

  // Skip the older ones.
  for (uint32_t i = 0; i < filled_zsl_buffers_.size() - num_buffers; i++) {
    zsl_buffer_iter++;
  }

  // Fallback to realtime pipeline capture if there are any flash-fired frame
  // in zsl buffers with AE_MODE_ON_AUTO_FLASH.
  camera_metadata_ro_entry entry = {};
  res = zsl_buffer_iter->second.metadata->Get(ANDROID_CONTROL_AE_MODE, &entry);
  if (res == OK && entry.data.u8[0] == ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH) {
    for (auto search_iter = filled_zsl_buffers_.begin();
         search_iter != filled_zsl_buffers_.end(); search_iter++) {
      res = search_iter->second.metadata->Get(ANDROID_FLASH_STATE, &entry);
      if (res == OK && entry.count == 1) {
        if (entry.data.u8[0] == ANDROID_FLASH_STATE_FIRED) {
          ALOGD("%s: Returns empty zsl_buffers due to flash fired",
                __FUNCTION__);
          return;
        }
      }
    }
  }

  for (uint32_t i = 0; i < num_buffers; i++) {
    camera_metadata_ro_entry entry = {};
    int64_t buffer_timestamp;
    res =
        zsl_buffer_iter->second.metadata->Get(ANDROID_SENSOR_TIMESTAMP, &entry);
    if (res != OK || entry.count != 1) {
      ALOGW("%s: Getting sensor timestamp failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return;
    }

    buffer_timestamp = entry.data.i64[0];
    // Only include recent buffers.
    if (current_timestamp - buffer_timestamp < kMaxBufferTimestampDiff) {
      zsl_buffers->push_back(std::move(zsl_buffer_iter->second));
    }

    zsl_buffer_iter = filled_zsl_buffers_.erase(zsl_buffer_iter);
  }
}

void ZslBufferManager::ReturnZslBuffer(ZslBuffer zsl_buffer) {
  ATRACE_CALL();
  std::unique_lock<std::mutex> lock(zsl_buffers_lock_);
  filled_zsl_buffers_[zsl_buffer.frame_number] = std::move(zsl_buffer);
}

void ZslBufferManager::ReturnZslBuffers(std::vector<ZslBuffer> zsl_buffers) {
  ATRACE_CALL();
  for (auto& zsl_buffer : zsl_buffers) {
    ReturnZslBuffer(std::move(zsl_buffer));
  }
}

bool ZslBufferManager::IsPendingBufferEmpty() {
  std::lock_guard<std::mutex> lock(pending_zsl_buffers_mutex);
  bool empty = (pending_zsl_buffers_.size() == 0);
  if (!empty) {
    ALOGW("%s: Pending buffer is not empty:%zu.", __FUNCTION__,
          pending_zsl_buffers_.size());
    return false;
  }

  return true;
}

void ZslBufferManager::AddPendingBuffers(const std::vector<ZslBuffer>& buffers) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(pending_zsl_buffers_mutex);
  for (auto& buffer : buffers) {
    ZslBuffer zsl_buffer = {
        .frame_number = buffer.frame_number,
        .buffer = buffer.buffer,
        .metadata = HalCameraMetadata::Clone(buffer.metadata.get()),
    };

    pending_zsl_buffers_.emplace(buffer.buffer.buffer, std::move(zsl_buffer));
  }
}

status_t ZslBufferManager::CleanPendingBuffers(std::vector<ZslBuffer>* buffers) {
  ATRACE_CALL();
  if (buffers == nullptr) {
    ALOGE("%s: buffers is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(pending_zsl_buffers_mutex);
  if (pending_zsl_buffers_.empty()) {
    ALOGE("%s: There is no empty buffer.", __FUNCTION__);
    return BAD_VALUE;
  }

  for (auto zsl_buffer_iter = pending_zsl_buffers_.begin();
       zsl_buffer_iter != pending_zsl_buffers_.end(); zsl_buffer_iter++) {
    buffers->push_back(std::move(zsl_buffer_iter->second));
  }

  pending_zsl_buffers_.clear();
  return OK;
}

}  // namespace google_camera_hal
}  // namespace android
