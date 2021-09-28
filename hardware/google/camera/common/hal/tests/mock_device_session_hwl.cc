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

#define LOG_TAG "MockDeviceSessionHwl"
#include <log/log.h>

#include <hardware/gralloc.h>
#include <system/graphics-base.h>

#include "mock_device_session_hwl.h"

namespace android {
namespace google_camera_hal {

using ::testing::_;
using ::testing::Invoke;

FakeCameraDeviceSessionHwl::FakeCameraDeviceSessionHwl(
    uint32_t camera_id, const std::vector<uint32_t>& physical_camera_ids)
    : kCameraId(camera_id), kPhysicalCameraIds(physical_camera_ids) {
}

status_t FakeCameraDeviceSessionHwl::ConstructDefaultRequestSettings(
    RequestTemplate /*type*/,
    std::unique_ptr<HalCameraMetadata>* default_settings) {
  if (default_settings == nullptr) {
    return BAD_VALUE;
  }

  static constexpr uint32_t kDataBytes = 256;
  static constexpr uint32_t kNumEntries = 10;
  static constexpr int32_t kSensitivity = 200;

  *default_settings = HalCameraMetadata::Create(kNumEntries, kDataBytes);
  if (default_settings == nullptr) {
    ALOGE("%s: Cannot create a HalCameraMetadata", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  return (*default_settings)->Set(ANDROID_SENSOR_SENSITIVITY, &kSensitivity, 1);
}

status_t FakeCameraDeviceSessionHwl::PrepareConfigureStreams(
    const StreamConfiguration& /*overall_config*/) {
  return OK;
}

status_t FakeCameraDeviceSessionHwl::ConfigurePipeline(
    uint32_t camera_id, HwlPipelineCallback hwl_pipeline_callback,
    const StreamConfiguration& request_config,
    const StreamConfiguration& /*overall_config*/, uint32_t* pipeline_id) {
  if (pipeline_id == nullptr) {
    return BAD_VALUE;
  }

  // Check if the camera ID belongs to this camera.
  if (camera_id != kCameraId &&
      std::find(kPhysicalCameraIds.begin(), kPhysicalCameraIds.end(),
                camera_id) == kPhysicalCameraIds.end()) {
    ALOGE("%s: Unknown camera ID: %u", __FUNCTION__, camera_id);
    return BAD_VALUE;
  }

  static constexpr uint32_t kDefaultMaxBuffers = 3;
  std::vector<HalStream> hal_configured_streams;

  for (auto& stream : request_config.streams) {
    HalStream hal_stream = {};
    hal_stream.id = stream.id;
    hal_stream.override_format = stream.format;
    hal_stream.producer_usage = stream.usage;
    hal_stream.consumer_usage = GRALLOC_USAGE_HW_CAMERA_WRITE;
    hal_stream.max_buffers = kDefaultMaxBuffers;
    hal_stream.override_data_space = stream.data_space;
    hal_stream.is_physical_camera_stream = stream.is_physical_camera_stream;
    hal_stream.physical_camera_id = stream.physical_camera_id;

    if (hal_stream.override_format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
      hal_stream.override_format = HAL_PIXEL_FORMAT_YCRCB_420_SP;
    }

    hal_configured_streams.push_back(hal_stream);
  }

  {
    std::lock_guard<std::mutex> lock(hwl_pipeline_lock_);

    // Remember pipeline callback.
    static uint32_t next_available_pipeline_id = 0;
    hwl_pipeline_callbacks_.emplace(next_available_pipeline_id,
                                    hwl_pipeline_callback);
    *pipeline_id = next_available_pipeline_id++;

    // Rememember configured HAL streams.
    pipeline_hal_streams_map_.emplace(*pipeline_id, hal_configured_streams);
  }

  return OK;
}

status_t FakeCameraDeviceSessionHwl::BuildPipelines() {
  std::lock_guard<std::mutex> lock(hwl_pipeline_lock_);
  if (pipeline_hal_streams_map_.empty()) {
    return NO_INIT;
  }

  return OK;
}

status_t FakeCameraDeviceSessionHwl::GetRequiredIntputStreams(
    const StreamConfiguration& /*overall_config*/,
    HwlOfflinePipelineRole pipeline_role, std::vector<Stream>* streams) {
  // Only supports offline ST
  if (pipeline_role != HwlOfflinePipelineRole::kOfflineSmoothTransitionRole) {
    return BAD_VALUE;
  }
  if (streams == nullptr) {
    return BAD_VALUE;
  }

  for (int i = 0; i < 6; ++i) {
    Stream internal_stream;

    internal_stream.id = i;
    streams->push_back(internal_stream);
  }

  return OK;
}

status_t FakeCameraDeviceSessionHwl::GetConfiguredHalStream(
    uint32_t pipeline_id, std::vector<HalStream>* hal_streams) const {
  std::lock_guard<std::mutex> lock(hwl_pipeline_lock_);
  if (hal_streams == nullptr) {
    return BAD_VALUE;
  }

  if (pipeline_hal_streams_map_.empty()) {
    return NO_INIT;
  }

  if (pipeline_hal_streams_map_.find(pipeline_id) ==
      pipeline_hal_streams_map_.end()) {
    return NAME_NOT_FOUND;
  }

  *hal_streams = pipeline_hal_streams_map_.at(pipeline_id);
  return OK;
}

void FakeCameraDeviceSessionHwl::DestroyPipelines() {
  std::lock_guard<std::mutex> lock(hwl_pipeline_lock_);
  hwl_pipeline_callbacks_.clear();
  pipeline_hal_streams_map_.clear();
}

status_t FakeCameraDeviceSessionHwl::SubmitRequests(
    uint32_t frame_number, const std::vector<HwlPipelineRequest>& requests) {
  std::lock_guard<std::mutex> lock(hwl_pipeline_lock_);

  for (auto& request : requests) {
    auto callback = hwl_pipeline_callbacks_.find(request.pipeline_id);
    if (callback == hwl_pipeline_callbacks_.end()) {
      ALOGE("%s: Could not find callback for pipeline %u", __FUNCTION__,
            request.pipeline_id);
      return BAD_VALUE;
    }

    // Notify shutter.
    NotifyMessage shutter_message = {.type = MessageType::kShutter,
                                     .message.shutter = {
                                         .frame_number = frame_number,
                                         .timestamp_ns = 0,
                                     }};
    callback->second.notify(request.pipeline_id, shutter_message);

    // Send out result.
    auto result = std::make_unique<HwlPipelineResult>();
    result->camera_id = kCameraId;
    result->pipeline_id = request.pipeline_id;
    result->frame_number = frame_number;
    result->result_metadata = HalCameraMetadata::Clone(request.settings.get());
    result->input_buffers = request.input_buffers;
    result->output_buffers = request.output_buffers;
    result->partial_result = 1;
    callback->second.process_pipeline_result(std::move(result));
  }

  return OK;
}

status_t FakeCameraDeviceSessionHwl::Flush() {
  return OK;
}

uint32_t FakeCameraDeviceSessionHwl::GetCameraId() const {
  return kCameraId;
}

std::vector<uint32_t> FakeCameraDeviceSessionHwl::GetPhysicalCameraIds() const {
  return kPhysicalCameraIds;
}

status_t FakeCameraDeviceSessionHwl::GetCameraCharacteristics(
    std::unique_ptr<HalCameraMetadata>* characteristics) const {
  if (characteristics == nullptr) {
    return BAD_VALUE;
  }

  (*characteristics) = HalCameraMetadata::Create(/*num_entries=*/0,
                                                 /*data_bytes=*/0);

  if (*characteristics == nullptr) {
    return NO_MEMORY;
  }

  return OK;
}

status_t FakeCameraDeviceSessionHwl::GetPhysicalCameraCharacteristics(
    uint32_t /*physical_camera_id*/,
    std::unique_ptr<HalCameraMetadata>* characteristics) const {
  if (characteristics == nullptr) {
    return BAD_VALUE;
  }

  (*characteristics) = HalCameraMetadata::Create(/*num_entries=*/0,
                                                 /*data_bytes=*/0);

  if (*characteristics == nullptr) {
    return NO_MEMORY;
  }

  return OK;
}

status_t FakeCameraDeviceSessionHwl::SetSessionData(SessionDataKey /*key*/,
                                                    void* /*value*/) {
  return OK;
}

status_t FakeCameraDeviceSessionHwl::GetSessionData(SessionDataKey /*key*/,
                                                    void** /*value*/) const {
  return OK;
}

void FakeCameraDeviceSessionHwl::SetSessionCallback(
    const HwlSessionCallback& /*hwl_session_callback*/) {
  return;
}

status_t FakeCameraDeviceSessionHwl::FilterResultMetadata(
    HalCameraMetadata* /*metadata*/) const {
  return OK;
}

status_t FakeCameraDeviceSessionHwl::IsReconfigurationRequired(
    const HalCameraMetadata* /*old_session*/,
    const HalCameraMetadata* /*new_session*/,
    bool* reconfiguration_required) const {
  if (reconfiguration_required == nullptr) {
    return BAD_VALUE;
  }
  *reconfiguration_required = true;
  return OK;
}

std::unique_ptr<ZoomRatioMapperHwl>
FakeCameraDeviceSessionHwl::GetZoomRatioMapperHwl() {
  return nullptr;
}

std::unique_ptr<IMulticamCoordinatorHwl>
FakeCameraDeviceSessionHwl::CreateMulticamCoordinatorHwl() {
  // Multicam coordinator not supported in this mock
  return nullptr;
}

MockDeviceSessionHwl::MockDeviceSessionHwl(
    uint32_t camera_id, const std::vector<uint32_t>& physical_camera_ids)
    : fake_session_hwl_(camera_id, physical_camera_ids) {
}

void MockDeviceSessionHwl::DelegateCallsToFakeSession() {
  ON_CALL(*this, ConstructDefaultRequestSettings(_, _))
      .WillByDefault(
          Invoke(&fake_session_hwl_,
                 &FakeCameraDeviceSessionHwl::ConstructDefaultRequestSettings));

  ON_CALL(*this, ConfigurePipeline(_, _, _, _, _))
      .WillByDefault(Invoke(&fake_session_hwl_,
                            &FakeCameraDeviceSessionHwl::ConfigurePipeline));

  ON_CALL(*this, BuildPipelines())
      .WillByDefault(Invoke(&fake_session_hwl_,
                            &FakeCameraDeviceSessionHwl::BuildPipelines));

  ON_CALL(*this, PreparePipeline(_, _))
      .WillByDefault(Invoke(&fake_session_hwl_,
                            &FakeCameraDeviceSessionHwl::PreparePipeline));

  ON_CALL(*this, GetRequiredIntputStreams(_, _, _))
      .WillByDefault(
          Invoke(&fake_session_hwl_,
                 &FakeCameraDeviceSessionHwl::GetRequiredIntputStreams));

  ON_CALL(*this, GetConfiguredHalStream(_, _))
      .WillByDefault(
          Invoke(&fake_session_hwl_,
                 &FakeCameraDeviceSessionHwl::GetConfiguredHalStream));

  ON_CALL(*this, DestroyPipelines())
      .WillByDefault(Invoke(&fake_session_hwl_,
                            &FakeCameraDeviceSessionHwl::DestroyPipelines));

  ON_CALL(*this, SubmitRequests(_, _))
      .WillByDefault(Invoke(&fake_session_hwl_,
                            &FakeCameraDeviceSessionHwl::SubmitRequests));

  ON_CALL(*this, Flush())
      .WillByDefault(
          Invoke(&fake_session_hwl_, &FakeCameraDeviceSessionHwl::Flush));

  ON_CALL(*this, GetCameraId())
      .WillByDefault(
          Invoke(&fake_session_hwl_, &FakeCameraDeviceSessionHwl::GetCameraId));

  ON_CALL(*this, GetPhysicalCameraIds())
      .WillByDefault(Invoke(&fake_session_hwl_,
                            &FakeCameraDeviceSessionHwl::GetPhysicalCameraIds));

  ON_CALL(*this, GetCameraCharacteristics(_))
      .WillByDefault(
          Invoke(&fake_session_hwl_,
                 &FakeCameraDeviceSessionHwl::GetCameraCharacteristics));

  ON_CALL(*this, GetPhysicalCameraCharacteristics(_, _))
      .WillByDefault(Invoke(
          &fake_session_hwl_,
          &FakeCameraDeviceSessionHwl::GetPhysicalCameraCharacteristics));
}

}  // namespace google_camera_hal
}  // namespace android
