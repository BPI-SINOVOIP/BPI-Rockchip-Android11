/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef CHRE_CORE_SENSOR_TYPE_H_
#define CHRE_CORE_SENSOR_TYPE_H_

#include <cstddef>
#include <cstdint>

#include "chre_api/chre/sensor.h"

/**
 * @file
 * This file contains miscellaneous types useful in the core framework and
 * platform implementations for dealing with sensors.
 *
 * Additionally, it contains methods useful for converting between these types
 * and types used by the CHRE APIs and other miscellaneous helper methods that
 * help make the rest of the codebase more readable.
 */
namespace chre {

//! Indicates the reporting mode of the sensor
enum class ReportingMode : uint8_t {
  OnChange,
  OneShot,
  Continuous,
};

//! The union of possible CHRE sensor data event type with one sample.
union ChreSensorData {
  struct chreSensorDataHeader header;
  struct chreSensorThreeAxisData threeAxisData;
  struct chreSensorOccurrenceData occurrenceData;
  struct chreSensorFloatData floatData;
  struct chreSensorByteData byteData;
};

// Validate that aliasing into the header is valid for all types of the union
static_assert(offsetof(ChreSensorData, threeAxisData.header) == 0,
              "Three axis data header not at offset 0");
static_assert(offsetof(ChreSensorData, occurrenceData.header) == 0,
              "Occurrence data header not at offset 0");
static_assert(offsetof(ChreSensorData, floatData.header) == 0,
              "Float data header not at offset 0");
static_assert(offsetof(ChreSensorData, byteData.header) == 0,
              "Byte data header not at offset 0");

/**
 * Returns a sensor sample event type for a given sensor type. The sensor type
 * must not be SensorType::Unknown. This is a fatal error.
 *
 * @param sensorType The type of the sensor.
 * @return The event type for a sensor sample of the given sensor type.
 */
constexpr uint16_t getSampleEventTypeForSensorType(uint8_t sensorType) {
  return CHRE_EVENT_SENSOR_DATA_EVENT_BASE + sensorType;
}

/**
 * Returns a sensor type for a given sensor sample event type.
 *
 * @param eventType The event type for a sensor sample.
 * @return The type of the sensor.
 */
constexpr uint8_t getSensorTypeForSampleEventType(uint16_t eventType) {
  return static_cast<uint8_t>(eventType - CHRE_EVENT_SENSOR_DATA_EVENT_BASE);
}

/**
 * This SensorMode is designed to wrap constants provided by the CHRE API to
 * imrpove type-safety. The details of these modes are left to the CHRE API mode
 * definitions contained in the chreSensorConfigureMode enum.
 */
enum class SensorMode {
  Off,
  ActiveContinuous,
  ActiveOneShot,
  PassiveContinuous,
  PassiveOneShot,
};

/**
 * Returns a string representation of the given sensor mode.
 *
 * @param sensorMode The sensor mode to obtain a string for.
 * @return A string representation of the sensor mode.
 */
const char *getSensorModeName(SensorMode sensorMode);

/**
 * @return true if the sensor mode is considered to be active and would cause a
 *         sensor to be powered on in order to get sensor data.
 */
constexpr bool sensorModeIsActive(SensorMode sensorMode) {
  return (sensorMode == SensorMode::ActiveContinuous ||
          sensorMode == SensorMode::ActiveOneShot);
}

/**
 * @return true if the sensor mode is considered to be passive and would not
 *         cause a sensor to be powered on in order to get sensor data.
 */
constexpr bool sensorModeIsPassive(SensorMode sensorMode) {
  return (sensorMode == SensorMode::PassiveContinuous ||
          sensorMode == SensorMode::PassiveOneShot);
}

/**
 * @return true if the sensor mode is considered to be contunuous.
 */
constexpr bool sensorModeIsContinuous(SensorMode sensorMode) {
  return (sensorMode == SensorMode::ActiveContinuous ||
          sensorMode == SensorMode::PassiveContinuous);
}

/**
 * @return true if the sensor mode is considered to be one-shot.
 */
constexpr bool sensorModeIsOneShot(SensorMode sensorMode) {
  return (sensorMode == SensorMode::ActiveOneShot ||
          sensorMode == SensorMode::PassiveOneShot);
}

/**
 * Translates a CHRE API enum sensor mode to a SensorMode. This function also
 * performs input validation and will default to SensorMode::Off if the provided
 * value is not a valid enum value.
 *
 * @param enumSensorMode A potentially unsafe CHRE API enum sensor mode.
 * @return Returns a SensorMode given a CHRE API enum sensor mode.
 */
SensorMode getSensorModeFromEnum(enum chreSensorConfigureMode enumSensorMode);

/**
 * Translates a SensorMode enum to the CHRE API enum sensor mode.
 *
 * @param enumSensorMode A valid SensorMode value
 * @return A valid CHRE API enum sensor mode.
 */
chreSensorConfigureMode getConfigureModeFromSensorMode(
    enum SensorMode enumSensorMode);

}  // namespace chre

#endif  // CHRE_CORE_SENSOR_TYPE_H_
