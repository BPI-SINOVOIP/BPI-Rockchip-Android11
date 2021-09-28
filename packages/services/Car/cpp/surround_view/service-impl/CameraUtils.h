/*
 * Copyright 2020 The Android Open Source Project
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

#pragma once

#include <android/hardware/automotive/evs/1.1/IEvsCamera.h>
#include <system/camera_metadata.h>

#include <string>
#include <vector>

#include "core_lib.h"

using ::android::hardware::automotive::evs::V1_1::IEvsCamera;
using ::android_auto::surround_view::SurroundViewCameraParams;

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

const int kSizeLensDistortion = 5;
const int kSizeLensIntrinsicCalibration = 5;
const int kSizeLensPoseTranslation = 3;
const int kSizeLensPoseRotation = 4;

// Camera parameters that the Android Camera team defines.
struct AndroidCameraParams {
    float lensDistortion[kSizeLensDistortion];
    float lensIntrinsicCalibration[kSizeLensIntrinsicCalibration];
    float lensPoseTranslation[kSizeLensPoseTranslation];
    float lensPoseRotation[kSizeLensPoseRotation];
};

// Gets the underlying physical camera ids for logical camera.
// If the given camera is not a logical, its own id will be returned.
std::vector<std::string> getPhysicalCameraIds(android::sp<IEvsCamera> camera);

// Gets the intrinsic/extrinsic parameters for the given physical camera id.
// Returns true if the parameters are obtained successfully. Returns false
// otherwise.
bool getAndroidCameraParams(android::sp<IEvsCamera> camera,
                            const std::string& cameraId,
                            AndroidCameraParams& params);

// Converts the camera parameters from Android Camera format into Surround View
// core lib format.
std::vector<SurroundViewCameraParams> convertToSurroundViewCameraParams(
        const std::map<std::string, AndroidCameraParams>& androidCameraParamsMap);

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
