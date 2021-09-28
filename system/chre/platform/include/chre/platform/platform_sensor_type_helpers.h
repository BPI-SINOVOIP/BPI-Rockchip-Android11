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

#ifndef CHRE_PLATFORM_PLATFORM_SENSOR_TYPE_HELPERS_H_
#define CHRE_PLATFORM_PLATFORM_SENSOR_TYPE_HELPERS_H_

#include "chre/core/sensor_type.h"
#include "chre/target_platform/platform_sensor_type_helpers_base.h"

#define CHRE_SENSOR_TYPE_INVALID 0
#define CHRE_VENDOR_SENSOR_TYPE(x) (CHRE_SENSOR_TYPE_VENDOR_START + x)

namespace chre {

/**
 * Exposes several static methods to assist in determining sensor information
 * from the sensor type that are specific to the platform implementation.
 *
 * These sensor type helper methods should only be invoked if
 * SensorTypeHelpers::isVendorSensorType() returns true.
 */
class PlatformSensorTypeHelpers : public PlatformSensorTypeHelpersBase {
 protected:
  /**
   * @return the reporting mode for this vendor sensor.
   */
  static ReportingMode getVendorSensorReportingMode(uint8_t sensorType);

  /**
   * @return Whether this vendor sensor is calibrated.
   */
  static bool getVendorSensorIsCalibrated(uint8_t sensorType);

  /**
   * @param eventType A non-null pointer to the event type to populate
   * @return true if this vendor sensor has a bias event type.
   */
  static bool getVendorSensorBiasEventType(uint8_t sensorType,
                                           uint16_t *eventType);

  /**
   * @return The memory size needed to store the last on-change sample.
   */
  static size_t getVendorSensorLastEventSize(uint8_t sensorType);

  /**
   * @param sensorType The vendor sensor type to obtain a string for.
   * @return A string representation of the vendor sensor type.
   */
  static const char *getVendorSensorTypeName(uint8_t sensorType);

  /**
   * Extracts the last sample from the suppled event and updates it to the
   * supplied last event memory as a single-sample event.
   *
   * @param sensorType The vendor sensor type to get the last sample for.
   * @param event A non-null data event of the specified sensorType, must
   *     contain one more more samples.
   * @param lastEvent The sensor's last event memory to store the last event
   *     of an on-change sensor.
   */
  static void getVendorLastSample(uint8_t sensorType,
                                  const ChreSensorData *event,
                                  ChreSensorData *lastEvent);
};

}  // namespace chre

#endif  // CHRE_PLATFORM_PLATFORM_SENSOR_TYPE_HELPERS_H_
