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
#define LOG_TAG "GCH_RealtimeZslRequestProcessor"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include "hal_utils.h"
#include "realtime_zsl_request_processor.h"
#include "vendor_tag_defs.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<RealtimeZslRequestProcessor> RealtimeZslRequestProcessor::Create(
    CameraDeviceSessionHwl* device_session_hwl) {
  ATRACE_CALL();
  if (device_session_hwl == nullptr) {
    ALOGE("%s: device_session_hwl is nullptr", __FUNCTION__);
    return nullptr;
  }

  uint32_t num_physical_cameras =
      device_session_hwl->GetPhysicalCameraIds().size();
  if (num_physical_cameras > 1) {
    ALOGE("%s: only support 1 physical camera but it has %u", __FUNCTION__,
          num_physical_cameras);
    return nullptr;
  }

  auto request_processor = std::unique_ptr<RealtimeZslRequestProcessor>(
      new RealtimeZslRequestProcessor());
  if (request_processor == nullptr) {
    ALOGE("%s: Creating RealtimeZslRequestProcessor failed.", __FUNCTION__);
    return nullptr;
  }

  status_t res = request_processor->Initialize(device_session_hwl);
  if (res != OK) {
    ALOGE("%s: Initializing RealtimeZslRequestProcessor failed: %s (%d).",
          __FUNCTION__, strerror(-res), res);
    return nullptr;
  }

  return request_processor;
}

status_t RealtimeZslRequestProcessor::Initialize(
    CameraDeviceSessionHwl* device_session_hwl) {
  ATRACE_CALL();
  std::unique_ptr<HalCameraMetadata> characteristics;
  status_t res = device_session_hwl->GetCameraCharacteristics(&characteristics);
  if (res != OK) {
    ALOGE("%s: GetCameraCharacteristics failed.", __FUNCTION__);
    return BAD_VALUE;
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

  res = characteristics->Get(VendorTagIds::kHdrUsageMode, &entry);
  if (res == OK) {
    hdr_mode_ = static_cast<HdrMode>(entry.data.u8[0]);
  }

  return OK;
}

status_t RealtimeZslRequestProcessor::ConfigureStreams(
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

  // Register internal raw stream
  Stream raw_stream;
  raw_stream.stream_type = StreamType::kOutput;
  raw_stream.width = active_array_width_;
  raw_stream.height = active_array_height_;
  raw_stream.format = HAL_PIXEL_FORMAT_RAW10;
  raw_stream.usage = 0;
  raw_stream.rotation = StreamRotation::kRotation0;
  raw_stream.data_space = HAL_DATASPACE_ARBITRARY;

  status_t result = internal_stream_manager->RegisterNewInternalStream(
      raw_stream, &raw_stream_id_);
  if (result != OK) {
    ALOGE("%s: RegisterNewInternalStream failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  internal_stream_manager_ = internal_stream_manager;
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

status_t RealtimeZslRequestProcessor::SetProcessBlock(
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

status_t RealtimeZslRequestProcessor::ProcessRequest(
    const CaptureRequest& request) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(process_block_lock_);
  if (process_block_ == nullptr) {
    ALOGE("%s: Not configured yet.", __FUNCTION__);
    return NO_INIT;
  }

  if (is_hdrplus_zsl_enabled_ && request.settings != nullptr) {
    camera_metadata_ro_entry entry = {};
    status_t res =
        request.settings->Get(VendorTagIds::kThermalThrottling, &entry);
    if (res != OK || entry.count != 1) {
      ALOGW("%s: Getting thermal throttling entry failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
    } else if (entry.data.u8[0] == true) {
      // Disable HDR+ ZSL once thermal throttles.
      is_hdrplus_zsl_enabled_ = false;
      ALOGI("%s: HDR+ ZSL disabled due to thermal throttling", __FUNCTION__);
    }
  }

  // Update if preview intent has been requested.
  camera_metadata_ro_entry entry;
  if (!preview_intent_seen_ && request.settings != nullptr &&
      request.settings->Get(ANDROID_CONTROL_CAPTURE_INTENT, &entry) == OK) {
    if (entry.count == 1 &&
        *entry.data.u8 == ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW) {
      preview_intent_seen_ = true;
      ALOGI("%s: First request with preview intent. ZSL starts.", __FUNCTION__);
    }
  }

  CaptureRequest block_request;

  block_request.frame_number = request.frame_number;
  block_request.settings = HalCameraMetadata::Clone(request.settings.get());
  block_request.input_buffers = request.input_buffers;
  block_request.output_buffers = request.output_buffers;

  for (auto& metadata : request.input_buffer_metadata) {
    block_request.input_buffer_metadata.push_back(
        HalCameraMetadata::Clone(metadata.get()));
  }

  for (auto& [camera_id, physical_metadata] : request.physical_camera_settings) {
    block_request.physical_camera_settings[camera_id] =
        HalCameraMetadata::Clone(physical_metadata.get());
  }

  if (is_hdrplus_zsl_enabled_) {
    // Get one RAW bffer from internal stream manager
    StreamBuffer buffer = {};
    status_t result;
    if (preview_intent_seen_) {
      result =
          internal_stream_manager_->GetStreamBuffer(raw_stream_id_, &buffer);
      if (result != OK) {
        ALOGE("%s: frame:%d GetStreamBuffer failed.", __FUNCTION__,
              request.frame_number);
        return UNKNOWN_ERROR;
      }
    }

    // Add RAW output to capture request
    if (preview_intent_seen_) {
      block_request.output_buffers.push_back(buffer);
    }

    if (block_request.settings != nullptr) {
      bool enable_hybrid_ae =
          (hdr_mode_ == HdrMode::kNonHdrplusMode ? false : true);
      result = hal_utils::ModifyRealtimeRequestForHdrplus(
          block_request.settings.get(), enable_hybrid_ae);
      if (result != OK) {
        ALOGE("%s: ModifyRealtimeRequestForHdrplus (%d) fail", __FUNCTION__,
              request.frame_number);
        return UNKNOWN_ERROR;
      }

      if (hdr_mode_ != HdrMode::kHdrplusMode) {
        uint8_t processing_mode =
            static_cast<uint8_t>(ProcessingMode::kIntermediateProcessing);
        block_request.settings->Set(VendorTagIds::kProcessingMode,
                                    &processing_mode,
                                    /*data_count=*/1);
      }
    }
  }

  std::vector<ProcessBlockRequest> block_requests(1);
  block_requests[0].request = std::move(block_request);

  return process_block_->ProcessRequests(block_requests, request);
}

status_t RealtimeZslRequestProcessor::Flush() {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(process_block_lock_);
  if (process_block_ == nullptr) {
    return OK;
  }

  return process_block_->Flush();
}

}  // namespace google_camera_hal
}  // namespace android