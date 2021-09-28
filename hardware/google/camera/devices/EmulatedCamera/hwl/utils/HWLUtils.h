/*
 * Copyright (C) 2013-2019 The Android Open Source Project
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

#ifndef EMULATOR_CAMERA_HAL_HWL_HWL_UTILS_H_
#define EMULATOR_CAMERA_HAL_HWL_HWL_UTILS_H_

#include "EmulatedSensor.h"
#include "hal_types.h"
#include "hwl_types.h"
#include "system/camera_metadata.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif

#ifndef RAT_TO_FLOAT
#define RAT_TO_FLOAT(a) (static_cast<float>(a.numerator) / a.denominator)
#endif

namespace android {

using google_camera_hal::CameraDeviceStatus;
using google_camera_hal::HalCameraMetadata;
using std::pair;
using std::unique_ptr;
using std::unordered_map;

typedef unordered_map<uint32_t, pair<CameraDeviceStatus, unique_ptr<HalCameraMetadata>>>
    PhysicalDeviceMap;
typedef std::unique_ptr<PhysicalDeviceMap> PhysicalDeviceMapPtr;

// Metadata utility functions start
bool HasCapability(const HalCameraMetadata* metadata, uint8_t capability);
status_t GetSensorCharacteristics(const HalCameraMetadata* metadata,
                                  SensorCharacteristics* sensor_chars /*out*/);
PhysicalDeviceMapPtr ClonePhysicalDeviceMap(const PhysicalDeviceMapPtr& src);
// Metadata utility functions end

}  // namespace android

#endif  // EMULATOR_CAMERA_HAL_HWL_HWL_UTILS_H_
