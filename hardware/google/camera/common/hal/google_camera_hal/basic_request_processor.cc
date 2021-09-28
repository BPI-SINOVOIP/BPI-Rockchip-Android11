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
#define LOG_TAG "GCH_BasicRequestProcessor"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include "basic_request_processor.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<BasicRequestProcessor> BasicRequestProcessor::Create(
    CameraDeviceSessionHwl* device_session_hwl) {
  ATRACE_CALL();
  if (device_session_hwl == nullptr) {
    ALOGE("%s: device_session_hwl is nullptr", __FUNCTION__);
    return nullptr;
  }

  auto request_processor =
      std::unique_ptr<BasicRequestProcessor>(new BasicRequestProcessor());
  if (request_processor == nullptr) {
    ALOGE("%s: Creating BasicRequestProcessor failed.", __FUNCTION__);
    return nullptr;
  }

  return request_processor;
}

status_t BasicRequestProcessor::ConfigureStreams(
    InternalStreamManager* /*internal_stream_manager*/,
    const StreamConfiguration& stream_config,
    StreamConfiguration* process_block_stream_config) {
  ATRACE_CALL();
  if (process_block_stream_config == nullptr) {
    ALOGE("%s: process_block_stream_config is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  process_block_stream_config->streams = stream_config.streams;
  process_block_stream_config->operation_mode = stream_config.operation_mode;
  process_block_stream_config->session_params =
      HalCameraMetadata::Clone(stream_config.session_params.get());
  process_block_stream_config->stream_config_counter =
      stream_config.stream_config_counter;

  return OK;
}

status_t BasicRequestProcessor::SetProcessBlock(
    std::unique_ptr<ProcessBlock> process_block) {
  ATRACE_CALL();
  if (process_block == nullptr) {
    ALOGE("%s: process_block is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  std::lock_guard lock(process_block_shared_lock_);
  if (process_block_ != nullptr) {
    ALOGE("%s: Already configured.", __FUNCTION__);
    return ALREADY_EXISTS;
  }

  process_block_ = std::move(process_block);
  return OK;
}

status_t BasicRequestProcessor::ProcessRequest(const CaptureRequest& request) {
  ATRACE_CALL();
  std::shared_lock lock(process_block_shared_lock_);
  if (process_block_ == nullptr) {
    ALOGE("%s: Not configured yet.", __FUNCTION__);
    return NO_INIT;
  }

  CaptureRequest block_request;
  block_request.frame_number = request.frame_number;
  block_request.settings = HalCameraMetadata::Clone(request.settings.get());
  block_request.input_buffers = request.input_buffers;

  for (auto& metadata : request.input_buffer_metadata) {
    block_request.input_buffer_metadata.push_back(
        HalCameraMetadata::Clone(metadata.get()));
  }

  block_request.output_buffers = request.output_buffers;
  for (auto& [camera_id, physical_metadata] : request.physical_camera_settings) {
    block_request.physical_camera_settings[camera_id] =
        HalCameraMetadata::Clone(physical_metadata.get());
  }

  std::vector<ProcessBlockRequest> block_requests(1);
  block_requests[0].request = std::move(block_request);

  return process_block_->ProcessRequests(block_requests, request);
}

status_t BasicRequestProcessor::Flush() {
  ATRACE_CALL();
  std::shared_lock lock(process_block_shared_lock_);
  if (process_block_ == nullptr) {
    return OK;
  }

  return process_block_->Flush();
}

}  // namespace google_camera_hal
}  // namespace android
