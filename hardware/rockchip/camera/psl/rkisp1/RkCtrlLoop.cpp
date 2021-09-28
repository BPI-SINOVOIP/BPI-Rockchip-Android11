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

#define LOG_TAG "RkCtrlLoop"

#include <utils/Errors.h>
#include <math.h>
#include <sys/stat.h>

#include "PlatformData.h"
#include "CameraMetadataHelper.h"
#include "LogHelper.h"
#include "Utils.h"
#include "RkCtrlLoop.h"
#include "PerformanceTraces.h"

NAMESPACE_DECLARATION {
#if defined(ANDROID_VERSION_ABOVE_8_X)
#define RK_3A_TUNING_FILE_PATH  "/vendor/etc/camera/rkisp1/"
#else
#define RK_3A_TUNING_FILE_PATH  "/etc/camera/rkisp1/"
#endif

RkCtrlLoop::RkCtrlLoop(int camId):
        mCameraId(camId),
        mIsStarted(false)
{
    LOGI("@%s", __FUNCTION__);
}


status_t RkCtrlLoop::init(const char* sensorName,
                          const cl_result_callback_ops_t *cb)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_INFO);
    PERFORMANCE_ATRACE_NAME("RkCtrlLoop::init");
    status_t status = OK;
    /* get AIQ xml path */
    const CameraCapInfo* cap = PlatformData::getCameraCapInfo(mCameraId);
    std::string iq_file = cap->getIqTuningFile();
    std::string iq_file_path(RK_3A_TUNING_FILE_PATH);
    std::string iq_file_full_path = iq_file_path + iq_file;
#if 0
    struct stat fileInfo;

    CLEAR(fileInfo);
    if (stat(iq_file_full_path .c_str(), &fileInfo) < 0) {
        if (errno == ENOENT) {
            LOGI("sensor tuning file missing: \"%s\"!", sensorName);
            return NAME_NOT_FOUND;
        } else {
            LOGE("ERROR querying sensor tuning filestat for \"%s\": %s!",
                 iq_file_full_path.c_str(), strerror(errno));
            return UNKNOWN_ERROR;
        }
    }
#endif
    bool ret = (rkisp_cl_init(&mControlLoopCtx , iq_file_full_path.c_str(), cb) == 0 ? true : false);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, Error in isp control loop init", __FUNCTION__);
    return status;
}

void RkCtrlLoop::deinit()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_INFO);
    PERFORMANCE_ATRACE_NAME("RkCtrlLoop::deinit");

    rkisp_cl_deinit(mControlLoopCtx);
    mControlLoopCtx = NULL;
}

status_t RkCtrlLoop::start(const struct rkisp_cl_prepare_params_s& params)
{
    if (mIsStarted == true)
        return OK;

    PERFORMANCE_ATRACE_NAME("RkCtrlLoop::start");
    HAL_TRACE_CALL(CAM_GLBL_DBG_INFO);
    int ret = 0;

    LOGI("@%s %d: isp:%s, param:%s, stat:%s, sensor:%s", __FUNCTION__, __LINE__,
         params.isp_sd_node_path, params.isp_vd_params_path, params.isp_vd_stats_path, params.sensor_sd_node_path);

    ret = rkisp_cl_prepare(mControlLoopCtx, &params);
    if (ret < 0) {
        LOGE("%s: rkisp control loop prepare failed !", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    ret = rkisp_cl_start(mControlLoopCtx);
    if (ret < 0) {
        LOGE("%s: rkisp control loop start failed !", __FUNCTION__);
        return UNKNOWN_ERROR;
    }
    mIsStarted = true;
    return NO_ERROR;
}

status_t RkCtrlLoop::setFrameParams(rkisp_cl_frame_metadata_s* frame_params)
{
    int ret = 0;

    ret = rkisp_cl_set_frame_params(mControlLoopCtx, frame_params);
    if (ret < 0) {
        LOGE("%s: rkisp control loop set frame params failed !", __FUNCTION__);
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;

}

status_t RkCtrlLoop::stop()
{
    if (mIsStarted == false)
        return OK;

    int ret = 0;
    HAL_TRACE_CALL(CAM_GLBL_DBG_INFO);
    PERFORMANCE_ATRACE_NAME("RkCtrlLoop::stop");

    ret = rkisp_cl_stop(mControlLoopCtx);
    if (ret < 0) {
        LOGE("%s: rkisp control loop stop failed !", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    mIsStarted = false;
    return NO_ERROR;
 }

} NAMESPACE_DECLARATION_END

