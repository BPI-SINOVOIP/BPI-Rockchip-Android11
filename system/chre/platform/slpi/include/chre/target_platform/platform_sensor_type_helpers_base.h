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

#ifndef CHRE_TARGET_PLATFORM_SENSOR_TYPE_HELPERS_BASE_H_
#define CHRE_TARGET_PLATFORM_SENSOR_TYPE_HELPERS_BASE_H_

#define CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_ACCEL CHRE_VENDOR_SENSOR_TYPE(3)
#define CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_UNCAL_ACCEL CHRE_VENDOR_SENSOR_TYPE(6)
#define CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_UNCAL_GYRO CHRE_VENDOR_SENSOR_TYPE(7)
#define CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_UNCAL_MAG CHRE_VENDOR_SENSOR_TYPE(8)
#define CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_LIGHT CHRE_VENDOR_SENSOR_TYPE(9)

namespace chre {

/**
 * This SensorSampleType is designed to help classify sensor's data type in
 * event handling.
 */
enum class SensorSampleType {
  Byte,
  Float,
  Occurrence,
  ThreeAxis,
  Vendor0,
  Vendor1,
  Vendor2,
  Vendor3,
  Vendor4,
  Vendor5,
  Vendor6,
  Vendor7,
  Vendor8,
  Vendor9,
  Unknown,
};

/**
 * Exposes SLPI-specific methods used by platform code and the SLPI-specific
 * PlatformSensorTypeHelpers implementation to transform sensor types as needed.
 */
class PlatformSensorTypeHelpersBase {
 public:
  /**
   * Obtains the temperature sensor type of the specified sensor type.
   *
   * @param sensorType The sensor type to obtain its temperature sensor type
   *     for.
   * @return The temperature sensor type or CHRE_SENSOR_TYPE_INVALID if not
   *     supported by CHRE.
   */
  static uint8_t getTempSensorType(uint8_t sensorType);

  /**
   * Maps a sensorType to its SensorSampleType.
   *
   * @param sensorType The type of the sensor to obtain its SensorSampleType
   *     for.
   * @return The SensorSampleType of the sensorType.
   */
  static SensorSampleType getSensorSampleTypeFromSensorType(uint8_t sensorType);

  /**
   * @param sensorType The sensor type.
   * @return The corresponding runtime-calibrated sensor type. If the sensor
   *     does not have one or is already runtime-calibrated, then the input
   *     sensorType is returned.
   */
  static uint8_t toCalibratedSensorType(uint8_t sensorType);

  /**
   * @param sensorType The sensor type.
   * @return The corresponding uncalibrated sensor type. If the sensor does not
   *     have one or is already uncalibrated, then the input sensorType is
   *     returned.
   */
  static uint8_t toUncalibratedSensorType(uint8_t sensorType);

  /**
   * @return Whether the given sensor type reports bias events.
   */
  static bool reportsBias(uint8_t sensorType);
};

}  // namespace chre

#endif  // CHRE_TARGET_PLATFORM_SENSOR_TYPE_HELPERS_BASE_H_
