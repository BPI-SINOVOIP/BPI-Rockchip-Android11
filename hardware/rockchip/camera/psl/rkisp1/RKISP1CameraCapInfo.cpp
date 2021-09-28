/*
 * Copyright (C) 2014-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

 #define LOG_TAG "RKISP1CameraCapInfo"

 #include "RKISP1CameraCapInfo.h"

namespace android {
namespace camera2 {

RKISP1CameraCapInfo::RKISP1CameraCapInfo(SensorType type):
    mSensorType(type),
    mSensorFlipping(SENSOR_FLIP_OFF),
    mExposureSync(false),
    mDigiGainOnSensor(false),
    mGainExposureComp(false),
    mGainLag(0),
    mExposureLag(0),
    mFrameInitialSkip(0),
    mStatisticsInitialSkip(0),
    mCITMaxMargin(0),
    mSupportIsoMap(false),
    mNvmDirectory(""),
    mSensorName(""),
    mModuleIndexStr(""),
    mNvmData({nullptr,0}),
    mTestPatternBayerFormat(""),
    mForceAutoGenAndroidMetas(false)
{
    CLEAR(mFov);
}

RKISP1CameraCapInfo::~RKISP1CameraCapInfo()
{
    CLEAR(mFov);
    delete mGCMNodes;
}

const RKISP1CameraCapInfo *getRKISP1CameraCapInfo(int cameraId)
{
    if (cameraId > MAX_CAMERAS) {
        LOGE("ERROR: Invalid camera: %d", cameraId);
        cameraId = 0;
    }
    return (RKISP1CameraCapInfo *)(PlatformData::getCameraCapInfo(cameraId));
}

const string RKISP1CameraCapInfo::getMediaCtlEntityName(string type) const
{
    LOGI("@%s", __FUNCTION__);
    string name = getMediaCtlEntityNames(type)[0];

    return name;
}

const std::vector<string> RKISP1CameraCapInfo::getMediaCtlEntityNames(string type) const
{
    LOGI("@%s", __FUNCTION__);
    string name = string("none");
    std::vector<string> names;

    for (unsigned int numidx = 0; numidx < mMediaCtlElements.size(); numidx++) {
        if (type == mMediaCtlElements[numidx].type) {
            name = mMediaCtlElements[numidx].name;
            LOGI("@%s: found type %s, with name %s", __FUNCTION__, type.c_str(), name.c_str());
            names.push_back(name);
        }
    }

    if (names.size() == 0)
        names.push_back(name);

    return names;
}

const std::string RKISP1CameraCapInfo::getMediaCtlEntityType(std::string name) const
{
    LOGI("@%s", __FUNCTION__);
    std::string type("none");

    uint32_t idx;
    for (idx = 0; idx < mMediaCtlElements.size(); idx++) {
        if (name == mMediaCtlElements[idx].name) {
            type = mMediaCtlElements[idx].type;
            LOGI("@%s: found name %s, with type %s", __FUNCTION__,
                                                     name.c_str(),
                                                     type.c_str());
            break;
        }
    }
    return type;
}

} // namespace camera2
} // namespace android
