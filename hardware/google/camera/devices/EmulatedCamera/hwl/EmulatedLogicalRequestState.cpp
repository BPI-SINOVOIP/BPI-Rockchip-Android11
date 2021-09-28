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

#define LOG_TAG "EmulatedLogicalState"
#define ATRACE_TAG ATRACE_TAG_CAMERA

#include "EmulatedLogicalRequestState.h"

#include <log/log.h>

#include "vendor_tag_defs.h"

namespace android {

EmulatedLogicalRequestState::EmulatedLogicalRequestState(uint32_t camera_id)
    : logical_camera_id_(camera_id),
      logical_request_state_(std::make_unique<EmulatedRequestState>(camera_id)) {
}

EmulatedLogicalRequestState::~EmulatedLogicalRequestState() {
}

status_t EmulatedLogicalRequestState::Initialize(
    std::unique_ptr<HalCameraMetadata> static_meta,
    PhysicalDeviceMapPtr physical_devices) {
  if ((physical_devices.get() != nullptr) && (!physical_devices->empty())) {
    physical_device_map_ = std::move(physical_devices);
    // If possible map the available focal lengths to individual physical devices
    camera_metadata_ro_entry_t logical_entry, physical_entry;
    auto ret = static_meta->Get(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS,
                                &logical_entry);
    if ((ret == OK) && (logical_entry.count > 1)) {
      for (size_t i = 0; i < logical_entry.count; i++) {
        for (const auto& it : *physical_device_map_) {
          if (it.second.first != CameraDeviceStatus::kPresent) {
            continue;
          }
          ret = it.second.second->Get(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS,
                               &physical_entry);
          if ((ret == OK) && (physical_entry.count > 0)) {
            if (logical_entry.data.f[i] == physical_entry.data.f[0]) {
              physical_focal_length_map_[physical_entry.data.f[0]] = it.first;
              break;
            }
          }
        }
      }
    }

    if (physical_focal_length_map_.size() > 1) {
      is_logical_device_ = true;
      current_focal_length_ = logical_entry.data.f[0];
      for (const auto& it : *physical_device_map_) {
        std::unique_ptr<EmulatedRequestState> physical_request_state =
            std::make_unique<EmulatedRequestState>(it.first);
        auto ret = physical_request_state->Initialize(
            HalCameraMetadata::Clone(it.second.second.get()));
        if (ret != OK) {
          ALOGE("%s: Physical device: %u request state initialization failed!",
                __FUNCTION__, it.first);
          return ret;
        }
        physical_request_states_.emplace(it.first,
                                         std::move(physical_request_state));
      }
    }
  }

  return logical_request_state_->Initialize(std::move(static_meta));
}

status_t EmulatedLogicalRequestState::GetDefaultRequest(
    RequestTemplate type,
    std::unique_ptr<HalCameraMetadata>* default_settings /*out*/) {
  return logical_request_state_->GetDefaultRequest(type, default_settings);
};

std::unique_ptr<HwlPipelineResult>
EmulatedLogicalRequestState::InitializeLogicalResult(uint32_t pipeline_id,
                                                     uint32_t frame_number) {
  auto ret = logical_request_state_->InitializeResult(pipeline_id, frame_number);
  if (is_logical_device_) {
    if ((physical_camera_output_ids_.get() != nullptr) &&
        (!physical_camera_output_ids_->empty())) {
      ret->physical_camera_results.reserve(physical_camera_output_ids_->size());
      for (const auto& it : *physical_camera_output_ids_) {
        ret->physical_camera_results[it] =
            std::move(physical_request_states_[it]
                          ->InitializeResult(pipeline_id, frame_number)
                          ->result_metadata);
      }
    }
    auto physical_device_id =
        std::to_string(physical_focal_length_map_[current_focal_length_]);
    std::vector<uint8_t> result;
    result.reserve(physical_device_id.size() + 1);
    result.insert(result.end(), physical_device_id.begin(),
                  physical_device_id.end());
    result.push_back('\0');

    ret->result_metadata->Set(ANDROID_LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_ID,
                              result.data(), result.size());
  }

  return ret;
}

status_t EmulatedLogicalRequestState::InitializeLogicalSettings(
    std::unique_ptr<HalCameraMetadata> request_settings,
    std::unique_ptr<std::set<uint32_t>> physical_camera_output_ids,
    EmulatedSensor::LogicalCameraSettings* logical_settings /*out*/) {
  if (logical_settings == nullptr) {
    return BAD_VALUE;
  }

  // All logical and physical devices can potentially receive individual client
  // requests (Currently this is not the case due to HWL API limitations).
  // The emulated sensor can adapt its characteristics and apply most of them
  // independently however the frame duration needs to be the same across all
  // settings.
  // Track the maximum frame duration and override this value at the end for all
  // logical settings.
  nsecs_t max_frame_duration = 0;
  if (is_logical_device_) {
    std::swap(physical_camera_output_ids_, physical_camera_output_ids);

    for (const auto& physical_request_state : physical_request_states_) {
      // All physical devices will receive requests and will keep
      // updating their respective request state.
      // However only physical devices referenced by client need to propagate
      // and apply their settings.
      EmulatedSensor::SensorSettings physical_sensor_settings;
      auto ret = physical_request_state.second->InitializeSensorSettings(
          HalCameraMetadata::Clone(request_settings.get()),
          &physical_sensor_settings);
      if (ret != OK) {
        ALOGE(
            "%s: Initialization of physical sensor settings for device id: %u  "
            "failed!",
            __FUNCTION__, physical_request_state.first);
        return ret;
      }

      if (physical_camera_output_ids_->find(physical_request_state.first) !=
          physical_camera_output_ids_->end()) {
        logical_settings->emplace(physical_request_state.first,
                                  physical_sensor_settings);
        if (max_frame_duration < physical_sensor_settings.exposure_time) {
          max_frame_duration = physical_sensor_settings.exposure_time;
        }
      }
    }

    camera_metadata_ro_entry entry;
    auto stat = request_settings->Get(ANDROID_LENS_FOCAL_LENGTH, &entry);
    if ((stat == OK) && (entry.count == 1)) {
      if (physical_focal_length_map_.find(entry.data.f[0]) !=
          physical_focal_length_map_.end()) {
        current_focal_length_ = entry.data.f[0];
      } else {
        ALOGE("%s: Unsupported focal length set: %5.2f, re-using older value!",
              __FUNCTION__, entry.data.f[0]);
      }
    } else {
      ALOGW("%s: Focal length absent from request, re-using older value!",
            __FUNCTION__);
    }
  }

  EmulatedSensor::SensorSettings sensor_settings;
  auto ret = logical_request_state_->InitializeSensorSettings(
      std::move(request_settings), &sensor_settings);
  logical_settings->emplace(logical_camera_id_, sensor_settings);
  if (max_frame_duration < sensor_settings.exposure_time) {
    max_frame_duration = sensor_settings.exposure_time;
  }

  for (auto it : *logical_settings) {
    it.second.frame_duration = max_frame_duration;
  }

  return ret;
}

std::unique_ptr<HalCameraMetadata>
EmulatedLogicalRequestState::AdaptLogicalCharacteristics(
    std::unique_ptr<HalCameraMetadata> logical_chars,
    PhysicalDeviceMapPtr physical_devices) {
  if ((logical_chars.get() == nullptr) || (physical_devices.get() == nullptr)) {
    return nullptr;
  }

  // Update 'android.logicalMultiCamera.physicalIds' according to the newly
  // assigned physical ids.
  // Additionally if possible try to emulate a logical camera device backed by
  // physical devices with different focal lengths. Usually real logical
  // cameras like that will have device specific logic to switch between
  // physical sensors. Unfortunately we cannot infer this behavior using only
  // static camera characteristics. Instead of this, detect the different
  // focal lengths and update the logical
  // "android.lens.info.availableFocalLengths" accordingly.
  std::vector<uint8_t> physical_ids;
  std::set<float> focal_lengths;
  camera_metadata_ro_entry_t entry;
  for (const auto& physical_device : *physical_devices) {
    auto physical_id = std::to_string(physical_device.first);
    physical_ids.insert(physical_ids.end(), physical_id.begin(),
                        physical_id.end());
    physical_ids.push_back('\0');
    auto ret = physical_device.second.second->Get(
        ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, &entry);
    if ((ret == OK) && (entry.count > 0)) {
      focal_lengths.insert(entry.data.f, entry.data.f + entry.count);
    }
  }
  logical_chars->Set(ANDROID_LOGICAL_MULTI_CAMERA_PHYSICAL_IDS,
                     physical_ids.data(), physical_ids.size());

  if (focal_lengths.size() > 1) {
    std::vector<float> focal_buffer;
    focal_buffer.reserve(focal_lengths.size());
    focal_buffer.insert(focal_buffer.end(), focal_lengths.begin(),
                        focal_lengths.end());
    logical_chars->Set(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS,
                       focal_buffer.data(), focal_buffer.size());

    // Possibly needs to be removed at some later point:
    int32_t default_physical_id = physical_devices->begin()->first;
    logical_chars->Set(google_camera_hal::kLogicalCamDefaultPhysicalId,
                       &default_physical_id, 1);

    logical_chars->Get(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS, &entry);
    std::set<int32_t> keys(entry.data.i32, entry.data.i32 + entry.count);
    keys.emplace(ANDROID_LENS_FOCAL_LENGTH);
    keys.emplace(ANDROID_LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_ID);
    std::vector<int32_t> keys_buffer(keys.begin(), keys.end());
    logical_chars->Set(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS,
                       keys_buffer.data(), keys_buffer.size());

    keys.clear();
    keys_buffer.clear();
    logical_chars->Get(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS, &entry);
    keys.insert(entry.data.i32, entry.data.i32 + entry.count);
    // Due to API limitations we currently don't support individual physical requests
    logical_chars->Erase(ANDROID_REQUEST_AVAILABLE_PHYSICAL_CAMERA_REQUEST_KEYS);
    keys.erase(ANDROID_REQUEST_AVAILABLE_PHYSICAL_CAMERA_REQUEST_KEYS);
    keys.emplace(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS);
    keys.emplace(ANDROID_LOGICAL_MULTI_CAMERA_PHYSICAL_IDS);
    keys_buffer.insert(keys_buffer.end(), keys.begin(), keys.end());
    logical_chars->Set(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS,
                       keys_buffer.data(), keys_buffer.size());

    keys.clear();
    keys_buffer.clear();
    logical_chars->Get(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS, &entry);
    keys.insert(entry.data.i32, entry.data.i32 + entry.count);
    keys.emplace(ANDROID_LENS_FOCAL_LENGTH);
    keys_buffer.insert(keys_buffer.end(), keys.begin(), keys.end());
    logical_chars->Set(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS,
                       keys_buffer.data(), keys_buffer.size());
  } else {
    ALOGW(
        "%s: The logical camera doesn't support different focal lengths. "
        "Emulation "
        "could be"
        " very limited in this case!",
        __FUNCTION__);
  }

  return logical_chars;
}

}  // namespace android
