/*
 * Copyright (C) 2013-2017 Intel Corporation
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
#define LOG_TAG "Camera3HALModule"

#include <dlfcn.h>
#include <stdlib.h>
#include <mutex>
#include <hardware/camera3.h>

#include "LogHelper.h"
#include "PlatformData.h"
#include "rkcamera_vendor_tags.h"
#include "PerformanceTraces.h"
#include <cutils/properties.h>

#include "Camera3HAL.h"
#include "FlashLight.h"


using namespace android;
USING_DECLARED_NAMESPACE;

/**
 * \macro VISIBILITY_PUBLIC
 *
 * Controls the visibility of symbols in the shared library.
 * In production builds all symbols in the shared library are hidden
 * except the ones using this linker attribute.
 */
#define VISIBILITY_PUBLIC __attribute__ ((visibility ("default")))

// Refer to file VERSION for version details.
// vA.B.C:
// A and B is updated by platform, and C is updated by product
#define CAM_HAL3_PROPERTY_KEY  "vendor.cam.hal3.ver"
static char rkHal3Version[PROPERTY_VALUE_MAX] = "v2.1.0";

static int hal_dev_close(hw_device_t* device);

/**********************************************************************
 * Camera Module API (C API)
 **********************************************************************/

static bool sInstances[MAX_CAMERAS] = {false, false};
static int sInstanceCount = 0;

static const camera_module_callbacks_t *sCallbacks;

/**
 * Global mutex used to protect sInstanceCount and sInstances
 */
static std::mutex sCameraHalMutex;

int openCameraHardware(int id, const hw_module_t* module, hw_device_t** device)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    if (sInstances[id])
        return 0;

    FlashLight& flash = FlashLight::getInstance();

    flash.init(id);
    flash.setCallbacks(sCallbacks);
    flash.reserveFlashForCamera(id);

    Camera3HAL* halDev = new Camera3HAL(id, module);

    if (halDev->init() != NO_ERROR) {
        LOGE("HAL initialization fail!");
        delete halDev;
        return -EINVAL;
    }
    camera3_device_t *cam3Device = halDev->getDeviceStruct();

    cam3Device->common.close = hal_dev_close;
    *device = &cam3Device->common;

    sInstanceCount++;
    sInstances[id] = true;

    return 0;
}

static int hal_get_number_of_cameras(void)
{
    rk_camera_debug_open();
    PerformanceTraces::reset();
    PerformanceTraces::HalAtrace::reset();

    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    PERFORMANCE_ATRACE_CALL();

    return PlatformData::numberOfCameras();
}

static void hal_get_vendor_tag_ops(vendor_tag_ops_t* ops)
{

    PERFORMANCE_ATRACE_CALL();

    RkCamera3VendorTags::get_vendor_tag_ops(ops);
}

static int hal_get_camera_info(int cameraId, struct camera_info *cameraInfo)
{
    PERFORMANCE_ATRACE_CALL();
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    if (cameraId < 0 || !cameraInfo ||
          cameraId >= hal_get_number_of_cameras())
        return -EINVAL;

    PlatformData::getCameraInfo(cameraId, cameraInfo);

    return 0;
}

static int hal_set_callbacks(const camera_module_callbacks_t *callbacks)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    sCallbacks = callbacks;

    return 0;
}

static int hal_dev_open(const hw_module_t* module, const char* name,
                        hw_device_t** device)
{
    int status = -EINVAL;
    int camera_id;

    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    PerformanceTraces::reset();
    PerformanceTraces::HalAtrace::reset();
    PERFORMANCE_ATRACE_CALL();

    rk_camera_debug_open();

    if (!name || !module || !device) {
        LOGE("Camera name is nullptr");
        return status;
    }

    LOGI("%s, camera id: %s", __FUNCTION__, name);
    camera_id = atoi(name);


    if (!PlatformData::isInitialized()) {
        PlatformData::init(); // try to init the PlatformData again.
        if (!PlatformData::isInitialized()) {
            LOGE("%s: open Camera id %d fails due to PlatformData init fails",
                __func__, camera_id);
            return -ENODEV;
        }
    }

    if (camera_id < 0 || camera_id >= hal_get_number_of_cameras()) {
        LOGE("%s: Camera id %d is out of bounds, num. of cameras (%d)",
             __func__, camera_id, hal_get_number_of_cameras());
        return -ENODEV;
    }

    std::lock_guard<std::mutex> l(sCameraHalMutex);

    if ((!PlatformData::supportDualVideo()) &&
         sInstanceCount > 0 && !sInstances[camera_id]) {
        LOGE("Don't support front/primary open at the same time");
        return -EUSERS;
    }

    return openCameraHardware(camera_id, module, device);
}

static int hal_dev_close(hw_device_t* device)
{
    PERFORMANCE_ATRACE_CALL();
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    if (!device || sInstanceCount == 0) {
        LOGW("hal close, instance count %d", sInstanceCount);
        return -EINVAL;
    }

    camera3_device_t *camera3_dev = (struct camera3_device *)device;
    Camera3HAL* camera_priv = (Camera3HAL*)(camera3_dev->priv);

    if (camera_priv != nullptr) {
        std::lock_guard<std::mutex> l(sCameraHalMutex);
        camera_priv->deinit();
        int id = camera_priv->getCameraId();
        delete camera_priv;
        sInstanceCount--;
        sInstances[id] = false;

        FlashLight& flash = FlashLight::getInstance();
        flash.releaseFlashFromCamera(id);
        flash.deinit(id);
    }

    LOGI("%s, instance count %d", __FUNCTION__, sInstanceCount);

    return 0;
}

#ifdef CAMERA_MODULE_API_VERSION_2_4
static int hal_set_torch_mode (const char* camera_id, bool enabled){
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    int retVal(0);
    long cameraIdLong(-1);
    int cameraIdInt(-1);
    char* endPointer = NULL;
    errno = 0;
    FlashLight& flash = FlashLight::getInstance();

    cameraIdLong = strtol(camera_id, &endPointer, 10);

    if ((errno == ERANGE) ||
            (cameraIdLong < 0) ||
            (cameraIdLong >= static_cast<long>(hal_get_number_of_cameras())) ||
            (endPointer == camera_id) ||
            (*endPointer != '\0')) {
        retVal = -ENOSYS;
    } else if (enabled) {
        cameraIdInt = static_cast<int>(cameraIdLong);
        retVal = flash.init(cameraIdInt);

        if (retVal == 0) {
            retVal = flash.setFlashMode(cameraIdInt, enabled);
            if ((retVal == 0) && (sCallbacks != NULL)) {
                sCallbacks->torch_mode_status_change(sCallbacks,
                        camera_id,
                        TORCH_MODE_STATUS_AVAILABLE_ON);
            } else if (retVal == -EALREADY) {
                // Flash is already on, so treat this as a success.
                retVal = 0;
            }
        }
    } else {
        cameraIdInt = static_cast<int>(cameraIdLong);
        retVal = flash.setFlashMode(cameraIdInt, enabled);

        if (retVal == 0) {
            retVal = flash.deinit(cameraIdInt);
            if ((retVal == 0) && (sCallbacks != NULL)) {
                sCallbacks->torch_mode_status_change(sCallbacks,
                        camera_id,
                        TORCH_MODE_STATUS_AVAILABLE_OFF);
            }
        } else if (retVal == -EALREADY) {
            // Flash is already off, so treat this as a success.
            retVal = 0;
        }
    }

    return retVal;
}
#endif

static struct hw_module_methods_t hal_module_methods = {
    NAMED_FIELD_INITIALIZER(open) hal_dev_open
};

camera_module_t VISIBILITY_PUBLIC HAL_MODULE_INFO_SYM = {
    NAMED_FIELD_INITIALIZER(common) {
        NAMED_FIELD_INITIALIZER(tag) HARDWARE_MODULE_TAG,
        NAMED_FIELD_INITIALIZER(module_api_version) CAMERA_MODULE_API_VERSION_2_4,
        NAMED_FIELD_INITIALIZER(hal_api_version) 0,
        NAMED_FIELD_INITIALIZER(id) CAMERA_HARDWARE_MODULE_ID,
        NAMED_FIELD_INITIALIZER(name) "Rockchip Camera3HAL Module",
        NAMED_FIELD_INITIALIZER(author) "Rockchip",
        NAMED_FIELD_INITIALIZER(methods) &hal_module_methods,
        NAMED_FIELD_INITIALIZER(dso) nullptr,
        NAMED_FIELD_INITIALIZER(reserved) {0},
    },
    NAMED_FIELD_INITIALIZER(get_number_of_cameras) hal_get_number_of_cameras,
    NAMED_FIELD_INITIALIZER(get_camera_info) hal_get_camera_info,
    NAMED_FIELD_INITIALIZER(set_callbacks) hal_set_callbacks,
    NAMED_FIELD_INITIALIZER(get_vendor_tag_ops) hal_get_vendor_tag_ops,
    NAMED_FIELD_INITIALIZER(open_legacy) nullptr,
#ifdef CAMERA_MODULE_API_VERSION_2_4
    NAMED_FIELD_INITIALIZER(set_torch_mode) hal_set_torch_mode,
    NAMED_FIELD_INITIALIZER(init) nullptr,
#endif
    NAMED_FIELD_INITIALIZER(reserved) {0}
};

// PSL-specific constructor values to start from 200
// to have enough reserved priorities to common HAL
static void initCameraHAL(void) __attribute__((constructor));
static void initCameraHAL(void) {
    rk_camera_debug_open();
    ALOGI("@%s: RockChip Camera Hal3 Release version %s ", __FUNCTION__, rkHal3Version);
    property_set(CAM_HAL3_PROPERTY_KEY,rkHal3Version);
    PerformanceTraces::reset();
    PlatformData::init();
    int ret = PlatformData::numberOfCameras();
    if (ret == 0) {
      LOGE("No camera device was found!");
      return;
    }
}

static void deinitCameraHAL(void) __attribute__((destructor));
static void deinitCameraHAL(void) {
    rk_camera_debug_close();
    PlatformData::deinit();
}
