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
#define LOG_TAG "GCH_ResultDispatcher"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include <inttypes.h>

#include "result_dispatcher.h"
#include "utils.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<ResultDispatcher> ResultDispatcher::Create(
    uint32_t partial_result_count,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify) {
  ATRACE_CALL();
  auto dispatcher = std::unique_ptr<ResultDispatcher>(new ResultDispatcher(
      partial_result_count, process_capture_result, notify));
  if (dispatcher == nullptr) {
    ALOGE("%s: Creating ResultDispatcher failed.", __FUNCTION__);
    return nullptr;
  }

  return dispatcher;
}

ResultDispatcher::ResultDispatcher(
    uint32_t partial_result_count,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify)
    : kPartialResultCount(partial_result_count),
      process_capture_result_(process_capture_result),
      notify_(notify) {
  ATRACE_CALL();
  notify_callback_thread_ =
      std::thread([this] { this->NotifyCallbackThreadLoop(); });

  if (utils::SupportRealtimeThread()) {
    status_t res =
        utils::SetRealtimeThread(notify_callback_thread_.native_handle());
    if (res != OK) {
      ALOGE("%s: SetRealtimeThread fail", __FUNCTION__);
    } else {
      ALOGI("%s: SetRealtimeThread OK", __FUNCTION__);
    }
  }
}

ResultDispatcher::~ResultDispatcher() {
  ATRACE_CALL();
  {
    std::unique_lock<std::mutex> lock(notify_callback_lock);
    notify_callback_thread_exiting = true;
  }

  notify_callback_condition_.notify_one();
  notify_callback_thread_.join();
}

void ResultDispatcher::RemovePendingRequest(uint32_t frame_number) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(result_lock_);
  RemovePendingRequestLocked(frame_number);
}

status_t ResultDispatcher::AddPendingRequest(
    const CaptureRequest& pending_request) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(result_lock_);

  status_t res = AddPendingRequestLocked(pending_request);
  if (res != OK) {
    ALOGE("%s: Adding a pending request failed: %s(%d).", __FUNCTION__,
          strerror(-res), res);
    RemovePendingRequestLocked(pending_request.frame_number);
    return res;
  }

  return OK;
}

status_t ResultDispatcher::AddPendingRequestLocked(
    const CaptureRequest& pending_request) {
  ATRACE_CALL();
  uint32_t frame_number = pending_request.frame_number;

  status_t res = AddPendingShutterLocked(frame_number);
  if (res != OK) {
    ALOGE("%s: Adding pending shutter for frame %u failed: %s(%d)",
          __FUNCTION__, frame_number, strerror(-res), res);
    return res;
  }

  res = AddPendingFinalResultMetadataLocked(frame_number);
  if (res != OK) {
    ALOGE("%s: Adding pending result metadata for frame %u failed: %s(%d)",
          __FUNCTION__, frame_number, strerror(-res), res);
    return res;
  }

  for (auto& buffer : pending_request.input_buffers) {
    res = AddPendingBufferLocked(frame_number, buffer, /*is_input=*/true);
    if (res != OK) {
      ALOGE("%s: Adding pending input buffer for frame %u failed: %s(%d)",
            __FUNCTION__, frame_number, strerror(-res), res);
      return res;
    }
  }

  for (auto& buffer : pending_request.output_buffers) {
    res = AddPendingBufferLocked(frame_number, buffer, /*is_input=*/false);
    if (res != OK) {
      ALOGE("%s: Adding pending output buffer for frame %u failed: %s(%d)",
            __FUNCTION__, frame_number, strerror(-res), res);
      return res;
    }
  }

  return OK;
}

status_t ResultDispatcher::AddPendingShutterLocked(uint32_t frame_number) {
  ATRACE_CALL();
  if (pending_shutters_.find(frame_number) != pending_shutters_.end()) {
    ALOGE("%s: Pending shutter for frame %u already exists.", __FUNCTION__,
          frame_number);
    return ALREADY_EXISTS;
  }

  pending_shutters_[frame_number] = PendingShutter();
  return OK;
}

status_t ResultDispatcher::AddPendingFinalResultMetadataLocked(
    uint32_t frame_number) {
  ATRACE_CALL();
  if (pending_final_metadata_.find(frame_number) !=
      pending_final_metadata_.end()) {
    ALOGE("%s: Pending final result metadata for frame %u already exists.",
          __FUNCTION__, frame_number);
    return ALREADY_EXISTS;
  }

  pending_final_metadata_[frame_number] = PendingFinalResultMetadata();
  return OK;
}

status_t ResultDispatcher::AddPendingBufferLocked(uint32_t frame_number,
                                                  const StreamBuffer& buffer,
                                                  bool is_input) {
  ATRACE_CALL();
  uint32_t stream_id = buffer.stream_id;
  if (stream_pending_buffers_map_.find(stream_id) ==
      stream_pending_buffers_map_.end()) {
    stream_pending_buffers_map_[stream_id] = std::map<uint32_t, PendingBuffer>();
  }

  if (stream_pending_buffers_map_[stream_id].find(frame_number) !=
      stream_pending_buffers_map_[stream_id].end()) {
    ALOGE("%s: Pending buffer of stream %u for frame %u already exists.",
          __FUNCTION__, stream_id, frame_number);
    return ALREADY_EXISTS;
  }

  PendingBuffer pending_buffer = {.is_input = is_input};
  stream_pending_buffers_map_[stream_id][frame_number] = pending_buffer;
  return OK;
}

void ResultDispatcher::RemovePendingRequestLocked(uint32_t frame_number) {
  ATRACE_CALL();
  pending_shutters_.erase(frame_number);
  pending_final_metadata_.erase(frame_number);

  for (auto& pending_buffers : stream_pending_buffers_map_) {
    pending_buffers.second.erase(frame_number);
  }
}

status_t ResultDispatcher::AddResult(std::unique_ptr<CaptureResult> result) {
  ATRACE_CALL();
  status_t res;
  bool failed = false;
  uint32_t frame_number = result->frame_number;

  if (result->result_metadata != nullptr) {
    res = AddResultMetadata(frame_number, std::move(result->result_metadata),
                            std::move(result->physical_metadata),
                            result->partial_result);
    if (res != OK) {
      ALOGE("%s: Adding result metadata failed: %s (%d)", __FUNCTION__,
            strerror(-res), res);
      failed = true;
    }
  }

  for (auto& buffer : result->output_buffers) {
    res = AddBuffer(frame_number, buffer);
    if (res != OK) {
      ALOGE("%s: Adding an output buffer failed: %s (%d)", __FUNCTION__,
            strerror(-res), res);
      failed = true;
    }
  }

  for (auto& buffer : result->input_buffers) {
    res = AddBuffer(frame_number, buffer);
    if (res != OK) {
      ALOGE("%s: Adding an input buffer failed: %s (%d)", __FUNCTION__,
            strerror(-res), res);
      failed = true;
    }
  }

  notify_callback_condition_.notify_one();
  return failed ? UNKNOWN_ERROR : OK;
}

status_t ResultDispatcher::AddShutter(uint32_t frame_number,
                                      int64_t timestamp_ns) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(result_lock_);

  auto shutter_it = pending_shutters_.find(frame_number);
  if (shutter_it == pending_shutters_.end()) {
    ALOGE("%s: Cannot find the pending shutter for frame %u", __FUNCTION__,
          frame_number);
    return NAME_NOT_FOUND;
  }

  if (shutter_it->second.ready) {
    ALOGE("%s: Already received shutter (%" PRId64
          ") for frame %u. New "
          "timestamp %" PRId64,
          __FUNCTION__, shutter_it->second.timestamp_ns, frame_number,
          timestamp_ns);
    return ALREADY_EXISTS;
  }

  shutter_it->second.timestamp_ns = timestamp_ns;
  shutter_it->second.ready = true;

  notify_callback_condition_.notify_one();
  return OK;
}

status_t ResultDispatcher::AddError(const ErrorMessage& error) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(result_lock_);
  uint32_t frame_number = error.frame_number;
  // No need to deliver the shutter message on an error
  pending_shutters_.erase(frame_number);
  // No need to deliver the result metadata on a result metadata error
  if (error.error_code == ErrorCode::kErrorResult) {
    pending_final_metadata_.erase(frame_number);
  }

  NotifyMessage message = {.type = MessageType::kError, .message.error = error};
  ALOGD("%s: Notify error %u for frame %u stream %d", __FUNCTION__,
        error.error_code, frame_number, error.error_stream_id);
  notify_(message);

  return OK;
}

void ResultDispatcher::NotifyResultMetadata(
    uint32_t frame_number, std::unique_ptr<HalCameraMetadata> metadata,
    std::vector<PhysicalCameraMetadata> physical_metadata,
    uint32_t partial_result) {
  ATRACE_CALL();
  auto result = std::make_unique<CaptureResult>(CaptureResult({}));
  result->frame_number = frame_number;
  result->result_metadata = std::move(metadata);
  result->physical_metadata = std::move(physical_metadata);
  result->partial_result = partial_result;

  process_capture_result_(std::move(result));
}

status_t ResultDispatcher::AddFinalResultMetadata(
    uint32_t frame_number, std::unique_ptr<HalCameraMetadata> final_metadata,
    std::vector<PhysicalCameraMetadata> physical_metadata) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(result_lock_);

  auto metadata_it = pending_final_metadata_.find(frame_number);
  if (metadata_it == pending_final_metadata_.end()) {
    ALOGE("%s: Cannot find the pending result metadata for frame %u",
          __FUNCTION__, frame_number);
    return NAME_NOT_FOUND;
  }

  if (metadata_it->second.ready) {
    ALOGE("%s: Already received final result metadata for frame %u.",
          __FUNCTION__, frame_number);
    return ALREADY_EXISTS;
  }

  metadata_it->second.metadata = std::move(final_metadata);
  metadata_it->second.physical_metadata = std::move(physical_metadata);
  metadata_it->second.ready = true;
  return OK;
}

status_t ResultDispatcher::AddResultMetadata(
    uint32_t frame_number, std::unique_ptr<HalCameraMetadata> metadata,
    std::vector<PhysicalCameraMetadata> physical_metadata,
    uint32_t partial_result) {
  ATRACE_CALL();
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  if (partial_result > kPartialResultCount) {
    ALOGE("%s: partial_result %u cannot be larger than partial result count %u",
          __FUNCTION__, partial_result, kPartialResultCount);
    return BAD_VALUE;
  }

  if (partial_result < kPartialResultCount) {
    // Send out partial results immediately.
    NotifyResultMetadata(frame_number, std::move(metadata),
                         std::move(physical_metadata), partial_result);
    return OK;
  }

  return AddFinalResultMetadata(frame_number, std::move(metadata),
                                std::move(physical_metadata));
}

status_t ResultDispatcher::AddBuffer(uint32_t frame_number,
                                     StreamBuffer buffer) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(result_lock_);

  uint32_t stream_id = buffer.stream_id;
  auto pending_buffers_it = stream_pending_buffers_map_.find(stream_id);
  if (pending_buffers_it == stream_pending_buffers_map_.end()) {
    ALOGE("%s: Cannot find the pending buffer for stream %u", __FUNCTION__,
          stream_id);
    return NAME_NOT_FOUND;
  }

  auto pending_buffer_it = pending_buffers_it->second.find(frame_number);
  if (pending_buffer_it == pending_buffers_it->second.end()) {
    ALOGE("%s: Cannot find the pending buffer for stream %u for frame %u",
          __FUNCTION__, stream_id, frame_number);
    return NAME_NOT_FOUND;
  }

  if (pending_buffer_it->second.ready) {
    ALOGE("%s: Already received a buffer for stream %u for frame %u",
          __FUNCTION__, stream_id, frame_number);
    return ALREADY_EXISTS;
  }

  pending_buffer_it->second.buffer = std::move(buffer);
  pending_buffer_it->second.ready = true;

  return OK;
}

void ResultDispatcher::NotifyCallbackThreadLoop() {
  while (1) {
    NotifyShutters();
    NotifyFinalResultMetadata();
    NotifyBuffers();

    std::unique_lock<std::mutex> lock(notify_callback_lock);
    if (notify_callback_thread_exiting) {
      ALOGV("%s: NotifyCallbackThreadLoop exits.", __FUNCTION__);
      return;
    }

    if (notify_callback_condition_.wait_for(
            lock, std::chrono::milliseconds(kCallbackThreadTimeoutMs)) ==
        std::cv_status::timeout) {
      PrintTimeoutMessages();
    }
  }
}

void ResultDispatcher::PrintTimeoutMessages() {
  std::lock_guard<std::mutex> lock(result_lock_);
  for (auto& [frame_number, shutter] : pending_shutters_) {
    ALOGW("%s: pending shutter for frame %u ready %d", __FUNCTION__,
          frame_number, shutter.ready);
  }

  for (auto& [frame_number, final_metadata] : pending_final_metadata_) {
    ALOGW("%s: pending final result metadaata for frame %u ready %d",
          __FUNCTION__, frame_number, final_metadata.ready);
  }

  for (auto& [stream_id, pending_buffers] : stream_pending_buffers_map_) {
    for (auto& [frame_number, pending_buffer] : pending_buffers) {
      ALOGW("%s: pending buffer of stream %d for frame %u ready %d",
            __FUNCTION__, stream_id, frame_number, pending_buffer.ready);
    }
  }
}

status_t ResultDispatcher::GetReadyShutterMessage(NotifyMessage* message) {
  ATRACE_CALL();
  if (message == nullptr) {
    ALOGE("%s: message is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(result_lock_);

  auto shutter_it = pending_shutters_.begin();
  if (shutter_it == pending_shutters_.end() || !shutter_it->second.ready) {
    // The first pending shutter is not ready.
    return NAME_NOT_FOUND;
  }

  message->type = MessageType::kShutter;
  message->message.shutter.frame_number = shutter_it->first;
  message->message.shutter.timestamp_ns = shutter_it->second.timestamp_ns;
  pending_shutters_.erase(shutter_it);

  return OK;
}

void ResultDispatcher::NotifyShutters() {
  ATRACE_CALL();
  NotifyMessage message = {};

  while (GetReadyShutterMessage(&message) == OK) {
    ALOGV("%s: Notify shutter for frame %u timestamp %" PRIu64, __FUNCTION__,
          message.message.shutter.frame_number,
          message.message.shutter.timestamp_ns);
    notify_(message);
  }
}

status_t ResultDispatcher::GetReadyFinalMetadata(
    uint32_t* frame_number, std::unique_ptr<HalCameraMetadata>* final_metadata,
    std::vector<PhysicalCameraMetadata>* physical_metadata) {
  ATRACE_CALL();
  if (final_metadata == nullptr || frame_number == nullptr) {
    ALOGE("%s: final_metadata (%p) or frame_number (%p) is nullptr",
          __FUNCTION__, final_metadata, frame_number);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(result_lock_);

  auto final_metadata_it = pending_final_metadata_.begin();
  if (final_metadata_it == pending_final_metadata_.end() ||
      !final_metadata_it->second.ready) {
    // The first pending final metadata is not ready.
    return NAME_NOT_FOUND;
  }

  *frame_number = final_metadata_it->first;
  *final_metadata = std::move(final_metadata_it->second.metadata);
  *physical_metadata = std::move(final_metadata_it->second.physical_metadata);
  pending_final_metadata_.erase(final_metadata_it);

  return OK;
}

void ResultDispatcher::NotifyFinalResultMetadata() {
  ATRACE_CALL();
  uint32_t frame_number;
  std::unique_ptr<HalCameraMetadata> final_metadata;
  std::vector<PhysicalCameraMetadata> physical_metadata;

  while (GetReadyFinalMetadata(&frame_number, &final_metadata,
                               &physical_metadata) == OK) {
    ALOGV("%s: Notify final metadata for frame %u", __FUNCTION__, frame_number);
    NotifyResultMetadata(frame_number, std::move(final_metadata),
                         std::move(physical_metadata), kPartialResultCount);
  }
}

status_t ResultDispatcher::GetReadyBufferResult(
    std::unique_ptr<CaptureResult>* result) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(result_lock_);
  if (result == nullptr) {
    ALOGE("%s: result is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  *result = nullptr;

  for (auto& pending_buffers : stream_pending_buffers_map_) {
    auto buffer_it = pending_buffers.second.begin();
    while (buffer_it != pending_buffers.second.end()) {
      if (!buffer_it->second.ready) {
        // No more buffer ready.
        break;
      }

      auto buffer_result = std::make_unique<CaptureResult>(CaptureResult({}));

      buffer_result->frame_number = buffer_it->first;
      if (buffer_it->second.is_input) {
        buffer_result->input_buffers.push_back(buffer_it->second.buffer);
      } else {
        buffer_result->output_buffers.push_back(buffer_it->second.buffer);
      }

      pending_buffers.second.erase(buffer_it);
      *result = std::move(buffer_result);
      return OK;
    }
  }

  return NAME_NOT_FOUND;
}

void ResultDispatcher::NotifyBuffers() {
  ATRACE_CALL();
  std::unique_ptr<CaptureResult> result;

  while (GetReadyBufferResult(&result) == OK) {
    if (result == nullptr) {
      ALOGE("%s: result is nullptr", __FUNCTION__);
      return;
    }
    process_capture_result_(std::move(result));
  }
}

}  // namespace google_camera_hal
}  // namespace android