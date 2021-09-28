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

#include "chre/core/sensor_type_helpers.h"

#include <cinttypes>

#include "chre/platform/assert.h"

namespace chre {

ReportingMode SensorTypeHelpers::getReportingMode(uint8_t sensorType) {
  if (isVendorSensorType(sensorType)) {
    return getVendorSensorReportingMode(sensorType);
  }

  switch (sensorType) {
    case CHRE_SENSOR_TYPE_INSTANT_MOTION_DETECT:
    case CHRE_SENSOR_TYPE_STATIONARY_DETECT:
      return ReportingMode::OneShot;
    case CHRE_SENSOR_TYPE_LIGHT:
    case CHRE_SENSOR_TYPE_PROXIMITY:
      return ReportingMode::OnChange;
    default:
      return ReportingMode::Continuous;
  }
}

bool SensorTypeHelpers::isCalibrated(uint8_t sensorType) {
  if (isVendorSensorType(sensorType)) {
    return getVendorSensorIsCalibrated(sensorType);
  }

  switch (sensorType) {
    case CHRE_SENSOR_TYPE_ACCELEROMETER:
    case CHRE_SENSOR_TYPE_GYROSCOPE:
    case CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD:
      return true;
    default:
      return false;
  }
}

bool SensorTypeHelpers::getBiasEventType(uint8_t sensorType,
                                         uint16_t *eventType) {
  CHRE_ASSERT(eventType != nullptr);

  if (isVendorSensorType(sensorType)) {
    return getVendorSensorBiasEventType(sensorType, eventType);
  }

  bool success = true;
  switch (sensorType) {
    case CHRE_SENSOR_TYPE_ACCELEROMETER:
      *eventType = CHRE_EVENT_SENSOR_ACCELEROMETER_BIAS_INFO;
      break;
    case CHRE_SENSOR_TYPE_GYROSCOPE:
      *eventType = CHRE_EVENT_SENSOR_GYROSCOPE_BIAS_INFO;
      break;
    case CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD:
      *eventType = CHRE_EVENT_SENSOR_GEOMAGNETIC_FIELD_BIAS_INFO;
      break;
    case CHRE_SENSOR_TYPE_UNCALIBRATED_ACCELEROMETER:
      *eventType = CHRE_EVENT_SENSOR_UNCALIBRATED_ACCELEROMETER_BIAS_INFO;
      break;
    case CHRE_SENSOR_TYPE_UNCALIBRATED_GYROSCOPE:
      *eventType = CHRE_EVENT_SENSOR_UNCALIBRATED_GYROSCOPE_BIAS_INFO;
      break;
    case CHRE_SENSOR_TYPE_UNCALIBRATED_GEOMAGNETIC_FIELD:
      *eventType = CHRE_EVENT_SENSOR_UNCALIBRATED_GEOMAGNETIC_FIELD_BIAS_INFO;
      break;
    default:
      success = false;
  }

  return success;
}

size_t SensorTypeHelpers::getLastEventSize(uint8_t sensorType) {
  if (isOnChange(sensorType)) {
    if (isVendorSensorType(sensorType)) {
      return getVendorSensorLastEventSize(sensorType);
    }

    switch (sensorType) {
      case CHRE_SENSOR_TYPE_ACCELEROMETER:
      case CHRE_SENSOR_TYPE_GYROSCOPE:
      case CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD:
      case CHRE_SENSOR_TYPE_UNCALIBRATED_ACCELEROMETER:
      case CHRE_SENSOR_TYPE_UNCALIBRATED_GYROSCOPE:
      case CHRE_SENSOR_TYPE_UNCALIBRATED_GEOMAGNETIC_FIELD:
        return sizeof(chreSensorThreeAxisData);
      case CHRE_SENSOR_TYPE_PRESSURE:
      case CHRE_SENSOR_TYPE_LIGHT:
      case CHRE_SENSOR_TYPE_ACCELEROMETER_TEMPERATURE:
      case CHRE_SENSOR_TYPE_GYROSCOPE_TEMPERATURE:
      case CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD_TEMPERATURE:
        return sizeof(chreSensorFloatData);
      case CHRE_SENSOR_TYPE_INSTANT_MOTION_DETECT:
      case CHRE_SENSOR_TYPE_STATIONARY_DETECT:
      case CHRE_SENSOR_TYPE_STEP_DETECT:
        return sizeof(chreSensorOccurrenceData);
      case CHRE_SENSOR_TYPE_PROXIMITY:
        return sizeof(chreSensorByteData);
      default:
        // Update implementation to prevent undefined from being used.
        CHRE_ASSERT(false);
        return 0;
    }
  }
  return 0;
}

const char *SensorTypeHelpers::getSensorTypeName(uint8_t sensorType) {
  if (isVendorSensorType(sensorType)) {
    return getVendorSensorTypeName(sensorType);
  }

  switch (sensorType) {
    case CHRE_SENSOR_TYPE_INVALID:
      return "Unknown";
    case CHRE_SENSOR_TYPE_ACCELEROMETER:
      return "Accelerometer";
    case CHRE_SENSOR_TYPE_INSTANT_MOTION_DETECT:
      return "Instant Motion";
    case CHRE_SENSOR_TYPE_STATIONARY_DETECT:
      return "Stationary Detect";
    case CHRE_SENSOR_TYPE_GYROSCOPE:
      return "Gyroscope";
    case CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD:
      return "Geomagnetic Field";
    case CHRE_SENSOR_TYPE_PRESSURE:
      return "Pressure";
    case CHRE_SENSOR_TYPE_LIGHT:
      return "Light";
    case CHRE_SENSOR_TYPE_PROXIMITY:
      return "Proximity";
    case CHRE_SENSOR_TYPE_STEP_DETECT:
      return "Step Detect";
    case CHRE_SENSOR_TYPE_ACCELEROMETER_TEMPERATURE:
      return "Accelerometer Temp";
    case CHRE_SENSOR_TYPE_GYROSCOPE_TEMPERATURE:
      return "Gyroscope Temp";
    case CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD_TEMPERATURE:
      return "Geomagnetic Field Temp";
    case CHRE_SENSOR_TYPE_UNCALIBRATED_ACCELEROMETER:
      return "Uncal Accelerometer";
    case CHRE_SENSOR_TYPE_UNCALIBRATED_GYROSCOPE:
      return "Uncal Gyroscope";
    case CHRE_SENSOR_TYPE_UNCALIBRATED_GEOMAGNETIC_FIELD:
      return "Uncal Geomagnetic Field";
    default:
      CHRE_ASSERT(false);
      return "";
  }
}

void SensorTypeHelpers::getLastSample(uint8_t sensorType,
                                      const ChreSensorData *event,
                                      ChreSensorData *lastEvent) {
  if (!isOnChange(sensorType)) {
    // no op
    return;
  }

  if (isVendorSensorType(sensorType)) {
    getVendorLastSample(sensorType, event, lastEvent);
  } else {
    switch (sensorType) {
      case CHRE_SENSOR_TYPE_LIGHT:
        copyLastSample<chreSensorFloatData>(&event->floatData,
                                            &lastEvent->floatData);
        break;
      case CHRE_SENSOR_TYPE_PROXIMITY:
        copyLastSample<chreSensorByteData>(&event->byteData,
                                           &lastEvent->byteData);
        break;
      default:
        LOGE("Unhandled sensor type %" PRIu8, sensorType);
    }
  }
}

}  // namespace chre
