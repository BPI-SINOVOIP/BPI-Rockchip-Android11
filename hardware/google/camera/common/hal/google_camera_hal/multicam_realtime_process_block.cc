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
#define LOG_TAG "GCH_MultiCameraRtProcessBlock"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include <unordered_set>

#include "hal_utils.h"
#include "multicam_realtime_process_block.h"
#include "result_processor.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<MultiCameraRtProcessBlock> MultiCameraRtProcessBlock::Create(
    CameraDeviceSessionHwl* device_session_hwl) {
  ATRACE_CALL();
  if (!IsSupported(device_session_hwl)) {
    ALOGE("%s: Not supported.", __FUNCTION__);
    return nullptr;
  }

  auto block = std::unique_ptr<MultiCameraRtProcessBlock>(
      new MultiCameraRtProcessBlock(device_session_hwl));
  if (block == nullptr) {
    ALOGE("%s: Creating MultiCameraRtProcessBlock failed.", __FUNCTION__);
    return nullptr;
  }
  block->request_id_manager_ = PipelineRequestIdManager::Create();
  if (block->request_id_manager_ == nullptr) {
    ALOGE("%s: Creating PipelineRequestIdManager failed.", __FUNCTION__);
    return nullptr;
  }
  return block;
}

bool MultiCameraRtProcessBlock::IsSupported(
    CameraDeviceSessionHwl* device_session_hwl) {
  ATRACE_CALL();
  if (device_session_hwl == nullptr) {
    ALOGE("%s: device_session_hwl is nullptr", __FUNCTION__);
    return false;
  }

  if (device_session_hwl->GetPhysicalCameraIds().size() <= 1) {
    ALOGE("%s: Only support multiple physical cameras", __FUNCTION__);
    return false;
  }

  return true;
}

MultiCameraRtProcessBlock::MultiCameraRtProcessBlock(
    CameraDeviceSessionHwl* device_session_hwl)
    : kCameraId(device_session_hwl->GetCameraId()),
      device_session_hwl_(device_session_hwl) {
  hwl_pipeline_callback_.process_pipeline_result = HwlProcessPipelineResultFunc(
      [this](std::unique_ptr<HwlPipelineResult> result) {
        NotifyHwlPipelineResult(std::move(result));
      });

  hwl_pipeline_callback_.notify = NotifyHwlPipelineMessageFunc(
      [this](uint32_t pipeline_id, const NotifyMessage& message) {
        NotifyHwlPipelineMessage(pipeline_id, message);
      });
}

status_t MultiCameraRtProcessBlock::SetResultProcessor(
    std::unique_ptr<ResultProcessor> result_processor) {
  ATRACE_CALL();
  if (result_processor == nullptr) {
    ALOGE("%s: result_processor is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(result_processor_mutex_);
  if (result_processor_ != nullptr) {
    ALOGE("%s: result_processor_ was already set.", __FUNCTION__);
    return ALREADY_EXISTS;
  }

  result_processor_ = std::move(result_processor);
  return OK;
}

status_t MultiCameraRtProcessBlock::GetCameraStreamConfigurationMap(
    const StreamConfiguration& stream_config,
    CameraStreamConfigurationMap* camera_stream_config_map) const {
  if (camera_stream_config_map == nullptr) {
    ALOGE("%s: camera_stream_config_map is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  // Create one stream configuration for each camera.
  camera_stream_config_map->clear();
  for (auto& stream : stream_config.streams) {
    if (stream.stream_type != StreamType::kOutput ||
        !stream.is_physical_camera_stream) {
      ALOGE("%s: Only physical output streams are supported.", __FUNCTION__);
      return BAD_VALUE;
    }

    (*camera_stream_config_map)[stream.physical_camera_id].streams.push_back(
        stream);
  }

  // Copy the rest of the stream configuration fields
  for (auto& [camera_id, config] : *camera_stream_config_map) {
    config.operation_mode = stream_config.operation_mode;
    config.session_params =
        HalCameraMetadata::Clone(stream_config.session_params.get());
    config.stream_config_counter = stream_config.stream_config_counter;
  }

  return OK;
}

status_t MultiCameraRtProcessBlock::ConfigureStreams(
    const StreamConfiguration& stream_config,
    const StreamConfiguration& overall_config) {
  ATRACE_CALL();
  std::lock_guard lock(configure_shared_mutex_);
  if (is_configured_) {
    ALOGE("%s: Already configured.", __FUNCTION__);
    return ALREADY_EXISTS;
  }

  CameraStreamConfigurationMap camera_stream_configs;
  status_t res =
      GetCameraStreamConfigurationMap(stream_config, &camera_stream_configs);
  if (res != OK) {
    ALOGE("%s: Failed to get camera stream config map: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  configured_streams_.clear();
  camera_pipeline_ids_.clear();

  // Configuration a pipeline for each camera.
  for (auto& [camera_id, config] : camera_stream_configs) {
    uint32_t pipeline_id = 0;
    res = device_session_hwl_->ConfigurePipeline(
        camera_id, hwl_pipeline_callback_, config, overall_config, &pipeline_id);
    if (res != OK) {
      ALOGE("%s: Configuring stream for camera %u failed: %s(%d)", __FUNCTION__,
            camera_id, strerror(-res), res);
      return res;
    }
    ALOGV("%s: config realtime pipeline camera id %u pipeline_id %u",
          __FUNCTION__, camera_id, pipeline_id);

    camera_pipeline_ids_[camera_id] = pipeline_id;
    for (auto& stream : config.streams) {
      configured_streams_[stream.id].pipeline_id = pipeline_id;
      configured_streams_[stream.id].stream = stream;
    }
  }

  is_configured_ = true;
  return OK;
}

status_t MultiCameraRtProcessBlock::GetConfiguredHalStreams(
    std::vector<HalStream>* hal_streams) const {
  ATRACE_CALL();
  std::lock_guard lock(configure_shared_mutex_);
  if (hal_streams == nullptr) {
    ALOGE("%s: hal_streams is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  if (!is_configured_) {
    ALOGE("%s: Not configured yet.", __FUNCTION__);
    return NO_INIT;
  }

  hal_streams->clear();
  for (auto& [camera_id, pipeline_id] : camera_pipeline_ids_) {
    std::vector<HalStream> pipeline_hal_stream;
    status_t res = device_session_hwl_->GetConfiguredHalStream(
        pipeline_id, &pipeline_hal_stream);
    if (res != OK) {
      ALOGE("%s: Getting configured HAL streams for pipeline %u failed: %s(%d)",
            __FUNCTION__, pipeline_id, strerror(-res), res);
      return res;
    }

    hal_streams->insert(hal_streams->end(), pipeline_hal_stream.begin(),
                        pipeline_hal_stream.end());
  }

  return OK;
}

status_t MultiCameraRtProcessBlock::GetBufferPhysicalCameraIdLocked(
    const StreamBuffer& buffer, uint32_t* camera_id) const {
  if (camera_id == nullptr) {
    ALOGE("%s: camera_id is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  int32_t stream_id = buffer.stream_id;
  auto configured_stream_iter = configured_streams_.find(stream_id);
  if (configured_stream_iter == configured_streams_.end()) {
    ALOGE("%s: Stream %d was not configured.", __FUNCTION__, stream_id);
    return BAD_VALUE;
  }

  const ConfiguredStream& configure_stream = configured_stream_iter->second;
  if (!configure_stream.stream.is_physical_camera_stream) {
    ALOGE("%s: Stream %d is not a physical stream.", __FUNCTION__, stream_id);
    return BAD_VALUE;
  }

  *camera_id = configure_stream.stream.physical_camera_id;
  return OK;
}

status_t MultiCameraRtProcessBlock::GetOutputBufferPipelineIdLocked(
    const StreamBuffer& buffer, uint32_t* pipeline_id) const {
  if (pipeline_id == nullptr) {
    ALOGE("%s: pipeline_id is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  uint32_t camera_id;
  status_t res = GetBufferPhysicalCameraIdLocked(buffer, &camera_id);
  if (res != OK) {
    ALOGE("%s: Getting buffer's physical camera ID failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  auto camera_pipeline_id_iter = camera_pipeline_ids_.find(camera_id);
  if (camera_pipeline_id_iter == camera_pipeline_ids_.end()) {
    ALOGE("%s: Cannot find the pipeline ID for camera %u", __FUNCTION__,
          camera_id);
    return BAD_VALUE;
  }

  *pipeline_id = camera_pipeline_id_iter->second;
  return OK;
}

bool MultiCameraRtProcessBlock::AreRequestsValidLocked(
    const std::vector<ProcessBlockRequest>& block_requests) const {
  ATRACE_CALL();
  if (block_requests.empty()) {
    ALOGE("%s: requests is empty.", __FUNCTION__);
    return false;
  }

  std::unordered_set<int32_t> request_camera_ids;
  uint32_t frame_number = block_requests[0].request.frame_number;
  for (auto& block_request : block_requests) {
    if (!block_request.request.input_buffers.empty()) {
      ALOGE("%s: Input buffers are not supported.", __FUNCTION__);
      return false;
    }

    if (block_request.request.output_buffers.size() == 0) {
      ALOGE("%s: request %u doesn't contain any output streams.", __FUNCTION__,
            block_request.request.frame_number);
      return false;
    }

    if (block_request.request.frame_number != frame_number) {
      ALOGE("%s: Not all frame numbers in requests are the same.",
            __FUNCTION__);
      return false;
    }

    // Check all output buffers will be captured from the same physical camera.
    uint32_t physical_camera_id;
    for (uint32_t i = 0; i < block_request.request.output_buffers.size(); i++) {
      uint32_t buffer_camera_id;
      status_t res = GetBufferPhysicalCameraIdLocked(
          block_request.request.output_buffers[0], &buffer_camera_id);
      if (res != OK) {
        ALOGE("%s: Getting buffer's physical camera ID failed: %s(%d)",
              __FUNCTION__, strerror(-res), res);
        return false;
      }

      if (i == 0) {
        physical_camera_id = buffer_camera_id;
      } else if (buffer_camera_id != physical_camera_id) {
        ALOGE("%s: Buffers should belong to the same camera ID in a request.",
              __FUNCTION__);
        return false;
      }
    }

    // Check no two requests will be captured from the same physical camera.
    if (request_camera_ids.find(physical_camera_id) !=
        request_camera_ids.end()) {
      ALOGE("%s: No two requests can be captured from the same camera ID (%u).",
            __FUNCTION__, physical_camera_id);
      return false;
    }

    request_camera_ids.insert(physical_camera_id);
  }

  return true;
}

status_t MultiCameraRtProcessBlock::ForwardPendingRequests(
    const std::vector<ProcessBlockRequest>& process_block_requests,
    const CaptureRequest& remaining_session_request) {
  std::lock_guard<std::mutex> lock(result_processor_mutex_);
  if (result_processor_ == nullptr) {
    ALOGE("%s: result processor was not set.", __FUNCTION__);
    return NO_INIT;
  }

  return result_processor_->AddPendingRequests(process_block_requests,
                                               remaining_session_request);
}

status_t MultiCameraRtProcessBlock::PrepareBlockByCameraId(
    uint32_t camera_id, uint32_t frame_number) {
  return device_session_hwl_->PreparePipeline(camera_pipeline_ids_[camera_id],
                                              frame_number);
}

status_t MultiCameraRtProcessBlock::ProcessRequests(
    const std::vector<ProcessBlockRequest>& process_block_requests,
    const CaptureRequest& remaining_session_request) {
  ATRACE_CALL();
  std::shared_lock lock(configure_shared_mutex_);
  if (configured_streams_.empty()) {
    ALOGE("%s: block is not configured.", __FUNCTION__);
    return NO_INIT;
  }

  if (!AreRequestsValidLocked(process_block_requests)) {
    ALOGE("%s: Requests are not supported.", __FUNCTION__);
    return BAD_VALUE;
  }

  status_t res =
      ForwardPendingRequests(process_block_requests, remaining_session_request);
  if (res != OK) {
    ALOGE("%s: Forwarding pending requests failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  // Get pipeline ID for each request.
  std::vector<uint32_t> pipeline_ids;
  for (auto& block_request : process_block_requests) {
    uint32_t pipeline_id;

    // Get the camera ID of the request's first output buffer. All output
    // buffers should belong to the same pipeline ID, which is checked in
    // AreRequestsSupportedLocked.
    res = GetOutputBufferPipelineIdLocked(
        block_request.request.output_buffers[0], &pipeline_id);
    if (res != OK) {
      ALOGE("%s: Getting buffer's pipeline ID failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return res;
    }

    res = request_id_manager_->SetPipelineRequestId(
        block_request.request_id, block_request.request.frame_number,
        pipeline_id);
    if (res != OK) {
      ALOGE("%s: Adding pipeline request id info failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return res;
    }

    pipeline_ids.push_back(pipeline_id);
    ALOGV("%s: frame_number %u pipeline_id %u request_id %u", __FUNCTION__,
          block_request.request.frame_number, pipeline_id,
          block_request.request_id);
  }

  std::vector<HwlPipelineRequest> hwl_requests;
  res = hal_utils::CreateHwlPipelineRequests(&hwl_requests, pipeline_ids,
                                             process_block_requests);
  if (res != OK) {
    ALOGE("%s: Creating HWL pipeline requests failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  return device_session_hwl_->SubmitRequests(
      process_block_requests[0].request.frame_number, hwl_requests);
}

status_t MultiCameraRtProcessBlock::Flush() {
  ATRACE_CALL();
  std::shared_lock lock(configure_shared_mutex_);
  if (configured_streams_.empty()) {
    return OK;
  }

  status_t res = device_session_hwl_->Flush();
  if (res != OK) {
    ALOGE("%s: Flushing hwl device session failed.", __FUNCTION__);
    return res;
  }

  if (result_processor_ == nullptr) {
    ALOGW("%s: result processor is nullptr.", __FUNCTION__);
    return res;
  }

  return result_processor_->FlushPendingRequests();
}

void MultiCameraRtProcessBlock::NotifyHwlPipelineResult(
    std::unique_ptr<HwlPipelineResult> hwl_result) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(result_processor_mutex_);
  if (result_processor_ == nullptr) {
    ALOGE("%s: result processor is nullptr. Dropping a result", __FUNCTION__);
    return;
  }

  uint32_t frame_number = hwl_result->frame_number;
  uint32_t pipeline_id = hwl_result->pipeline_id;
  if (hwl_result->result_metadata == nullptr &&
      hwl_result->input_buffers.empty() && hwl_result->output_buffers.empty()) {
    ALOGV("%s: Skip empty result. pipeline_id %u frame_number %u", __FUNCTION__,
          pipeline_id, frame_number);
    return;
  }
  auto capture_result =
      hal_utils::ConvertToCaptureResult(std::move(hwl_result));
  if (capture_result == nullptr) {
    ALOGE("%s: Converting to capture result failed.", __FUNCTION__);
    return;
  }

  ALOGV(
      "%s: pipeline id %u frame_number %u output_buffer size %zu input_buffers "
      "size %zu metadata %p ",
      __FUNCTION__, pipeline_id, frame_number,
      capture_result->output_buffers.size(),
      capture_result->input_buffers.size(),
      capture_result->result_metadata.get());

  uint32_t request_id = 0;
  status_t res = request_id_manager_->GetPipelineRequestId(
      pipeline_id, frame_number, &request_id);
  if (res != OK) {
    ALOGE("%s: Get request Id and remove pending failed. res %d", __FUNCTION__,
          res);
    return;
  }

  ProcessBlockResult block_result = {.request_id = request_id,
                                     .result = std::move(capture_result)};
  result_processor_->ProcessResult(std::move(block_result));
}

void MultiCameraRtProcessBlock::NotifyHwlPipelineMessage(
    uint32_t pipeline_id, const NotifyMessage& message) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(result_processor_mutex_);
  if (result_processor_ == nullptr) {
    ALOGE("%s: result processor is nullptr. Dropping a message", __FUNCTION__);
    return;
  }
  uint32_t frame_number = message.type == MessageType::kShutter
                              ? message.message.shutter.frame_number
                              : message.message.error.frame_number;
  ALOGV("%s: pipeline id %u frame_number %u type %d", __FUNCTION__, pipeline_id,
        frame_number, message.type);
  uint32_t request_id = 0;
  status_t res = request_id_manager_->GetPipelineRequestId(
      pipeline_id, frame_number, &request_id);
  if (res != OK) {
    ALOGE("%s: Get request Id and remove pending failed. res %d", __FUNCTION__,
          res);
    return;
  }
  ProcessBlockNotifyMessage block_message = {.request_id = request_id,
                                             .message = message};
  result_processor_->Notify(std::move(block_message));
}

}  // namespace google_camera_hal
}  // namespace android
