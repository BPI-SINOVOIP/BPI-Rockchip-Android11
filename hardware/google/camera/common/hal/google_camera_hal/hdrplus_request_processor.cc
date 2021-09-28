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
#define LOG_TAG "GCH_HdrplusRequestProcessor"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include "hdrplus_request_processor.h"
#include "vendor_tag_defs.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<HdrplusRequestProcessor> HdrplusRequestProcessor::Create(
    CameraDeviceSessionHwl* device_session_hwl, int32_t raw_stream_id,
    uint32_t physical_camera_id) {
  ATRACE_CALL();
  if (device_session_hwl == nullptr) {
    ALOGE("%s: device_session_hwl (%p) is nullptr", __FUNCTION__,
          device_session_hwl);
    return nullptr;
  }

  auto request_processor = std::unique_ptr<HdrplusRequestProcessor>(
      new HdrplusRequestProcessor(physical_camera_id));
  if (request_processor == nullptr) {
    ALOGE("%s: Creating HdrplusRequestProcessor failed.", __FUNCTION__);
    return nullptr;
  }

  status_t res =
      request_processor->Initialize(device_session_hwl, raw_stream_id);
  if (res != OK) {
    ALOGE("%s: Initializing HdrplusRequestProcessor failed: %s (%d).",
          __FUNCTION__, strerror(-res), res);
    return nullptr;
  }

  return request_processor;
}

status_t HdrplusRequestProcessor::Initialize(
    CameraDeviceSessionHwl* device_session_hwl, int32_t raw_stream_id) {
  ATRACE_CALL();
  std::unique_ptr<HalCameraMetadata> characteristics;
  status_t res = NO_INIT;
  uint32_t num_physical_cameras =
      device_session_hwl->GetPhysicalCameraIds().size();
  if (num_physical_cameras > 0) {
    res = device_session_hwl->GetPhysicalCameraCharacteristics(
        kCameraId, &characteristics);
    if (res != OK) {
      ALOGE("%s: GetPhysicalCameraCharacteristics failed.", __FUNCTION__);
      return BAD_VALUE;
    }
  } else {
    res = device_session_hwl->GetCameraCharacteristics(&characteristics);
    if (res != OK) {
      ALOGE("%s: GetCameraCharacteristics failed.", __FUNCTION__);
      return BAD_VALUE;
    }
  }

  camera_metadata_ro_entry entry;
  res = characteristics->Get(
      ANDROID_SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE, &entry);
  if (res == OK) {
    active_array_width_ = entry.data.i32[2];
    active_array_height_ = entry.data.i32[3];
    ALOGI("%s Active size (%d x %d).", __FUNCTION__, active_array_width_,
          active_array_height_);
  } else {
    ALOGE("%s Get active size failed: %s (%d).", __FUNCTION__, strerror(-res),
          res);
    return res;
  }

  res = characteristics->Get(VendorTagIds::kHdrplusPayloadFrames, &entry);
  if (res != OK || entry.data.i32[0] <= 0) {
    ALOGE("%s: Getting kHdrplusPayloadFrames failed or number <= 0",
          __FUNCTION__);
    return BAD_VALUE;
  }
  payload_frames_ = entry.data.i32[0];
  ALOGI("%s: HDR+ payload_frames_: %d", __FUNCTION__, payload_frames_);
  raw_stream_id_ = raw_stream_id;

  return OK;
}

status_t HdrplusRequestProcessor::ConfigureStreams(
    InternalStreamManager* internal_stream_manager,
    const StreamConfiguration& stream_config,
    StreamConfiguration* process_block_stream_config) {
  ATRACE_CALL();
  if (process_block_stream_config == nullptr ||
      internal_stream_manager == nullptr) {
    ALOGE(
        "%s: process_block_stream_config (%p) is nullptr or "
        "internal_stream_manager (%p) is nullptr",
        __FUNCTION__, process_block_stream_config, internal_stream_manager);
    return BAD_VALUE;
  }

  internal_stream_manager_ = internal_stream_manager;

  Stream raw_stream;
  raw_stream.stream_type = StreamType::kInput;
  raw_stream.width = active_array_width_;
  raw_stream.height = active_array_height_;
  raw_stream.format = HAL_PIXEL_FORMAT_RAW10;
  raw_stream.usage = 0;
  raw_stream.rotation = StreamRotation::kRotation0;
  raw_stream.data_space = HAL_DATASPACE_ARBITRARY;
  // Set id back to raw_stream and then HWL can get correct HAL stream ID
  raw_stream.id = raw_stream_id_;

  process_block_stream_config->streams = stream_config.streams;
  // Add internal RAW stream
  process_block_stream_config->streams.push_back(raw_stream);
  process_block_stream_config->operation_mode = stream_config.operation_mode;
  process_block_stream_config->session_params =
      HalCameraMetadata::Clone(stream_config.session_params.get());
  process_block_stream_config->stream_config_counter =
      stream_config.stream_config_counter;

  return OK;
}

status_t HdrplusRequestProcessor::SetProcessBlock(
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

bool HdrplusRequestProcessor::IsReadyForNextRequest() {
  ATRACE_CALL();
  if (internal_stream_manager_ == nullptr) {
    ALOGW("%s: internal_stream_manager_ nullptr", __FUNCTION__);
    return false;
  }
  if (internal_stream_manager_->IsPendingBufferEmpty(raw_stream_id_) == false) {
    return false;
  }
  return true;
}

void HdrplusRequestProcessor::RemoveJpegMetadata(
    std::vector<std::unique_ptr<HalCameraMetadata>>* metadata) {
  const uint32_t tags[] = {
      ANDROID_JPEG_THUMBNAIL_SIZE,  ANDROID_JPEG_ORIENTATION,
      ANDROID_JPEG_QUALITY,         ANDROID_JPEG_THUMBNAIL_QUALITY,
      ANDROID_JPEG_GPS_COORDINATES, ANDROID_JPEG_GPS_PROCESSING_METHOD,
      ANDROID_JPEG_GPS_TIMESTAMP};
  if (metadata == nullptr) {
    ALOGW("%s: metadata is nullptr", __FUNCTION__);
    return;
  }

  for (uint32_t i = 0; i < metadata->size(); i++) {
    for (uint32_t tag_index = 0; tag_index < sizeof(tags) / sizeof(uint32_t);
         tag_index++) {
      if (metadata->at(i) == nullptr) {
        continue;
      }
      status_t res = metadata->at(i)->Erase(tags[tag_index]);
      if (res != OK) {
        ALOGW("%s: (%d)erase index(%d) failed: %s(%d)", __FUNCTION__, i,
              tag_index, strerror(-res), res);
      }
    }
  }
}

status_t HdrplusRequestProcessor::ProcessRequest(const CaptureRequest& request) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(process_block_lock_);
  if (process_block_ == nullptr) {
    ALOGE("%s: Not configured yet.", __FUNCTION__);
    return NO_INIT;
  }

  if (IsReadyForNextRequest() == false) {
    return BAD_VALUE;
  }

  CaptureRequest block_request;
  block_request.frame_number = request.frame_number;
  block_request.settings = HalCameraMetadata::Clone(request.settings.get());
  block_request.output_buffers = request.output_buffers;
  for (auto& [camera_id, physical_metadata] : request.physical_camera_settings) {
    block_request.physical_camera_settings[camera_id] =
        HalCameraMetadata::Clone(physical_metadata.get());
  }

  // Get multiple raw buffer and metadata from internal stream as input
  status_t result = internal_stream_manager_->GetMostRecentStreamBuffer(
      raw_stream_id_, &(block_request.input_buffers),
      &(block_request.input_buffer_metadata), payload_frames_);
  if (result != OK) {
    ALOGE("%s: frame:%d GetStreamBuffer failed.", __FUNCTION__,
          request.frame_number);
    return UNKNOWN_ERROR;
  }

  RemoveJpegMetadata(&(block_request.input_buffer_metadata));
  std::vector<ProcessBlockRequest> block_requests(1);
  block_requests[0].request = std::move(block_request);
  ALOGD("%s: frame number %u is an HDR+ request.", __FUNCTION__,
        request.frame_number);

  return process_block_->ProcessRequests(block_requests, request);
}

status_t HdrplusRequestProcessor::Flush() {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(process_block_lock_);
  if (process_block_ == nullptr) {
    return OK;
  }

  return process_block_->Flush();
}

}  // namespace google_camera_hal
}  // namespace android