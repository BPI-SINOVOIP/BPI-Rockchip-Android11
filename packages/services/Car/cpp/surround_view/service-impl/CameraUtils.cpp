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

#include "CameraUtils.h"

#include <android-base/logging.h>
#include <android/hardware/automotive/evs/1.1/types.h>

#include <math.h>

using namespace android::hardware::automotive::evs::V1_1;

using ::android::sp;
using ::std::string;
using ::std::vector;
using ::std::map;

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

bool isLogicalCamera(const camera_metadata_t* metadata) {
    if (metadata == nullptr) {
        // A logical camera device must have a valid camera metadata.
        return false;
    }

    // Looking for LOGICAL_MULTI_CAMERA capability from metadata.
    camera_metadata_ro_entry_t entry;
    int rc =
        find_camera_metadata_ro_entry(metadata,
                                      ANDROID_REQUEST_AVAILABLE_CAPABILITIES,
                                      &entry);
    if (0 != rc) {
        // No capabilities are found.
        return false;
    }

    for (size_t i = 0; i < entry.count; ++i) {
        uint8_t cap = entry.data.u8[i];
        if (cap ==
            ANDROID_REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA) {
            return true;
        }
    }

    return false;
}

vector<string> getPhysicalCameraIds(sp<IEvsCamera> camera) {
    if (camera == nullptr) {
        LOG(WARNING) << __FUNCTION__ << "The EVS camera object is invalid";
        return {};
    }

    CameraDesc desc;
    camera->getCameraInfo_1_1([&desc](const CameraDesc& info) {
        desc = info;
    });

    vector<string> physicalCameras;
    const camera_metadata_t* metadata =
        reinterpret_cast<camera_metadata_t*>(&desc.metadata[0]);

    if (!isLogicalCamera(metadata)) {
        // EVS assumes that the device w/o a valid metadata is a physical
        // device.
        LOG(INFO) << desc.v1.cameraId << " is not a logical camera device.";
        physicalCameras.emplace_back(desc.v1.cameraId);
        return physicalCameras;
    }

    // Look for physical camera identifiers
    camera_metadata_ro_entry entry;
    int rc =
        find_camera_metadata_ro_entry(metadata,
                                      ANDROID_LOGICAL_MULTI_CAMERA_PHYSICAL_IDS,
                                      &entry);
    if (rc != 0) {
        LOG(ERROR) << "No physical camera ID is found for "
                   << desc.v1.cameraId;
        return {};
    }

    const uint8_t* ids = entry.data.u8;
    size_t start = 0;
    for (size_t i = 0; i < entry.count; ++i) {
        if (ids[i] == '\0') {
            if (start != i) {
                string id(reinterpret_cast<const char*>(ids + start));
                physicalCameras.emplace_back(id);
            }
            start = i + 1;
        }
    }

    LOG(INFO) << desc.v1.cameraId << " consists of " << physicalCameras.size()
              << " physical camera devices";
    return physicalCameras;
}

string tagToString(uint32_t tag) {
    switch (tag) {
        case ANDROID_LENS_DISTORTION:
            return "ANDROID_LENS_DISTORTION";
        case ANDROID_LENS_INTRINSIC_CALIBRATION:
            return "ANDROID_LENS_INTRINSIC_CALIBRATION";
        case ANDROID_LENS_POSE_TRANSLATION:
            return "ANDROID_LENS_POSE_TRANSLATION";
        case ANDROID_LENS_POSE_ROTATION:
            return "ANDROID_LENS_POSE_ROTATION";
        default:
            LOG(WARNING) << "Cannot recognize the tag: " << tag;
            return {};
    }
}

bool getParam(const camera_metadata_t* metadata,
              uint32_t tag,
              int size,
              float* param) {
    camera_metadata_ro_entry_t entry = camera_metadata_ro_entry_t();
    int rc = find_camera_metadata_ro_entry(metadata, tag, &entry);

    if (rc != 0) {
        LOG(ERROR) << "No metadata found for " << tagToString(tag);
        return false;
    }

    if (entry.count != size || entry.type != TYPE_FLOAT) {
        LOG(ERROR) << "Unexpected size or type for " << tagToString(tag);
        return false;
    }

    const float* lensParam = entry.data.f;
    for (int i = 0; i < size; i++) {
        param[i] = lensParam[i];
    }
    return true;
}

bool getAndroidCameraParams(sp<IEvsCamera> camera,
                            const string& cameraId,
                            AndroidCameraParams& params) {
    if (camera == nullptr) {
        LOG(WARNING) << __FUNCTION__ << "The EVS camera object is invalid";
        return {};
    }

    CameraDesc desc = {};
    camera->getPhysicalCameraInfo(cameraId, [&desc](const CameraDesc& info) {
        desc = info;
    });

    if (desc.metadata.size() == 0) {
        LOG(ERROR) << "No metadata found for " << desc.v1.cameraId;
        return false;
    }

    const camera_metadata_t* metadata =
        reinterpret_cast<camera_metadata_t*>(&desc.metadata[0]);

    // Look for ANDROID_LENS_DISTORTION
    if (!getParam(metadata,
                  ANDROID_LENS_DISTORTION,
                  kSizeLensDistortion,
                  &params.lensDistortion[0])) {
        return false;
    }

    // Look for ANDROID_LENS_INTRINSIC_CALIBRATION
    if (!getParam(metadata,
                  ANDROID_LENS_INTRINSIC_CALIBRATION,
                  kSizeLensIntrinsicCalibration,
                  &params.lensIntrinsicCalibration[0])) {
        return false;
    }

    // Look for ANDROID_LENS_POSE_TRANSLATION
    if (!getParam(metadata,
                  ANDROID_LENS_POSE_TRANSLATION,
                  kSizeLensPoseTranslation,
                  &params.lensPoseTranslation[0])) {
        return false;
    }

    // Look for ANDROID_LENS_POSE_ROTATION
    if (!getParam(metadata,
                  ANDROID_LENS_POSE_ROTATION,
                  kSizeLensPoseRotation,
                  &params.lensPoseRotation[0])) {
        return false;
    }

    return true;
}

vector<SurroundViewCameraParams> convertToSurroundViewCameraParams(
        const map<string, AndroidCameraParams>& androidCameraParamsMap) {
    vector<SurroundViewCameraParams> result;

    // TODO(b/156101189): the cameras are in random order now. They need to be
    // sorted based on the camera position info from config file.
    for (const auto& entry : androidCameraParamsMap) {
        SurroundViewCameraParams svParams;

        // Android Camera format for intrinsics: [f_x, f_y, c_x, c_y, s]
        //
        // To corelib:
        // SurroundViewCameraParams.intrinsics =
        //         [ f_x,   s, c_x,
        //             0, f_y, c_y,
        //             0,   0,   1 ];
        const float* intrinsics = &entry.second.lensIntrinsicCalibration[0];
        svParams.intrinsics[0] = intrinsics[0];
        svParams.intrinsics[1] = intrinsics[4];
        svParams.intrinsics[2] = intrinsics[2];
        svParams.intrinsics[3] = 0;
        svParams.intrinsics[4] = intrinsics[1];
        svParams.intrinsics[5] = intrinsics[3];
        svParams.intrinsics[6] = 0;
        svParams.intrinsics[7] = 0;
        svParams.intrinsics[8] = 1;

        // Android Camera format for lens distortion:
        //         Radial: [kappa_1, kappa_2, kappa_3]
        //         Tangential: [kappa_4, kappa_5]
        //
        // To corelib:
        // SurroundViewCameraParams.distortion =
        //         [kappa_1, kappa_2, kappa_3, kappa_4];
        const float* distortion = &entry.second.lensDistortion[0];
        svParams.distorion[0] = distortion[0];
        svParams.distorion[1] = distortion[1];
        svParams.distorion[2] = distortion[2];
        svParams.distorion[3] = distortion[3];

        // Android Camera format for rotation:
        //         quaternion coefficients (x,y,z,w)
        //
        // To corelib:
        //         theta = 2 * acos(w)
        //         a_x = x / sin(theta/2)
        //         a_y = y / sin(theta/2)
        //         a_z = z / sin(theta/2)
        // SurroundViewCameraParams.rvec =
        //         [theta * a_x, theta * a_y, theta * a_z];
        const float* rotation = &entry.second.lensPoseRotation[0];
        const float theta = 2 * acos(rotation[3]);
        const float a_x = rotation[0] / sin(theta / 2);
        const float a_y = rotation[1] / sin(theta / 2);
        const float a_z = rotation[2] / sin(theta / 2);
        svParams.rvec[0] = theta * a_x;
        svParams.rvec[1] = theta * a_y;
        svParams.rvec[2] = theta * a_z;

        // Android Camera format for translation: Translation = (x,y,z)
        //
        // To corelib:
        // SurroundViewCameraParams.tvec = [x, y, z];
        const float* translation = &entry.second.lensPoseTranslation[0];
        svParams.tvec[0] = translation[0];
        svParams.tvec[1] = translation[1];
        svParams.tvec[2] = translation[2];

        LOG(INFO) << "Camera parameters for " << entry.first
                  << " have been converted to SV core lib format successfully";
        result.emplace_back(svParams);
    }

    return result;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android

