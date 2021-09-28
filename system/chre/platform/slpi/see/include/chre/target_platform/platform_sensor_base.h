/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef CHRE_PLATFORM_SLPI_SEE_PLATFORM_SENSOR_BASE_H_
#define CHRE_PLATFORM_SLPI_SEE_PLATFORM_SENSOR_BASE_H_

#include "chre/core/sensor_request.h"
#include "chre/core/sensor_type_helpers.h"
#include "chre/platform/slpi/see/see_helper.h"

#ifdef CHREX_SENSOR_SUPPORT
#include "chre/extensions/platform/vendor_sensor_types.h"
#endif  // CHREX_SENSOR_SUPPORT

namespace chre {

//! The max length of sensorName
constexpr size_t kSensorNameMaxLen = 64;

/**
 * Storage for the SLPI SEE implementation of the PlatformSensor class.
 */
class PlatformSensorBase {
 public:
  /**
   * Initializes various members of PlatformSensorBase.
   */
  void initBase(uint8_t sensorType, uint64_t minInterval,
                const char *sensorName, bool passiveSupported);

  //! Stores the last received sampling status from SEE for this sensor making
  //! it easier to dedup updates that come in later from SEE.
  SeeHelperCallbackInterface::SamplingStatusData mLastReceivedSamplingStatus{};

  //! The name (type and model) of this sensor.
  char mSensorName[kSensorNameMaxLen];

  //! The minimum interval of this sensor.
  uint64_t mMinInterval;

  //! The sensor type of this sensor.
  uint8_t mSensorType;

  //! Whether this sensor supports passive sensor requests.
  bool mPassiveSupported = false;
};

}  // namespace chre

#endif  // CHRE_PLATFORM_SLPI_SEE_PLATFORM_SENSOR_BASE_H_
