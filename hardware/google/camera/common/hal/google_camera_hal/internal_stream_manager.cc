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
#define LOG_TAG "GCH_InternalStreamManager"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include "hal_utils.h"
#include "internal_stream_manager.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<InternalStreamManager> InternalStreamManager::Create(
    IHalBufferAllocator* buffer_allocator) {
  ATRACE_CALL();
  auto stream_manager =
      std::unique_ptr<InternalStreamManager>(new InternalStreamManager());
  if (stream_manager == nullptr) {
    ALOGE("%s: Creating InternalStreamManager failed.", __FUNCTION__);
    return nullptr;
  }

  stream_manager->Initialize(buffer_allocator);

  return stream_manager;
}

void InternalStreamManager::Initialize(IHalBufferAllocator* buffer_allocator) {
  hwl_buffer_allocator_ = buffer_allocator;
}

status_t InternalStreamManager::IsStreamRegisteredLocked(int32_t stream_id) const {
  return registered_streams_.find(stream_id) != registered_streams_.end();
}

status_t InternalStreamManager::IsStreamAllocatedLocked(int32_t stream_id) const {
  // Check if this stream is sharing buffers with another stream or owns a
  // a buffer manager.
  return shared_stream_owner_ids_.find(stream_id) !=
             shared_stream_owner_ids_.end() ||
         buffer_managers_.find(stream_id) != buffer_managers_.end();
}

int32_t InternalStreamManager::GetBufferManagerOwnerIdLocked(int32_t stream_id) {
  int32_t owner_stream_id = stream_id;
  auto owner_id_it = shared_stream_owner_ids_.find(stream_id);
  if (owner_id_it != shared_stream_owner_ids_.end()) {
    owner_stream_id = owner_id_it->second;
  }

  if (buffer_managers_.find(owner_stream_id) == buffer_managers_.end()) {
    return kInvalidStreamId;
  }

  return owner_stream_id;
}

status_t InternalStreamManager::RegisterNewInternalStream(const Stream& stream,
                                                          int32_t* stream_id) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(stream_mutex_);
  if (stream_id == nullptr) {
    ALOGE("%s: stream_id is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  Stream internal_stream = stream;
  int32_t id = stream.id;
  // if stream.id is one of reserved ids in camera_buffer_allocator_hwl.h, we
  // will use the given id so that HWL can use its predefined id to setup
  // implementation defined internal stream format. other wise will use the next
  // available unique id.
  if (stream.id < kStreamIdReserve) {
    id = next_available_stream_id_++;
    internal_stream.id = id;
  }
  registered_streams_[id] = std::move(internal_stream);

  *stream_id = id;
  return OK;
}

status_t InternalStreamManager::GetBufferDescriptor(
    const Stream& stream, const HalStream& hal_stream,
    uint32_t additional_num_buffers, HalBufferDescriptor* buffer_descriptor) {
  ATRACE_CALL();
  if (buffer_descriptor == nullptr) {
    ALOGE("%s: buffer_descriptor is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  if (stream.id != hal_stream.id) {
    ALOGE("%s: IDs don't match: stream %d hal stream %d", __FUNCTION__,
          stream.id, hal_stream.id);
    return BAD_VALUE;
  }

  buffer_descriptor->stream_id = stream.id;
  buffer_descriptor->width = stream.width;
  buffer_descriptor->height = stream.height;
  buffer_descriptor->format = hal_stream.override_format;
  buffer_descriptor->producer_flags = hal_stream.producer_usage;
  buffer_descriptor->consumer_flags = hal_stream.consumer_usage;
  buffer_descriptor->immediate_num_buffers = hal_stream.max_buffers;
  buffer_descriptor->max_num_buffers =
      hal_stream.max_buffers + additional_num_buffers;

  return OK;
}

status_t InternalStreamManager::AllocateBuffers(const HalStream& hal_stream,
                                                uint32_t additional_num_buffers,
                                                bool need_vendor_buffer) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(stream_mutex_);
  return AllocateBuffersLocked(hal_stream, additional_num_buffers,
                               need_vendor_buffer);
}

status_t InternalStreamManager::AllocateBuffersLocked(
    const HalStream& hal_stream, uint32_t additional_num_buffers,
    bool need_vendor_buffer) {
  ATRACE_CALL();
  int32_t stream_id = hal_stream.id;

  if (!IsStreamRegisteredLocked(stream_id)) {
    ALOGE("%s: Stream %d was not registered.", __FUNCTION__, stream_id);
    return NAME_NOT_FOUND;
  }

  if (IsStreamAllocatedLocked(stream_id)) {
    ALOGE("%s: Stream %d is already allocated.", __FUNCTION__, stream_id);
    return ALREADY_EXISTS;
  }

  HalBufferDescriptor buffer_descriptor;
  status_t res = GetBufferDescriptor(registered_streams_[stream_id], hal_stream,
                                     additional_num_buffers, &buffer_descriptor);
  if (res != OK) {
    ALOGE("%s: Getting buffer descriptor failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  auto buffer_manager = std::make_unique<ZslBufferManager>(
      need_vendor_buffer ? hwl_buffer_allocator_ : nullptr);
  if (buffer_manager == nullptr) {
    ALOGE("%s: Failed to create a buffer manager for stream %d", __FUNCTION__,
          stream_id);
    return UNKNOWN_ERROR;
  }

  res = buffer_manager->AllocateBuffers(buffer_descriptor);
  if (res != OK) {
    ALOGE(
        "%s: Failed to allocate %u immediate buffers (max: %u) for stream %d: "
        "%s(%d)",
        __FUNCTION__, buffer_descriptor.immediate_num_buffers,
        buffer_descriptor.max_num_buffers, stream_id, strerror(-res), res);
    return res;
  }

  buffer_managers_[stream_id] = std::move(buffer_manager);
  return OK;
}

bool InternalStreamManager::AreStreamsCompatible(
    const Stream& stream_0, const HalStream& hal_stream_0,
    const Stream& stream_1, const HalStream& hal_stream_1) const {
  return stream_0.width == stream_1.width &&
         stream_0.height == stream_1.height &&
         stream_0.rotation == stream_1.rotation &&
         hal_stream_0.override_format == hal_stream_1.override_format &&
         hal_stream_0.producer_usage == hal_stream_1.producer_usage &&
         hal_stream_0.consumer_usage == hal_stream_1.consumer_usage &&
         hal_stream_0.override_data_space == hal_stream_1.override_data_space;
}

bool InternalStreamManager::CanHalStreamsShareBuffersLocked(
    const std::vector<HalStream>& hal_streams) const {
  if (hal_streams.size() < 2) {
    ALOGV("%s: Cannot sharing buffers for %zu stream.", __FUNCTION__,
          hal_streams.size());
    return BAD_VALUE;
  }

  int32_t first_stream_id = hal_streams[0].id;
  for (uint32_t i = 0; i < hal_streams.size(); i++) {
    int32_t stream_id = hal_streams[i].id;
    if (!IsStreamRegisteredLocked(stream_id)) {
      ALOGE("%s: stream id %d was not registered.", __FUNCTION__, stream_id);
      return false;
    }

    if (i == 0) {
      // Skip the first one.
      continue;
    }

    if (!AreStreamsCompatible(registered_streams_.at(first_stream_id),
                              hal_streams[0], registered_streams_.at(stream_id),
                              hal_streams[i])) {
      ALOGV("%s: Stream %d and %d are not compatible", __FUNCTION__,
            first_stream_id, stream_id);
      IF_ALOGV() {
        hal_utils::DumpStream(registered_streams_.at(first_stream_id),
                              "stream_0");
        hal_utils::DumpStream(registered_streams_.at(stream_id), "stream_1");
        hal_utils::DumpHalStream(hal_streams[0], "hal_stream_0");
        hal_utils::DumpHalStream(hal_streams[i], "hal_stream_1");
      }

      return false;
    }
  }

  return true;
}

status_t InternalStreamManager::AllocateSharedBuffers(
    const std::vector<HalStream>& hal_streams, uint32_t additional_num_buffers,
    bool need_vendor_buffer) {
  std::lock_guard<std::mutex> lock(stream_mutex_);
  if (hal_streams.size() < 2) {
    ALOGE("%s: Cannot sharing buffers for %zu stream.", __FUNCTION__,
          hal_streams.size());
    return BAD_VALUE;
  }

  uint32_t max_buffers = 0;
  uint32_t total_max_buffers = 0;

  // Find the maximum and total of all hal_streams' max_buffers.
  for (auto& hal_stream : hal_streams) {
    if (!IsStreamRegisteredLocked(hal_stream.id)) {
      ALOGE("%s: Stream %d was not registered.", __FUNCTION__, hal_stream.id);
      return BAD_VALUE;
    }

    if (IsStreamAllocatedLocked(hal_stream.id)) {
      ALOGE("%s: Stream %d has been allocated.", __FUNCTION__, hal_stream.id);
      return BAD_VALUE;
    }

    total_max_buffers += hal_stream.max_buffers;
    max_buffers = std::max(max_buffers, hal_stream.max_buffers);
  }

  if (!CanHalStreamsShareBuffersLocked(hal_streams)) {
    ALOGE("%s: Streams cannot share buffers.", __FUNCTION__);
    return BAD_VALUE;
  }

  // Allocate the maximum of all hal_streams' max_buffers immediately and
  // additional (total_max_buffers + additional_num_buffers - max_buffers)
  // buffers.
  HalStream hal_stream = hal_streams[0];
  hal_stream.max_buffers = max_buffers;
  uint32_t total_additional_num_buffers =
      total_max_buffers + additional_num_buffers - max_buffers;

  status_t res = AllocateBuffersLocked(hal_stream, total_additional_num_buffers,
                                       need_vendor_buffer);
  if (res != OK) {
    ALOGE("%s: Allocating buffers for stream %d failed: %s(%d)", __FUNCTION__,
          hal_stream.id, strerror(-res), res);
    return res;
  }

  for (uint32_t i = 1; i < hal_streams.size(); i++) {
    shared_stream_owner_ids_[hal_streams[i].id] = hal_streams[0].id;
  }

  return OK;
}

status_t InternalStreamManager::RemoveOwnerStreamIdLocked(
    int32_t old_owner_stream_id) {
  int32_t new_owner_stream_id = kInvalidStreamId;

  if (buffer_managers_.find(old_owner_stream_id) == buffer_managers_.end()) {
    ALOGE("%s: Stream %d does not own any buffer manager.", __FUNCTION__,
          old_owner_stream_id);
    return BAD_VALUE;
  }

  // Find the first stream that shares the buffer manager that
  // old_owner_stream_id owns, and update the rest of the streams to point to
  // the new owner.
  auto owner_stream_id_it = shared_stream_owner_ids_.begin();
  while (owner_stream_id_it != shared_stream_owner_ids_.end()) {
    if (owner_stream_id_it->second == old_owner_stream_id) {
      if (new_owner_stream_id == kInvalidStreamId) {
        // Found the first stream sharing the old owner's buffer manager.
        // Make this the new buffer manager owner.
        new_owner_stream_id = owner_stream_id_it->first;
        owner_stream_id_it = shared_stream_owner_ids_.erase(owner_stream_id_it);
        continue;
      } else {
        // Update the rest of the stream to point to the new owner.
        owner_stream_id_it->second = new_owner_stream_id;
      }
    }
    owner_stream_id_it++;
  }

  if (new_owner_stream_id != kInvalidStreamId) {
    // Update buffer manager owner.
    buffer_managers_[new_owner_stream_id] =
        std::move(buffer_managers_[old_owner_stream_id]);
  }

  buffer_managers_.erase(old_owner_stream_id);
  return OK;
}

void InternalStreamManager::FreeStream(int32_t stream_id) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(stream_mutex_);
  registered_streams_.erase(stream_id);

  int32_t owner_stream_id = GetBufferManagerOwnerIdLocked(stream_id);
  if (owner_stream_id == kInvalidStreamId) {
    ALOGE("%s: Cannot find a owner stream ID for stream %d", __FUNCTION__,
          stream_id);
    return;
  }

  if (stream_id == owner_stream_id) {
    // Find a new owner if the owner is being freed.
    status_t res = RemoveOwnerStreamIdLocked(owner_stream_id);
    if (res != OK) {
      ALOGE("%s: Removing owner stream failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return;
    }
  } else {
    // If this stream is not the owner, just remove it from
    // shared_stream_owner_ids_.
    shared_stream_owner_ids_.erase(stream_id);
  }
}

status_t InternalStreamManager::GetStreamBuffer(int32_t stream_id,
                                                StreamBuffer* buffer) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(stream_mutex_);

  if (!IsStreamAllocatedLocked(stream_id)) {
    ALOGE("%s: Stream %d was not allocated.", __FUNCTION__, stream_id);
    return ALREADY_EXISTS;
  }

  if (buffer == nullptr) {
    ALOGE("%s: buffer is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  int32_t owner_stream_id = GetBufferManagerOwnerIdLocked(stream_id);
  if (owner_stream_id == kInvalidStreamId) {
    ALOGE("%s: Cannot find a owner stream ID for stream %d", __FUNCTION__,
          stream_id);
    return BAD_VALUE;
  }

  buffer->buffer = buffer_managers_[owner_stream_id]->GetEmptyBuffer();
  if (buffer->buffer == kInvalidBufferHandle) {
    ALOGE("%s: Failed to get an empty buffer for stream %d (owner %d)",
          __FUNCTION__, stream_id, owner_stream_id);
    return UNKNOWN_ERROR;
  }

  buffer->stream_id = stream_id;
  buffer->buffer_id = 0;  // Buffer ID should be irrelevant internally in HAL.
  buffer->status = BufferStatus::kOk;
  buffer->acquire_fence = nullptr;
  buffer->release_fence = nullptr;
  return OK;
}

bool InternalStreamManager::IsPendingBufferEmpty(int32_t stream_id) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(stream_mutex_);
  if (!IsStreamAllocatedLocked(stream_id)) {
    ALOGE("%s: Stream %d was not allocated.", __FUNCTION__, stream_id);
    return false;
  }

  int32_t owner_stream_id = GetBufferManagerOwnerIdLocked(stream_id);
  if (owner_stream_id == kInvalidStreamId) {
    ALOGE("%s: Cannot find a owner stream ID for stream %d", __FUNCTION__,
          stream_id);
    return false;
  }

  return buffer_managers_[owner_stream_id]->IsPendingBufferEmpty();
}

status_t InternalStreamManager::GetMostRecentStreamBuffer(
    int32_t stream_id, std::vector<StreamBuffer>* input_buffers,
    std::vector<std::unique_ptr<HalCameraMetadata>>* input_buffer_metadata,
    uint32_t payload_frames) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(stream_mutex_);

  if (!IsStreamAllocatedLocked(stream_id)) {
    ALOGE("%s: Stream %d was not allocated.", __FUNCTION__, stream_id);
    return BAD_VALUE;
  }

  int32_t owner_stream_id = GetBufferManagerOwnerIdLocked(stream_id);
  if (owner_stream_id == kInvalidStreamId) {
    ALOGE("%s: Cannot find a owner stream ID for stream %d", __FUNCTION__,
          stream_id);
    return BAD_VALUE;
  }

  if (input_buffers == nullptr || input_buffer_metadata == nullptr) {
    ALOGE("%s: input_buffers (%p) or input_buffer_metadata (%p) is nullptr",
          __FUNCTION__, input_buffers, input_buffer_metadata);
    return BAD_VALUE;
  }

  std::vector<ZslBufferManager::ZslBuffer> filled_buffers;
  buffer_managers_[owner_stream_id]->GetMostRecentZslBuffers(
      &filled_buffers, payload_frames, kMinFilledBuffers);

  if (filled_buffers.size() == 0) {
    ALOGE("%s: There is no input buffers.", __FUNCTION__);
    return INVALID_OPERATION;
  }

  // TODO(b/138592133): Remove AddPendingBuffers because internal stream manager
  // should not be responsible for saving the pending buffers' metadata.
  buffer_managers_[owner_stream_id]->AddPendingBuffers(filled_buffers);

  for (uint32_t i = 0; i < filled_buffers.size(); i++) {
    StreamBuffer buffer = {};
    buffer.stream_id = stream_id;
    buffer.buffer_id = 0;  // Buffer ID should be irrelevant internally in HAL.
    buffer.status = BufferStatus::kOk;
    buffer.acquire_fence = nullptr;
    buffer.release_fence = nullptr;
    buffer.buffer = filled_buffers[i].buffer.buffer;
    input_buffers->push_back(buffer);
    if (filled_buffers[i].metadata == nullptr) {
      std::vector<ZslBufferManager::ZslBuffer> buffers;
      buffer_managers_[owner_stream_id]->CleanPendingBuffers(&buffers);
      buffer_managers_[owner_stream_id]->ReturnZslBuffers(std::move(buffers));
      return INVALID_OPERATION;
    }
    input_buffer_metadata->push_back(std::move(filled_buffers[i].metadata));
  }

  return OK;
}

status_t InternalStreamManager::ReturnZslStreamBuffers(uint32_t frame_number,
                                                       int32_t stream_id) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(stream_mutex_);

  if (!IsStreamAllocatedLocked(stream_id)) {
    ALOGE("%s: Unknown stream ID %d.", __FUNCTION__, stream_id);
    return BAD_VALUE;
  }

  int32_t owner_stream_id = GetBufferManagerOwnerIdLocked(stream_id);
  if (owner_stream_id == kInvalidStreamId) {
    ALOGE("%s: Cannot find a owner stream ID for stream %d", __FUNCTION__,
          stream_id);
    return BAD_VALUE;
  }

  std::vector<ZslBufferManager::ZslBuffer> zsl_buffers;
  status_t res =
      buffer_managers_[owner_stream_id]->CleanPendingBuffers(&zsl_buffers);
  if (res != OK) {
    ALOGE("%s: frame (%d)fail to return zsl stream buffers", __FUNCTION__,
          frame_number);
    return res;
  }
  buffer_managers_[owner_stream_id]->ReturnZslBuffers(std::move(zsl_buffers));

  return OK;
}

status_t InternalStreamManager::ReturnStreamBuffer(const StreamBuffer& buffer) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(stream_mutex_);
  int32_t stream_id = buffer.stream_id;

  if (!IsStreamAllocatedLocked(stream_id)) {
    ALOGE("%s: Unknown stream ID %d.", __FUNCTION__, stream_id);
    return BAD_VALUE;
  }

  int32_t owner_stream_id = GetBufferManagerOwnerIdLocked(stream_id);
  if (owner_stream_id == kInvalidStreamId) {
    ALOGE("%s: Cannot find a owner stream ID for stream %d", __FUNCTION__,
          stream_id);
    return BAD_VALUE;
  }

  return buffer_managers_[owner_stream_id]->ReturnEmptyBuffer(buffer.buffer);
}

status_t InternalStreamManager::ReturnFilledBuffer(uint32_t frame_number,
                                                   const StreamBuffer& buffer) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(stream_mutex_);
  int32_t stream_id = buffer.stream_id;

  if (!IsStreamAllocatedLocked(stream_id)) {
    ALOGE("%s: Unknown stream ID %d.", __FUNCTION__, stream_id);
    return BAD_VALUE;
  }

  int32_t owner_stream_id = GetBufferManagerOwnerIdLocked(stream_id);
  if (owner_stream_id == kInvalidStreamId) {
    ALOGE("%s: Cannot find a owner stream ID for stream %d", __FUNCTION__,
          stream_id);
    return BAD_VALUE;
  }

  return buffer_managers_[owner_stream_id]->ReturnFilledBuffer(frame_number,
                                                               buffer);
}

status_t InternalStreamManager::ReturnMetadata(
    int32_t stream_id, uint32_t frame_number,
    const HalCameraMetadata* metadata) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(stream_mutex_);

  if (!IsStreamAllocatedLocked(stream_id)) {
    ALOGE("%s: Unknown stream ID %d.", __FUNCTION__, stream_id);
    return BAD_VALUE;
  }

  int32_t owner_stream_id = GetBufferManagerOwnerIdLocked(stream_id);
  if (owner_stream_id == kInvalidStreamId) {
    ALOGE("%s: Cannot find a owner stream ID for stream %d", __FUNCTION__,
          stream_id);
    return BAD_VALUE;
  }

  return buffer_managers_[owner_stream_id]->ReturnMetadata(frame_number,
                                                           metadata);
}

}  // namespace google_camera_hal
}  // namespace android