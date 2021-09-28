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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_PENDING_REQUESTS_TRACKER_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_PENDING_REQUESTS_TRACKER_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "hal_types.h"

namespace android {
namespace google_camera_hal {

// PendingRequestsTracker tracks pending requests and can be used to throttle
// capture requests so the number of stream buffers won't exceed its stream's
// max number of buffers.
class PendingRequestsTracker {
 public:
  static std::unique_ptr<PendingRequestsTracker> Create(
      const std::vector<HalStream>& hal_configured_streams);

  // Wait until the requested streams have enough buffers and track
  // the requested buffers.
  // first_requested_stream_ids will be filled with the stream IDs that
  // have not been requested previously.
  status_t WaitAndTrackRequestBuffers(
      const CaptureRequest& request,
      std::vector<int32_t>* first_requested_stream_ids);

  // Track buffers returned, which was counted at request arrival time
  status_t TrackReturnedResultBuffers(
      const std::vector<StreamBuffer>& returned_buffers);

  // Wait until the actually acquired buffers have drop below the max buffer
  // count and then release the lock to continue the work.
  status_t WaitAndTrackAcquiredBuffers(int32_t stream_id, uint32_t num_buffers);

  // Decrease from the tracker the amount of buffer added previously in
  // WaitAndTrackAcquiredBuffers but was not actually acquired due to buffer
  // acquisition failure.
  void TrackBufferAcquisitionFailure(int32_t stream_id, uint32_t num_buffers);

  // Track buffers returned, which was counted at buffer acquisition time
  status_t TrackReturnedAcquiredBuffers(
      const std::vector<StreamBuffer>& returned_buffers);

  virtual ~PendingRequestsTracker() = default;

 protected:
  PendingRequestsTracker() = default;

 private:
  // Duration to wait for stream buffers to be available.
  static constexpr uint32_t kTrackerTimeoutMs = 3000;

  // Duration to wait for when requesting buffer
  static constexpr uint32_t kAcquireBufferTimeoutMs = 50;

  // Initialize the tracker.
  status_t Initialize(const std::vector<HalStream>& hal_configured_streams);

  // Return if all the buffers' streams have enough buffers to be requested.
  // Must be protected with pending_requests_mutex_.
  bool DoStreamsHaveEnoughBuffersLocked(
      const std::vector<StreamBuffer>& buffers) const;

  // Return if the stream with stream_id have enough buffers to be requested.
  // Must be protected with pending_acquisition_mutex_.
  bool DoesStreamHaveEnoughBuffersToAcquireLocked(int32_t stream_id,
                                                  uint32_t num_buffers) const;

  // Update requested stream ID and return the stream IDs that have not been
  // requested previously in first_requested_stream_ids.
  // Must be protected with pending_requests_mutex_.
  status_t UpdateRequestedStreamIdsLocked(
      const std::vector<StreamBuffer>& requested_buffers,
      std::vector<int32_t>* first_requested_stream_ids);

  // Track buffers in capture requests.
  // Must be protected with pending_requests_mutex_.
  void TrackRequestBuffersLocked(
      const std::vector<StreamBuffer>& requested_buffers);

  // Return if a stream ID is configured when Create() was called.
  bool IsStreamConfigured(int32_t stream_id) const;

  // Map from stream ID to the stream's max number of buffers.
  std::unordered_map<int32_t, uint32_t> stream_max_buffers_;

  // Condition to signal when a buffer is returned to the client.
  std::condition_variable tracker_request_condition_;

  std::mutex pending_requests_mutex_;

  // Map from stream ID to the stream's number of pending buffers.
  // It must have an entry for keys present in stream_max_buffers_.
  // Must be protected with pending_requests_mutex_.
  std::unordered_map<int32_t, uint32_t> stream_pending_buffers_;

  // Condition to signal when a buffer is returned to the client through process
  // capture result or return stream buffer api.
  std::condition_variable tracker_acquisition_condition_;

  std::mutex pending_acquisition_mutex_;

  // Map from stream ID to the stream's number of actually acquired buffers.
  // It must have an entry for keys present in stream_max_buffers_.
  // Must be protected with pending_acquisition_mutex_.
  std::unordered_map<int32_t, uint32_t> stream_acquired_buffers_;

  // Contains the stream IDs that have been requested previously.
  // Must be protected with pending_requests_mutex_.
  std::unordered_set<int32_t> requested_stream_ids_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_PENDING_REQUESTS_TRACKER_H_
