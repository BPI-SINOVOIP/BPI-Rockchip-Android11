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

#define LOG_TAG "GCH_HidlThermalUtils"
//#define LOG_NDEBUG 0
#include <log/log.h>

#include "hidl_thermal_utils.h"
#include "hidl_utils.h"

namespace android {
namespace hardware {
namespace hidl_thermal_utils {

namespace hidl_utils = ::android::hardware::camera::implementation::hidl_utils;

std::unique_ptr<HidlThermalChangedCallback> HidlThermalChangedCallback::Create(
    google_camera_hal::NotifyThrottlingFunc notify_throttling) {
  auto thermal_changed_callback = std::unique_ptr<HidlThermalChangedCallback>(
      new HidlThermalChangedCallback(notify_throttling));
  if (thermal_changed_callback == nullptr) {
    ALOGE("%s: Failed to create a thermal changed callback", __FUNCTION__);
    return nullptr;
  }

  return thermal_changed_callback;
}

HidlThermalChangedCallback::HidlThermalChangedCallback(
    google_camera_hal::NotifyThrottlingFunc notify_throttling)
    : kNotifyThrottling(notify_throttling) {
}

status_t ConvertToHidlTemperatureType(
    const google_camera_hal::TemperatureType& hal_temperature_type,
    TemperatureType* hidl_temperature_type) {
  if (hidl_temperature_type == nullptr) {
    ALOGE("%s: hidl_temperature_type is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  switch (hal_temperature_type) {
    case google_camera_hal::TemperatureType::kUnknown:
      *hidl_temperature_type = TemperatureType::UNKNOWN;
      break;
    case google_camera_hal::TemperatureType::kCpu:
      *hidl_temperature_type = TemperatureType::CPU;
      break;
    case google_camera_hal::TemperatureType::kGpu:
      *hidl_temperature_type = TemperatureType::GPU;
      break;
    case google_camera_hal::TemperatureType::kBattery:
      *hidl_temperature_type = TemperatureType::BATTERY;
      break;
    case google_camera_hal::TemperatureType::kSkin:
      *hidl_temperature_type = TemperatureType::SKIN;
      break;
    case google_camera_hal::TemperatureType::kUsbPort:
      *hidl_temperature_type = TemperatureType::USB_PORT;
      break;
    case google_camera_hal::TemperatureType::kPowerAmplifier:
      *hidl_temperature_type = TemperatureType::POWER_AMPLIFIER;
      break;
    case google_camera_hal::TemperatureType::kBclVoltage:
      *hidl_temperature_type = TemperatureType::BCL_VOLTAGE;
      break;
    case google_camera_hal::TemperatureType::kBclCurrent:
      *hidl_temperature_type = TemperatureType::BCL_CURRENT;
      break;
    case google_camera_hal::TemperatureType::kBclPercentage:
      *hidl_temperature_type = TemperatureType::BCL_PERCENTAGE;
      break;
    case google_camera_hal::TemperatureType::kNpu:
      *hidl_temperature_type = TemperatureType::NPU;
      break;
    default:
      ALOGE("%s: Unknown temperature type: %d", __FUNCTION__,
            hal_temperature_type);
      return BAD_VALUE;
  }

  return OK;
}

status_t HidlThermalChangedCallback::ConvertToHalTemperatureType(
    const TemperatureType& hidl_temperature_type,
    google_camera_hal::TemperatureType* hal_temperature_type) {
  if (hal_temperature_type == nullptr) {
    ALOGE("%s: hal_temperature_type is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  switch (hidl_temperature_type) {
    case TemperatureType::UNKNOWN:
      *hal_temperature_type = google_camera_hal::TemperatureType::kUnknown;
      break;
    case TemperatureType::CPU:
      *hal_temperature_type = google_camera_hal::TemperatureType::kCpu;
      break;
    case TemperatureType::GPU:
      *hal_temperature_type = google_camera_hal::TemperatureType::kGpu;
      break;
    case TemperatureType::BATTERY:
      *hal_temperature_type = google_camera_hal::TemperatureType::kBattery;
      break;
    case TemperatureType::SKIN:
      *hal_temperature_type = google_camera_hal::TemperatureType::kSkin;
      break;
    case TemperatureType::USB_PORT:
      *hal_temperature_type = google_camera_hal::TemperatureType::kUsbPort;
      break;
    case TemperatureType::POWER_AMPLIFIER:
      *hal_temperature_type =
          google_camera_hal::TemperatureType::kPowerAmplifier;
      break;
    case TemperatureType::BCL_VOLTAGE:
      *hal_temperature_type = google_camera_hal::TemperatureType::kBclVoltage;
      break;
    case TemperatureType::BCL_CURRENT:
      *hal_temperature_type = google_camera_hal::TemperatureType::kBclCurrent;
      break;
    case TemperatureType::BCL_PERCENTAGE:
      *hal_temperature_type = google_camera_hal::TemperatureType::kBclPercentage;
      break;
    case TemperatureType::NPU:
      *hal_temperature_type = google_camera_hal::TemperatureType::kNpu;
      break;
    default:
      ALOGE("%s: Unknown temperature type: %d", __FUNCTION__,
            hidl_temperature_type);
      return BAD_VALUE;
  }

  return OK;
}

status_t HidlThermalChangedCallback::ConvertToHalThrottlingSeverity(
    const ThrottlingSeverity& hidl_throttling_severity,
    google_camera_hal::ThrottlingSeverity* hal_throttling_severity) {
  if (hal_throttling_severity == nullptr) {
    ALOGE("%s: hal_throttling_severity is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  switch (hidl_throttling_severity) {
    case ThrottlingSeverity::NONE:
      *hal_throttling_severity = google_camera_hal::ThrottlingSeverity::kNone;
      break;
    case ThrottlingSeverity::LIGHT:
      *hal_throttling_severity = google_camera_hal::ThrottlingSeverity::kLight;
      break;
    case ThrottlingSeverity::MODERATE:
      *hal_throttling_severity =
          google_camera_hal::ThrottlingSeverity::kModerate;
      break;
    case ThrottlingSeverity::SEVERE:
      *hal_throttling_severity = google_camera_hal::ThrottlingSeverity::kSevere;
      break;
    case ThrottlingSeverity::CRITICAL:
      *hal_throttling_severity =
          google_camera_hal::ThrottlingSeverity::kCritical;
      break;
    case ThrottlingSeverity::EMERGENCY:
      *hal_throttling_severity =
          google_camera_hal::ThrottlingSeverity::kEmergency;
      break;
    case ThrottlingSeverity::SHUTDOWN:
      *hal_throttling_severity =
          google_camera_hal::ThrottlingSeverity::kShutdown;
      break;
    default:
      ALOGE("%s: Unknown temperature severity: %d", __FUNCTION__,
            hidl_throttling_severity);
      return BAD_VALUE;
  }

  return OK;
}

status_t HidlThermalChangedCallback::ConvertToHalTemperature(
    const Temperature& hidl_temperature,
    google_camera_hal::Temperature* hal_temperature) {
  if (hal_temperature == nullptr) {
    ALOGE("%s: hal_temperature is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  status_t res = ConvertToHalTemperatureType(hidl_temperature.type,
                                             &hal_temperature->type);
  if (res != OK) {
    ALOGE("%s: Converting to hal temperature type failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  hal_temperature->name = hidl_temperature.name;
  hal_temperature->value = hidl_temperature.value;

  res = ConvertToHalThrottlingSeverity(hidl_temperature.throttlingStatus,
                                       &hal_temperature->throttling_status);
  if (res != OK) {
    ALOGE("%s: Converting to hal throttling severity type failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  return OK;
}

Return<void> HidlThermalChangedCallback::notifyThrottling(
    const Temperature& temperature) {
  google_camera_hal::Temperature hal_temperature;
  status_t res = ConvertToHalTemperature(temperature, &hal_temperature);
  if (res != OK) {
    ALOGE("%s: Converting to hal temperature failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return Void();
  }

  kNotifyThrottling(hal_temperature);
  return Void();
}

}  // namespace hidl_thermal_utils
}  // namespace hardware
}  // namespace android
