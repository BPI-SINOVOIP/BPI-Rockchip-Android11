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
#define LOG_TAG "GCH_HalUtils"
#include "hal_utils.h"

#include <cutils/properties.h>
#include <inttypes.h>
#include <log/log.h>

#include <string>

#include "vendor_tag_defs.h"

namespace android {
namespace google_camera_hal {
namespace hal_utils {

status_t CreateHwlPipelineRequest(HwlPipelineRequest* hwl_request,
                                  uint32_t pipeline_id,
                                  const CaptureRequest& request) {
  if (hwl_request == nullptr) {
    ALOGE("%s: hwl_request is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  hwl_request->pipeline_id = pipeline_id;
  hwl_request->settings = HalCameraMetadata::Clone(request.settings.get());
  hwl_request->input_buffers = request.input_buffers;
  hwl_request->output_buffers = request.output_buffers;

  for (auto& metadata : request.input_buffer_metadata) {
    hwl_request->input_buffer_metadata.push_back(
        HalCameraMetadata::Clone(metadata.get()));
  }

  return OK;
}

status_t CreateHwlPipelineRequests(
    std::vector<HwlPipelineRequest>* hwl_requests,
    const std::vector<uint32_t>& pipeline_ids,
    const std::vector<ProcessBlockRequest>& requests) {
  if (hwl_requests == nullptr) {
    ALOGE("%s: hwl_requests is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  if (pipeline_ids.size() != requests.size()) {
    ALOGE("%s: There are %zu pipeline IDs but %zu requests", __FUNCTION__,
          pipeline_ids.size(), requests.size());
    return BAD_VALUE;
  }

  status_t res;
  for (size_t i = 0; i < pipeline_ids.size(); i++) {
    HwlPipelineRequest hwl_request;
    res = CreateHwlPipelineRequest(&hwl_request, pipeline_ids[i],
                                   requests[i].request);
    if (res != OK) {
      ALOGE("%s: Creating a HWL pipeline request failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return res;
    }

    hwl_requests->push_back(std::move(hwl_request));
  }

  return OK;
}

std::unique_ptr<CaptureResult> ConvertToCaptureResult(
    std::unique_ptr<HwlPipelineResult> hwl_result) {
  if (hwl_result == nullptr) {
    ALOGE("%s: hwl_result is nullptr", __FUNCTION__);
    return nullptr;
  }

  auto capture_result = std::make_unique<CaptureResult>();
  if (capture_result == nullptr) {
    ALOGE("%s: Creating capture_result failed.", __FUNCTION__);
    return nullptr;
  }

  capture_result->frame_number = hwl_result->frame_number;
  capture_result->result_metadata = std::move(hwl_result->result_metadata);
  capture_result->output_buffers = std::move(hwl_result->output_buffers);
  capture_result->input_buffers = std::move(hwl_result->input_buffers);
  capture_result->partial_result = hwl_result->partial_result;

  capture_result->physical_metadata.reserve(
      hwl_result->physical_camera_results.size());
  for (const auto& [camera_id, metadata] : hwl_result->physical_camera_results) {
    capture_result->physical_metadata.push_back(PhysicalCameraMetadata(
        {camera_id, HalCameraMetadata::Clone(metadata.get())}));
  }

  return capture_result;
}

bool ContainsOutputBuffer(const CaptureRequest& request,
                          const buffer_handle_t& buffer) {
  for (auto& request_buffer : request.output_buffers) {
    if (request_buffer.buffer == buffer) {
      return true;
    }
  }

  return false;
}

bool AreAllRemainingBuffersRequested(
    const std::vector<ProcessBlockRequest>& process_block_requests,
    const CaptureRequest& remaining_session_request) {
  for (auto& buffer : remaining_session_request.output_buffers) {
    bool found = false;

    for (auto& block_request : process_block_requests) {
      if (ContainsOutputBuffer(block_request.request, buffer.buffer)) {
        found = true;
        break;
      }
    }

    if (!found) {
      ALOGE("%s: A buffer %" PRIu64 " of stream %d is not requested.",
            __FUNCTION__, buffer.buffer_id, buffer.stream_id);
      return false;
    }
  }

  return true;
}

static status_t GetColorFilterArrangement(
    const HalCameraMetadata* characteristics, uint8_t* cfa) {
  if (characteristics == nullptr || cfa == nullptr) {
    ALOGE("%s: characteristics (%p) or cfa (%p) is nullptr", __FUNCTION__,
          characteristics, cfa);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry entry;
  status_t res = characteristics->Get(
      ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT, &entry);
  if (res != OK || entry.count != 1) {
    ALOGE("%s: Getting COLOR_FILTER_ARRANGEMENT failed: %s(%d) count: %zu",
          __FUNCTION__, strerror(-res), res, entry.count);
    return res;
  }

  *cfa = entry.data.u8[0];
  return OK;
}

bool IsIrCamera(const HalCameraMetadata* characteristics) {
  uint8_t cfa;
  status_t res = GetColorFilterArrangement(characteristics, &cfa);
  if (res != OK) {
    ALOGE("%s: Getting color filter arrangement failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return false;
  }

  return cfa == ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_NIR;
}

bool IsMonoCamera(const HalCameraMetadata* characteristics) {
  uint8_t cfa;
  status_t res = GetColorFilterArrangement(characteristics, &cfa);
  if (res != OK) {
    ALOGE("%s: Getting color filter arrangement failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return false;
  }

  return cfa == ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_MONO;
}

bool IsBayerCamera(const HalCameraMetadata* characteristics) {
  uint8_t cfa;
  status_t res = GetColorFilterArrangement(characteristics, &cfa);
  if (res != OK) {
    ALOGE("%s: Getting color filter arrangement failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return false;
  }

  if (cfa == ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB ||
      cfa == ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GRBG ||
      cfa == ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GBRG ||
      cfa == ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_BGGR) {
    return true;
  }

  return false;
}

bool IsFixedFocusCamera(const HalCameraMetadata* characteristics) {
  if (characteristics == nullptr) {
    ALOGE("%s: characteristics (%p) is nullptr", __FUNCTION__, characteristics);
    return false;
  }

  camera_metadata_ro_entry entry = {};
  status_t res =
      characteristics->Get(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE, &entry);
  if (res != OK || entry.count != 1) {
    ALOGE("%s: Getting ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return false;
  }

  return entry.data.f[0] == 0.0f;
}

bool IsRequestHdrplusCompatible(const CaptureRequest& request,
                                int32_t preview_stream_id) {
  if (request.settings == nullptr) {
    return false;
  }

  camera_metadata_ro_entry entry;
  if (request.settings->Get(ANDROID_CONTROL_CAPTURE_INTENT, &entry) != OK ||
      *entry.data.u8 != ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE) {
    ALOGV("%s: ANDROID_CONTROL_CAPTURE_INTENT is not STILL_CAPTURE",
          __FUNCTION__);
    return false;
  }

  if (request.settings->Get(ANDROID_CONTROL_ENABLE_ZSL_TRUE, &entry) != OK ||
      *entry.data.u8 != ANDROID_CONTROL_ENABLE_ZSL_TRUE) {
    ALOGV("%s: ANDROID_CONTROL_ENABLE_ZSL is not true", __FUNCTION__);
    return false;
  }

  if (request.settings->Get(ANDROID_NOISE_REDUCTION_MODE, &entry) != OK ||
      *entry.data.u8 != ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY) {
    ALOGV("%s: ANDROID_NOISE_REDUCTION_MODE is not HQ", __FUNCTION__);
    return false;
  }

  if (request.settings->Get(ANDROID_EDGE_MODE, &entry) != OK ||
      *entry.data.u8 != ANDROID_EDGE_MODE_HIGH_QUALITY) {
    ALOGV("%s: ANDROID_EDGE_MODE is not HQ", __FUNCTION__);
    return false;
  }

  if (request.settings->Get(ANDROID_COLOR_CORRECTION_ABERRATION_MODE, &entry) !=
          OK ||
      *entry.data.u8 != ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY) {
    ALOGV("%s: ANDROID_COLOR_CORRECTION_ABERRATION_MODE is not HQ",
          __FUNCTION__);
    return false;
  }

  if (request.settings->Get(ANDROID_CONTROL_AE_MODE, &entry) != OK ||
      (*entry.data.u8 != ANDROID_CONTROL_AE_MODE_ON &&
       *entry.data.u8 != ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH)) {
    ALOGV("%s: ANDROID_CONTROL_AE_MODE is not ON or ON_AUTO_FLASH",
          __FUNCTION__);
    return false;
  }

  if (request.settings->Get(ANDROID_CONTROL_AWB_MODE, &entry) != OK ||
      *entry.data.u8 != ANDROID_CONTROL_AWB_MODE_AUTO) {
    ALOGV("%s: ANDROID_CONTROL_AWB_MODE is not HQ", __FUNCTION__);
    return false;
  }

  if (request.settings->Get(ANDROID_CONTROL_EFFECT_MODE, &entry) != OK ||
      *entry.data.u8 != ANDROID_CONTROL_EFFECT_MODE_OFF) {
    ALOGV("%s: ANDROID_CONTROL_EFFECT_MODE is not HQ", __FUNCTION__);
    return false;
  }

  if (request.settings->Get(ANDROID_CONTROL_MODE, &entry) != OK ||
      (*entry.data.u8 != ANDROID_CONTROL_MODE_AUTO &&
       *entry.data.u8 != ANDROID_CONTROL_MODE_USE_SCENE_MODE)) {
    ALOGV("%s: ANDROID_CONTROL_MODE is not AUTO or USE_SCENE_MODE",
          __FUNCTION__);
    return false;
  }

  if (request.settings->Get(ANDROID_FLASH_MODE, &entry) != OK ||
      *entry.data.u8 != ANDROID_FLASH_MODE_OFF) {
    ALOGV("%s: ANDROID_FLASH_MODE is not OFF", __FUNCTION__);
    return false;
  }

  if (request.settings->Get(ANDROID_TONEMAP_MODE, &entry) != OK ||
      *entry.data.u8 != ANDROID_TONEMAP_MODE_HIGH_QUALITY) {
    ALOGV("%s: ANDROID_TONEMAP_MODE is not HQ", __FUNCTION__);
    return false;
  }

  // For b/129798167 - AOSP camera AP can't trigger the snapshot
  if (request.settings->Get(ANDROID_CONTROL_AF_TRIGGER, &entry) != OK ||
      *entry.data.u8 != ANDROID_CONTROL_AF_TRIGGER_IDLE) {
    ALOGI("%s: (%d)ANDROID_CONTROL_AF_TRIGGER is not IDLE", __FUNCTION__,
          request.frame_number);
    return false;
  }

  // For b/130768200, treat the request as non-HDR+ request
  // if only request one preview frame output.
  if (preview_stream_id != -1 && request.output_buffers.size() == 1 &&
      request.output_buffers[0].stream_id == preview_stream_id) {
    ALOGI("%s: (%d)Only request preview frame", __FUNCTION__,
          request.frame_number);
    return false;
  }

  return true;
}

bool IsStreamHdrplusCompatible(const StreamConfiguration& stream_config,
                               const HalCameraMetadata* characteristics) {
  static const uint32_t kHdrplusSensorMaxFps = 30;
  if (characteristics == nullptr) {
    ALOGE("%s: characteristics is nullptr", __FUNCTION__);
    return false;
  }

  if (property_get_bool("persist.camera.hdrplus.disable", false)) {
    ALOGI("%s: HDR+ is disabled by property", __FUNCTION__);
    return false;
  }

  camera_metadata_ro_entry entry;
  status_t res =
      characteristics->Get(VendorTagIds::kHdrplusPayloadFrames, &entry);
  if (res != OK || entry.data.i32[0] <= 0) {
    ALOGW("%s: Getting kHdrplusPayloadFrames failed or number <= 0",
          __FUNCTION__);
    return false;
  }

  if (stream_config.operation_mode != StreamConfigurationMode::kNormal) {
    ALOGI("%s: Only support normal mode. operation_mode = %d", __FUNCTION__,
          stream_config.operation_mode);
    return false;
  }

  if (property_get_bool("persist.camera.fatp.enable", false)) {
    ALOGI("%s: Do not use HDR+ for FATP mode", __FUNCTION__);
    return false;
  }

  if (stream_config.session_params != nullptr &&
      stream_config.session_params->Get(ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
                                        &entry) == OK) {
    uint32_t max_fps = entry.data.i32[1];
    if (max_fps > kHdrplusSensorMaxFps) {
      ALOGI("%s: the fps (%d) is over HDR+ support.", __FUNCTION__, max_fps);
      return false;
    }
  }

  if (stream_config.session_params != nullptr) {
    camera_metadata_ro_entry entry;
    status_t result = stream_config.session_params->Get(
        VendorTagIds::kHdrPlusDisabled, &entry);

    if ((result == OK) && (entry.data.u8[0] == 1)) {
      ALOGI("%s: request.disable_hdrplus true", __FUNCTION__);
      return false;
    }
  }

  bool preview_stream = false;
  bool jpeg_stream = false;
  bool has_logical_stream = false;
  bool has_physical_stream = false;
  uint32_t yuv_num = 0;
  uint32_t last_physical_cam_id = 0;

  for (auto stream : stream_config.streams) {
    if (utils::IsPreviewStream(stream)) {
      preview_stream = true;
    } else if (utils::IsJPEGSnapshotStream(stream)) {
      jpeg_stream = true;
    } else if (utils::IsDepthStream(stream)) {
      ALOGI("%s: Don't support depth stream", __FUNCTION__);
      return false;
    } else if (utils::IsVideoStream(stream)) {
      ALOGI("%s: Don't support video stream", __FUNCTION__);
      return false;
    } else if (utils::IsArbitraryDataSpaceRawStream(stream)) {
      ALOGI("%s: Don't support raw stream", __FUNCTION__);
      return false;
    } else if (utils::IsYUVSnapshotStream(stream)) {
      yuv_num++;
    } else {
      ALOGE("%s: Unknown stream type %d, res %ux%u, format %d, usage %" PRIu64,
            __FUNCTION__, stream.stream_type, stream.width, stream.height,
            stream.format, stream.usage);
      return false;
    }

    if (stream.is_physical_camera_stream) {
      if (has_physical_stream &&
          stream.physical_camera_id != last_physical_cam_id) {
        // b/137721824, we don't support HDR+ if stream configuration contains
        // different physical camera id streams.
        ALOGI("%s: Don't support different physical camera id streams",
              __FUNCTION__);
        return false;
      }
      has_physical_stream = true;
      last_physical_cam_id = stream.physical_camera_id;
    } else {
      has_logical_stream = true;
    }
  }

  // Only preview is configured.
  if (preview_stream == true && jpeg_stream == false && yuv_num == 0) {
    ALOGI("%s: Only preview is configured.", __FUNCTION__);
    return false;
  }

  // No preview is configured.
  if (preview_stream == false) {
    ALOGI("%s: no preview is configured.", __FUNCTION__);
    return false;
  }

  // b/137721824, we don't support HDR+ if stream configuration contains
  // logical and physical streams.
  if (has_logical_stream == true && has_physical_stream == true) {
    ALOGI("%s: Don't support logical and physical combination", __FUNCTION__);
    return false;
  }

  // TODO(b/128633958): remove this after depth block is in place
  if (property_get_bool("persist.camera.rgbird.forceinternal", false)) {
    return false;
  }

  return true;
}

status_t SetEnableZslMetadata(HalCameraMetadata* metadata, bool enable) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  uint8_t enable_zsl = enable ? 1 : 0;
  status_t res = metadata->Set(ANDROID_CONTROL_ENABLE_ZSL, &enable_zsl, 1);
  if (res != OK) {
    ALOGE("%s: set %d fail", __FUNCTION__, enable_zsl);
    return res;
  }

  return OK;
}

status_t SetHybridAeMetadata(HalCameraMetadata* metadata, bool enable) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  status_t res;
  int32_t enable_hybrid_ae = enable ? 1 : 0;
  res = metadata->Set(VendorTagIds::kHybridAeEnabled, &enable_hybrid_ae,
                      /*data_count=*/1);
  if (res != OK) {
    ALOGE("%s: enable_hybrid_ae(%d) fail", __FUNCTION__, enable_hybrid_ae);
    return res;
  }

  return OK;
}

status_t ForceLensShadingMapModeOn(HalCameraMetadata* metadata) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry entry;
  if (metadata->Get(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE, &entry) == OK &&
      *entry.data.u8 == ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF) {
    // Force enabling LENS_SHADING_MAP_MODE_ON.
    uint8_t mode = ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_ON;
    status_t result =
        metadata->Set(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE, &mode, 1);
    if (result != OK) {
      ALOGE("%s: Set LENS_SHADING_MAP_MODE on fail", __FUNCTION__);
      return result;
    }
  }

  return OK;
}

status_t ModifyRealtimeRequestForHdrplus(HalCameraMetadata* metadata,
                                         const bool hybrid_ae_enable) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  // Update hybrid AE
  status_t result = SetHybridAeMetadata(metadata, hybrid_ae_enable);
  if (result != OK) {
    ALOGE("%s: SetHybridAeMetadata fail", __FUNCTION__);
    return result;
  }

  // Update FD mode
  camera_metadata_ro_entry entry;
  if (metadata->Get(ANDROID_STATISTICS_FACE_DETECT_MODE, &entry) == OK &&
      *entry.data.u8 == ANDROID_STATISTICS_FACE_DETECT_MODE_OFF) {
    // Force enabling face detect mode to simple.
    uint8_t mode = ANDROID_STATISTICS_FACE_DETECT_MODE_SIMPLE;
    result = metadata->Set(ANDROID_STATISTICS_FACE_DETECT_MODE, &mode, 1);
    if (result != OK) {
      ALOGE("%s: update FD simple mode fail", __FUNCTION__);
      return result;
    }
  }

  // Force lens shading mode to on
  result = ForceLensShadingMapModeOn(metadata);
  if (result != OK) {
    ALOGE("%s: ForceLensShadingMapModeOn fail", __FUNCTION__);
    return result;
  }

  return OK;
}

status_t GetLensShadingMapMode(const CaptureRequest& request,
                               uint8_t* lens_shading_mode) {
  if (request.settings == nullptr || lens_shading_mode == nullptr) {
    ALOGE("%s: request.settings or lens_shading_mode is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry entry;
  status_t result =
      request.settings->Get(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE, &entry);
  if (result != OK) {
    ALOGV("%s: Get LENS_SHADING_MAP_MODE fail", __FUNCTION__);
    return result;
  }
  *lens_shading_mode = *entry.data.u8;

  return OK;
}

status_t RemoveLsInfoFromResult(HalCameraMetadata* metadata) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry entry;
  status_t res;
  if (metadata->Get(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE, &entry) == OK) {
    // Change lens shading map mode to OFF.
    uint8_t mode = ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF;
    res = metadata->Set(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE, &mode, 1);
    if (res != OK) {
      ALOGE("%s: Set LENS_SHADING_MAP_MODE off fail", __FUNCTION__);
      return res;
    }
  }

  // Erase lens shading map.
  res = metadata->Erase(ANDROID_STATISTICS_LENS_SHADING_MAP);
  if (res != OK) {
    ALOGE("%s: erase LENS_SHADING_MAP fail", __FUNCTION__);
    return res;
  }

  return OK;
}

status_t GetFdMode(const CaptureRequest& request, uint8_t* face_detect_mode) {
  if (request.settings == nullptr || face_detect_mode == nullptr) {
    ALOGE("%s: request.settings or face_detect_mode is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry entry;
  status_t result =
      request.settings->Get(ANDROID_STATISTICS_FACE_DETECT_MODE, &entry);
  if (result != OK) {
    ALOGV("%s: Get FACE_DETECT_MODE fail", __FUNCTION__);
    return result;
  }
  *face_detect_mode = *entry.data.u8;

  return OK;
}

status_t RemoveFdInfoFromResult(HalCameraMetadata* metadata) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry entry;
  status_t res;
  if (metadata->Get(ANDROID_STATISTICS_FACE_DETECT_MODE, &entry) == OK) {
    uint8_t mode = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
    res = metadata->Set(ANDROID_STATISTICS_FACE_DETECT_MODE, &mode, 1);
    if (res != OK) {
      ALOGE("%s: update FD off mode fail", __FUNCTION__);
      return res;
    }
  }

  res = metadata->Erase(ANDROID_STATISTICS_FACE_RECTANGLES);
  if (res != OK) {
    ALOGE("%s: erase face rectangles fail", __FUNCTION__);
    return res;
  }

  res = metadata->Erase(ANDROID_STATISTICS_FACE_SCORES);
  if (res != OK) {
    ALOGE("%s: erase face scores fail", __FUNCTION__);
    return res;
  }

  return OK;
}

void DumpStreamConfiguration(const StreamConfiguration& stream_configuration,
                             std::string title) {
  std::string str = "======== " + title + " ========";
  ALOGI("%s", str.c_str());
  ALOGI("== stream num: %zu, operation_mode:%d",
        stream_configuration.streams.size(),
        stream_configuration.operation_mode);
  for (uint32_t i = 0; i < stream_configuration.streams.size(); i++) {
    auto& stream = stream_configuration.streams[i];
    ALOGI("==== [%u]stream_id %d, format %d, res %ux%u, usage %" PRIu64
          ", is_phy %d, phy_cam_id %u",
          i, stream.id, stream.format, stream.width, stream.height, stream.usage,
          stream.is_physical_camera_stream, stream.physical_camera_id);
  }
  ALOGI("%s", str.c_str());
}

void DumpHalConfiguredStreams(
    const std::vector<HalStream>& hal_configured_streams, std::string title) {
  std::string str = "======== " + title + " ========";
  ALOGI("%s", str.c_str());
  ALOGI("== stream num: %zu", hal_configured_streams.size());
  for (uint32_t i = 0; i < hal_configured_streams.size(); i++) {
    auto& stream = hal_configured_streams[i];
    ALOGI("==== [%u]stream_id:%5d override_format:%8x p_usage:%" PRIu64
          " c_usage:%" PRIu64 " max_buf:%u is_phy:%d",
          i, stream.id, stream.override_format, stream.producer_usage,
          stream.consumer_usage, stream.max_buffers,
          stream.is_physical_camera_stream);
  }
  ALOGI("%s", str.c_str());
}

void DumpCaptureRequest(const CaptureRequest& request, std::string title) {
  std::string str = "======== " + title + " ========";
  ALOGI("%s", str.c_str());
  ALOGI("== frame_number:%u", request.frame_number);
  ALOGI("== settings:%p", request.settings.get());
  ALOGI("== num_output_buffers:%zu", request.output_buffers.size());
  for (uint32_t i = 0; i < request.output_buffers.size(); i++) {
    ALOGI("==== buf[%d] stream_id:%d buf:%p", i,
          request.output_buffers[i].stream_id, request.output_buffers[i].buffer);
  }
  ALOGI("== num_input_buffers:%zu", request.input_buffers.size());
  for (uint32_t i = 0; i < request.input_buffers.size(); i++) {
    ALOGI("==== buf[%d] stream_id:%d buf:%p", i,
          request.input_buffers[i].stream_id, request.input_buffers[i].buffer);
  }
  ALOGI("%s", str.c_str());
}

void DumpCaptureResult(const ProcessBlockResult& result, std::string title) {
  std::string str = "======== " + title + " ========";
  ALOGI("%s", str.c_str());
  ALOGI("== frame_number:%u", result.result->frame_number);
  ALOGI("== num_output_buffers:%zu", result.result->output_buffers.size());
  for (uint32_t i = 0; i < result.result->output_buffers.size(); i++) {
    ALOGI("==== buf[%d] stream_id:%d bud:%" PRIu64 " handle: %p status: %d", i,
          result.result->output_buffers[i].stream_id,
          result.result->output_buffers[i].buffer_id,
          result.result->output_buffers[i].buffer,
          result.result->output_buffers[i].status);
  }
  ALOGI("== has_metadata:%d", result.result->result_metadata != nullptr);
  ALOGI("== request_id:%d", result.request_id);
  ALOGI("%s", str.c_str());
}

void DumpCaptureResult(const CaptureResult& result, std::string title) {
  std::string str = "======== " + title + " ========";
  ALOGI("%s", str.c_str());
  ALOGI("== frame_number:%u", result.frame_number);
  ALOGI("== num_output_buffers:%zu", result.output_buffers.size());
  for (uint32_t i = 0; i < result.output_buffers.size(); i++) {
    ALOGI("==== buf[%d] stream_id:%d bud:%" PRIu64 " handle: %p status: %d", i,
          result.output_buffers[i].stream_id, result.output_buffers[i].buffer_id,
          result.output_buffers[i].buffer, result.output_buffers[i].status);
  }
  ALOGI("== has_metadata:%d", result.result_metadata != nullptr);
  ALOGI("%s", str.c_str());
}

void DumpNotify(const NotifyMessage& message, std::string title) {
  std::string str = "======== " + title + " ========";
  ALOGI("%s", str.c_str());
  if (message.type == MessageType::kShutter) {
    ALOGI("== frame_number:%u", message.message.shutter.frame_number);
    ALOGI("== time_stamp:%" PRIu64, message.message.shutter.timestamp_ns);
  } else if (message.type == MessageType::kError) {
    ALOGI("== frame_number:%u", message.message.error.frame_number);
    ALOGI("== error_code:%u", message.message.error.error_code);
  }
  ALOGI("%s", str.c_str());
}

void DumpStream(const Stream& stream, std::string title) {
  std::string str = "======== " + title + " ========";
  ALOGI("%s", str.c_str());
  ALOGI("== stream_id %d, format %d, res %ux%u, usage %" PRIu64
        ", is_phy %d, phy_cam_id %u",
        stream.id, stream.format, stream.width, stream.height, stream.usage,
        stream.is_physical_camera_stream, stream.physical_camera_id);
  ALOGI("%s", str.c_str());
}

// Dump HalStream
void DumpHalStream(const HalStream& hal_stream, std::string title) {
  std::string str = "======== " + title + " ========";
  ALOGI("%s", str.c_str());
  ALOGI("== id %d, override_format %d, producer_usage %" PRIu64 ", %" PRIu64
        ", max_buffers %u, override_data_space %u, is_phy %u, phy_cam_id %d",
        hal_stream.id, hal_stream.override_format, hal_stream.producer_usage,
        hal_stream.consumer_usage, hal_stream.max_buffers,
        hal_stream.override_data_space, hal_stream.is_physical_camera_stream,
        hal_stream.physical_camera_id);
  ALOGI("%s", str.c_str());
}

void DumpBufferReturn(const std::vector<StreamBuffer>& stream_buffers,
                      std::string title) {
  std::string str = "======== " + title + " ========";
  ALOGI("%s", str.c_str());
  for (auto stream_buffer : stream_buffers) {
    ALOGI("== Strm id:%d, buf id:%" PRIu64, stream_buffer.stream_id,
          stream_buffer.buffer_id);
  }
  ALOGI("%s", str.c_str());
}

void DumpBufferRequest(const std::vector<BufferRequest>& hal_buffer_requests,
                       const std::vector<BufferReturn>* hal_buffer_returns,
                       std::string title) {
  std::string str = "======== " + title + " ========";
  ALOGI("%s", str.c_str());
  for (const auto& buffer_request : hal_buffer_requests) {
    ALOGI("== Strm id:%d", buffer_request.stream_id);
  }
  ALOGI("===");
  for (const auto& buffer_return : *hal_buffer_returns) {
    for (const auto& stream_buffer : buffer_return.val.buffers) {
      ALOGI("== buf id:%" PRIu64 " stm id:%d buf:%p", stream_buffer.buffer_id,
            stream_buffer.stream_id, stream_buffer.buffer);
    }
  }
  ALOGI("%s", str.c_str());
}

}  // namespace hal_utils
}  // namespace google_camera_hal
}  // namespace android
