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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_INTERNAL_STREAM_MANAGER_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_INTERNAL_STREAM_MANAGER_H_

#include <hardware/gralloc.h>
#include <utils/Errors.h>
#include <unordered_map>

#include "camera_buffer_allocator_hwl.h"
#include "hal_buffer_allocator.h"
#include "hal_types.h"
#include "hwl_buffer_allocator.h"
#include "zsl_buffer_manager.h"

namespace android {
namespace google_camera_hal {

// InternalStreamManager manages internal streams. It can be used to
// create internal streams and allocate internal stream buffers.
class InternalStreamManager {
 public:
  static std::unique_ptr<InternalStreamManager> Create(
      IHalBufferAllocator* buffer_allocator = nullptr);
  virtual ~InternalStreamManager() = default;

  // stream contains the stream info to be registered. if stream.id is smaller
  // than kStreamIdReserve, stream_id will be ignored and will be filled with
  // a unique stream ID.
  status_t RegisterNewInternalStream(const Stream& stream, int32_t* stream_id);

  // Allocate buffers for a stream.
  // hal_stream is the HAL configured stream. It will be combined with the
  // stream information (set via RegisterNewInternalStream) to allocate buffers.
  // This method will allocate hal_stream.max_buffers immediately and at most
  // (hal_stream.max_buffers + additional_num_buffers) buffers.
  // If need_vendor_buffer is true, the external buffer allocator must be passed
  // in when create the internal stream manager in create() function.
  status_t AllocateBuffers(const HalStream& hal_stream,
                           uint32_t additional_num_buffers = 0,
                           bool need_vendor_buffer = false);

  // Allocate shared buffers for streams.
  // hal_streams are the HAL configured streams. They will be combined with the
  // stream information (set via RegisterNewInternalStream) to allocate buffers.
  // This method will allocate the maximum of all hal_stream.max_buffers
  // immediately and at most (total of hal_stream.max_buffers +
  // additional_num_buffers).
  // If need_vendor_buffer is true, the external buffer allocator must be passed
  // in when create the internal stream manager in create() function.
  status_t AllocateSharedBuffers(const std::vector<HalStream>& hal_streams,
                                 uint32_t additional_num_buffers = 0,
                                 bool need_vendor_buffer = false);

  // Free a stream and its stream buffers.
  void FreeStream(int32_t stream_id);

  // Get a stream buffer from internal stream manager.
  status_t GetStreamBuffer(int32_t stream_id, StreamBuffer* buffer);

  // Return a stream buffer to internal stream manager.
  status_t ReturnStreamBuffer(const StreamBuffer& buffer);

  // Return a stream buffer with frame number to internal stream manager.
  status_t ReturnFilledBuffer(uint32_t frame_number, const StreamBuffer& buffer);

  // Return a metadata to internal stream manager.
  status_t ReturnMetadata(int32_t stream_id, uint32_t frame_number,
                          const HalCameraMetadata* metadata);

  // Get the most recent buffer and metadata.
  status_t GetMostRecentStreamBuffer(
      int32_t stream_id, std::vector<StreamBuffer>* input_buffers,
      std::vector<std::unique_ptr<HalCameraMetadata>>* input_buffer_metadata,
      uint32_t payload_frames);

  // Return the buffer from GetMostRecentStreamBuffer
  status_t ReturnZslStreamBuffers(uint32_t frame_number, int32_t stream_id);

  // Check the pending buffer is empty or not
  bool IsPendingBufferEmpty(int32_t stream_id);

 private:
  static constexpr int32_t kMinFilledBuffers = 3;
  static constexpr int32_t kStreamIdStart = kHalInternalStreamStart;
  static constexpr int32_t kStreamIdReserve =
      kImplementationDefinedInternalStreamStart;
  static constexpr int32_t kInvalidStreamId = -1;

  // Initialize internal stream manager
  void Initialize(IHalBufferAllocator* buffer_allocator);

  // Return if a stream is registered. Must be called with stream_mutex_ locked.
  status_t IsStreamRegisteredLocked(int32_t stream_id) const;

  // Return if a stream is allocated. Must be called with stream_mutex_ locked.
  status_t IsStreamAllocatedLocked(int32_t stream_id) const;

  // Get a buffer descriptor by combining stream and hal stream.
  status_t GetBufferDescriptor(const Stream& stream, const HalStream& hal_stream,
                               uint32_t additional_num_buffers,
                               HalBufferDescriptor* buffer_descriptor);

  // Get the stream ID of the owner of stream_id's buffer manager. Protected by
  // stream_mutex_.
  int32_t GetBufferManagerOwnerIdLocked(int32_t stream_id);

  // Return if two streams and hal_streams are compatible and can share buffers.
  bool AreStreamsCompatible(const Stream& stream_0,
                            const HalStream& hal_stream_0,
                            const Stream& stream_1,
                            const HalStream& hal_stream_1) const;

  // Return if all hal_streams can share buffers. Protected by stream_mutex_.
  bool CanHalStreamsShareBuffersLocked(
      const std::vector<HalStream>& hal_streams) const;

  // Find a new owner for the buffer manager that old_owner_stream_id owns and
  // remove the old stream. A new owner is one of the streams that share the
  // same buffer manager. If a new owner cannot be found, the buffer manager
  // will be destroyed. Protected by stream_mutex_.
  status_t RemoveOwnerStreamIdLocked(int32_t old_owner_stream_id);

  // Allocate buffers. Protected by stream_mutex_.
  status_t AllocateBuffersLocked(const HalStream& hal_stream,
                                 uint32_t additional_num_buffers,
                                 bool need_vendor_buffer);

  std::mutex stream_mutex_;

  // Next available stream ID. Protected by stream_mutex_.
  int32_t next_available_stream_id_ = kStreamIdStart;

  // Map from stream ID to registered stream. Protected by stream_mutex_.
  std::unordered_map<int32_t, Stream> registered_streams_;

  // Map from stream ID to its buffer manager owner's stream ID.
  // For example, if shared_stream_owner_ids_[A] == B, stream A and stream B
  // share the same buffer manager and stream B is the owner.
  std::unordered_map<int32_t, int32_t> shared_stream_owner_ids_;

  // Map from stream ID to ZSL buffer manager it owns. If a stream doesn't own
  // a buffer manager, the owner stream can be looked up with
  // shared_stream_owner_ids_. Protected by stream_mutex_.
  std::unordered_map<int32_t, std::unique_ptr<ZslBufferManager>> buffer_managers_;

  // external buffer allocator
  IHalBufferAllocator* hwl_buffer_allocator_ = nullptr;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_INTERNAL_STREAM_MANAGER_H_
