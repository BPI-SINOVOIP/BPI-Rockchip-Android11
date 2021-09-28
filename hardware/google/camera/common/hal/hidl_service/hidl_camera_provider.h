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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_CAMERA_PROVIDER_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_CAMERA_PROVIDER_H_

#include <android/hardware/camera/provider/2.6/ICameraProvider.h>
#include <android/hardware/camera/provider/2.6/ICameraProviderCallback.h>
#include "camera_provider.h"

namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace V2_6 {
namespace implementation {

using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::camera::common::V1_0::CameraDeviceStatus;
using ::android::hardware::camera::common::V1_0::Status;
using ::android::hardware::camera::common::V1_0::TorchModeStatus;
using ::android::hardware::camera::common::V1_0::VendorTag;
using ::android::hardware::camera::common::V1_0::VendorTagSection;
using ::android::hardware::camera::provider::V2_4::ICameraProviderCallback;
using ::android::hardware::camera::provider::V2_5::DeviceState;
using ::android::hardware::camera::provider::V2_6::CameraIdAndStreamCombination;
using ::android::hardware::camera::provider::V2_6::ICameraProvider;

using ::android::google_camera_hal::CameraProvider;

// HidlCameraProvider implements the HIDL camera provider interface,
// ICameraProvider, to enumerate the available individual camera devices
// in the system, and provide updates about changes to device status.
class HidlCameraProvider : public ICameraProvider {
 public:
  static const std::string kProviderName;
  static std::unique_ptr<HidlCameraProvider> Create();
  virtual ~HidlCameraProvider() = default;

  // Override functions in ICameraProvider.
  Return<Status> setCallback(
      const sp<ICameraProviderCallback>& callback) override;

  Return<void> getVendorTags(getVendorTags_cb _hidl_cb) override;

  Return<void> getCameraIdList(getCameraIdList_cb _hidl_cb) override;

  Return<void> isSetTorchModeSupported(
      isSetTorchModeSupported_cb _hidl_cb) override;

  Return<void> getConcurrentStreamingCameraIds(
      getConcurrentStreamingCameraIds_cb _hidl_cb) override;

  Return<void> isConcurrentStreamCombinationSupported(
      const hidl_vec<CameraIdAndStreamCombination>& configs,
      isConcurrentStreamCombinationSupported_cb _hidl_cb) override;

  Return<void> getCameraDeviceInterface_V1_x(
      const hidl_string& cameraDeviceName,
      getCameraDeviceInterface_V1_x_cb _hidl_cb) override;

  Return<void> getCameraDeviceInterface_V3_x(
      const hidl_string& cameraDeviceName,
      getCameraDeviceInterface_V3_x_cb _hidl_cb) override;

  Return<void> notifyDeviceStateChange(
      hardware::hidl_bitfield<DeviceState> newState) override;
  // End of override functions in ICameraProvider.

 protected:
  HidlCameraProvider() = default;

 private:
  static const std::regex kDeviceNameRegex;

  status_t Initialize();

  // Parse device version and camera ID.
  bool ParseDeviceName(const hidl_string& device_name,
                       std::string* device_version, std::string* camera_id);

  std::mutex callbacks_lock_;
  sp<ICameraProviderCallback> callbacks_;

  std::unique_ptr<CameraProvider> google_camera_provider_;
  google_camera_hal::CameraProviderCallback camera_provider_callback_;
};

extern "C" ICameraProvider* HIDL_FETCH_ICameraProvider(const char* name);

}  // namespace implementation
}  // namespace V2_6
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_CAMERA_PROVIDER_H_
