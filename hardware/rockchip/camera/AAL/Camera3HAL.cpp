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

#define LOG_TAG "Camera3HAL"

#include <mutex>
#include "Camera3HAL.h"
#include "ICameraHw.h"
#include "PerformanceTraces.h"

NAMESPACE_DECLARATION {
/******************************************************************************
 *  C DEVICE INTERFACE IMPLEMENTATION WRAPPER
 *****************************************************************************/

//Common check before the function call
#define FUNCTION_PREPARED_RETURN \
    if (!dev)\
        return -EINVAL;\
    Camera3HAL* camera_priv = (Camera3HAL*)(dev->priv);

static int
hal_dev_initialize(const struct camera3_device * dev,
                   const camera3_callback_ops_t *callback_ops)
{
    PERFORMANCE_ATRACE_CALL();
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    // As per interface requirement this call should not take longer than 10ms
    HAL_KPI_TRACE_CALL(1,10000000);
    FUNCTION_PREPARED_RETURN

    return camera_priv->initialize(callback_ops);
}

static int
hal_dev_configure_streams(const struct camera3_device * dev,
                          camera3_stream_configuration_t *stream_list)
{
    PERFORMANCE_ATRACE_CALL();
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    // As per interface requirement this call should not take longer than 1s
    HAL_KPI_TRACE_CALL(1,1000000000);
    FUNCTION_PREPARED_RETURN

    return camera_priv->configure_streams(stream_list);
}

static const camera_metadata_t*
hal_dev_construct_default_request_settings(const struct camera3_device * dev,
                                           int type)
{
    PERFORMANCE_ATRACE_CALL();
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    // As per interface requirement this call should not take longer than 5ms
    HAL_KPI_TRACE_CALL(1, 5000000);

    if (!dev)
        return nullptr;
    Camera3HAL* camera_priv = (Camera3HAL*)(dev->priv);

    return camera_priv->construct_default_request_settings(type);
}

static int
hal_dev_process_capture_request(const struct camera3_device * dev,
                                camera3_capture_request_t *request)
{
    PERFORMANCE_ATRACE_CALL();
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    /**
     *  As per interface requirement this call should not take longer than 4
     *  frame intervals. Here we choose that value to be 4 frame intervals at
     *  30fps = 4 x 33.3 ms = 133 ms
     */
    HAL_KPI_TRACE_CALL(2, 133000000);
    FUNCTION_PREPARED_RETURN

    return camera_priv->process_capture_request(request);
}

static void
hal_dev_dump(const struct camera3_device * dev, int fd)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    // As per interface requirement this call should not take longer than 10ms
    HAL_KPI_TRACE_CALL(1, 10000000);
    if (!dev)
        return;

    Camera3HAL* camera_priv = (Camera3HAL*)(dev->priv);

    camera_priv->dump(fd);
}

static int
hal_dev_flush(const struct camera3_device * dev)
{
    PERFORMANCE_ATRACE_CALL();
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    // As per interface requirement this call should not take longer than 1000ms
    HAL_KPI_TRACE_CALL(1, 1000000000);
    if (!dev)
        return -EINVAL;

    Camera3HAL* camera_priv = (Camera3HAL*)(dev->priv);
    return camera_priv->flush();
}

static camera3_device_ops hal_dev_ops = {
    NAMED_FIELD_INITIALIZER(initialize)                         hal_dev_initialize,
    NAMED_FIELD_INITIALIZER(configure_streams)                  hal_dev_configure_streams,
    NAMED_FIELD_INITIALIZER(register_stream_buffers)            nullptr,
    NAMED_FIELD_INITIALIZER(construct_default_request_settings) hal_dev_construct_default_request_settings,
    NAMED_FIELD_INITIALIZER(process_capture_request)            hal_dev_process_capture_request,
    NAMED_FIELD_INITIALIZER(get_metadata_vendor_tag_ops)        nullptr,
    NAMED_FIELD_INITIALIZER(dump)                               hal_dev_dump,
    NAMED_FIELD_INITIALIZER(flush)                              hal_dev_flush,
    NAMED_FIELD_INITIALIZER(reserved)                           {0},
};

/******************************************************************************
 *  C++ CLASS IMPLEMENTATION
 *****************************************************************************/
Camera3HAL::Camera3HAL(int cameraId, const hw_module_t* module) :
    mCameraId(cameraId),
    mCameraHw(nullptr),
    mRequestThread(nullptr)
{
    LOGI("@%s", __FUNCTION__);

    struct camera_info info;
    PlatformData::getCameraInfo(cameraId, &info);

    CLEAR(mDevice);
    mDevice.common.tag = HARDWARE_DEVICE_TAG;
    mDevice.common.version = info.device_version;
    mDevice.common.module = (hw_module_t *)(module);
    // hal_dev_close is kept in the module for symmetry with dev_open
    // it will be set there
    mDevice.common.close = nullptr;
    mDevice.ops = &hal_dev_ops;
    mDevice.priv = this;
}

Camera3HAL::~Camera3HAL()
{
    mDevice.priv = nullptr;
}

status_t Camera3HAL::init(void)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = UNKNOWN_ERROR;

    mCameraHw = ICameraHw::createCameraHW(mCameraId);

    status = mCameraHw->init();
    if (status != NO_ERROR) {
        LOGE("Error initializing Camera HW");
        goto bail;
    }

    mRequestThread = std::unique_ptr<RequestThread>(new RequestThread(mCameraId, mCameraHw));

    return NO_ERROR;

bail:
    deinit();
    return status;
}

status_t Camera3HAL::deinit(void)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    status_t status = NO_ERROR;

    // flush request first
    if (mRequestThread != nullptr)
        status = mRequestThread->flush();

    if (mCameraHw != nullptr) {
        delete mCameraHw;
        mCameraHw = nullptr;
    }

    if (mRequestThread != nullptr) {
        status |= mRequestThread->deinit();
        mRequestThread.reset();
    }

    return status;
}

/* *********************************************************************
 * Camera3 device  APIs
 * ********************************************************************/
int Camera3HAL::initialize(const camera3_callback_ops_t *callback_ops)
{
    status_t status = NO_ERROR;
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    if (callback_ops == nullptr)
        return -ENODEV;

    status = mRequestThread->init(callback_ops);
    if (status != NO_ERROR) {
        LOGE("Error initializing Request Thread status = %d", status);
        return -ENODEV;
    }
    return status;
}

int Camera3HAL::configure_streams(camera3_stream_configuration_t *stream_list)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    if (!stream_list)
        return -EINVAL;

    ALOGD("%s: streams list ptr: %p, num %d", __FUNCTION__,
        stream_list->streams, stream_list->num_streams);

    if (!stream_list->streams || !stream_list->num_streams) {
        LOGE("%s: Bad input! streams list ptr: %p, num %d", __FUNCTION__,
            stream_list->streams, stream_list->num_streams);
        return -EINVAL;
    }
    int num = stream_list->num_streams;
    while (num--) {
        if (!stream_list->streams[num]){
            LOGE("%s: Bad input! streams (%d) 's ptr: %p", __FUNCTION__,
                num, stream_list->streams[num]);
            return -EINVAL;
        }
    }

    status_t status = mRequestThread->configureStreams(stream_list);
    return (status == NO_ERROR) ? 0 : -EINVAL;
}

void Camera3HAL::dumpTemplateMeta(CameraMetadata& metadata, int type) {
    LOGD("%s:%d: enter", __func__, __LINE__);
    std::string fileName(gDumpPath);
    if (CC_UNLIKELY(LogHelper::isDumpTypeEnable(CAMERA_DUMP_META))) {
        const char intent_val[7][20] = {"CUSTOM", "PREVIEW", "STILL_CAPTURE", "VIDEO_RECORD", "VIDEO_SNAPSHOT", "ZERO_SHUTTER_LAG", "MANUAL"};
        std::string strIntentName;
        if(type < 7)
            strIntentName = intent_val[type];

        fileName += "dumpmeta_" + std::to_string(mCameraId) + "_TEMPLATE_" + strIntentName;
        LOGI("%s filename is %s", __FUNCTION__, fileName.data());
        int fd = open(fileName.data(), O_RDWR | O_CREAT, 0666);
        if (fd != -1) {
            metadata.dump(fd, 2);
        } else {
            LOGE("dumpTemplate: open failed, errmsg: %s\n", strerror(errno));
        }
        close(fd);
    }
}

camera_metadata_t* Camera3HAL::construct_default_request_settings(int type)
{
    LOGI("@%s, type:%d", __FUNCTION__, type);
    camera_metadata_t * meta;

    if (type < CAMERA3_TEMPLATE_PREVIEW || type >= CAMERA3_TEMPLATE_COUNT)
        return nullptr;

    status_t status = mRequestThread->constructDefaultRequest(type, &meta);
    if (status != NO_ERROR)
        return nullptr;

    CameraMetadata metadata;
    metadata = (const camera_metadata_t *)meta;
    dumpTemplateMeta(metadata, type);

    return meta;
}

int Camera3HAL::process_capture_request(camera3_capture_request_t *request)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    if (request == nullptr) {
        LOGE("%s: request is null!", __FUNCTION__);
        return -EINVAL;
    } else if (!request->num_output_buffers || request->output_buffers == nullptr) {
        LOGE("%s: num_output_buffers %d, output_buffers %p", __FUNCTION__,
              request->num_output_buffers, request->output_buffers);
        return -EINVAL;
    } else if (request->output_buffers->stream == nullptr) {
        LOGE("%s: output_buffers->stream is null!", __FUNCTION__);
        return -EINVAL;
    } else if (request->output_buffers->stream->priv == nullptr) {
        LOGE("%s: output_buffers->stream->priv is null!", __FUNCTION__);
        return -EINVAL;
    } else if (request->output_buffers->buffer == nullptr
          || *(request->output_buffers->buffer) == nullptr) {
        LOGE("%s: output buffer is invalid", __FUNCTION__);
        return -EINVAL;
    }

    status_t status = mRequestThread->processCaptureRequest(request);
    if (status == NO_ERROR)
        return NO_ERROR;

    return (status == BAD_VALUE) ? -EINVAL : -ENODEV;
}

void Camera3HAL::dump(int fd)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    if (mRequestThread != nullptr)
        mRequestThread->dump(fd);
    if (mCameraHw != nullptr)
        mCameraHw->dump(fd);
}

int Camera3HAL::flush()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    if (CC_LIKELY(mRequestThread != nullptr))
        return mRequestThread->flush();
    return -ENODEV;
}

//----------------------------------------------------------------------------
} NAMESPACE_DECLARATION_END
