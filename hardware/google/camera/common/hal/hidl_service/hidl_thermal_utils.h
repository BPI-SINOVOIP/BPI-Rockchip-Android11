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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_THERMAL_UTILS_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_THERMAL_UTILS_H_

#include <android/hardware/thermal/2.0/IThermalChangedCallback.h>

#include "thermal_types.h"

namespace android {
namespace hardware {
namespace hidl_thermal_utils {

using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::thermal::V2_0::IThermalChangedCallback;
using ::android::hardware::thermal::V2_0::Temperature;
using ::android::hardware::thermal::V2_0::TemperatureType;
using ::android::hardware::thermal::V2_0::ThrottlingSeverity;

// HidlThermalChangedCallback implements the HIDL thermal changed callback
// interface, IThermalChangedCallback, to be registered for thermal status
// change.
class HidlThermalChangedCallback : public IThermalChangedCallback {
 public:
  static std::unique_ptr<HidlThermalChangedCallback> Create(
      google_camera_hal::NotifyThrottlingFunc notify_throttling);
  virtual ~HidlThermalChangedCallback() = default;

  // Override functions in HidlThermalChangedCallback.
  Return<void> notifyThrottling(const Temperature& temperature) override;
  // End of override functions in HidlThermalChangedCallback.

 protected:
  HidlThermalChangedCallback(
      google_camera_hal::NotifyThrottlingFunc notify_throttling);

 private:
  const google_camera_hal::NotifyThrottlingFunc kNotifyThrottling;

  status_t ConvertToHalTemperatureType(
      const TemperatureType& hidl_temperature_type,
      google_camera_hal::TemperatureType* hal_temperature_type);

  status_t ConvertToHalThrottlingSeverity(
      const ThrottlingSeverity& hidl_throttling_severity,
      google_camera_hal::ThrottlingSeverity* hal_throttling_severity);

  status_t ConvertToHalTemperature(
      const Temperature& hidl_temperature,
      google_camera_hal::Temperature* hal_temperature);
};

status_t ConvertToHidlTemperatureType(
    const google_camera_hal::TemperatureType& hal_temperature_type,
    TemperatureType* hidl_temperature_type);

}  // namespace hidl_thermal_utils
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_THERMAL_UTILS_H_