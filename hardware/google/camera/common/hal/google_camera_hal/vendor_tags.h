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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_CAMERA_VENDOR_TAGS_H
#define HARDWARE_GOOGLE_CAMERA_HAL_CAMERA_VENDOR_TAGS_H

#include <mutex>
#include <vector>

#include "hal_types.h"

namespace android {
namespace google_camera_hal {
namespace hal_vendor_tag_utils {

// Modifies the HWL default request settings to include any default values
// for HAL vendor tags as needed
status_t ModifyDefaultRequestSettings(RequestTemplate type,
                                      HalCameraMetadata* default_settings);

// Modifies the result/request/session/characteristics keys with HAL vendor tag
// IDs which should be listed in each. This should be invoked before passing
// the HWL characteristics to HIDL layer
status_t ModifyCharacteristicsKeys(HalCameraMetadata* metadata);

}  // namespace hal_vendor_tag_utils
}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_CAMERA_VENDOR_TAGS_H
