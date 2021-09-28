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
#define LOG_TAG "GCH_PipelineRequestIdManager"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include "pipeline_request_id_manager.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<PipelineRequestIdManager> PipelineRequestIdManager::Create(
    size_t max_pending_request) {
  ATRACE_CALL();
  auto request_id_manager = std::unique_ptr<PipelineRequestIdManager>(
      new PipelineRequestIdManager(max_pending_request));

  if (request_id_manager == nullptr) {
    ALOGE("%s: Creating PipelineRequestIdManager failed.", __FUNCTION__);
    return nullptr;
  }

  return request_id_manager;
}

PipelineRequestIdManager::PipelineRequestIdManager(size_t max_pending_request)
    : kMaxPendingRequest(max_pending_request) {
}

status_t PipelineRequestIdManager::SetPipelineRequestId(uint32_t request_id,
                                                        uint32_t frame_number,
                                                        uint32_t pipeline_id) {
  ATRACE_CALL();
  if (kMaxPendingRequest == 0) {
    ALOGE("%s: max pending request is 0", __FUNCTION__);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(pipeline_request_ids_mutex_);

  if (pipeline_request_ids_.find(pipeline_id) == pipeline_request_ids_.end()) {
    pipeline_request_ids_.emplace(
        pipeline_id, std::vector<RequestIdInfo>(kMaxPendingRequest));
  }

  size_t frame_index = frame_number % kMaxPendingRequest;
  auto& request_id_info = pipeline_request_ids_[pipeline_id][frame_index];

  // Frame number 0 is same as default value in RequestIdInfo. Skip checking.
  if (frame_number != 0 && frame_number == request_id_info.frame_number) {
    ALOGE(
        "%s: Setting request_id %u failed. frame_number %u has been mapped to "
        "request_id %u in pipeline_id %u",
        __FUNCTION__, request_id, frame_number, request_id_info.request_id,
        pipeline_id);
    return ALREADY_EXISTS;
  }

  request_id_info.frame_number = frame_number;
  request_id_info.request_id = request_id;

  ALOGV(
      "%s: Setting mapping from frame_number %u to request_id %u in "
      "pipeline_id %u",
      __FUNCTION__, frame_number, request_id, pipeline_id);

  return OK;
}

status_t PipelineRequestIdManager::GetPipelineRequestId(uint32_t pipeline_id,
                                                        uint32_t frame_number,
                                                        uint32_t* request_id) {
  ATRACE_CALL();
  if (kMaxPendingRequest == 0) {
    ALOGE("%s: max pending request is 0", __FUNCTION__);
    return BAD_VALUE;
  }

  if (request_id == nullptr) {
    ALOGE("%s: request_id is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(pipeline_request_ids_mutex_);
  if (pipeline_request_ids_.find(pipeline_id) == pipeline_request_ids_.end()) {
    ALOGE("%s: Can't found pipeline_id %u from map", __FUNCTION__, pipeline_id);
    return BAD_VALUE;
  }

  size_t frame_index = frame_number % kMaxPendingRequest;
  auto& request_id_info = pipeline_request_ids_[pipeline_id][frame_index];
  if (frame_number != request_id_info.frame_number) {
    ALOGE(
        "%s: Getting request id failed. frame number %u request_id_info has "
        "been overwritten by other frame number %u.",
        __FUNCTION__, frame_number, request_id_info.frame_number);
    return BAD_VALUE;
  }
  *request_id = request_id_info.request_id;

  return OK;
}

}  // namespace google_camera_hal
}  // namespace android
