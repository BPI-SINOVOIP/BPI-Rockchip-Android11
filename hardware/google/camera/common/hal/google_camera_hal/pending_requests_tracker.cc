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
#define LOG_TAG "GCH_PendingRequestsTracker"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include "pending_requests_tracker.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<PendingRequestsTracker> PendingRequestsTracker::Create(
    const std::vector<HalStream>& hal_configured_streams) {
  auto tracker =
      std::unique_ptr<PendingRequestsTracker>(new PendingRequestsTracker());
  if (tracker == nullptr) {
    ALOGE("%s: Failed to create PendingRequestsTracker", __FUNCTION__);
    return nullptr;
  }

  status_t res = tracker->Initialize(hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Initializing stream buffer tracker failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return nullptr;
  }

  return tracker;
}

status_t PendingRequestsTracker::Initialize(
    const std::vector<HalStream>& hal_configured_streams) {
  for (auto& hal_stream : hal_configured_streams) {
    auto [max_buffer_it, max_buffer_inserted] =
        stream_max_buffers_.emplace(hal_stream.id, hal_stream.max_buffers);
    if (!max_buffer_inserted) {
      ALOGE("%s: There are duplicated stream id %d", __FUNCTION__,
            hal_stream.id);
      return BAD_VALUE;
    }

    stream_pending_buffers_.emplace(hal_stream.id, /*pending_buffers=*/0);
    stream_acquired_buffers_.emplace(hal_stream.id, /*pending_buffers=*/0);
  }

  return OK;
}

bool PendingRequestsTracker::IsStreamConfigured(int32_t stream_id) const {
  return stream_max_buffers_.find(stream_id) != stream_max_buffers_.end();
}

void PendingRequestsTracker::TrackRequestBuffersLocked(
    const std::vector<StreamBuffer>& requested_buffers) {
  ATRACE_CALL();

  for (auto& buffer : requested_buffers) {
    int32_t stream_id = buffer.stream_id;
    if (!IsStreamConfigured(stream_id)) {
      ALOGW("%s: stream %d was not configured.", __FUNCTION__, stream_id);
      // Continue to track other buffers.
      continue;
    }

    stream_pending_buffers_[stream_id]++;
  }
}

status_t PendingRequestsTracker::TrackReturnedResultBuffers(
    const std::vector<StreamBuffer>& returned_buffers) {
  ATRACE_CALL();

  {
    std::lock_guard<std::mutex> lock(pending_requests_mutex_);
    for (auto& buffer : returned_buffers) {
      int32_t stream_id = buffer.stream_id;
      if (!IsStreamConfigured(stream_id)) {
        ALOGW("%s: stream %d was not configured.", __FUNCTION__, stream_id);
        // Continue to track other buffers.
        continue;
      }

      if (stream_pending_buffers_[stream_id] == 0) {
        ALOGE("%s: stream %d should not have any pending quota buffers.",
              __FUNCTION__, stream_id);
        // Continue to track other buffers.
        continue;
      }

      stream_pending_buffers_[stream_id]--;
    }
  }

  tracker_request_condition_.notify_one();
  return OK;
}

status_t PendingRequestsTracker::TrackReturnedAcquiredBuffers(
    const std::vector<StreamBuffer>& returned_buffers) {
  ATRACE_CALL();

  {
    std::lock_guard<std::mutex> lock(pending_acquisition_mutex_);
    for (auto& buffer : returned_buffers) {
      int32_t stream_id = buffer.stream_id;
      if (!IsStreamConfigured(stream_id)) {
        ALOGW("%s: stream %d was not configured.", __FUNCTION__, stream_id);
        // Continue to track other buffers.
        continue;
      }

      if (stream_acquired_buffers_[stream_id] == 0) {
        ALOGE("%s: stream %d should not have any pending acquired buffers.",
              __FUNCTION__, stream_id);
        // Continue to track other buffers.
        continue;
      }

      stream_acquired_buffers_[stream_id]--;
    }
  }

  tracker_acquisition_condition_.notify_one();
  return OK;
}

bool PendingRequestsTracker::DoStreamsHaveEnoughBuffersLocked(
    const std::vector<StreamBuffer>& buffers) const {
  for (auto& buffer : buffers) {
    int32_t stream_id = buffer.stream_id;
    if (!IsStreamConfigured(stream_id)) {
      ALOGE("%s: stream %d was not configured.", __FUNCTION__, stream_id);
      return false;
    }

    if (stream_pending_buffers_.at(stream_id) >=
        stream_max_buffers_.at(stream_id)) {
      ALOGV("%s: stream %d is not ready. max_buffers=%u", __FUNCTION__,
            stream_id, stream_max_buffers_.at(stream_id));
      return false;
    }
  }

  return true;
}

bool PendingRequestsTracker::DoesStreamHaveEnoughBuffersToAcquireLocked(
    int32_t stream_id, uint32_t num_buffers) const {
  if (!IsStreamConfigured(stream_id)) {
    ALOGE("%s: stream %d was not configured.", __FUNCTION__, stream_id);
    return false;
  }

  if (stream_acquired_buffers_.at(stream_id) + num_buffers >
      stream_max_buffers_.at(stream_id)) {
    ALOGV("%s: stream %d is not ready. max_buffers=%u", __FUNCTION__, stream_id,
          stream_max_buffers_.at(stream_id));
    return false;
  }

  return true;
}

status_t PendingRequestsTracker::UpdateRequestedStreamIdsLocked(
    const std::vector<StreamBuffer>& requested_buffers,
    std::vector<int32_t>* first_requested_stream_ids) {
  if (first_requested_stream_ids == nullptr) {
    ALOGE("%s: first_requested_stream_ids is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  for (auto& buffer : requested_buffers) {
    int32_t stream_id = buffer.stream_id;
    auto stream_id_iter = requested_stream_ids_.find(stream_id);
    if (stream_id_iter == requested_stream_ids_.end()) {
      first_requested_stream_ids->push_back(stream_id);
      requested_stream_ids_.emplace(stream_id);
    }
  }

  return OK;
}

status_t PendingRequestsTracker::WaitAndTrackRequestBuffers(
    const CaptureRequest& request,
    std::vector<int32_t>* first_requested_stream_ids) {
  ATRACE_CALL();

  if (first_requested_stream_ids == nullptr) {
    ALOGE("%s: first_requested_stream_ids is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  std::unique_lock<std::mutex> lock(pending_requests_mutex_);
  if (!tracker_request_condition_.wait_for(
          lock, std::chrono::milliseconds(kTrackerTimeoutMs), [this, &request] {
            return DoStreamsHaveEnoughBuffersLocked(request.output_buffers);
          })) {
    ALOGE("%s: Waiting for buffer ready timed out.", __FUNCTION__);
    return TIMED_OUT;
  }

  ALOGV("%s: all streams are ready", __FUNCTION__);

  TrackRequestBuffersLocked(request.output_buffers);

  first_requested_stream_ids->clear();
  status_t res = UpdateRequestedStreamIdsLocked(request.output_buffers,
                                                first_requested_stream_ids);
  if (res != OK) {
    ALOGE("%s: Updating requested stream ID for output buffers failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  return OK;
}

status_t PendingRequestsTracker::WaitAndTrackAcquiredBuffers(
    int32_t stream_id, uint32_t num_buffers) {
  ATRACE_CALL();

  if (!IsStreamConfigured(stream_id)) {
    ALOGW("%s: stream %d was not configured.", __FUNCTION__, stream_id);
    // Continue to track other buffers.
    return BAD_VALUE;
  }

  std::unique_lock<std::mutex> lock(pending_acquisition_mutex_);
  if (!tracker_acquisition_condition_.wait_for(
          lock, std::chrono::milliseconds(kAcquireBufferTimeoutMs),
          [this, stream_id, num_buffers] {
            return DoesStreamHaveEnoughBuffersToAcquireLocked(stream_id,
                                                              num_buffers);
          })) {
    ALOGW("%s: Waiting to acquire buffer timed out.", __FUNCTION__);
    return TIMED_OUT;
  }

  stream_acquired_buffers_[stream_id] += num_buffers;

  return OK;
}

void PendingRequestsTracker::TrackBufferAcquisitionFailure(int32_t stream_id,
                                                           uint32_t num_buffers) {
  if (!IsStreamConfigured(stream_id)) {
    ALOGW("%s: stream %d was not configured.", __FUNCTION__, stream_id);
    // Continue to track other buffers.
    return;
  }

  std::unique_lock<std::mutex> lock(pending_acquisition_mutex_);
  stream_acquired_buffers_[stream_id] -= num_buffers;
}

}  // namespace google_camera_hal
}  // namespace android
