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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_TESTS_TEST_UTILS_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_TESTS_TEST_UTILS_H_

#include "camera_device_session_hwl.h"
#include "hal_types.h"

namespace android {
namespace google_camera_hal {
namespace test_utils {

static const uint32_t kDefaultPreviewWidth = 1920;
static const uint32_t kDefaultPreviewHeight = 1080;

// Return a stream configuration that consists of only a preview stream.
void GetPreviewOnlyStreamConfiguration(StreamConfiguration* config,
                                       uint32_t width = kDefaultPreviewWidth,
                                       uint32_t height = kDefaultPreviewHeight);

// Return a stream configuration that consists of one preview stream for each
// physical camera.
void GetPhysicalPreviewStreamConfiguration(
    StreamConfiguration* config,
    const std::vector<uint32_t>& physical_camera_ids,
    uint32_t width = kDefaultPreviewWidth,
    uint32_t height = kDefaultPreviewHeight);

// If the session belongs to a logical camera that consists of multiple
// physical cameras.
bool IsLogicalCamera(CameraDeviceSessionHwl* session_hwl);

}  // namespace test_utils
}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_TESTS_TEST_UTILS_H_