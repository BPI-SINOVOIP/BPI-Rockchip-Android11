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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_CAMERA_DEVICE_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_CAMERA_DEVICE_H_

#include <android/hardware/camera/device/3.5/ICameraDevice.h>
#include <android/hardware/camera/device/3.5/ICameraDeviceCallback.h>

#include "camera_device.h"

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace V3_5 {
namespace implementation {

using ::android::hardware::camera::common::V1_0::CameraResourceCost;
using ::android::hardware::camera::common::V1_0::Status;
using ::android::hardware::camera::common::V1_0::TorchMode;
using ::android::hardware::camera::device::V3_5::ICameraDevice;
using ::android::hardware::camera::device::V3_5::ICameraDeviceCallback;

using ::android::google_camera_hal::CameraDevice;

// HidlCameraDevice implements the HIDL camera device interface, ICameraDevice,
// using Google Camera HAL to provide information about the associated camera
// device.
class HidlCameraDevice : public ICameraDevice {
 public:
  static const std::string kDeviceVersion;

  // Create a HidlCameraDevice.
  // google_camera_device is a google camera device that HidlCameraDevice
  // is going to manage. Creating a HidlCameraDevice will fail if
  // google_camera_device is nullptr.
  static std::unique_ptr<HidlCameraDevice> Create(
      std::unique_ptr<CameraDevice> google_camera_device);
  virtual ~HidlCameraDevice() = default;

  // Override functions in ICameraDevice
  Return<void> getResourceCost(
      ICameraDevice::getResourceCost_cb _hidl_cb) override;

  Return<void> getCameraCharacteristics(
      ICameraDevice::getCameraCharacteristics_cb _hidl_cb) override;

  Return<Status> setTorchMode(TorchMode mode) override;

  Return<void> open(const sp<V3_2::ICameraDeviceCallback>& callback,
                    ICameraDevice::open_cb _hidl_cb) override;

  Return<void> dumpState(const ::android::hardware::hidl_handle& handle) override;

  Return<void> getPhysicalCameraCharacteristics(
      const hidl_string& physicalCameraId,
      ICameraDevice::getPhysicalCameraCharacteristics_cb _hidl_cb) override;

  Return<void> isStreamCombinationSupported(
      const V3_4::StreamConfiguration& streams,
      ICameraDevice::isStreamCombinationSupported_cb _hidl_cb) override;
  // End of override functions in ICameraDevice

 protected:
  HidlCameraDevice() = default;

 private:
  status_t Initialize(std::unique_ptr<CameraDevice> google_camera_device);

  std::unique_ptr<CameraDevice> google_camera_device_;
  uint32_t camera_id_ = 0;
};

}  // namespace implementation
}  // namespace V3_5
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_CAMERA_DEVICE_H_
