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

#define LOG_TAG "ProcUnit_Task"

#include "RKISP2ExecuteTaskBase.h"
#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "CameraMetadataHelper.h"
#include "RKISP2ProcUnitSettings.h"
#include "Camera3GFXFormat.h"

namespace android {
namespace camera2 {
namespace rkisp2 {

RKISP2ExecuteTaskBase::RKISP2ExecuteTaskBase(const char *name, int priority):
    TaskThreadBase(name),
    mMessageQueue(mName.c_str(), static_cast<int>(MESSAGE_ID_MAX))
{
    UNUSED(name);
    UNUSED(priority);
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
}

RKISP2ExecuteTaskBase::~RKISP2ExecuteTaskBase()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    deInit();
}

status_t RKISP2ExecuteTaskBase::init()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    return initMessageThread();
}

status_t RKISP2ExecuteTaskBase::executeTask(RKISP2ProcTaskMsg &/*msg*/)
{
    // no-op for base class
    return NO_ERROR;
}

status_t RKISP2ExecuteTaskBase::handleMessageIterationDone(Message &msg)
{
    // no-op for base class
    UNUSED(msg);
    return NO_ERROR;
}

void
RKISP2ExecuteTaskBase::messageThreadLoop()
{
    mThreadRunning = true;
    while (mThreadRunning) {
        status_t status = NO_ERROR;

        Message msg;
        mMessageQueue.receive(&msg);

        PERFORMANCE_HAL_ATRACE_PARAM1("msg", msg.id);
        switch (msg.id) {
        case MESSAGE_ID_EXIT:
            mThreadRunning = false;
            status = NO_ERROR;
            break;
        case MESSAGE_ID_EXECUTE_TASK:
            status = handleExecuteTask(msg);
            break;
        case MESSAGE_ID_ITERATION_DONE:
            status = handleMessageIterationDone(msg);
            break;
        default:
            LOGE("ERROR @%s: Unknow message %d", __FUNCTION__, msg.id);
            status = BAD_VALUE;
            break;
        }
        if (status != NO_ERROR)
            LOGE("error %d in handling message: %d",
                 status, static_cast<int>(msg.id));
        mMessageQueue.reply(msg.id, status);
    }
}

status_t
RKISP2ExecuteTaskBase::requestExitAndWait()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    status_t status = mMessageQueue.send(&msg, MESSAGE_ID_EXIT);
    if (mMessageThread != nullptr) {
        status |= mMessageThread->requestExitAndWait();
    }
    return status;
}

/**
 * analyzeIntent
 * Analyze the intent of the request. Currently being used to determine which
 * pipeline to use to get the YUV data for the JPEG encoding
 */
uint8_t RKISP2ExecuteTaskBase::analyzeIntent(RKISP2ProcTaskMsg &pMsg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    uint8_t intent, requestType;
    const CameraMetadata* settings = pMsg.processingSettings->request->getSettings();
    if (settings == nullptr) {
        LOGE("no settings in request - BUG");
        return ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW; // just a guess
    }

    requestType = ANDROID_REQUEST_TYPE_CAPTURE;
    intent = ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM;
    MetadataHelper::getMetadataValue(*settings, ANDROID_REQUEST_TYPE, requestType);
    MetadataHelper::getMetadataValue(*settings, ANDROID_CONTROL_CAPTURE_INTENT, intent);

    if (requestType == ANDROID_REQUEST_TYPE_REPROCESS) {
        LOGI("%s: Request type: ANDROID_REQUEST_TYPE_REPROCESS", __FUNCTION__);
    }

    switch (intent) {
    case ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW:
        LOGI("%s: Request intent: ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW", __FUNCTION__);
        break;
    case ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE:
        LOGI("%s: Request intent: ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE", __FUNCTION__);
        break;
    case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD:
        LOGI("%s: Request intent: ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD", __FUNCTION__);
         break;
    case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT:
        LOGI("%s: Request intent: ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT", __FUNCTION__);
        break;
    case ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG:
        LOGI("%s: Request intent: ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG", __FUNCTION__);
        break;
    case ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM:
        LOGI("%s: Request intent: ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM", __FUNCTION__);
        break;
    case ANDROID_CONTROL_CAPTURE_INTENT_MANUAL:
        LOGI("%s: Request intent: ANDROID_CONTROL_CAPTURE_INTENT_MANUAL", __FUNCTION__);
        break;
    default:
        LOGE("Analyzing intent: not implement for %d yet!", intent);
        break;
    }
    return intent;
}

/**
 * Allocate an intermediate buffer and store it in a map for
 * future use. The key of the map is the stream pointer.
 *
 * The map is the member variable mInterBufMap
 *
 * This allows us to detect when we need to use intermediate buffers towards
 * the ISP pipeline. In cases as JPEG encoding or other post processing after
 * the ISP has produced the buffer.
 *
 * \param[in] isFallback
 * \param[in] stream Pointer to the camera stream that we are serving.
 * \param[in] h height of the stream
 * \param[in] w width of the stream
 * \param[in] cameraId
 * \param[out] interBufMap map of intermediate buffers that is populated
 * \return OK
 * \return NO_MEMORY if it failed to allocate
 * \return BAD_VALUE if input parameter is nullptr
 */

status_t RKISP2ExecuteTaskBase::allocateInterBuffer(bool isFallback,
        camera3_stream_t *stream,
        int w,
        int h,
        int cameraId,
        std::map<camera3_stream_t *, std::shared_ptr<CameraBuffer>> &interBufMap)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    std::shared_ptr<CameraBuffer> intermediateBuffer;

    if (CC_UNLIKELY(stream == nullptr)) {
        LOGE("Client stream is nullptr, cannot allocate intermediate buffer");
        return BAD_VALUE;
    }
    if (!isFallback && (stream->format == HAL_PIXEL_FORMAT_BLOB)) {
        intermediateBuffer = MemoryUtils::allocateHeapBuffer(w,
                                                             h,
                                                             ALIGN64(w),
                                                             V4L2_PIX_FMT_NV12,
                                                             cameraId,
                                                             0);
    } else {
        LOGE("@%s, fail", __FUNCTION__);
        return NO_MEMORY;
    }
    if (intermediateBuffer == nullptr) {
        LOGE("Failed to allocate intermediate buffer for stream %p", stream);
        return NO_MEMORY;
    }

    interBufMap[stream] = intermediateBuffer;
    return OK;
}

/**
 * Allocate intermediate buffer if needed.
 *
 * We need to always set intermediate buffer in fallback case and everytime we
 * have a blob stream.
 *
 * \param[in] isFallback
 * \param[in] it pipeline connection
 * \param[in] cameraId
 * \param[out] interBufMap map of intermediate buffers that is populated
 * \return OK if success
 */
status_t RKISP2ExecuteTaskBase::setIntermediateBuffer(
        bool isFallback,
        std::vector<RKISP2GraphConfig::PSysPipelineConnection>::iterator it,
        int cameraId,
        std::map<camera3_stream_t *, std::shared_ptr<CameraBuffer>> &interBufMap)
{
    status_t status = OK;

    /* If graph config is using fallback settings, we need to
     * use intermediate for all buffers. Take NV12 only streams. For blobs we
     * need to use width and height from the request, because we are not
     * allowed to upscale.
     */
    const string nv12format = "21VN";
    string format = v4l2Fmt2Str(it->portFormatSettings.fourcc);
    if (isFallback && ((format == nv12format)
            || (it->stream->format == HAL_PIXEL_FORMAT_BLOB))) {
        status = allocateInterBuffer(isFallback, it->stream,
                it->portFormatSettings.width,
                it->portFormatSettings.height,
                cameraId,
                interBufMap);
    } else if (it->stream->format == HAL_PIXEL_FORMAT_BLOB) {
        status = allocateInterBuffer(isFallback, it->stream,
                                     it->stream->width,
                                     it->stream->height,
                                     cameraId,
                                     interBufMap);
    }
    return status;
}

/**
 * Check the gralloc hint flags and decide whether this stream should be served
 * by Video Pipe or Still Pipe
 */
bool RKISP2ExecuteTaskBase::isVideoStream(CameraStream *stream)
{
    bool display = false;
    bool videoEnc = false;
    display = CHECK_FLAG(stream->usage(), GRALLOC_USAGE_HW_COMPOSER);
    display |= CHECK_FLAG(stream->usage(), GRALLOC_USAGE_HW_TEXTURE);
    display |= CHECK_FLAG(stream->usage(), GRALLOC_USAGE_HW_RENDER);

    videoEnc = CHECK_FLAG(stream->usage(), GRALLOC_USAGE_HW_VIDEO_ENCODER);

    return (display || videoEnc);
}

}  // namespace rkisp2
}  // namespace camera2
}  // namespace android
