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

#include "chre/core/sensor_type.h"

#include "chre/platform/assert.h"

namespace chre {

const char *getSensorModeName(SensorMode sensorMode) {
  switch (sensorMode) {
    case SensorMode::Off:
      return "Off";
    case SensorMode::ActiveOneShot:
      return "ActiveOneShot";
    case SensorMode::PassiveOneShot:
      return "PassiveOneShot";
    case SensorMode::ActiveContinuous:
      return "ActiveContinuous";
    case SensorMode::PassiveContinuous:
      return "PassiveContinuous";
    default:
      CHRE_ASSERT(false);
      return "";
  }
}

SensorMode getSensorModeFromEnum(enum chreSensorConfigureMode enumSensorMode) {
  switch (enumSensorMode) {
    case CHRE_SENSOR_CONFIGURE_MODE_DONE:
      return SensorMode::Off;
    case CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS:
      return SensorMode::ActiveContinuous;
    case CHRE_SENSOR_CONFIGURE_MODE_ONE_SHOT:
      return SensorMode::ActiveOneShot;
    case CHRE_SENSOR_CONFIGURE_MODE_PASSIVE_CONTINUOUS:
      return SensorMode::PassiveContinuous;
    case CHRE_SENSOR_CONFIGURE_MODE_PASSIVE_ONE_SHOT:
      return SensorMode::PassiveOneShot;
    default:
      // Default to off since it is the least harmful and has no power impact.
      return SensorMode::Off;
  }
}

chreSensorConfigureMode getConfigureModeFromSensorMode(
    enum SensorMode enumSensorMode) {
  switch (enumSensorMode) {
    case SensorMode::Off:
      return CHRE_SENSOR_CONFIGURE_MODE_DONE;
    case SensorMode::ActiveContinuous:
      return CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS;
    case SensorMode::ActiveOneShot:
      return CHRE_SENSOR_CONFIGURE_MODE_ONE_SHOT;
    case SensorMode::PassiveContinuous:
      return CHRE_SENSOR_CONFIGURE_MODE_PASSIVE_CONTINUOUS;
    case SensorMode::PassiveOneShot:
      return CHRE_SENSOR_CONFIGURE_MODE_PASSIVE_ONE_SHOT;
    default:
      // Default to off since it is the least harmful and has no power impact.
      return CHRE_SENSOR_CONFIGURE_MODE_DONE;
  }
}

}  // namespace chre
