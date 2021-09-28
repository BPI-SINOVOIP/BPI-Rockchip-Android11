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

#ifndef EMULATOR_CAMERA_HAL_LOGICAL_REQUEST_STATE_H
#define EMULATOR_CAMERA_HAL_LOGICAL_REQUEST_STATE_H

#include "EmulatedRequestState.h"
#include "hwl_types.h"
#include "utils/HWLUtils.h"

namespace android {

using google_camera_hal::HalCameraMetadata;

class EmulatedLogicalRequestState {
 public:
  EmulatedLogicalRequestState(uint32_t camera_id);
  virtual ~EmulatedLogicalRequestState();

  status_t Initialize(std::unique_ptr<HalCameraMetadata> static_meta,
                      PhysicalDeviceMapPtr physical_device_map);

  status_t GetDefaultRequest(
      RequestTemplate type,
      std::unique_ptr<HalCameraMetadata>* default_settings /*out*/);

  std::unique_ptr<HwlPipelineResult> InitializeLogicalResult(
      uint32_t pipeline_id, uint32_t frame_number);

  status_t InitializeLogicalSettings(
      std::unique_ptr<HalCameraMetadata> request_settings,
      std::unique_ptr<std::set<uint32_t>> physical_camera_output_ids,
      EmulatedSensor::LogicalCameraSettings* logical_settings /*out*/);

  static std::unique_ptr<HalCameraMetadata> AdaptLogicalCharacteristics(
      std::unique_ptr<HalCameraMetadata> logical_chars,
      PhysicalDeviceMapPtr physical_devices);

 private:
  uint32_t logical_camera_id_ = 0;
  std::unique_ptr<EmulatedRequestState> logical_request_state_;
  bool is_logical_device_ = false;
  std::unique_ptr<std::set<uint32_t>> physical_camera_output_ids_;
  PhysicalDeviceMapPtr physical_device_map_;
  // Maps a physical device id to its respective request state
  std::unordered_map<uint32_t, std::unique_ptr<EmulatedRequestState>>
      physical_request_states_;
  // Maps particular focal length to physical device id
  std::unordered_map<float, uint32_t> physical_focal_length_map_;
  float current_focal_length_ = 0.f;

  EmulatedLogicalRequestState(const EmulatedLogicalRequestState&) = delete;
  EmulatedLogicalRequestState& operator=(const EmulatedLogicalRequestState&) =
      delete;
};

}  // namespace android

#endif  // EMULATOR_CAMERA_HAL_LOGICAL_REQUEST_STATE_H
