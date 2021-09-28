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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_COMMON_THERMAL_TYPES_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_COMMON_THERMAL_TYPES_H_

namespace android {
namespace google_camera_hal {

// See the definition of
// ::android::hardware::thermal::V2_0::TemperatureType
enum class TemperatureType : int32_t {
  kUnknown = -1,
  kCpu = 0,
  kGpu = 1,
  kBattery = 2,
  kSkin = 3,
  kUsbPort = 4,
  kPowerAmplifier = 5,
  kBclVoltage = 6,
  kBclCurrent = 7,
  kBclPercentage = 8,
  kNpu = 9,
};

// See the definition of
// ::android::hardware::thermal::V2_0::ThrottlingSeverity
enum class ThrottlingSeverity : uint32_t {
  kNone = 0,
  kLight,
  kModerate,
  kSevere,
  kCritical,
  kEmergency,
  kShutdown,
};

// See the definition of
// ::android::hardware::thermal::V2_0::Temperature
struct Temperature {
  TemperatureType type = TemperatureType::kUnknown;
  std::string name;
  float value = 0.0f;
  ThrottlingSeverity throttling_status = ThrottlingSeverity::kNone;
};

// Function to invoke when thermal status changes.
using NotifyThrottlingFunc =
    std::function<void(const Temperature& /*temperature*/)>;

// Callback to register a thermal throttling notify function.
// If filter_type is true, type will be used. If filter_type is false,
// type will be ignored and all types will be notified.
using RegisterThermalChangedCallbackFunc =
    std::function<status_t(NotifyThrottlingFunc /*notify_throttling*/,
                           bool /*filter_type*/, TemperatureType /*type*/)>;

// Unregister the thermal callback.
using UnregisterThermalChangedCallbackFunc = std::function<void()>;

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_COMMON_THERMAL_TYPES_H_
