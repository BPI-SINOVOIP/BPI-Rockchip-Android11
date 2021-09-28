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
#define LOG_TAG "GCH_BasicResultProcessor"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include <inttypes.h>

#include "basic_result_processor.h"
#include "hal_utils.h"

namespace android {
namespace google_camera_hal {

BasicResultProcessor::~BasicResultProcessor() {
  // Avoid a possible timing issue that could result
  // in invalid memory access. Once 'process_capture_result_'
  // in 'ProcessResult' returns on the very last buffer of the
  // last pending request, camera service will be able to
  // re-configure the camera streams at any time. Depending
  // on scheduling the current 'BasicResultProcessor' instance
  // might be destroyed before 'ProcessResult' is able to
  // unlock 'callback_lock_' which will result in undefined behavior.
  // To resolve this block the destructor until the result
  // callback is able to correctly unlock the mutex.
  std::lock_guard<std::mutex> lock(callback_lock_);
}

std::unique_ptr<BasicResultProcessor> BasicResultProcessor::Create() {
  auto result_processor =
      std::unique_ptr<BasicResultProcessor>(new BasicResultProcessor());
  if (result_processor == nullptr) {
    ALOGE("%s: Creating BasicResultProcessor failed.", __FUNCTION__);
    return nullptr;
  }

  return result_processor;
}

void BasicResultProcessor::SetResultCallback(
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(callback_lock_);
  process_capture_result_ = process_capture_result;
  notify_ = notify;
}

status_t BasicResultProcessor::AddPendingRequests(
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

void BasicResultProcessor::ProcessResult(ProcessBlockResult block_result) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(callback_lock_);
  if (block_result.result == nullptr) {
    ALOGW("%s: Received a nullptr result.", __FUNCTION__);
    return;
  }

  if (process_capture_result_ == nullptr) {
    ALOGE("%s: process_capture_result_ is nullptr. Dropping a result.",
          __FUNCTION__);
    return;
  }

  process_capture_result_(std::move(block_result.result));
}

void BasicResultProcessor::Notify(const ProcessBlockNotifyMessage& block_message) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(callback_lock_);
  if (notify_ == nullptr) {
    ALOGE("%s: notify_ is nullptr. Dropping a message.", __FUNCTION__);
    return;
  }

  notify_(block_message.message);
}

status_t BasicResultProcessor::FlushPendingRequests() {
  ATRACE_CALL();
  return INVALID_OPERATION;
}

}  // namespace google_camera_hal
}  // namespace android
