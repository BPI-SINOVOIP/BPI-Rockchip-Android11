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
#define LOG_TAG "GCH_RgbirdDepthResultProcessor"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include <inttypes.h>

#include "hal_utils.h"
#include "rgbird_depth_result_processor.h"

namespace android {
namespace google_camera_hal {
std::unique_ptr<RgbirdDepthResultProcessor> RgbirdDepthResultProcessor::Create(
    InternalStreamManager* internal_stream_manager) {
  if (internal_stream_manager == nullptr) {
    ALOGE("%s: internal_stream_manager is null.", __FUNCTION__);
    return nullptr;
  }

  auto result_processor = std::unique_ptr<RgbirdDepthResultProcessor>(
      new RgbirdDepthResultProcessor(internal_stream_manager));
  if (result_processor == nullptr) {
    ALOGE("%s: Failed to create RgbirdDepthResultProcessor.", __FUNCTION__);
    return nullptr;
  }

  return result_processor;
}

RgbirdDepthResultProcessor::RgbirdDepthResultProcessor(
    InternalStreamManager* internal_stream_manager)
    : internal_stream_manager_(internal_stream_manager) {
}

void RgbirdDepthResultProcessor::SetResultCallback(
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify) {
  std::lock_guard<std::mutex> lock(callback_lock_);
  process_capture_result_ = process_capture_result;
  notify_ = notify;
}

status_t RgbirdDepthResultProcessor::AddPendingRequests(
    const std::vector<ProcessBlockRequest>& process_block_requests,
    const CaptureRequest& remaining_session_request) {
  ATRACE_CALL();
  // This is the last result processor. Sanity check if requests contains
  // all remaining output buffers.
  if (!hal_utils::AreAllRemainingBuffersRequested(process_block_requests,
                                                  remaining_session_request)) {
    ALOGE("%s: Some output buffers will not be completed.", __FUNCTION__);
    return BAD_VALUE;
  }

  return OK;
}

void RgbirdDepthResultProcessor::ProcessResult(ProcessBlockResult block_result) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(callback_lock_);
  std::unique_ptr<CaptureResult> result = std::move(block_result.result);
  if (result == nullptr) {
    ALOGW("%s: block_result has a null result.", __FUNCTION__);
    return;
  }

  if (process_capture_result_ == nullptr) {
    ALOGE("%s: process_capture_result_ is null, dropping a result.",
          __FUNCTION__);
    return;
  }

  // Depth Process Block should not return result metadata
  if (result->result_metadata != nullptr) {
    ALOGE("%s: non-null result metadata received from the depth process block",
          __FUNCTION__);
    return;
  }

  // Depth Process Block only returns depth stream buffer, so recycle any input
  // buffers to internal stream manager and forward the depth buffer to the
  // framework right away.
  for (auto& buffer : result->input_buffers) {
    // If the stream id is invalid. The input buffer is only a place holder
    // corresponding to the input buffer metadata for the rgb pipeline.
    if (buffer.stream_id == kInvalidStreamId) {
      continue;
    }

    status_t res = internal_stream_manager_->ReturnStreamBuffer(buffer);
    if (res != OK) {
      ALOGE(
          "%s: Failed to returned internal buffer[buffer_handle:%p, "
          "stream_id:%d, buffer_id%" PRIu64 "].",
          __FUNCTION__, buffer.buffer, buffer.stream_id, buffer.buffer_id);
    } else {
      ALOGV(
          "%s: Successfully returned internal buffer[buffer_handle:%p, "
          "stream_id:%d, buffer_id%" PRIu64 "].",
          __FUNCTION__, buffer.buffer, buffer.stream_id, buffer.buffer_id);
    }
  }
  result->input_buffers.clear();

  process_capture_result_(std::move(result));
}

void RgbirdDepthResultProcessor::Notify(
    const ProcessBlockNotifyMessage& block_message) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(callback_lock_);
  const NotifyMessage& message = block_message.message;
  if (notify_ == nullptr) {
    ALOGE("%s: notify_ is null, dropping a message", __FUNCTION__);
    return;
  }

  if (message.type != MessageType::kError) {
    ALOGE(
        "%s: depth result processor is not supposed to return shutter, "
        "dropping a message.",
        __FUNCTION__);
    return;
  }

  notify_(message);
}

status_t RgbirdDepthResultProcessor::FlushPendingRequests() {
  ATRACE_CALL();
  return INVALID_OPERATION;
}

}  // namespace google_camera_hal
}  // namespace android