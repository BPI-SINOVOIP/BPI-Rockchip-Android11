/*
 * Copyright (C) 2011 The Android Open Source Project
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

/*
 * Contains implementation of a class EmulatedCameraFactory that manages cameras
 * available for emulation.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "EmulatedCamera_Factory"

#include "EmulatedCameraFactory.h"
#include "EmulatedCameraHotplugThread.h"
#include "EmulatedFakeCamera.h"
#include "EmulatedFakeCamera2.h"
#include "EmulatedFakeCamera3.h"
#include "EmulatedQemuCamera.h"
#include "EmulatedQemuCamera3.h"

#include <log/log.h>
#include <cutils/properties.h>

extern camera_module_t HAL_MODULE_INFO_SYM;

/*
 * A global instance of EmulatedCameraFactory is statically instantiated and
 * initialized when camera emulation HAL is loaded.
 */
android::EmulatedCameraFactory gEmulatedCameraFactory;

namespace android {

EmulatedCameraFactory::EmulatedCameraFactory() :
        mQemuClient(),
        mConstructedOK(false),
        mGBM(&GraphicBufferMapper::get()),
        mCallbacks(nullptr) {

    /*
     * Figure out how many cameras need to be created, so we can allocate the
     * array of emulated cameras before populating it.
     */

    // QEMU Cameras
    std::vector<QemuCameraInfo> qemuCameras;
    if (mQemuClient.connectClient(nullptr) == NO_ERROR) {
        findQemuCameras(&qemuCameras);
    }

    int fakeCameraNum = 0;
    waitForQemuSfFakeCameraPropertyAvailable();
    // Fake Cameras
    if (isFakeCameraEmulationOn(/* backCamera */ true)) {
        fakeCameraNum++;
    }
    if (isFakeCameraEmulationOn(/* backCamera */ false)) {
        fakeCameraNum++;
    }

    /*
     * We have the number of cameras we need to create, now allocate space for
     * them.
     */
    mEmulatedCameras.reserve(qemuCameras.size() + fakeCameraNum);

    createQemuCameras(qemuCameras);

    // Create fake cameras, if enabled.
    if (isFakeCameraEmulationOn(/* backCamera */ true)) {
        createFakeCamera(/* backCamera */ true);
    }
    if (isFakeCameraEmulationOn(/* backCamera */ false)) {
        createFakeCamera(/* backCamera */ false);
    }

    ALOGE("%zu cameras are being emulated. %d of them are fake cameras.",
            mEmulatedCameras.size(), fakeCameraNum);

    // Create hotplug thread.
    {
        std::vector<int> cameraIdVector;
        for (const auto &camera: mEmulatedCameras) {
            cameraIdVector.push_back(camera->getCameraId());
        }
        mHotplugThread = new EmulatedCameraHotplugThread(std::move(cameraIdVector));
        mHotplugThread->run("EmulatedCameraHotplugThread");
    }

    mConstructedOK = true;
}

EmulatedCameraFactory::~EmulatedCameraFactory() {
    mEmulatedCameras.clear();

    if (mHotplugThread != nullptr) {
        mHotplugThread->requestExit();
        mHotplugThread->join();
    }
}

/******************************************************************************
 * Camera HAL API handlers.
 *
 * Each handler simply verifies existence of an appropriate EmulatedBaseCamera
 * instance, and dispatches the call to that instance.
 *
 *****************************************************************************/

int EmulatedCameraFactory::cameraDeviceOpen(int cameraId,
                                            hw_device_t **device) {
    ALOGV("%s: id = %d", __FUNCTION__, cameraId);

    *device = nullptr;

    if (!isConstructedOK()) {
        ALOGE("%s: EmulatedCameraFactory has failed to initialize",
                __FUNCTION__);
        return -EINVAL;
    }

    if (cameraId < 0 || cameraId >= getEmulatedCameraNum()) {
        ALOGE("%s: Camera id %d is out of bounds (%d)",
                __FUNCTION__, cameraId, getEmulatedCameraNum());
        return -ENODEV;
    }

    return mEmulatedCameras[cameraId]->connectCamera(device);
}

int EmulatedCameraFactory::getCameraInfo(int cameraId,
                                         struct camera_info *info) {
    ALOGV("%s: id = %d", __FUNCTION__, cameraId);

    if (!isConstructedOK()) {
        ALOGE("%s: EmulatedCameraFactory has failed to initialize",
                __FUNCTION__);
        return -EINVAL;
    }

    if (cameraId < 0 || cameraId >= getEmulatedCameraNum()) {
        ALOGE("%s: Camera id %d is out of bounds (%d)",
                __FUNCTION__, cameraId, getEmulatedCameraNum());
        return -ENODEV;
    }

    return mEmulatedCameras[cameraId]->getCameraInfo(info);
}

int EmulatedCameraFactory::setCallbacks(
        const camera_module_callbacks_t *callbacks) {
    ALOGV("%s: callbacks = %p", __FUNCTION__, callbacks);

    mCallbacks = callbacks;

    return OK;
}

void EmulatedCameraFactory::getVendorTagOps(vendor_tag_ops_t* ops) {
    ALOGV("%s: ops = %p", __FUNCTION__, ops);
    // No vendor tags defined for emulator yet, so not touching ops.
}

/****************************************************************************
 * Camera HAL API callbacks.
 ***************************************************************************/

int EmulatedCameraFactory::device_open(const hw_module_t *module, const char
        *name, hw_device_t **device) {
    /*
     * Simply verify the parameters, and dispatch the call inside the
     * EmulatedCameraFactory instance.
     */

    if (module != &HAL_MODULE_INFO_SYM.common) {
        ALOGE("%s: Invalid module %p expected %p",
                __FUNCTION__, module, &HAL_MODULE_INFO_SYM.common);
        return -EINVAL;
    }
    if (name == nullptr) {
        ALOGE("%s: NULL name is not expected here", __FUNCTION__);
        return -EINVAL;
    }

    return gEmulatedCameraFactory.cameraDeviceOpen(atoi(name), device);
}

int EmulatedCameraFactory::get_number_of_cameras() {
    return gEmulatedCameraFactory.getEmulatedCameraNum();
}

int EmulatedCameraFactory::get_camera_info(int camera_id,
                        struct camera_info *info) {
    return gEmulatedCameraFactory.getCameraInfo(camera_id, info);
}

int EmulatedCameraFactory::set_callbacks(
        const camera_module_callbacks_t *callbacks) {
    return gEmulatedCameraFactory.setCallbacks(callbacks);
}

void EmulatedCameraFactory::get_vendor_tag_ops(vendor_tag_ops_t *ops) {
    gEmulatedCameraFactory.getVendorTagOps(ops);
}

int EmulatedCameraFactory::open_legacy(const struct hw_module_t *module,
        const char *id, uint32_t halVersion, struct hw_device_t **device) {
    // Not supporting legacy open.
    return -ENOSYS;
}

/********************************************************************************
 * Internal API
 *******************************************************************************/

/*
 * Camera information tokens passed in response to the "list" factory query.
 */

// Device name token.
static const char *kListNameToken = "name=";
// Frame dimensions token.
static const char *kListDimsToken = "framedims=";
// Facing direction token.
static const char *kListDirToken = "dir=";


bool EmulatedCameraFactory::getTokenValue(const char *token,
        const std::string &s, char **value) {
    // Find the start of the token.
    size_t tokenStart = s.find(token);
    if (tokenStart == std::string::npos) {
        return false;
    }

    // Advance to the beginning of the token value.
    size_t valueStart = tokenStart + strlen(token);

    // Find the length of the token value.
    size_t valueLength = s.find(' ', valueStart) - valueStart;

    // Extract the value substring.
    std::string valueStr = s.substr(valueStart, valueLength);

    // Convert to char*.
    *value = new char[valueStr.length() + 1];
    if (*value == nullptr) {
        return false;
    }
    strcpy(*value, valueStr.c_str());

    ALOGV("%s: Parsed value is \"%s\"", __FUNCTION__, *value);

    return true;
}

void EmulatedCameraFactory::findQemuCameras(
        std::vector<QemuCameraInfo> *qemuCameras) {
    // Obtain camera list.
    char *cameraList = nullptr;
    status_t res = mQemuClient.listCameras(&cameraList);

    /*
     * Empty list, or list containing just an EOL means that there were no
     * connected cameras found.
     */
    if (res != NO_ERROR || cameraList == nullptr || *cameraList == '\0' ||
        *cameraList == '\n') {
        if (cameraList != nullptr) {
            free(cameraList);
        }
        return;
    }

    /*
     * Calculate number of connected cameras. Number of EOLs in the camera list
     * is the number of the connected cameras.
     */

    std::string cameraListStr(cameraList);
    free(cameraList);

    size_t lineBegin = 0;
    size_t lineEnd = cameraListStr.find('\n');
    while (lineEnd != std::string::npos) {
        std::string cameraStr = cameraListStr.substr(lineBegin, lineEnd - lineBegin);
        // Parse the 'name', 'framedims', and 'dir' tokens.
        char *name, *frameDims, *dir;
        if (getTokenValue(kListNameToken, cameraStr, &name) &&
                getTokenValue(kListDimsToken, cameraStr, &frameDims) &&
                getTokenValue(kListDirToken, cameraStr, &dir)) {
            // Push the camera info if it was all successfully parsed.
            qemuCameras->push_back(QemuCameraInfo{
                .name = name,
                .frameDims = frameDims,
                .dir = dir,
            });
        } else {
            ALOGW("%s: Bad camera information: %s", __FUNCTION__,
                    cameraStr.c_str());
        }
        // Skip over the newline for the beginning of the next line.
        lineBegin = lineEnd + 1;
        lineEnd = cameraListStr.find('\n', lineBegin);
    }
}

std::unique_ptr<EmulatedBaseCamera>
EmulatedCameraFactory::createQemuCameraImpl(int halVersion,
                                            const QemuCameraInfo& camInfo,
                                            int cameraId,
                                            struct hw_module_t* module) {
    status_t res;

    switch (halVersion) {
    case 1: {
            auto camera = std::make_unique<EmulatedQemuCamera>(cameraId, module, mGBM);
            res = camera->Initialize(camInfo.name, camInfo.frameDims, camInfo.dir);
            if (res == NO_ERROR) {
                return camera;
            }
        }
        break;

    case 3: {
            auto camera = std::make_unique<EmulatedQemuCamera3>(cameraId, module, mGBM);
            res = camera->Initialize(camInfo.name, camInfo.frameDims, camInfo.dir);
            if (res == NO_ERROR) {
                return camera;
            }
        }
        break;

    default:
        ALOGE("%s: QEMU support for camera hal version %d is not "
              "implemented", __func__, halVersion);
        break;
    }

    return nullptr;
}

void EmulatedCameraFactory::createQemuCameras(
        const std::vector<QemuCameraInfo> &qemuCameras) {
    /*
     * Iterate the list, creating, and initializing emulated QEMU cameras for each
     * entry in the list.
     */

    /*
     * We use this index only for determining which direction the webcam should
     * face. Otherwise, mEmulatedCameraNum represents the camera ID and the
     * index into mEmulatedCameras.
     */
    int qemuIndex = 0;
    for (const auto &cameraInfo : qemuCameras) {
        /*
         * Here, we're assuming the first webcam is intended to be the back
         * camera and any other webcams are front cameras.
         */
        const bool isBackcamera = (qemuIndex == 0);
        const int halVersion = getCameraHalVersion(isBackcamera);

        std::unique_ptr<EmulatedBaseCamera> camera =
            createQemuCameraImpl(halVersion,
                                 cameraInfo,
                                 mEmulatedCameras.size(),
                                 &HAL_MODULE_INFO_SYM.common);
        if (camera) {
            mEmulatedCameras.push_back(std::move(camera));
        }

        qemuIndex++;
    }
}

std::unique_ptr<EmulatedBaseCamera>
EmulatedCameraFactory::createFakeCameraImpl(bool backCamera,
                                            int halVersion,
                                            int cameraId,
                                            struct hw_module_t* module) {
    switch (halVersion) {
    case 1:
        return std::make_unique<EmulatedFakeCamera>(cameraId, backCamera, module, mGBM);

    case 2:
        return std::make_unique<EmulatedFakeCamera2>(cameraId, backCamera, module, mGBM);

    case 3: {
            static const char key[] = "ro.kernel.qemu.camera.fake.rotating";
            char prop[PROPERTY_VALUE_MAX];

            if (property_get(key, prop, nullptr) > 0) {
                return std::make_unique<EmulatedFakeCamera>(cameraId, backCamera, module, mGBM);
            } else {
                return std::make_unique<EmulatedFakeCamera3>(cameraId, backCamera, module, mGBM);
            }
        }

    default:
        ALOGE("%s: Unknown %s camera hal version requested: %d",
              __func__, backCamera ? "back" : "front", halVersion);
        return nullptr;
    }
}

void EmulatedCameraFactory::createFakeCamera(bool backCamera) {
    const int halVersion = getCameraHalVersion(backCamera);

    std::unique_ptr<EmulatedBaseCamera> camera = createFakeCameraImpl(
        backCamera, halVersion, mEmulatedCameras.size(),
        &HAL_MODULE_INFO_SYM.common);

    status_t res = camera->Initialize();
    if (res == NO_ERROR) {
        mEmulatedCameras.push_back(std::move(camera));
    } else {
        ALOGE("%s: Unable to initialize %s camera %zu: %s (%d)",
              __func__, backCamera ? "back" : "front",
              mEmulatedCameras.size(), strerror(-res), res);
    }
}

void EmulatedCameraFactory::waitForQemuSfFakeCameraPropertyAvailable() {
    /*
     * Camera service may start running before qemu-props sets
     * qemu.sf.fake_camera to any of the follwing four values:
     * "none,front,back,both"; so we need to wait.
     *
     * android/camera/camera-service.c
     * bug: 30768229
     */
    int numAttempts = 100;
    char prop[PROPERTY_VALUE_MAX];
    bool timeout = true;
    for (int i = 0; i < numAttempts; ++i) {
        if (property_get("qemu.sf.fake_camera", prop, nullptr) != 0 ) {
            timeout = false;
            break;
        }
        usleep(5000);
    }
    if (timeout) {
        ALOGE("timeout (%dms) waiting for property qemu.sf.fake_camera to be set\n", 5 * numAttempts);
    }
}

bool EmulatedCameraFactory::isFakeCameraEmulationOn(bool backCamera) {
    // Always return false, because another HAL (Google Camera HAL)
    // will create the fake cameras
    if (!property_get_bool("ro.kernel.qemu.legacy_fake_camera", false)) {
        return false;
    }
    char prop[PROPERTY_VALUE_MAX];
    if ((property_get("qemu.sf.fake_camera", prop, nullptr) > 0) &&
        (!strcmp(prop, "both") ||
         !strcmp(prop, backCamera ? "back" : "front"))) {
        return true;
    } else {
        return false;
    }
}

int EmulatedCameraFactory::getCameraHalVersion(bool backCamera) {
    /*
     * Defined by 'qemu.sf.front_camera_hal_version' and
     * 'qemu.sf.back_camera_hal_version' boot properties. If the property
     * doesn't exist, it is assumed we are working with HAL v1.
     */
    char prop[PROPERTY_VALUE_MAX];
    const char *propQuery = backCamera ?
            "qemu.sf.back_camera_hal" :
            "qemu.sf.front_camera_hal";
    if (property_get(propQuery, prop, nullptr) > 0) {
        char *propEnd = prop;
        int val = strtol(prop, &propEnd, 10);
        if (*propEnd == '\0') {
            return val;
        }
        // Badly formatted property. It should just be a number.
        ALOGE("qemu.sf.back_camera_hal is not a number: %s", prop);
    }
    return 3;
}

void EmulatedCameraFactory::onStatusChanged(int cameraId, int newStatus) {

    EmulatedBaseCamera *cam = mEmulatedCameras[cameraId].get();
    if (!cam) {
        ALOGE("%s: Invalid camera ID %d", __FUNCTION__, cameraId);
        return;
    }

    /*
     * (Order is important)
     * Send the callback first to framework, THEN close the camera.
     */

    if (newStatus == cam->getHotplugStatus()) {
        ALOGW("%s: Ignoring transition to the same status", __FUNCTION__);
        return;
    }

    const camera_module_callbacks_t* cb = mCallbacks;
    if (cb != nullptr && cb->camera_device_status_change != nullptr) {
        cb->camera_device_status_change(cb, cameraId, newStatus);
    }

    if (newStatus == CAMERA_DEVICE_STATUS_NOT_PRESENT) {
        cam->unplugCamera();
    } else if (newStatus == CAMERA_DEVICE_STATUS_PRESENT) {
        cam->plugCamera();
    }
}

/********************************************************************************
 * Initializer for the static member structure.
 *******************************************************************************/

// Entry point for camera HAL API.
struct hw_module_methods_t EmulatedCameraFactory::mCameraModuleMethods = {
    .open = EmulatedCameraFactory::device_open
};

}; // end of namespace android
