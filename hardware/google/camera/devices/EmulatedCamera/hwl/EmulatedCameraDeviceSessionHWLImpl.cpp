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
#define LOG_TAG "EmulatedCameraDevSession"
#define ATRACE_TAG ATRACE_TAG_CAMERA

#include "EmulatedCameraDeviceSessionHWLImpl.h"

#include <hardware/gralloc.h>
#include <inttypes.h>
#include <log/log.h>
#include <utils/Trace.h>

#include "EmulatedSensor.h"
#include "utils/HWLUtils.h"

namespace android {

std::unique_ptr<EmulatedCameraDeviceSessionHwlImpl>
EmulatedCameraDeviceSessionHwlImpl::Create(
    uint32_t camera_id, std::unique_ptr<HalCameraMetadata> static_meta,
    PhysicalDeviceMapPtr physical_devices,
    std::shared_ptr<EmulatedTorchState> torch_state) {
  ATRACE_CALL();
  if (static_meta.get() == nullptr) {
    return nullptr;
  }

  auto session = std::unique_ptr<EmulatedCameraDeviceSessionHwlImpl>(
      new EmulatedCameraDeviceSessionHwlImpl(std::move(physical_devices),
                                             torch_state));
  if (session == nullptr) {
    ALOGE("%s: Creating EmulatedCameraDeviceSessionHwlImpl failed",
          __FUNCTION__);
    return nullptr;
  }

  status_t res = session->Initialize(camera_id, std::move(static_meta));
  if (res != OK) {
    ALOGE("%s: Initializing EmulatedCameraDeviceSessionHwlImpl failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return nullptr;
  }

  return session;
}

status_t EmulatedCameraDeviceSessionHwlImpl::Initialize(
    uint32_t camera_id, std::unique_ptr<HalCameraMetadata> static_meta) {
  camera_id_ = camera_id;
  static_metadata_ = std::move(static_meta);
  stream_coniguration_map_ =
      std::make_unique<StreamConfigurationMap>(*static_metadata_);
  camera_metadata_ro_entry_t entry;
  auto ret = static_metadata_->Get(ANDROID_REQUEST_PIPELINE_MAX_DEPTH, &entry);
  if (ret != OK) {
    ALOGE("%s: Unable to extract ANDROID_REQUEST_PIPELINE_MAX_DEPTH, %s (%d)",
          __FUNCTION__, strerror(-ret), ret);
    return ret;
  }

  max_pipeline_depth_ = entry.data.u8[0];

  ret = GetSensorCharacteristics(static_metadata_.get(), &sensor_chars_);
  if (ret != OK) {
    ALOGE("%s: Unable to extract sensor characteristics %s (%d)", __FUNCTION__,
          strerror(-ret), ret);
    return ret;
  }

  auto logical_chars = std::make_unique<LogicalCharacteristics>();
  logical_chars->emplace(camera_id_, sensor_chars_);
  for (const auto& it : *physical_device_map_) {
    SensorCharacteristics physical_chars;
    auto stat = GetSensorCharacteristics(it.second.second.get(), &physical_chars);
    if (stat == OK) {
      logical_chars->emplace(it.first, physical_chars);
    } else {
      ALOGE("%s: Unable to extract physical device: %u characteristics %s (%d)",
            __FUNCTION__, it.first, strerror(-ret), ret);
      return ret;
    }
  }
  sp<EmulatedSensor> emulated_sensor = new EmulatedSensor();
  ret = emulated_sensor->StartUp(camera_id_, std::move(logical_chars));
  if (ret != OK) {
    ALOGE("%s: Failed on sensor start up %s (%d)", __FUNCTION__, strerror(-ret),
          ret);
    return ret;
  }

  request_processor_ =
      std::make_unique<EmulatedRequestProcessor>(camera_id_, emulated_sensor);

  return request_processor_->Initialize(
      HalCameraMetadata::Clone(static_metadata_.get()),
      ClonePhysicalDeviceMap(physical_device_map_));
}

EmulatedCameraDeviceSessionHwlImpl::~EmulatedCameraDeviceSessionHwlImpl() {
  if (torch_state_.get() != nullptr) {
    torch_state_->ReleaseFlashHw();
  }
}

status_t EmulatedCameraDeviceSessionHwlImpl::ConstructDefaultRequestSettings(
    RequestTemplate type, std::unique_ptr<HalCameraMetadata>* default_settings) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(api_mutex_);

  return request_processor_->GetDefaultRequest(type, default_settings);
}

status_t EmulatedCameraDeviceSessionHwlImpl::ConfigurePipeline(
    uint32_t physical_camera_id, HwlPipelineCallback hwl_pipeline_callback,
    const StreamConfiguration& request_config,
    const StreamConfiguration& /*overall_config*/, uint32_t* pipeline_id) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(api_mutex_);
  if (pipeline_id == nullptr) {
    ALOGE("%s pipeline_id is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  if (pipelines_built_) {
    ALOGE("%s Cannot configure pipelines after calling BuildPipelines()",
          __FUNCTION__);
    return ALREADY_EXISTS;
  }

  if (!EmulatedSensor::IsStreamCombinationSupported(
          request_config, *stream_coniguration_map_, sensor_chars_)) {
    ALOGE("%s: Stream combination not supported!", __FUNCTION__);
    return BAD_VALUE;
  }

  if ((physical_camera_id != camera_id_) &&
      (physical_device_map_.get() != nullptr)) {
    if (physical_device_map_->find(physical_camera_id) ==
        physical_device_map_->end()) {
      ALOGE("%s: Camera: %d doesn't include physical device with id: %u",
            __FUNCTION__, camera_id_, physical_camera_id);
      return BAD_VALUE;
    }
  }

  *pipeline_id = pipelines_.size();
  EmulatedPipeline emulated_pipeline{.cb = hwl_pipeline_callback,
                                     .physical_camera_id = physical_camera_id,
                                     .pipeline_id = *pipeline_id,};

  emulated_pipeline.streams.reserve(request_config.streams.size());
  for (const auto& stream : request_config.streams) {
    bool is_input = stream.stream_type == google_camera_hal::StreamType::kInput;
    emulated_pipeline.streams.emplace(
        stream.id,
        EmulatedStream(
            {{.id = stream.id,
              .override_format =
                  is_input ? stream.format
                           : EmulatedSensor::OverrideFormat(stream.format),
              .producer_usage = is_input ? 0
                                         : GRALLOC_USAGE_HW_CAMERA_WRITE |
                                               GRALLOC_USAGE_HW_CAMERA_READ,
              .consumer_usage = 0,
              .max_buffers = max_pipeline_depth_,
              .override_data_space = stream.data_space,
              .is_physical_camera_stream = stream.is_physical_camera_stream,
              .physical_camera_id = stream.physical_camera_id},
             .width = stream.width,
             .height = stream.height,
             .buffer_size = stream.buffer_size,
             .is_input = is_input,}));
  }

  pipelines_.push_back(emulated_pipeline);

  return OK;
}

status_t EmulatedCameraDeviceSessionHwlImpl::GetConfiguredHalStream(
    uint32_t pipeline_id, std::vector<HalStream>* hal_streams) const {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(api_mutex_);
  if (hal_streams == nullptr) {
    ALOGE("%s hal_streams is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  if (!pipelines_built_) {
    ALOGE("%s No pipeline was built.", __FUNCTION__);
    return NO_INIT;
  }

  if (pipeline_id >= pipelines_.size()) {
    ALOGE("%s: Unknown pipeline ID: %u", __FUNCTION__, pipeline_id);
    return NAME_NOT_FOUND;
  }

  const auto& streams = pipelines_[pipeline_id].streams;
  hal_streams->reserve(streams.size());
  for (const auto& it : streams) {
    hal_streams->push_back(it.second);
  }

  return OK;
}

status_t EmulatedCameraDeviceSessionHwlImpl::BuildPipelines() {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(api_mutex_);
  if (pipelines_built_) {
    ALOGE("%s Pipelines have already been built!", __FUNCTION__);
    return ALREADY_EXISTS;
  } else if (pipelines_.size() == 0) {
    ALOGE("%s No pipelines have been configured yet!", __FUNCTION__);
    return NO_INIT;
  }

  pipelines_built_ = true;

  return OK;
}

void EmulatedCameraDeviceSessionHwlImpl::DestroyPipelines() {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(api_mutex_);
  if (!pipelines_built_) {
    // Not an error - nothing to destroy
    ALOGV("%s nothing to destroy", __FUNCTION__);
    return;
  }

  pipelines_built_ = false;
  pipelines_.clear();
}

status_t EmulatedCameraDeviceSessionHwlImpl::SubmitRequests(
    uint32_t frame_number, const std::vector<HwlPipelineRequest>& requests) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(api_mutex_);

  // Check whether reprocess request has valid/supported outputs.
  for (const auto& request : requests) {
    if (!request.input_buffers.empty()) {
      for (const auto& input_buffer : request.input_buffers) {
        const auto& streams = pipelines_[request.pipeline_id].streams;
        auto input_stream = streams.at(input_buffer.stream_id);
        auto output_formats =
            stream_coniguration_map_->GetValidOutputFormatsForInput(
                input_stream.override_format);
        for (const auto& output_buffer : request.output_buffers) {
          auto output_stream = streams.at(output_buffer.stream_id);
          if (output_formats.find(output_stream.override_format) ==
              output_formats.end()) {
            ALOGE(
                "%s: Reprocess request with input format: 0x%x to output "
                "format: 0x%x"
                " not supported!",
                __FUNCTION__, input_stream.override_format,
                output_stream.override_format);
            return BAD_VALUE;
          }
        }
      }
    }
  }

  if (error_state_) {
    ALOGE("%s session is in error state and cannot process further requests",
          __FUNCTION__);
    return INVALID_OPERATION;
  }

  return request_processor_->ProcessPipelineRequests(frame_number, requests,
                                                     pipelines_);
}

status_t EmulatedCameraDeviceSessionHwlImpl::Flush() {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(api_mutex_);
  return request_processor_->Flush();
}

uint32_t EmulatedCameraDeviceSessionHwlImpl::GetCameraId() const {
  return camera_id_;
}

std::vector<uint32_t> EmulatedCameraDeviceSessionHwlImpl::GetPhysicalCameraIds()
    const {
  if ((physical_device_map_.get() == nullptr) ||
      (physical_device_map_->empty())) {
    return std::vector<uint32_t>{};
  }

  std::vector<uint32_t> ret;
  ret.reserve(physical_device_map_->size());
  for (const auto& it : *physical_device_map_) {
    ret.push_back(it.first);
  }

  return ret;
}

status_t EmulatedCameraDeviceSessionHwlImpl::GetCameraCharacteristics(
    std::unique_ptr<HalCameraMetadata>* characteristics) const {
  ATRACE_CALL();
  if (characteristics == nullptr) {
    return BAD_VALUE;
  }

  (*characteristics) = HalCameraMetadata::Clone(static_metadata_.get());

  if (*characteristics == nullptr) {
    ALOGE("%s metadata clone failed", __FUNCTION__);
    return NO_MEMORY;
  }

  return OK;
}

status_t EmulatedCameraDeviceSessionHwlImpl::GetPhysicalCameraCharacteristics(
    uint32_t physical_camera_id,
    std::unique_ptr<HalCameraMetadata>* characteristics) const {
  ATRACE_CALL();
  if (characteristics == nullptr) {
    return BAD_VALUE;
  }

  if (physical_device_map_.get() == nullptr) {
    ALOGE("%s: Camera: %d doesn't have physical device support!", __FUNCTION__,
          camera_id_);
    return BAD_VALUE;
  }

  if (physical_device_map_->find(physical_camera_id) ==
      physical_device_map_->end()) {
    ALOGE("%s: Camera: %d doesn't include physical device with id: %u",
          __FUNCTION__, camera_id_, physical_camera_id);
    return BAD_VALUE;
  }

  (*characteristics) = HalCameraMetadata::Clone(
      physical_device_map_->at(physical_camera_id).second.get());

  return OK;
}

}  // namespace android
