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

#ifndef CHRE_PLATFORM_LINUX_PLATFORM_SENSOR_MANAGER_BASE_H_
#define CHRE_PLATFORM_LINUX_PLATFORM_SENSOR_MANAGER_BASE_H_

#include <cstdint>

#include "chre/pal/sensor.h"

namespace chre {

/**
 * Storage for the Linux implementation of the PlatformSensorManager class.
 */
class PlatformSensorManagerBase {
 public:
  //! The instance of callbacks that are provided to the CHRE PAL.
  static const chrePalSensorCallbacks sSensorCallbacks;

  //! The instance of the CHRE PAL API. This will be set to nullptr if the
  //! platform does not supply an implementation.
  const chrePalSensorApi *mSensorApi;

 private:
  //! Event handlers for the CHRE Sensor PAL. Refer to chre/pal/sensor.h for
  //! further information.
  static void samplingStatusUpdateCallback(
      uint32_t sensorHandle, struct chreSensorSamplingStatus *status);
  static void dataEventCallback(uint32_t sensorHandle, void *data);
  static void biasEventCallback(uint32_t sensorHandle, void *biasData);
  static void flushCompleteCallback(uint32_t sensorHandle,
                                    uint32_t flushRequestId, uint8_t errorCode);
};

}  // namespace chre

#endif  // CHRE_PLATFORM_LINUX_PLATFORM_SENSOR_MANAGER_BASE_H_
