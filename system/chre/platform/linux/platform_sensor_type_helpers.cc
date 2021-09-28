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

#include "chre/platform/platform_sensor_type_helpers.h"

#ifdef CHREX_SENSOR_SUPPORT
#include "chre/extensions/platform/vendor_sensor_types.h"
#endif  // CHREX_SENSOR_SUPPORT

namespace chre {

ReportingMode PlatformSensorTypeHelpers::getVendorSensorReportingMode(
    uint8_t sensorType) {
  return ReportingMode::Continuous;
}

bool PlatformSensorTypeHelpers::getVendorSensorIsCalibrated(
    uint8_t sensorType) {
  return false;
}

bool PlatformSensorTypeHelpers::getVendorSensorBiasEventType(
    uint8_t sensorType, uint16_t *eventType) {
  return false;
}

const char *PlatformSensorTypeHelpers::getVendorSensorTypeName(
    uint8_t sensorType) {
  return "";
}

size_t PlatformSensorTypeHelpers::getVendorSensorLastEventSize(
    uint8_t sensorType) {
  return 0;
}

void PlatformSensorTypeHelpers::getVendorLastSample(uint8_t sensorType,
                                                    const ChreSensorData *event,
                                                    ChreSensorData *lastEvent) {
}

}  // namespace chre
