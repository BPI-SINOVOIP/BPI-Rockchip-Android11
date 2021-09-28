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

#ifndef CAMERA_HAL_GOOGLE_CAMERA_HAL_CAMERA_PROVIDER_CALLBACK_H_
#define CAMERA_HAL_GOOGLE_CAMERA_HAL_CAMERA_PROVIDER_CALLBACK_H_

#include <utils/Errors.h>
#include <memory>

namespace android {
namespace google_camera_hal {

// See the definition of
// ::android::hardware::camera::provider::V2_4::ICameraProviderCallback
using CameraDeviceStatusChangeFunc = std::function<void(
    std::string /*camera_id*/, CameraDeviceStatus /*new_status*/)>;

// See the definition of
// ::android::hardware::camera::provider::V2_6::ICameraProviderCallback
using PhysicalCameraDeviceStatusChangeFunc = std::function<void(
    std::string /*camera_id*/, std::string /*physical_camera_id*/,
    CameraDeviceStatus /*new_status*/)>;

// See the definition of
// ::android::hardware::camera::provider::V2_4::ICameraProviderCallback
using TorchModeStatusChangeFunc =
    std::function<void(std::string /*camera_id*/, TorchModeStatus /*new_status*/)>;

// Defines callbacks to notify when a status changed.
struct CameraProviderCallback {
  // Callback to notify when a camera device's status changed.
  CameraDeviceStatusChangeFunc camera_device_status_change;

  // Callback to notify when a physical camera device's status changed.
  PhysicalCameraDeviceStatusChangeFunc physical_camera_device_status_change;

  // Callback to notify when a torch mode status changed.
  TorchModeStatusChangeFunc torch_mode_status_change;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_CAMERA_PROVIDER_H_
