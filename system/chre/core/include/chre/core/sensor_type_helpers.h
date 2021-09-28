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

#ifndef CHRE_CORE_SENSOR_TYPE_HELPERS_H_
#define CHRE_CORE_SENSOR_TYPE_HELPERS_H_

#include <cstring>

#include "chre/core/sensor_type.h"
#include "chre/platform/platform_sensor_type_helpers.h"

namespace chre {

/**
 * Exposes several static methods to assist in determining sensor information
 * from the sensor type.
 */
class SensorTypeHelpers : public PlatformSensorTypeHelpers {
 public:
  /**
   * @param sensorType The type of sensor to check
   * @return Whether this sensor is a one-shot sensor.
   */
  static bool isOneShot(uint8_t sensorType) {
    return getReportingMode(sensorType) == ReportingMode::OneShot;
  }

  /**
   * @param sensorType The type of sensor to check
   * @return Whether this sensor is an on-change sensor.
   */
  static bool isOnChange(uint8_t sensorType) {
    return getReportingMode(sensorType) == ReportingMode::OnChange;
  }

  /**
   * @param sensorType The type of sensor to check
   * @return Whether this sensor is a continuous sensor.
   */
  static bool isContinuous(uint8_t sensorType) {
    return getReportingMode(sensorType) == ReportingMode::Continuous;
  }

  /**
   * @param sensorType The type of sensor to check
   * @return true if this sensor is a vendor sensor.
   */
  static bool isVendorSensorType(uint8_t sensorType) {
    return sensorType >= CHRE_SENSOR_TYPE_VENDOR_START;
  }

  /**
   * @param sensorType The type of sensor to get the reporting mode for.
   * @return the reporting mode for this sensor.
   */
  static ReportingMode getReportingMode(uint8_t sensorType);

  /**
   * @param sensorType The type of sensor to check
   * @return Whether this sensor is calibrated.
   */
  static bool isCalibrated(uint8_t sensorType);

  /**
   * @param sensorType The type of sensor to get the bias event type for.
   * @param eventType A non-null pointer to the event type to populate
   * @return true if this sensor has a bias event type.
   */
  static bool getBiasEventType(uint8_t sensorType, uint16_t *eventType);

  /**
   * Determines the size needed to store the latest event from a sensor. Since
   * only on-change sensors have their latest events retained, only those
   * sensors will receive a non-zero value from this method.
   *
   * @param sensorType The sensorType of this sensor.
   * @return the memory size for an on-change sensor to store its last data
   *     event.
   */
  static size_t getLastEventSize(uint8_t sensorType);

  /**
   * @param sensorType The sensor type to obtain a string for.
   * @return A string representation of the sensor type.
   */
  static const char *getSensorTypeName(uint8_t sensorType);

  /**
   * Extracts the last sample from the suppled event and updates it to the
   * supplied last event memory as a single-sample event. No-op if not an
   * on-change sensor.
   *
   * @param sensorType The type of this sensor.
   * @param event A non-null data event of the specified sensorType, must
   *     contain one more more samples.
   * @param lastEvent The sensor's last event memory to store the last event
   *     of an on-change sensor.
   */
  static void getLastSample(uint8_t sensorType, const ChreSensorData *event,
                            ChreSensorData *lastEvent);

  /**
   * Copies the last data sample from newEvent to lastEvent memory and modifies
   * its header accordingly.
   *
   * @param newEvent Sensor data event of type SensorDataType.
   * @param lastEvent Single-sample data event of type SensorDataType.
   */
  template <typename SensorDataType>
  static void copyLastSample(const SensorDataType *newEvent,
                             SensorDataType *lastEvent);
};

template <typename SensorDataType>
void SensorTypeHelpers::copyLastSample(const SensorDataType *newEvent,
                                       SensorDataType *lastEvent) {
  // Copy header and one sample over to last event memory.
  memcpy(lastEvent, newEvent, sizeof(SensorDataType));

  // Modify last event if there are more than one samples in the supplied event.
  if (newEvent->header.readingCount > 1) {
    // Identify last sample's timestamp.
    uint64_t sampleTimestampNs = newEvent->header.baseTimestamp;
    for (size_t i = 0; i < newEvent->header.readingCount; ++i) {
      sampleTimestampNs += newEvent->readings[i].timestampDelta;
    }

    // Update last event to match the last data sample.
    lastEvent->header.baseTimestamp = sampleTimestampNs;
    lastEvent->header.readingCount = 1;
    lastEvent->readings[0] =
        newEvent->readings[newEvent->header.readingCount - 1];
    lastEvent->readings[0].timestampDelta = 0;
  }
}

}  // namespace chre

#endif  // CHRE_CORE_SENSOR_TYPE_HELPERS_H_
