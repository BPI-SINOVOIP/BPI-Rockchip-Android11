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
#define LOG_TAG "GCH_RgbirdResultRequestProcessor"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <cutils/properties.h>
#include <log/log.h>
#include <sync/sync.h>
#include <utils/Trace.h>

#include <cutils/native_handle.h>
#include <inttypes.h>

#include "hal_utils.h"
#include "rgbird_result_request_processor.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<RgbirdResultRequestProcessor> RgbirdResultRequestProcessor::Create(
    const RgbirdResultRequestProcessorCreateData& create_data) {
  ATRACE_CALL();
  auto result_processor = std::unique_ptr<RgbirdResultRequestProcessor>(
      new RgbirdResultRequestProcessor(create_data));
  if (result_processor == nullptr) {
    ALOGE("%s: Creating RgbirdResultRequestProcessor failed.", __FUNCTION__);
    return nullptr;
  }

  // TODO(b/128633958): remove this after FLL syncing is verified
  result_processor->force_internal_stream_ =
      property_get_bool("persist.camera.rgbird.forceinternal", false);
  if (result_processor->force_internal_stream_) {
    ALOGI("%s: Force creating internal streams for IR pipelines", __FUNCTION__);
  }

  // TODO(b/129910835): Change the controlling prop into some deterministic
  // logic that controls when the front depth autocal will be triggered.
  result_processor->rgb_ir_auto_cal_enabled_ =
      property_get_bool("vendor.camera.frontdepth.enableautocal", true);
  if (result_processor->rgb_ir_auto_cal_enabled_) {
    ALOGI("%s: autocal is enabled.", __FUNCTION__);
  }

  return result_processor;
}

RgbirdResultRequestProcessor::RgbirdResultRequestProcessor(
    const RgbirdResultRequestProcessorCreateData& create_data)
    : kRgbCameraId(create_data.rgb_camera_id),
      kIr1CameraId(create_data.ir1_camera_id),
      kIr2CameraId(create_data.ir2_camera_id),
      rgb_raw_stream_id_(create_data.rgb_raw_stream_id),
      is_hdrplus_supported_(create_data.is_hdrplus_supported),
      rgb_internal_yuv_stream_id_(create_data.rgb_internal_yuv_stream_id) {
}

void RgbirdResultRequestProcessor::SetResultCallback(
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify) {
  std::lock_guard<std::mutex> lock(callback_lock_);
  process_capture_result_ = process_capture_result;
  notify_ = notify;
}

void RgbirdResultRequestProcessor::SaveFdForHdrplus(
    const CaptureRequest& request) {
  // Enable face detect mode for internal use
  if (request.settings != nullptr) {
    uint8_t fd_mode;
    status_t res = hal_utils::GetFdMode(request, &fd_mode);
    if (res == OK) {
      current_face_detect_mode_ = fd_mode;
    }
  }

  {
    std::lock_guard<std::mutex> lock(face_detect_lock_);
    requested_face_detect_modes_.emplace(request.frame_number,
                                         current_face_detect_mode_);
  }
}

void RgbirdResultRequestProcessor::SaveLsForHdrplus(
    const CaptureRequest& request) {
  if (request.settings != nullptr) {
    uint8_t lens_shading_map_mode;
    status_t res =
        hal_utils::GetLensShadingMapMode(request, &lens_shading_map_mode);
    if (res == OK) {
      current_lens_shading_map_mode_ = lens_shading_map_mode;
    }
  }

  {
    std::lock_guard<std::mutex> lock(lens_shading_lock_);
    requested_lens_shading_map_modes_.emplace(request.frame_number,
                                              current_lens_shading_map_mode_);
  }
}

status_t RgbirdResultRequestProcessor::HandleLsResultForHdrplus(
    uint32_t frameNumber, HalCameraMetadata* metadata) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }
  std::lock_guard<std::mutex> lock(lens_shading_lock_);
  auto iter = requested_lens_shading_map_modes_.find(frameNumber);
  if (iter == requested_lens_shading_map_modes_.end()) {
    ALOGW("%s: can't find frame (%d)", __FUNCTION__, frameNumber);
    return OK;
  }

  if (iter->second == ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF) {
    status_t res = hal_utils::RemoveLsInfoFromResult(metadata);
    if (res != OK) {
      ALOGW("%s: RemoveLsInfoFromResult fail", __FUNCTION__);
    }
  }
  requested_lens_shading_map_modes_.erase(iter);

  return OK;
}

bool RgbirdResultRequestProcessor::IsAutocalRequest(uint32_t frame_number) const {
  // TODO(b/129910835): Use the proper logic to control when internal yuv buffer
  // needs to be passed to the depth process block. Even if the auto cal is
  // enabled, there is no need to pass the internal yuv buffer for every
  // request, not even every device session. This is also related to how the
  // buffer is added into the request. Similar logic exists in realtime request
  // processor. However, this logic can further filter and determine which
  // requests contain the internal yuv stream buffers and send them to the depth
  // process block. Current implementation only treat the kAutocalFrameNumber
  // request as autocal request. This must be consistent with that of the
  // rt_request_processor.
  if (!rgb_ir_auto_cal_enabled_) {
    return false;
  }

  return frame_number == kAutocalFrameNumber;
}

void RgbirdResultRequestProcessor::TryReturnInternalBufferForDepth(
    CaptureResult* result, bool* has_internal) {
  ATRACE_CALL();
  if (result == nullptr || has_internal == nullptr) {
    ALOGE("%s: result or has_rgb_raw_output is nullptr", __FUNCTION__);
    return;
  }

  if (internal_stream_manager_ == nullptr) {
    ALOGE("%s: internal_stream_manager_ nullptr", __FUNCTION__);
    return;
  }

  std::vector<StreamBuffer> modified_output_buffers;
  for (uint32_t i = 0; i < result->output_buffers.size(); i++) {
    if (rgb_internal_yuv_stream_id_ == result->output_buffers[i].stream_id &&
        !IsAutocalRequest(result->frame_number)) {
      *has_internal = true;
      status_t res = internal_stream_manager_->ReturnStreamBuffer(
          result->output_buffers[i]);
      if (res != OK) {
        ALOGW("%s: Failed to return RGB internal raw buffer for frame %d",
              __FUNCTION__, result->frame_number);
      }
    } else {
      modified_output_buffers.push_back(result->output_buffers[i]);
    }
  }

  if (!result->output_buffers.empty()) {
    result->output_buffers = modified_output_buffers;
  }
}

status_t RgbirdResultRequestProcessor::HandleFdResultForHdrplus(
    uint32_t frameNumber, HalCameraMetadata* metadata) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }
  std::lock_guard<std::mutex> lock(face_detect_lock_);
  auto iter = requested_face_detect_modes_.find(frameNumber);
  if (iter == requested_face_detect_modes_.end()) {
    ALOGW("%s: can't find frame (%d)", __FUNCTION__, frameNumber);
    return OK;
  }

  if (iter->second == ANDROID_STATISTICS_FACE_DETECT_MODE_OFF) {
    status_t res = hal_utils::RemoveFdInfoFromResult(metadata);
    if (res != OK) {
      ALOGW("%s: RestoreFdMetadataForHdrplus fail", __FUNCTION__);
    }
  }
  requested_face_detect_modes_.erase(iter);

  return OK;
}

status_t RgbirdResultRequestProcessor::AddPendingRequests(
    const std::vector<ProcessBlockRequest>& /*process_block_requests*/,
    const CaptureRequest& remaining_session_request) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(depth_requests_mutex_);
  for (auto stream_buffer : remaining_session_request.output_buffers) {
    if (stream_buffer.acquire_fence != nullptr) {
      stream_buffer.acquire_fence =
          native_handle_clone(stream_buffer.acquire_fence);
      if (stream_buffer.acquire_fence == nullptr) {
        ALOGE("%s: Cloning acquire_fence of buffer failed", __FUNCTION__);
        return UNKNOWN_ERROR;
      }
    }
    if (depth_stream_id_ == stream_buffer.stream_id) {
      ALOGV("%s: request %d has a depth buffer", __FUNCTION__,
            remaining_session_request.frame_number);
      auto capture_request = std::make_unique<CaptureRequest>();
      capture_request->frame_number = remaining_session_request.frame_number;
      if (remaining_session_request.settings != nullptr) {
        capture_request->settings =
            HalCameraMetadata::Clone(remaining_session_request.settings.get());
      }
      capture_request->input_buffers.clear();
      capture_request->output_buffers.push_back(stream_buffer);
      depth_requests_.emplace(remaining_session_request.frame_number,
                              std::move(capture_request));
      break;
    }
  }

  if (is_hdrplus_supported_) {
    SaveFdForHdrplus(remaining_session_request);
    SaveLsForHdrplus(remaining_session_request);
  }
  return OK;
}

void RgbirdResultRequestProcessor::ProcessResultForHdrplus(CaptureResult* result,
                                                           bool* rgb_raw_output) {
  ATRACE_CALL();
  if (result == nullptr || rgb_raw_output == nullptr) {
    ALOGE("%s: result or rgb_raw_output is nullptr", __FUNCTION__);
    return;
  }

  if (internal_stream_manager_ == nullptr) {
    ALOGE("%s: internal_stream_manager_ nullptr", __FUNCTION__);
    return;
  }

  // Return filled raw buffer to internal stream manager
  // And remove raw buffer from result
  status_t res;
  std::vector<StreamBuffer> modified_output_buffers;
  for (uint32_t i = 0; i < result->output_buffers.size(); i++) {
    if (rgb_raw_stream_id_ == result->output_buffers[i].stream_id) {
      *rgb_raw_output = true;
      res = internal_stream_manager_->ReturnFilledBuffer(
          result->frame_number, result->output_buffers[i]);
      if (res != OK) {
        ALOGW("%s: (%d)ReturnStreamBuffer fail", __FUNCTION__,
              result->frame_number);
      }
    } else {
      modified_output_buffers.push_back(result->output_buffers[i]);
    }
  }

  if (result->output_buffers.size() > 0) {
    result->output_buffers = modified_output_buffers;
  }

  if (result->result_metadata) {
    res = internal_stream_manager_->ReturnMetadata(
        rgb_raw_stream_id_, result->frame_number, result->result_metadata.get());
    if (res != OK) {
      ALOGW("%s: (%d)ReturnMetadata fail", __FUNCTION__, result->frame_number);
    }

    res = HandleFdResultForHdrplus(result->frame_number,
                                   result->result_metadata.get());
    if (res != OK) {
      ALOGE("%s: HandleFdResultForHdrplus(%d) fail", __FUNCTION__,
            result->frame_number);
      return;
    }

    res = HandleLsResultForHdrplus(result->frame_number,
                                   result->result_metadata.get());
    if (res != OK) {
      ALOGE("%s: HandleLsResultForHdrplus(%d) fail", __FUNCTION__,
            result->frame_number);
      return;
    }
  }
}

status_t RgbirdResultRequestProcessor::ReturnInternalStreams(
    CaptureResult* result) {
  ATRACE_CALL();
  if (result == nullptr) {
    ALOGE("%s: block_result is null.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  std::vector<StreamBuffer> modified_output_buffers;
  for (auto& stream_buffer : result->output_buffers) {
    if (framework_stream_id_set_.find(stream_buffer.stream_id) ==
        framework_stream_id_set_.end()) {
      status_t res = internal_stream_manager_->ReturnStreamBuffer(stream_buffer);
      if (res != OK) {
        ALOGE("%s: Failed to return stream buffer.", __FUNCTION__);
        return UNKNOWN_ERROR;
      }
    } else {
      modified_output_buffers.push_back(stream_buffer);
    }
  }
  result->output_buffers = modified_output_buffers;
  return OK;
}

status_t RgbirdResultRequestProcessor::CheckFenceStatus(CaptureRequest* request) {
  int fence_status = 0;

  if (request == nullptr) {
    ALOGE("%s: request is null.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  for (uint32_t i = 0; i < request->output_buffers.size(); i++) {
    if (request->output_buffers[i].acquire_fence != nullptr) {
      auto fence =
          const_cast<native_handle_t*>(request->output_buffers[i].acquire_fence);
      if (fence->numFds == 1) {
        fence_status = sync_wait(fence->data[0], kSyncWaitTime);
      }
      if (0 != fence_status) {
        ALOGE("%s: Fence check failed.", __FUNCTION__);
        return UNKNOWN_ERROR;
      }
      native_handle_close(fence);
      native_handle_delete(fence);
      request->output_buffers[i].acquire_fence = nullptr;
    }
  }

  return OK;
}

bool RgbirdResultRequestProcessor::IsAutocalMetadataReadyLocked(
    const HalCameraMetadata& metadata) {
  camera_metadata_ro_entry entry = {};
  if (metadata.Get(VendorTagIds::kNonWarpedCropRegion, &entry) != OK) {
    ALOGV("%s Get kNonWarpedCropRegion, tag fail.", __FUNCTION__);
    return false;
  }

  uint8_t fd_mode = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
  if (metadata.Get(ANDROID_STATISTICS_FACE_DETECT_MODE, &entry) != OK) {
    ALOGV("%s Get ANDROID_STATISTICS_FACE_DETECT_MODE tag fail.", __FUNCTION__);
    return false;
  } else {
    fd_mode = *entry.data.u8;
  }

  // If FD mode is off, don't need to check FD related metadata.
  if (fd_mode != ANDROID_STATISTICS_FACE_DETECT_MODE_OFF) {
    if (metadata.Get(ANDROID_STATISTICS_FACE_RECTANGLES, &entry) != OK) {
      ALOGV("%s Get ANDROID_STATISTICS_FACE_RECTANGLES tag fail.", __FUNCTION__);
      return false;
    }
    if (metadata.Get(ANDROID_STATISTICS_FACE_SCORES, &entry) != OK) {
      ALOGV("%s Get ANDROID_STATISTICS_FACE_SCORES tag fail.", __FUNCTION__);
      return false;
    }
  }

  return true;
}

status_t RgbirdResultRequestProcessor::VerifyAndSubmitDepthRequest(
    uint32_t frame_number) {
  std::lock_guard<std::mutex> lock(depth_requests_mutex_);
  if (depth_requests_.find(frame_number) == depth_requests_.end()) {
    ALOGW("%s: Can not find depth request with frame number %u", __FUNCTION__,
          frame_number);
    return NAME_NOT_FOUND;
  }

  uint32_t valid_input_buffer_num = 0;
  auto& depth_request = depth_requests_[frame_number];
  for (auto& input_buffer : depth_request->input_buffers) {
    if (input_buffer.stream_id != kInvalidStreamId) {
      valid_input_buffer_num++;
    }
  }

  if (IsAutocalRequest(frame_number)) {
    if (valid_input_buffer_num != /*rgb+ir1+ir2*/ 3) {
      // not all input buffers are ready, early return properly
      ALOGV("%s: Not all input buffers are ready for frame %u", __FUNCTION__,
            frame_number);
      return OK;
    }
  } else {
    // The input buffer for RGB pipeline could be a place holder to be
    // consistent with the input buffer metadata.
    if (valid_input_buffer_num != /*ir1+ir2*/ 2) {
      // not all input buffers are ready, early return properly
      ALOGV("%s: Not all input buffers are ready for frame %u", __FUNCTION__,
            frame_number);
      return OK;
    }
  }

  if (depth_request->input_buffer_metadata.empty()) {
    // input buffer metadata is not ready(cloned) yet, early return properly
    ALOGV("%s: Input buffer metadata is not ready for frame %u", __FUNCTION__,
          frame_number);
    return OK;
  }

  // Check against all metadata needed before move on e.g. check against
  // cropping info, FD result for internal YUV stream
  status_t res = OK;
  if (IsAutocalRequest(frame_number)) {
    bool is_ready = false;
    for (auto& metadata : depth_request->input_buffer_metadata) {
      if (metadata != nullptr) {
        is_ready = IsAutocalMetadataReadyLocked(*(metadata.get()));
      }
    }
    if (!is_ready) {
      ALOGV("%s: Not all AutoCal Metadata is ready for frame %u.", __FUNCTION__,
            frame_number);
      return OK;
    }
  }

  res = CheckFenceStatus(depth_request.get());
  if (res != OK) {
    ALOGE("%s:Fence status wait failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  res = ProcessRequest(*depth_request.get());
  if (res != OK) {
    ALOGE("%s: Failed to submit process request to depth process block.",
          __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  depth_requests_.erase(frame_number);
  return OK;
}

status_t RgbirdResultRequestProcessor::TrySubmitDepthProcessBlockRequest(
    const ProcessBlockResult& block_result) {
  ATRACE_CALL();
  uint32_t request_id = block_result.request_id;
  CaptureResult* result = block_result.result.get();
  uint32_t frame_number = result->frame_number;

  bool pending_request_updated = false;
  for (auto& output_buffer : result->output_buffers) {
    if (request_id == kIr1CameraId || request_id == kIr2CameraId ||
        (request_id == kRgbCameraId &&
         rgb_internal_yuv_stream_id_ == output_buffer.stream_id &&
         IsAutocalRequest(frame_number))) {
      std::lock_guard<std::mutex> lock(depth_requests_mutex_);

      // In case depth request is flushed
      if (depth_requests_.find(frame_number) == depth_requests_.end()) {
        ALOGV("%s: Can not find depth request with frame number %u",
              __FUNCTION__, frame_number);
        status_t res =
            internal_stream_manager_->ReturnStreamBuffer(output_buffer);
        if (res != OK) {
          ALOGW(
              "%s: Failed to return internal buffer for flushed depth request"
              " %u",
              __FUNCTION__, frame_number);
        }
        continue;
      }

      // If input_buffer_metadata is not empty, the RGB pipeline result metadata
      // must have been cloned(other entries for IRs set to nullptr). The
      // yuv_internal_stream buffer has to be inserted into the corresponding
      // entry in input_buffers. Or if this is not a AutoCal request, the stream
      // id for the place holder of the RGB input buffer must be invalid. Refer
      // the logic below for result metadata handling.
      const auto& metadata_list =
          depth_requests_[frame_number]->input_buffer_metadata;
      auto& input_buffers = depth_requests_[frame_number]->input_buffers;
      if (!metadata_list.empty()) {
        uint32_t rgb_metadata_index = 0;
        for (; rgb_metadata_index < metadata_list.size(); rgb_metadata_index++) {
          // Only the RGB pipeline result metadata is needed and cloned
          if (metadata_list[rgb_metadata_index] != nullptr) {
            break;
          }
        }

        if (rgb_metadata_index == metadata_list.size()) {
          ALOGE("%s: RGB result metadata not found. rgb_metadata_index %u",
                __FUNCTION__, rgb_metadata_index);
          return UNKNOWN_ERROR;
        }

        if (input_buffers.size() < kNumOfAutoCalInputBuffers) {
          input_buffers.resize(kNumOfAutoCalInputBuffers);
        }

        if (request_id == kRgbCameraId) {
          if (input_buffers[rgb_metadata_index].stream_id != kInvalidStreamId) {
            ALOGE("%s: YUV buffer already exists.", __FUNCTION__);
            return UNKNOWN_ERROR;
          }
          input_buffers[rgb_metadata_index] = output_buffer;
        } else {
          for (uint32_t i_buffer = 0; i_buffer < input_buffers.size();
               i_buffer++) {
            if (input_buffers[i_buffer].stream_id == kInvalidStreamId &&
                rgb_metadata_index != i_buffer) {
              input_buffers[i_buffer] = output_buffer;
              break;
            }
          }
        }
      } else {
        input_buffers.push_back(output_buffer);
      }
      pending_request_updated = true;
    }
  }

  if (result->result_metadata != nullptr && request_id == kRgbCameraId) {
    std::lock_guard<std::mutex> lock(depth_requests_mutex_);

    // In case a depth request is flushed
    if (depth_requests_.find(frame_number) == depth_requests_.end()) {
      ALOGV("%s No depth request for Autocal", __FUNCTION__);
      return OK;
    }

    // If YUV buffer exists in the input_buffers, the RGB pipeline metadata
    // needs to be inserted into the corresponding entry in
    // input_buffer_metadata. Otherwise, insert the RGB pipeline metadata into
    // the entry that is not reserved for any existing IR input buffer. Refer
    // above logic for input buffer preparation.
    auto& input_buffers = depth_requests_[frame_number]->input_buffers;
    auto& metadata_list = depth_requests_[frame_number]->input_buffer_metadata;
    metadata_list.resize(kNumOfAutoCalInputBuffers);
    uint32_t yuv_buffer_index = 0;
    for (; yuv_buffer_index < input_buffers.size(); yuv_buffer_index++) {
      if (input_buffers[yuv_buffer_index].stream_id ==
          rgb_internal_yuv_stream_id_) {
        break;
      }
    }

    if (yuv_buffer_index >= kNumOfAutoCalInputBuffers) {
      ALOGE("%s: input_buffers is full and YUV buffer not found.", __FUNCTION__);
      return UNKNOWN_ERROR;
    }

    metadata_list[yuv_buffer_index] =
        HalCameraMetadata::Clone(result->result_metadata.get());
    if (metadata_list[yuv_buffer_index] == nullptr) {
      ALOGE("%s: clone RGB pipeline result metadata failed.", __FUNCTION__);
      return UNKNOWN_ERROR;
    }
    pending_request_updated = true;

    // If metadata arrives after all IR buffers and there is not RGB buffer
    if (input_buffers.size() < kNumOfAutoCalInputBuffers) {
      input_buffers.resize(kNumOfAutoCalInputBuffers);
    }
  }

  if (pending_request_updated) {
    status_t res = VerifyAndSubmitDepthRequest(frame_number);
    if (res != OK) {
      ALOGE("%s: Failed to verify and submit depth request.", __FUNCTION__);
      return res;
    }
  }

  return OK;
}

void RgbirdResultRequestProcessor::ProcessResult(ProcessBlockResult block_result) {
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

  CaptureResult* result = block_result.result.get();

  bool has_internal_stream_buffer = false;
  if (is_hdrplus_supported_) {
    ProcessResultForHdrplus(result, &has_internal_stream_buffer);
  } else if (depth_stream_id_ != -1) {
    TryReturnInternalBufferForDepth(result, &has_internal_stream_buffer);
  }

  status_t res = OK;
  if (result->result_metadata) {
    res = hal_utils::SetEnableZslMetadata(result->result_metadata.get(), false);
    if (res != OK) {
      ALOGW("%s: SetEnableZslMetadata (%d) fail", __FUNCTION__,
            result->frame_number);
    }
  }

  // Don't send result to framework if only internal raw callback
  if (has_internal_stream_buffer && result->result_metadata == nullptr &&
      result->output_buffers.size() == 0 && result->input_buffers.size() == 0) {
    return;
  }

  // TODO(b/128633958): remove the following once FLL syncing is verified
  {
    std::lock_guard<std::mutex> lock(depth_requests_mutex_);
    if (((force_internal_stream_) ||
         (depth_requests_.find(result->frame_number) == depth_requests_.end())) &&
        (depth_stream_id_ != -1)) {
      res = ReturnInternalStreams(result);
      if (res != OK) {
        ALOGE("%s: Failed to return internal buffers.", __FUNCTION__);
        return;
      }
    }
  }

  // Save necessary data for depth process block request
  res = TrySubmitDepthProcessBlockRequest(block_result);
  if (res != OK) {
    ALOGE("%s: Failed to submit depth process block request.", __FUNCTION__);
    return;
  }

  if (block_result.request_id != kRgbCameraId) {
    return;
  }

  // If internal yuv stream remains in the result output buffer list, it must
  // be used by some other purposes and will be returned separately. It should
  // not be returned through the process_capture_result_. So we remove them here.
  if (!result->output_buffers.empty()) {
    auto iter = result->output_buffers.begin();
    while (iter != result->output_buffers.end()) {
      if (iter->stream_id == rgb_internal_yuv_stream_id_) {
        result->output_buffers.erase(iter);
        break;
      }
      iter++;
    }
  }

  process_capture_result_(std::move(block_result.result));
}

void RgbirdResultRequestProcessor::Notify(
    const ProcessBlockNotifyMessage& block_message) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(callback_lock_);
  if (notify_ == nullptr) {
    ALOGE("%s: notify_ is nullptr. Dropping a message.", __FUNCTION__);
    return;
  }

  const NotifyMessage& message = block_message.message;
  // Request ID is set to camera ID by RgbirdRtRequestProcessor.
  uint32_t camera_id = block_message.request_id;
  if (message.type == MessageType::kShutter && camera_id != kRgbCameraId) {
    // Only send out shutters from the lead camera.
    return;
  }

  notify_(block_message.message);
}

status_t RgbirdResultRequestProcessor::ConfigureStreams(
    InternalStreamManager* internal_stream_manager,
    const StreamConfiguration& stream_config,
    StreamConfiguration* process_block_stream_config) {
  ATRACE_CALL();
  if (process_block_stream_config == nullptr) {
    ALOGE("%s: process_block_stream_config is null.", __FUNCTION__);
    return BAD_VALUE;
  }

  if (internal_stream_manager == nullptr) {
    ALOGE("%s: internal_stream_manager is null.", __FUNCTION__);
    return BAD_VALUE;
  }
  internal_stream_manager_ = internal_stream_manager;

  if (is_hdrplus_supported_) {
    return OK;
  }

  process_block_stream_config->streams.clear();
  Stream depth_stream = {};
  for (auto& stream : stream_config.streams) {
    // stream_config passed to this ConfigureStreams must contain only framework
    // output and internal input streams
    if (stream.stream_type == StreamType::kOutput) {
      if (utils::IsDepthStream(stream)) {
        ALOGI("%s: Depth stream id: %u observed by RgbirdResReqProcessor.",
              __FUNCTION__, stream.id);
        depth_stream_id_ = stream.id;
        depth_stream = stream;
      }
      // record all framework output, save depth only for depth process block
      framework_stream_id_set_.insert(stream.id);
    } else if (stream.stream_type == StreamType::kInput) {
      process_block_stream_config->streams.push_back(stream);
    }
  }

  // TODO(b/128633958): remove force flag after FLL syncing is verified
  if (force_internal_stream_ || depth_stream_id_ != -1) {
    process_block_stream_config->streams.push_back(depth_stream);
    process_block_stream_config->operation_mode = stream_config.operation_mode;
    process_block_stream_config->session_params =
        HalCameraMetadata::Clone(stream_config.session_params.get());
    process_block_stream_config->stream_config_counter =
        stream_config.stream_config_counter;
  }

  return OK;
}

status_t RgbirdResultRequestProcessor::SetProcessBlock(
    std::unique_ptr<ProcessBlock> process_block) {
  ATRACE_CALL();
  if (process_block == nullptr) {
    ALOGE("%s: process_block is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(depth_process_block_lock_);
  if (depth_process_block_ != nullptr) {
    ALOGE("%s: Already configured.", __FUNCTION__);
    return ALREADY_EXISTS;
  }

  depth_process_block_ = std::move(process_block);
  return OK;
}

status_t RgbirdResultRequestProcessor::ProcessRequest(
    const CaptureRequest& request) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(depth_process_block_lock_);
  if (depth_process_block_ == nullptr) {
    ALOGE("%s: depth_process_block_ is null.", __FUNCTION__);
    return BAD_VALUE;
  }

  // Depth Process Block only handles one process block request each time
  std::vector<ProcessBlockRequest> process_block_requests(1);
  auto& block_request = process_block_requests[0];
  block_request.request_id = 0;
  CaptureRequest& physical_request = block_request.request;
  physical_request.frame_number = request.frame_number;
  physical_request.settings = HalCameraMetadata::Clone(request.settings.get());
  for (auto& metadata : request.input_buffer_metadata) {
    physical_request.input_buffer_metadata.emplace_back(
        HalCameraMetadata::Clone(metadata.get()));
  }
  physical_request.input_buffers = request.input_buffers;
  physical_request.output_buffers = request.output_buffers;

  return depth_process_block_->ProcessRequests(process_block_requests, request);
}

status_t RgbirdResultRequestProcessor::Flush() {
  ATRACE_CALL();

  std::lock_guard<std::mutex> lock(depth_process_block_lock_);
  if (depth_process_block_ == nullptr) {
    ALOGW("%s: depth_process_block_ is null.", __FUNCTION__);
    return OK;
  }

  return depth_process_block_->Flush();
}

status_t RgbirdResultRequestProcessor::FlushPendingRequests() {
  ATRACE_CALL();

  std::lock_guard<std::mutex> lock(callback_lock_);
  if (notify_ == nullptr) {
    ALOGE("%s: notify_ is nullptr. Dropping a message.", __FUNCTION__);
    return OK;
  }

  if (process_capture_result_ == nullptr) {
    ALOGE("%s: process_capture_result_ is nullptr. Dropping a result.",
          __FUNCTION__);
    return OK;
  }

  std::lock_guard<std::mutex> requests_lock(depth_requests_mutex_);
  for (auto& [frame_number, capture_request] : depth_requests_) {
    // Returns all internal stream buffers
    for (auto& input_buffer : capture_request->input_buffers) {
      if (input_buffer.stream_id != kInvalidStreamId) {
        status_t res =
            internal_stream_manager_->ReturnStreamBuffer(input_buffer);
        if (res != OK) {
          ALOGW("%s: Failed to return internal buffer for depth request %d",
                __FUNCTION__, frame_number);
        }
      }
    }

    // Notify buffer error for the depth stream output buffer
    const NotifyMessage message = {
        .type = MessageType::kError,
        .message.error = {.frame_number = frame_number,
                          .error_stream_id = depth_stream_id_,
                          .error_code = ErrorCode::kErrorBuffer}};
    notify_(message);

    // Return output buffer for the depth stream
    auto result = std::make_unique<CaptureResult>();
    result->frame_number = frame_number;
    for (auto& output_buffer : capture_request->output_buffers) {
      if (output_buffer.stream_id == depth_stream_id_) {
        result->output_buffers.push_back(output_buffer);
        auto& buffer = result->output_buffers.back();
        buffer.status = BufferStatus::kError;
        buffer.acquire_fence = nullptr;
        buffer.release_fence = nullptr;
        break;
      }
    }
    process_capture_result_(std::move(result));
  }
  depth_requests_.clear();
  ALOGI("%s: Flushing depth requests done. ", __FUNCTION__);
  return OK;
}

}  // namespace google_camera_hal
}  // namespace android
