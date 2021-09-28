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
#define LOG_TAG "GCH_DualIrResultRequestProcessor"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include <inttypes.h>

#include "dual_ir_result_request_processor.h"
#include "hal_utils.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<DualIrResultRequestProcessor>
DualIrResultRequestProcessor::Create(CameraDeviceSessionHwl* device_session_hwl,
                                     const StreamConfiguration& stream_config,
                                     uint32_t lead_camera_id) {
  ATRACE_CALL();
  if (device_session_hwl == nullptr) {
    ALOGE("%s: device_session_hwl is nullptr.", __FUNCTION__);
    return nullptr;
  }

  uint32_t camera_id = device_session_hwl->GetCameraId();
  auto result_processor = std::unique_ptr<DualIrResultRequestProcessor>(
      new DualIrResultRequestProcessor(stream_config, camera_id,
                                       lead_camera_id));
  if (result_processor == nullptr) {
    ALOGE("%s: Creating DualIrResultRequestProcessor failed.", __FUNCTION__);
    return nullptr;
  }

  return result_processor;
}

DualIrResultRequestProcessor::DualIrResultRequestProcessor(
    const StreamConfiguration& stream_config, uint32_t logical_camera_id,
    uint32_t lead_camera_id)
    : kLogicalCameraId(logical_camera_id), kLeadCameraId(lead_camera_id) {
  ATRACE_CALL();
  // Initialize stream ID -> camera ID map based on framework's stream
  // configuration.
  for (auto& stream : stream_config.streams) {
    if (stream.is_physical_camera_stream) {
      stream_camera_ids_[stream.id] = stream.physical_camera_id;
    } else {
      stream_camera_ids_[stream.id] = kLogicalCameraId;
    }
  }
}

void DualIrResultRequestProcessor::SetResultCallback(
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(callback_lock_);
  process_capture_result_ = process_capture_result;
  notify_ = notify;
}

bool DualIrResultRequestProcessor::IsFrameworkPhyiscalStream(
    int32_t stream_id, uint32_t* physical_camera_id) const {
  ATRACE_CALL();
  auto camera_id_iter = stream_camera_ids_.find(stream_id);
  if (camera_id_iter == stream_camera_ids_.end()) {
    ALOGE("%s: Cannot find camera ID for stream %u", __FUNCTION__, stream_id);
    return false;
  }

  uint32_t camera_id = camera_id_iter->second;
  if (camera_id == kLogicalCameraId) {
    return false;
  }

  if (physical_camera_id != nullptr) {
    *physical_camera_id = camera_id;
  }

  return true;
}

status_t DualIrResultRequestProcessor::AddPendingPhysicalCameraMetadata(
    const ProcessBlockRequest& block_request,
    std::map<uint32_t, std::unique_ptr<HalCameraMetadata>>* physical_metadata) {
  ATRACE_CALL();
  if (physical_metadata == nullptr) {
    ALOGE("%s: physical_metadata is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  for (auto& buffer : block_request.request.output_buffers) {
    uint32_t physical_camera_id;
    if (IsFrameworkPhyiscalStream(buffer.stream_id, &physical_camera_id)) {
      // Add physical_camera_id to physical_metadata.
      (*physical_metadata)[physical_camera_id] = nullptr;
    }
  }

  return OK;
}

status_t DualIrResultRequestProcessor::AddPendingRequests(
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

  // Create new pending result metadata.
  PendingResultMetadata pending_result_metadata;
  for (auto& block_request : process_block_requests) {
    status_t res = AddPendingPhysicalCameraMetadata(
        block_request, &pending_result_metadata.physical_metadata);
    if (res != OK) {
      ALOGE("%s: Failed to fill pending physical camera metadata: %s(%d)",
            __FUNCTION__, strerror(-res), res);
      return res;
    }
  }

  uint32_t frame_number = process_block_requests[0].request.frame_number;

  std::lock_guard<std::mutex> lock(pending_result_metadata_mutex_);
  pending_result_metadata_[frame_number] = std::move(pending_result_metadata);

  return OK;
}

void DualIrResultRequestProcessor::TrySendingResultMetadataLocked(
    uint32_t frame_number) {
  ATRACE_CALL();
  auto pending_result_metadata_iter =
      pending_result_metadata_.find(frame_number);
  if (pending_result_metadata_iter == pending_result_metadata_.end()) {
    ALOGE("%s: Can't find pending result for frame number %u", __FUNCTION__,
          frame_number);
    return;
  }

  // Check if we got result metadata from all cameras for this frame.
  auto& pending_result_metadata = pending_result_metadata_iter->second;
  if (pending_result_metadata.metadata == nullptr) {
    // No metadata for logical camera yet.
    return;
  }

  for (auto& [camera_id, metadata] : pending_result_metadata.physical_metadata) {
    if (metadata == nullptr) {
      // No metadata for this physical camera yet.
      return;
    }
  }

  // Prepare the result.
  auto result = std::make_unique<CaptureResult>();
  result->frame_number = frame_number;
  result->partial_result = 1;
  result->result_metadata = std::move(pending_result_metadata.metadata);

  for (auto& [camera_id, metadata] : pending_result_metadata.physical_metadata) {
    PhysicalCameraMetadata physical_metadata = {
        .physical_camera_id = camera_id,
        .metadata = std::move(metadata),
    };

    result->physical_metadata.push_back(std::move(physical_metadata));
  }

  process_capture_result_(std::move(result));
  pending_result_metadata_.erase(pending_result_metadata_iter);
}

status_t DualIrResultRequestProcessor::ProcessResultMetadata(
    uint32_t frame_number, uint32_t physical_camera_id,
    std::unique_ptr<HalCameraMetadata> result_metadata) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(pending_result_metadata_mutex_);
  auto pending_result_metadata_iter =
      pending_result_metadata_.find(frame_number);
  if (pending_result_metadata_iter == pending_result_metadata_.end()) {
    ALOGE("%s: frame number %u is not expected.", __FUNCTION__, frame_number);
    return BAD_VALUE;
  }

  auto& pending_result_metadata = pending_result_metadata_iter->second;

  if (physical_camera_id == kLeadCameraId) {
    if (pending_result_metadata.metadata != nullptr) {
      ALOGE("%s: Already received metadata from camera %u for frame %u",
            __FUNCTION__, physical_camera_id, frame_number);
      return UNKNOWN_ERROR;
    }

    // Set lead camera id to multi camera metadata
    std::string activePhysicalId = std::to_string(kLeadCameraId);
    if (OK != result_metadata->Set(
                  ANDROID_LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_ID,
                  reinterpret_cast<const uint8_t*>(activePhysicalId.c_str()),
                  static_cast<uint32_t>(activePhysicalId.size() + 1))) {
      ALOGE("Failure in setting active physical camera");
    }

    // Logical camera's result metadata is a clone of the lead camera's
    // result metadata.
    pending_result_metadata.metadata = std::move(result_metadata);
  }

  // Add the physical result metadata to pending result metadata if needed.
  auto physical_metadata_iter =
      pending_result_metadata.physical_metadata.find(physical_camera_id);
  if (physical_metadata_iter != pending_result_metadata.physical_metadata.end()) {
    // If the pending result metadata have physical metadata for a physical
    // camera ID, the physical result metadata is needed.
    if (physical_metadata_iter->second != nullptr) {
      ALOGE("%s: Already received result metadata for camera %u for frame %u",
            __FUNCTION__, physical_camera_id, frame_number);
      return UNKNOWN_ERROR;
    }

    if (physical_camera_id == kLeadCameraId) {
      // If this physical camera is the lead camera, clone the result metadata
      // from the logical camera's result metadata.
      physical_metadata_iter->second =
          HalCameraMetadata::Clone(pending_result_metadata.metadata.get());
    } else {
      physical_metadata_iter->second = std::move(result_metadata);
    }
  }

  TrySendingResultMetadataLocked(frame_number);
  return OK;
}

void DualIrResultRequestProcessor::ProcessResult(ProcessBlockResult block_result) {
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

  // Request ID is set to camera ID by DualIrRequestProcessor.
  uint32_t camera_id = block_result.request_id;

  // Process result metadata separately because there could be two result
  // metadata (one from each camera).
  auto result = std::move(block_result.result);
  if (result->result_metadata != nullptr) {
    status_t res = ProcessResultMetadata(result->frame_number, camera_id,
                                         std::move(result->result_metadata));
    if (res != OK) {
      ALOGE("%s: Processing result metadata failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      // Continue processing rest of the result.
    }
  }

  if (result->output_buffers.size() == 0) {
    // No buffer to send out.
    return;
  }

  process_capture_result_(std::move(result));
}

void DualIrResultRequestProcessor::Notify(
    const ProcessBlockNotifyMessage& block_message) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(callback_lock_);
  if (notify_ == nullptr) {
    ALOGE("%s: notify_ is nullptr. Dropping a message.", __FUNCTION__);
    return;
  }

  const NotifyMessage& message = block_message.message;

  // Request ID is set to camera ID by DualIrRequestProcessor.
  uint32_t camera_id = block_message.request_id;
  if (message.type == MessageType::kShutter && camera_id != kLeadCameraId) {
    // Only send out shutters from the lead camera.
    return;
  }

  // TODO(b/129017376): if there are multiple requests for this frame, wait for
  // all notification to arrive before calling process_capture_result_().
  notify_(block_message.message);
}

status_t DualIrResultRequestProcessor::ConfigureStreams(
    InternalStreamManager* /*internal_stream_manager*/,
    const StreamConfiguration& /*stream_config*/,
    StreamConfiguration* /*process_block_stream_config*/) {
  ATRACE_CALL();
  // TODO(b/131618554): Implement this function.

  return INVALID_OPERATION;
}

status_t DualIrResultRequestProcessor::SetProcessBlock(
    std::unique_ptr<ProcessBlock> /*process_block*/) {
  ATRACE_CALL();
  // TODO(b/131618554): Implement this function.

  return INVALID_OPERATION;
}

status_t DualIrResultRequestProcessor::ProcessRequest(
    const CaptureRequest& /*request*/) {
  ATRACE_CALL();
  // TODO(b/131618554): Implement this function.

  return INVALID_OPERATION;
}

status_t DualIrResultRequestProcessor::Flush() {
  ATRACE_CALL();
  // TODO(b/131618554): Implement this function.

  return INVALID_OPERATION;
}

status_t DualIrResultRequestProcessor::FlushPendingRequests() {
  ATRACE_CALL();
  return OK;
}

}  // namespace google_camera_hal
}  // namespace android