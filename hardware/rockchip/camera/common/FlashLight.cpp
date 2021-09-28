/*
 * Copyright (C) 2014-2017 Intel Corporation.
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

#define LOG_TAG "FlashLight"

#include <sys/stat.h>
#include "LogHelper.h"
#include <FlashLight.h>
#include "PlatformData.h"

namespace android {
namespace camera2 {

FlashLight& FlashLight::getInstance()
{
    static FlashLight flashInstance;
    return flashInstance;
}

FlashLight::FlashLight() : mCallbacks(NULL)
{
    for (int camid = 0; camid< MAX_NUM_CAMERA; camid++) {
        for (int flindex = 0; flindex < MAX_NUM_FLASH_OF_ONE_MODULE; flindex++) {
            mFlashFds[camid][flindex] = -1;
            mCameraOpen[camid] = false;
            mFlashOn[camid][flindex] = false;
        }
    }
}

FlashLight::~FlashLight()
{
    for (int camid = 0; camid< MAX_NUM_CAMERA; camid++) {
        for (int flindex = 0; flindex < MAX_NUM_FLASH_OF_ONE_MODULE; flindex++) {
            if (mFlashFds[camid][flindex] >= 0)
                {
                    setFlashMode(camid, flindex, false);
                    close(mFlashFds[camid][flindex]);
                    mFlashFds[camid][flindex] = -1;
                }
        }
    }
}

int32_t FlashLight::getFlashLightInfo(
        const int cameraId, bool &hasFlash,
        const char* (&flashNode)[MAX_NUM_FLASH_OF_ONE_MODULE]) {

    int32_t retVal = 0;
    hasFlash = false;

    const CameraHWInfo* camHwInfo = PlatformData::getCameraHWInfo();
    CheckError(!camHwInfo, -EINVAL, "@%s,  camera hw info was not uninitialized",
                   __FUNCTION__);

    const struct SensorDriverDescriptor* sensorInfo = camHwInfo->getSensorDrvDes(cameraId);
    CheckError(!sensorInfo, -EINVAL, "@%s,  camera sensor info was not uninitialized",
                   __FUNCTION__);

    int flash_num = sensorInfo->mFlashNum;

    for (int i = 0; i < flash_num; i++) {

        const std::string& node = sensorInfo->mModuleFlashDevName[i];
        flashNode[i] = node.c_str();
    }

    hasFlash = flash_num > 0 ? true : false;
    LOGD("@%s : hasFlash %d, flashNode[0]: %s, flashNode[1]: %s",
         __FUNCTION__, hasFlash, flashNode[0], flashNode[1]);

    return retVal;
}

int32_t FlashLight::setCallbacks(
        const camera_module_callbacks_t* callbacks)
{
    int32_t retVal = 0;
    mCallbacks = callbacks;
    return retVal;
}

int32_t FlashLight::init(const int cameraId)
{
    int32_t retVal = 0;
    bool hasFlash = false;
    const char* flashPath[MAX_NUM_FLASH_OF_ONE_MODULE]
        = {NULL, NULL};

    if (cameraId < 0 || cameraId >= MAX_NUM_CAMERA) {
        LOGE("%s: Invalid camera id: %d", __FUNCTION__, cameraId);
        return -EINVAL;
    }

    getFlashLightInfo(cameraId, hasFlash, flashPath);

    if (!hasFlash) {
        LOGE("%s: No flash available for camera id: %d", __FUNCTION__, cameraId);
        retVal = -ENOSYS;
    } else if (mCameraOpen[cameraId]) {
        LOGE("%s: Camera in use for camera id: %d", __FUNCTION__, cameraId);
        retVal = -EBUSY;
    } else if (mFlashFds[cameraId][0] >= 0) {
        LOGD("%s: Flash is already inited for camera id: %d", __FUNCTION__, cameraId);
    } else {
        for (int i = 0; i < MAX_NUM_FLASH_OF_ONE_MODULE; i++) {
            if (flashPath[i]) {
                if (mFlashFds[cameraId][i] == -1)
                    mFlashFds[cameraId][i] = open(flashPath[i], O_RDWR | O_NONBLOCK);
                if (mFlashFds[cameraId][i] < 0) {
                    LOGE("%s: Unable to open node '%s'", __FUNCTION__, flashPath[i]);
                    retVal = -EBUSY;
                    break;
                }
            }
        }
    }

    LOGD("@%s : retval = %d", __FUNCTION__, retVal);
    return retVal;
}

int32_t FlashLight::deinit(const int cameraId)
{
    int32_t retVal = 0;

    if (cameraId < 0 || cameraId >= MAX_NUM_CAMERA) {
        LOGE("%s: Invalid camera id: %d", __FUNCTION__, cameraId);
        retVal = -EINVAL;
    } else {
        for (int i = 0; i < MAX_NUM_FLASH_OF_ONE_MODULE; i++) {
            if (mFlashFds[cameraId][i] >= 0) {
                setFlashMode(cameraId, i, false);
                close(mFlashFds[cameraId][i]);
                mFlashFds[cameraId][i] = -1;
            }
        }
    }

    return retVal;
}

int32_t FlashLight::setFlashMode(const int cameraId, const bool mode)
{
    int32_t retVal = 0;
    LOGD("@%s : cameraId %d, mode %d", __FUNCTION__, cameraId, mode);

    if (cameraId < 0 || cameraId >= MAX_NUM_CAMERA) {
        LOGE("%s: Invalid camera id: %d", __FUNCTION__, cameraId);
        retVal = -EINVAL;
    } else if (mode == mFlashOn[cameraId][0]) {
        LOGD("%s: flash %d is already in requested state: %d", __FUNCTION__, cameraId, mode);
        retVal = -EALREADY;
    } else if (mFlashFds[cameraId][0] < 0) {
        LOGE("%s: called for uninited flash: %d", __FUNCTION__, cameraId);
        retVal = -EINVAL;
    }  else {
        for (int i = 0; i < MAX_NUM_FLASH_OF_ONE_MODULE; i++) {
            if (mFlashFds[cameraId][i] >= 0) {
                retVal |= setFlashMode(cameraId, i, mode);
            }
        }
    }

    return retVal;
}

int32_t FlashLight::setFlashMode(const int cameraId, int flashIdx, const bool mode)
{
    int32_t retVal = 0;
    LOGD("@%s : cameraId %d, mode %d", __FUNCTION__, cameraId, mode);

    if (cameraId < 0 || cameraId >= MAX_NUM_CAMERA) {
        LOGE("%s: Invalid camera id: %d", __FUNCTION__, cameraId);
        retVal = -ENOSYS;
    } else if (mode == mFlashOn[cameraId][flashIdx]) {
        LOGD("%s: flash %d is already in requested state: %d", __FUNCTION__, cameraId, mode);
        retVal = -EALREADY;
    } else if (mFlashFds[cameraId][flashIdx] < 0) {
        LOGE("%s: called for uninited flash: %d", __FUNCTION__, cameraId);
        retVal = -ENOSYS;
    }  else {
        struct v4l2_control control;
        struct v4l2_queryctrl qctrl;

        memset(&control, 0, sizeof(control));
        memset(&qctrl, 0, sizeof(qctrl));

        qctrl.id = V4L2_CID_FLASH_TORCH_INTENSITY;
        if (ioctl(mFlashFds[cameraId][flashIdx], VIDIOC_QUERYCTRL, &qctrl) < 0) {
            LOGE("@%s : query falsh torch power failed", __FUNCTION__);
            return -ENOSYS;
        }
        LOGD(" qctrl.flags(0x%08x)", qctrl.flags);
        if (qctrl.flags != V4L2_CTRL_FLAG_READ_ONLY) {
            control.id = V4L2_CID_FLASH_TORCH_INTENSITY;
            control.value = qctrl.default_value;
            if (ioctl(mFlashFds[cameraId][flashIdx], VIDIOC_S_CTRL, &control, 0) < 0) {
                LOGE("@%s : set flash intensity failed, may be not support set torch intensity.", __FUNCTION__);
                return -ENOSYS;
            }
        }

        memset(&control, 0, sizeof(control));
        control.id = V4L2_CID_FLASH_LED_MODE;
        control.value = mode ? V4L2_FLASH_LED_MODE_TORCH : V4L2_FLASH_LED_MODE_NONE;
        if (ioctl(mFlashFds[cameraId][flashIdx], VIDIOC_S_CTRL, &control, 0) < 0) {
            LOGE("@%s : set flash mode %d failed", __FUNCTION__, mode);
            return -ENOSYS;
        }
        LOGI("@%s : set flash mode %d sucess", __FUNCTION__, mode);
        mFlashOn[cameraId][flashIdx] = mode;
    }
    return retVal;
}

int32_t FlashLight::reserveFlashForCamera(const int cameraId)
{
    int32_t retVal = 0;

    if (cameraId < 0 || cameraId >= MAX_NUM_CAMERA) {
        LOGE("%s: Invalid camera id: %d", __FUNCTION__, cameraId);
        retVal = -EINVAL;
    } else if (mCameraOpen[cameraId]) {
        LOGD("%s: Flash already reserved for camera id: %d", __FUNCTION__,
                cameraId);
    } else {
        deinit(cameraId);
        mCameraOpen[cameraId] = true;

        bool hasFlash = false;
        const char* flashPath[MAX_NUM_FLASH_OF_ONE_MODULE]
            = {NULL, NULL};

        getFlashLightInfo(cameraId, hasFlash, flashPath);

        if (mCallbacks == NULL ||
                mCallbacks->torch_mode_status_change == NULL) {
            LOGE("%s: Callback is not defined!", __FUNCTION__);
            retVal = -ENOSYS;
        } else if (!hasFlash) {
            LOGD("%s: no flash exists for camera id: %d", __FUNCTION__, cameraId);
        } else {
            char cameraIdStr[1];
            snprintf(cameraIdStr, 1, "%d", cameraId);
            mCallbacks->torch_mode_status_change(mCallbacks,
                    cameraIdStr,
                    TORCH_MODE_STATUS_NOT_AVAILABLE);
        }
    }

    return retVal;
}

int32_t FlashLight::releaseFlashFromCamera(const int cameraId)
{
    int32_t retVal = 0;

    if (cameraId < 0 || cameraId >= MAX_NUM_FLASH_OF_ONE_MODULE) {
        LOGE("%s: Invalid camera id: %d", __FUNCTION__, cameraId);
        retVal = -EINVAL;
    } else if (!mCameraOpen[cameraId]) {
        LOGD("%s: Flash not reserved for camera id: %d", __FUNCTION__, cameraId);
    } else {
        mCameraOpen[cameraId] = false;

        bool hasFlash = false;
        const char* flashPath[MAX_NUM_FLASH_OF_ONE_MODULE]
            = {NULL, NULL};

        getFlashLightInfo(cameraId, hasFlash, flashPath);

        if (mCallbacks == NULL ||
                mCallbacks->torch_mode_status_change == NULL) {
            LOGE("%s: Callback is not defined!", __FUNCTION__);
            retVal = -ENOSYS;
        } else if (!hasFlash) {
            LOGD("%s: no flash exists for camera id: %d", __FUNCTION__, cameraId);
        } else {
            char cameraIdStr[1];
            snprintf(cameraIdStr, 1, "%d", cameraId);
            mCallbacks->torch_mode_status_change(mCallbacks,
                    cameraIdStr,
                    TORCH_MODE_STATUS_AVAILABLE_OFF);
        }
    }

    return retVal;
}

} /* namespace camera2 */
} /* namespace android */

