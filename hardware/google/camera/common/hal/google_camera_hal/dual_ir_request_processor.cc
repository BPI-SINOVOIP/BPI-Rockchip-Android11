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
#define LOG_TAG "GCH_DualIrRequestProcessor"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include "dual_ir_request_processor.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<DualIrRequestProcessor> DualIrRequestProcessor::Create(
    CameraDeviceSessionHwl* device_session_hwl, uint32_t lead_ir_camera_id) {
  ATRACE_CALL();
  if (device_session_hwl == nullptr) {
    ALOGE("%s: device_session_hwl is nullptr", __FUNCTION__);
    return nullptr;
  }

  // Check there are two physical cameras.
  std::vector<uint32_t> camera_ids = device_session_hwl->GetPhysicalCameraIds();
  if (camera_ids.size() != 2) {
    ALOGE("%s: Only support two IR cameras but there are %zu cameras.",
          __FUNCTION__, camera_ids.size());
    return nullptr;
  }

  // TODO(b/129017376): Figure out default IR camera ID from static metadata.
  // Assume the first physical camera is the default for now.
  auto request_processor = std::unique_ptr<DualIrRequestProcessor>(
      new DualIrRequestProcessor(lead_ir_camera_id));
  if (request_processor == nullptr) {
    ALOGE("%s: Creating DualIrRequestProcessor failed.", __FUNCTION__);
    return nullptr;
  }

  return request_processor;
}

DualIrRequestProcessor::DualIrRequestProcessor(uint32_t lead_camera_id)
    : kLeadCameraId(lead_camera_id) {
}

status_t DualIrRequestProcessor::ConfigureStreams(
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

  for (auto& stream : process_block_stream_config->streams) {
    // Assign all logical streams to the lead camera.
    if (!stream.is_physical_camera_stream) {
      stream.is_physical_camera_stream = true;
      stream.physical_camera_id = kLeadCameraId;
    }

    stream_physical_camera_ids_[stream.id] = stream.physical_camera_id;
  }

  return OK;
}

status_t DualIrRequestProcessor::SetProcessBlock(
    std::unique_ptr<ProcessBlock> process_block) {
  ATRACE_CALL();
  if (process_block == nullptr) {
    ALOGE("%s: process_block is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(process_block_lock_);
  if (process_block_ != nullptr) {
    ALOGE("%s: Already configured.", __FUNCTION__);
    return ALREADY_EXISTS;
  }

  process_block_ = std::move(process_block);
  return OK;
}

status_t DualIrRequestProcessor::ProcessRequest(const CaptureRequest& request) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(process_block_lock_);
  if (process_block_ == nullptr) {
    ALOGE("%s: Not configured yet.", __FUNCTION__);
    return NO_INIT;
  }

  uint32_t frame_number = request.frame_number;

  // Create one physical request for each physical camera.
  // Map from camera_id to the camera's request.
  std::map<uint32_t, CaptureRequest> requests;

  for (auto& buffer : request.output_buffers) {
    uint32_t camera_id = stream_physical_camera_ids_[buffer.stream_id];
    CaptureRequest* physical_request = nullptr;

    auto request_iter = requests.find(camera_id);
    if (request_iter == requests.end()) {
      physical_request = &requests[camera_id];
      physical_request->frame_number = frame_number;
      // TODO: Combine physical camera settings?
      physical_request->settings =
          HalCameraMetadata::Clone(request.settings.get());
    } else {
      physical_request = &request_iter->second;
    }
    physical_request->output_buffers.push_back(buffer);
  }

  // Construct block requests.
  std::vector<ProcessBlockRequest> block_requests;
  for (auto& [camera_id, physical_request] : requests) {
    ProcessBlockRequest block_request = {
        .request_id = camera_id,
        .request = std::move(physical_request),
    };

    block_requests.push_back(std::move(block_request));
  }

  return process_block_->ProcessRequests(block_requests, request);
}

status_t DualIrRequestProcessor::Flush() {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(process_block_lock_);
  if (process_block_ == nullptr) {
    return OK;
  }

  return process_block_->Flush();
}

}  // namespace google_camera_hal
}  // namespace android