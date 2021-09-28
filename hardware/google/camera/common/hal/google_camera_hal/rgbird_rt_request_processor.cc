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
#define LOG_TAG "GCH_RgbirdRtRequestProcessor"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <cutils/properties.h>
#include <log/log.h>
#include <utils/Trace.h>

#include "hal_utils.h"
#include "rgbird_rt_request_processor.h"
#include "vendor_tag_defs.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<RgbirdRtRequestProcessor> RgbirdRtRequestProcessor::Create(
    CameraDeviceSessionHwl* device_session_hwl, bool is_hdrplus_supported) {
  ATRACE_CALL();
  if (device_session_hwl == nullptr) {
    ALOGE("%s: device_session_hwl is nullptr", __FUNCTION__);
    return nullptr;
  }

  std::vector<uint32_t> physical_camera_ids =
      device_session_hwl->GetPhysicalCameraIds();
  if (physical_camera_ids.size() != 3) {
    ALOGE("%s: Only support 3 cameras", __FUNCTION__);
    return nullptr;
  }

  std::unique_ptr<HalCameraMetadata> characteristics;
  status_t res = device_session_hwl->GetCameraCharacteristics(&characteristics);
  if (res != OK) {
    ALOGE("%s: GetCameraCharacteristics failed.", __FUNCTION__);
    return nullptr;
  }

  uint32_t active_array_width, active_array_height;
  camera_metadata_ro_entry entry;
  res = characteristics->Get(
      ANDROID_SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE, &entry);
  if (res == OK) {
    active_array_width = entry.data.i32[2];
    active_array_height = entry.data.i32[3];
    ALOGI("%s Active size (%d x %d).", __FUNCTION__, active_array_width,
          active_array_height);
  } else {
    ALOGE("%s Get active size failed: %s (%d).", __FUNCTION__, strerror(-res),
          res);
    return nullptr;
  }

  auto request_processor =
      std::unique_ptr<RgbirdRtRequestProcessor>(new RgbirdRtRequestProcessor(
          physical_camera_ids[0], physical_camera_ids[1],
          physical_camera_ids[2], active_array_width, active_array_height,
          is_hdrplus_supported, device_session_hwl));
  if (request_processor == nullptr) {
    ALOGE("%s: Creating RgbirdRtRequestProcessor failed.", __FUNCTION__);
    return nullptr;
  }

  // TODO(b/128633958): remove this after FLL syncing is verified
  request_processor->force_internal_stream_ =
      property_get_bool("persist.camera.rgbird.forceinternal", false);
  if (request_processor->force_internal_stream_) {
    ALOGI("%s: Force creating internal streams for IR pipelines", __FUNCTION__);
  }

  // TODO(b/129910835): This prop should be removed once that logic is in place.
  request_processor->rgb_ir_auto_cal_enabled_ =
      property_get_bool("vendor.camera.frontdepth.enableautocal", true);
  if (request_processor->rgb_ir_auto_cal_enabled_) {
    ALOGI("%s: ", __FUNCTION__);
  }
  request_processor->is_auto_cal_session_ =
      request_processor->IsAutocalSession();

  return request_processor;
}

bool RgbirdRtRequestProcessor::IsAutocalSession() const {
  // TODO(b/129910835): Use more specific logic to determine if a session needs
  // to run autocal or not. Even if rgb_ir_auto_cal_enabled_ is true, it is
  // more reasonable to only run auto cal for some sessions(e.g. 1st session
  // after device boot that has a depth stream configured).
  // To allow more tests, every session having a depth stream is an autocal
  // session now.
  return rgb_ir_auto_cal_enabled_;
}

bool RgbirdRtRequestProcessor::IsAutocalRequest(uint32_t frame_number) {
  // TODO(b/129910835): Refine the logic here to only trigger auto cal for
  // specific request. The result/request processor and depth process block has
  // final right to determine if an internal yuv stream buffer will be used for
  // autocal.
  // The current logic is to trigger the autocal in the kAutocalFrameNumber
  // frame. This must be consistent with that of result_request_processor.
  if (!is_auto_cal_session_ || auto_cal_triggered_ ||
      frame_number != kAutocalFrameNumber ||
      depth_stream_id_ == kStreamIdInvalid) {
    return false;
  }

  auto_cal_triggered_ = true;
  return true;
}

RgbirdRtRequestProcessor::RgbirdRtRequestProcessor(
    uint32_t rgb_camera_id, uint32_t ir1_camera_id, uint32_t ir2_camera_id,
    uint32_t active_array_width, uint32_t active_array_height,
    bool is_hdrplus_supported, CameraDeviceSessionHwl* device_session_hwl)
    : kRgbCameraId(rgb_camera_id),
      kIr1CameraId(ir1_camera_id),
      kIr2CameraId(ir2_camera_id),
      rgb_active_array_width_(active_array_width),
      rgb_active_array_height_(active_array_height),
      is_hdrplus_supported_(is_hdrplus_supported),
      is_hdrplus_zsl_enabled_(is_hdrplus_supported),
      device_session_hwl_(device_session_hwl) {
  ALOGI(
      "%s: Created a RGBIRD RT request processor for RGB %u, IR1 %u, IR2 %u, "
      "is_hdrplus_supported_ :%d",
      __FUNCTION__, kRgbCameraId, kIr1CameraId, kIr2CameraId,
      is_hdrplus_supported_);
}

status_t RgbirdRtRequestProcessor::FindSmallestNonWarpedYuvStreamResolution(
    uint32_t* yuv_w_adjusted, uint32_t* yuv_h_adjusted) {
  if (yuv_w_adjusted == nullptr || yuv_h_adjusted == nullptr) {
    ALOGE("%s: yuv_w_adjusted or yuv_h_adjusted is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  std::unique_ptr<HalCameraMetadata> characteristics;
  status_t res = device_session_hwl_->GetCameraCharacteristics(&characteristics);
  if (res != OK) {
    ALOGE("%s: GetCameraCharacteristics failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }
  camera_metadata_ro_entry entry;
  res = characteristics->Get(VendorTagIds::kAvailableNonWarpedYuvSizes, &entry);
  if (res != OK) {
    ALOGE("%s Get stream size failed: %s (%d).", __FUNCTION__, strerror(-res),
          res);
    return UNKNOWN_ERROR;
  }

  uint32_t min_area = std::numeric_limits<uint32_t>::max();
  uint32_t current_area = 0;
  for (size_t i = 0; i < entry.count; i += 2) {
    current_area = entry.data.i32[i] * entry.data.i32[i + 1];
    if (current_area < min_area) {
      *yuv_w_adjusted = entry.data.i32[i];
      *yuv_h_adjusted = entry.data.i32[i + 1];
      min_area = current_area;
    }
  }

  return OK;
}

status_t RgbirdRtRequestProcessor::FindSmallestResolutionForInternalYuvStream(
    const StreamConfiguration& process_block_stream_config,
    uint32_t* yuv_w_adjusted, uint32_t* yuv_h_adjusted) {
  if (yuv_w_adjusted == nullptr || yuv_h_adjusted == nullptr) {
    ALOGE("%s: yuv_w_adjusted or yuv_h_adjusted is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  *yuv_w_adjusted = kDefaultYuvStreamWidth;
  *yuv_h_adjusted = kDefaultYuvStreamHeight;
  uint32_t framework_non_raw_w = 0;
  uint32_t framework_non_raw_h = 0;
  bool non_raw_non_depth_stream_configured = false;
  for (auto& stream : process_block_stream_config.streams) {
    if (!utils::IsRawStream(stream) && !utils::IsDepthStream(stream)) {
      non_raw_non_depth_stream_configured = true;
      framework_non_raw_w = stream.width;
      framework_non_raw_h = stream.height;
      break;
    }
  }

  std::unique_ptr<HalCameraMetadata> characteristics;
  status_t res = device_session_hwl_->GetCameraCharacteristics(&characteristics);
  if (res != OK) {
    ALOGE("%s: GetCameraCharacteristics failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }
  camera_metadata_ro_entry entry;
  res = characteristics->Get(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                             &entry);
  if (res != OK) {
    ALOGE("%s Get stream size failed: %s (%d).", __FUNCTION__, strerror(-res),
          res);
    return UNKNOWN_ERROR;
  }

  uint32_t min_area = std::numeric_limits<uint32_t>::max();
  uint32_t current_area = 0;
  if (non_raw_non_depth_stream_configured) {
    bool found_matching_aspect_ratio = false;
    for (size_t i = 0; i < entry.count; i += 4) {
      uint8_t format = entry.data.i32[i];
      if ((format == HAL_PIXEL_FORMAT_YCbCr_420_888) &&
          (entry.data.i32[i + 3] ==
           ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT)) {
        current_area = entry.data.i32[i + 1] * entry.data.i32[i + 2];
        if ((entry.data.i32[i + 1] * framework_non_raw_h ==
             entry.data.i32[i + 2] * framework_non_raw_w) &&
            (current_area < min_area)) {
          *yuv_w_adjusted = entry.data.i32[i + 1];
          *yuv_h_adjusted = entry.data.i32[i + 2];
          min_area = current_area;
          found_matching_aspect_ratio = true;
        }
      }
    }
    if (!found_matching_aspect_ratio) {
      ALOGE(
          "%s: No matching aspect ratio can be found in the available stream"
          "config resolution list.",
          __FUNCTION__);
      return UNKNOWN_ERROR;
    }
  } else {
    ALOGI(
        "No YUV stream configured, ues smallest resolution for internal "
        "stream.");
    for (size_t i = 0; i < entry.count; i += 4) {
      if ((entry.data.i32[i] == HAL_PIXEL_FORMAT_YCbCr_420_888) &&
          (entry.data.i32[i + 3] ==
           ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT)) {
        current_area = entry.data.i32[i + 1] * entry.data.i32[i + 2];
        if (current_area < min_area) {
          *yuv_w_adjusted = entry.data.i32[i + 1];
          *yuv_h_adjusted = entry.data.i32[i + 2];
          min_area = current_area;
        }
      }
    }
  }

  if ((*yuv_w_adjusted == 0) || (*yuv_h_adjusted == 0)) {
    ALOGE("%s Get internal YUV stream size failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  return OK;
}

status_t RgbirdRtRequestProcessor::SetNonWarpedYuvStreamId(
    int32_t non_warped_yuv_stream_id,
    StreamConfiguration* process_block_stream_config) {
  if (process_block_stream_config == nullptr) {
    ALOGE("%s: process_block_stream_config is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  if (process_block_stream_config->session_params == nullptr) {
    uint32_t num_entries = 128;
    uint32_t data_bytes = 512;

    process_block_stream_config->session_params =
        HalCameraMetadata::Create(num_entries, data_bytes);
    if (process_block_stream_config->session_params == nullptr) {
      ALOGE("%s: Failed to create session parameter.", __FUNCTION__);
      return UNKNOWN_ERROR;
    }
  }

  auto logical_metadata = process_block_stream_config->session_params.get();

  status_t res = logical_metadata->Set(VendorTagIds::kNonWarpedYuvStreamId,
                                       &non_warped_yuv_stream_id, 1);
  if (res != OK) {
    ALOGE("%s: Failed to update VendorTagIds::kNonWarpedYuvStreamId: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return UNKNOWN_ERROR;
  }

  return res;
}

status_t RgbirdRtRequestProcessor::CreateDepthInternalStreams(
    InternalStreamManager* internal_stream_manager,
    StreamConfiguration* process_block_stream_config) {
  ATRACE_CALL();

  uint32_t yuv_w_adjusted = 0;
  uint32_t yuv_h_adjusted = 0;
  status_t result = OK;

  if (IsAutocalSession()) {
    result = FindSmallestNonWarpedYuvStreamResolution(&yuv_w_adjusted,
                                                      &yuv_h_adjusted);
    if (result != OK) {
      ALOGE("%s: Could not find non-warped YUV resolution for internal YUV.",
            __FUNCTION__);
      return UNKNOWN_ERROR;
    }
  } else {
    result = FindSmallestResolutionForInternalYuvStream(
        *process_block_stream_config, &yuv_w_adjusted, &yuv_h_adjusted);
    if (result != OK) {
      ALOGE("%s: Could not find compatible resolution for internal YUV.",
            __FUNCTION__);
      return UNKNOWN_ERROR;
    }
  }

  ALOGI("Depth internal YUV stream (%d x %d)", yuv_w_adjusted, yuv_h_adjusted);
  // create internal streams:
  // 1 YUV(must have for autocal and 3-sensor syncing)
  // 2 RAW(must have to generate depth)
  Stream yuv_stream;
  yuv_stream.stream_type = StreamType::kOutput;
  yuv_stream.width = yuv_w_adjusted;
  yuv_stream.height = yuv_h_adjusted;
  yuv_stream.format = HAL_PIXEL_FORMAT_YCBCR_420_888;
  yuv_stream.usage = 0;
  yuv_stream.rotation = StreamRotation::kRotation0;
  yuv_stream.data_space = HAL_DATASPACE_ARBITRARY;
  yuv_stream.is_physical_camera_stream = true;
  yuv_stream.physical_camera_id = kRgbCameraId;

  result = internal_stream_manager->RegisterNewInternalStream(
      yuv_stream, &rgb_yuv_stream_id_);
  if (result != OK) {
   ALOGE("%s: RegisterNewInternalStream failed.", __FUNCTION__);
   return UNKNOWN_ERROR;
  }
  yuv_stream.id = rgb_yuv_stream_id_;

  if (IsAutocalSession()) {
    result = SetNonWarpedYuvStreamId(rgb_yuv_stream_id_,
                                     process_block_stream_config);
  }

  if (result != OK) {
    ALOGE("%s: Failed to set no post processing yuv stream id.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  Stream raw_stream[2];
  for (uint32_t i = 0; i < 2; i++) {
    raw_stream[i].stream_type = StreamType::kOutput;
    raw_stream[i].width = 640;
    raw_stream[i].height = 480;
    raw_stream[i].format = HAL_PIXEL_FORMAT_Y8;
    raw_stream[i].usage = 0;
    raw_stream[i].rotation = StreamRotation::kRotation0;
    raw_stream[i].data_space = HAL_DATASPACE_ARBITRARY;
    raw_stream[i].is_physical_camera_stream = true;

    status_t result = internal_stream_manager->RegisterNewInternalStream(
        raw_stream[i], &ir_raw_stream_id_[i]);
    if (result != OK) {
     ALOGE("%s: RegisterNewInternalStream failed.", __FUNCTION__);
     return UNKNOWN_ERROR;
    }
    raw_stream[i].id = ir_raw_stream_id_[i];
  }

  raw_stream[0].physical_camera_id = kIr1CameraId;
  raw_stream[1].physical_camera_id = kIr2CameraId;

  process_block_stream_config->streams.push_back(yuv_stream);
  process_block_stream_config->streams.push_back(raw_stream[0]);
  process_block_stream_config->streams.push_back(raw_stream[1]);

  return OK;
}

status_t RgbirdRtRequestProcessor::RegisterHdrplusInternalRaw(
    StreamConfiguration* process_block_stream_config) {
  ATRACE_CALL();
  if (process_block_stream_config == nullptr) {
    ALOGE("%s: process_block_stream_config is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  // Register internal raw stream
  Stream raw_stream;
  raw_stream.stream_type = StreamType::kOutput;
  raw_stream.width = rgb_active_array_width_;
  raw_stream.height = rgb_active_array_height_;
  raw_stream.format = HAL_PIXEL_FORMAT_RAW10;
  raw_stream.usage = 0;
  raw_stream.rotation = StreamRotation::kRotation0;
  raw_stream.data_space = HAL_DATASPACE_ARBITRARY;

  status_t result = internal_stream_manager_->RegisterNewInternalStream(
      raw_stream, &rgb_raw_stream_id_);
  if (result != OK) {
    ALOGE("%s: RegisterNewInternalStream failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }
  // Set id back to raw_stream and then HWL can get correct HAL stream ID
  raw_stream.id = rgb_raw_stream_id_;

  raw_stream.is_physical_camera_stream = true;
  raw_stream.physical_camera_id = kRgbCameraId;

  // Add internal RAW stream
  process_block_stream_config->streams.push_back(raw_stream);
  return OK;
}

status_t RgbirdRtRequestProcessor::ConfigureStreams(
    InternalStreamManager* internal_stream_manager,
    const StreamConfiguration& stream_config,
    StreamConfiguration* process_block_stream_config) {
  ATRACE_CALL();
  if (process_block_stream_config == nullptr) {
    ALOGE("%s: process_block_stream_config is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  internal_stream_manager_ = internal_stream_manager;
  if (is_hdrplus_supported_) {
    status_t result = RegisterHdrplusInternalRaw(process_block_stream_config);
    if (result != OK) {
      ALOGE("%s: RegisterHdrplusInternalRaw failed.", __FUNCTION__);
      return UNKNOWN_ERROR;
    }
  }

  process_block_stream_config->operation_mode = stream_config.operation_mode;
  process_block_stream_config->session_params =
      HalCameraMetadata::Clone(stream_config.session_params.get());
  process_block_stream_config->stream_config_counter =
      stream_config.stream_config_counter;

  bool has_depth_stream = false;
  for (auto& stream : stream_config.streams) {
    if (utils::IsDepthStream(stream)) {
      has_depth_stream = true;
      depth_stream_id_ = stream.id;
      continue;
    }

    auto pb_stream = stream;
    // Assign all logical streams to RGB camera.
    if (!pb_stream.is_physical_camera_stream) {
      pb_stream.is_physical_camera_stream = true;
      pb_stream.physical_camera_id = kRgbCameraId;
    }

    process_block_stream_config->streams.push_back(pb_stream);
  }

  // TODO(b/128633958): remove the force flag after FLL syncing is verified
  if (force_internal_stream_ || has_depth_stream) {
    CreateDepthInternalStreams(internal_stream_manager,
                               process_block_stream_config);
  }

  return OK;
}

status_t RgbirdRtRequestProcessor::SetProcessBlock(
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

status_t RgbirdRtRequestProcessor::AddIrRawProcessBlockRequestLocked(
    std::vector<ProcessBlockRequest>* block_requests,
    const CaptureRequest& request, uint32_t camera_id) {
  ATRACE_CALL();
  uint32_t stream_id_index = 0;

  if (camera_id == kIr1CameraId) {
    stream_id_index = 0;
  } else if (camera_id == kIr2CameraId) {
    stream_id_index = 1;
  } else {
    ALOGE("%s: Unknown IR camera id %d", __FUNCTION__, camera_id);
    return INVALID_OPERATION;
  }

  ProcessBlockRequest block_request = {.request_id = camera_id};
  CaptureRequest& physical_request = block_request.request;
  physical_request.frame_number = request.frame_number;
  physical_request.settings = HalCameraMetadata::Clone(request.settings.get());

  // TODO(b/128633958): Remap the crop region for IR sensors properly.
  // The crop region cloned from logical camera control settings causes mass log
  // spew from the IR pipelines. Force the crop region for now as a WAR.
  if (physical_request.settings != nullptr) {
    camera_metadata_ro_entry_t entry_crop_region_user = {};
    if (physical_request.settings->Get(ANDROID_SCALER_CROP_REGION,
                                       &entry_crop_region_user) == OK) {
      const uint32_t ir_crop_region[4] = {0, 0, 640, 480};
      physical_request.settings->Set(
          ANDROID_SCALER_CROP_REGION,
          reinterpret_cast<const int32_t*>(&ir_crop_region),
          sizeof(ir_crop_region) / sizeof(int32_t));
    }
  }
  // Requests for IR pipelines should not include any input buffer or metadata
  // physical_request.input_buffers
  // physical_request.input_buffer_metadata

  StreamBuffer internal_buffer = {};
  status_t res = internal_stream_manager_->GetStreamBuffer(
      ir_raw_stream_id_[stream_id_index], &internal_buffer);
  if (res != OK) {
    ALOGE(
        "%s: Failed to get internal stream buffer for frame %d, stream id"
        " %d: %s(%d)",
        __FUNCTION__, request.frame_number, ir_raw_stream_id_[0],
        strerror(-res), res);
    return UNKNOWN_ERROR;
  }
  physical_request.output_buffers.push_back(internal_buffer);

  physical_request.physical_camera_settings[camera_id] =
      HalCameraMetadata::Clone(request.settings.get());

  block_requests->push_back(std::move(block_request));

  return OK;
}

status_t RgbirdRtRequestProcessor::TryAddRgbProcessBlockRequestLocked(
    std::vector<ProcessBlockRequest>* block_requests,
    const CaptureRequest& request) {
  ATRACE_CALL();
  if (block_requests == nullptr) {
    ALOGE("%s: block_requests is nullptr.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  ProcessBlockRequest block_request = {.request_id = kRgbCameraId};
  CaptureRequest& physical_request = block_request.request;

  for (auto& output_buffer : request.output_buffers) {
    if (output_buffer.stream_id != depth_stream_id_) {
      physical_request.output_buffers.push_back(output_buffer);
    }
  }

  if (is_hdrplus_zsl_enabled_ && request.settings != nullptr) {
    camera_metadata_ro_entry entry = {};
    status_t res =
        request.settings->Get(VendorTagIds::kThermalThrottling, &entry);
    if (res != OK || entry.count != 1) {
      ALOGW("%s: Getting thermal throttling entry failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
    } else if (entry.data.u8[0] == true) {
      // Disable HDR+ once thermal throttles.
      is_hdrplus_zsl_enabled_ = false;
      ALOGI("%s: HDR+ ZSL disabled due to thermal throttling", __FUNCTION__);
    }
  }

  // Disable HDR+ for thermal throttling.
  if (is_hdrplus_zsl_enabled_) {
    status_t res = TryAddHdrplusRawOutputLocked(&physical_request, request);
    if (res != OK) {
      ALOGE("%s: AddHdrplusRawOutput fail", __FUNCTION__);
      return res;
    }
  } else if (physical_request.output_buffers.empty() ||
             IsAutocalRequest(request.frame_number)) {
    status_t res = TryAddDepthInternalYuvOutputLocked(&physical_request);
    if (res != OK) {
      ALOGE("%s: AddDepthOnlyRawOutput failed.", __FUNCTION__);
      return res;
    }
  }

  // In case there is only one depth stream
  if (!physical_request.output_buffers.empty()) {
    physical_request.frame_number = request.frame_number;
    physical_request.settings = HalCameraMetadata::Clone(request.settings.get());

    if (is_hdrplus_zsl_enabled_ && physical_request.settings != nullptr) {
      status_t res = hal_utils::ModifyRealtimeRequestForHdrplus(
          physical_request.settings.get());
      if (res != OK) {
        ALOGE("%s: ModifyRealtimeRequestForHdrplus (%d) fail", __FUNCTION__,
              request.frame_number);
        return UNKNOWN_ERROR;
      }
    }

    physical_request.input_buffers = request.input_buffers;

    for (auto& metadata : request.input_buffer_metadata) {
      physical_request.input_buffer_metadata.push_back(
          HalCameraMetadata::Clone(metadata.get()));
    }

    block_requests->push_back(std::move(block_request));
  }
  return OK;
}

status_t RgbirdRtRequestProcessor::TryAddDepthInternalYuvOutputLocked(
    CaptureRequest* block_request) {
  if (block_request == nullptr) {
    ALOGE("%s: block_request is nullptr.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  StreamBuffer buffer = {};
  status_t result =
      internal_stream_manager_->GetStreamBuffer(rgb_yuv_stream_id_, &buffer);
  if (result != OK) {
    ALOGE("%s: GetStreamBuffer failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }
  block_request->output_buffers.push_back(buffer);

  return OK;
}

status_t RgbirdRtRequestProcessor::TryAddHdrplusRawOutputLocked(
    CaptureRequest* block_request, const CaptureRequest& request) {
  ATRACE_CALL();
  if (block_request == nullptr) {
    ALOGE("%s: block_request is nullptr.", __FUNCTION__);
    return UNKNOWN_ERROR;
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

  // Get one RAW bffer from internal stream manager
  // Add RAW output to capture request
  if (preview_intent_seen_) {
    StreamBuffer buffer = {};
    status_t result =
        internal_stream_manager_->GetStreamBuffer(rgb_raw_stream_id_, &buffer);
    if (result != OK) {
      ALOGE("%s: frame:%d GetStreamBuffer failed.", __FUNCTION__,
            request.frame_number);
      return UNKNOWN_ERROR;
    }
    block_request->output_buffers.push_back(buffer);
  }

  return OK;
}

status_t RgbirdRtRequestProcessor::ProcessRequest(const CaptureRequest& request) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(process_block_lock_);
  if (process_block_ == nullptr) {
    ALOGE("%s: Not configured yet.", __FUNCTION__);
    return NO_INIT;
  }

  // Rgbird should not have phys settings
  if (!request.physical_camera_settings.empty()) {
    ALOGE("%s: Rgbird capture session does not support physical settings.",
          __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  {
    std::vector<ProcessBlockRequest> block_requests;
    status_t res = TryAddRgbProcessBlockRequestLocked(&block_requests, request);
    if (res != OK) {
      ALOGE("%s: Failed to add process block request for rgb pipeline.",
            __FUNCTION__);
      return res;
    }

    // TODO(b/128633958): Remove the force flag after FLL sync is verified
    if (force_internal_stream_ || depth_stream_id_ != kStreamIdInvalid) {
      res = AddIrRawProcessBlockRequestLocked(&block_requests, request,
                                              kIr1CameraId);
      if (res != OK) {
        ALOGE("%s: Failed to add process block request for ir1 pipeline.",
              __FUNCTION__);
        return res;
      }
      res = AddIrRawProcessBlockRequestLocked(&block_requests, request,
                                              kIr2CameraId);
      if (res != OK) {
        ALOGE("%s: Failed to add process block request for ir2 pipeline.",
              __FUNCTION__);
        return res;
      }
    }

    return process_block_->ProcessRequests(block_requests, request);
  }
}

status_t RgbirdRtRequestProcessor::Flush() {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(process_block_lock_);
  if (process_block_ == nullptr) {
    return OK;
  }

  return process_block_->Flush();
}

}  // namespace google_camera_hal
}  // namespace android