/*
 * Copyright (C) 2016-2017 Intel Corporation.
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

#define LOG_TAG "RKISP2InputFrameWorker"

#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "RKISP2InputFrameWorker.h"
#include "NodeTypes.h"
#include <sys/mman.h>
#include "RKISP1CameraHw.h" // PartialResultEnum

namespace android {
namespace camera2 {
namespace rkisp2 {

RKISP2InputFrameWorker::RKISP2InputFrameWorker(int cameraId,
                camera3_stream_t* stream, std::vector<camera3_stream_t*>& outStreams,
                size_t pipelineDepth) :
                RKISP2IDeviceWorker(cameraId),
                mStream(stream),
                mOutputStreams(outStreams),
                mNeedPostProcess(false),
                mPipelineDepth(pipelineDepth),
                mPostPipeline(new RKISP2PostProcessPipeline(this, cameraId))
{
    mBufferReturned = 0;
    LOGI("@%s, instance(%p), mStream(%p)", __FUNCTION__, this, mStream);
}

RKISP2InputFrameWorker::~RKISP2InputFrameWorker()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    stopWorker();
    mPostPipeline.reset();
}

status_t
RKISP2InputFrameWorker::flushWorker()
{
    mMsg = nullptr;
    mPostPipeline->flush();
    mPostPipeline->stop();
    mProcessingInputBufs.clear();
    cleanListener();

    return OK;
}

status_t
RKISP2InputFrameWorker::stopWorker()
{
    return OK;
}

status_t
RKISP2InputFrameWorker::startWorker()
{
    return OK;
}

/******************************************************************************
 * In some burst reprocess case(CTS:testMandatoryReprocessConfigurations),
 * the bufferDone could disorder.
 * For example:  A request Loop: 'yuv->yuv' + 'yuv->jpeg' + 'yuv->yuv' + ...
 * in this case, the yuv->yuv request could be processed faster than yuv->jpeg
 * request, therefore cause the disorder of buffer done.
 * so, just store the buffer arrived ahead of time and rehandle it in a right
 * place
 *****************************************************************************/
status_t
RKISP2InputFrameWorker::bufferDone(std::shared_ptr<PostProcBuffer> buf) {
    Camera3Request* comingReq = buf->request;
    Camera3Request* processingReq = *mProcessingRequests.begin();

    if(!comingReq && !processingReq) {
        LOGE("@%s: Null request, comingReq:%p, processingReq:%p", __FUNCTION__, comingReq, processingReq);
        return UNKNOWN_ERROR;
    }

    ALOGD("@%s: coming request %d ,processing request %d", __FUNCTION__,
            comingReq->getId(), processingReq->getId());

    if(processingReq->getId() == comingReq->getId()) {
        CameraStream *stream = buf->cambuf->getOwner();
        stream->captureDone(buf->cambuf, comingReq);
        mBufferReturned++;
        LOGL("%s: mBufferReturned: %d/%d", __FUNCTION__, mBufferReturned, comingReq->getNumberOutputBufs());
        // when all output buffer returned, captureDone the input buffer
        if(mBufferReturned == comingReq->getNumberOutputBufs()) {
            mBufferReturned = 0;
            mProcessingRequests.erase(mProcessingRequests.begin());
            std::shared_ptr<CameraBuffer> camBuf = *mProcessingInputBufs.begin();
            if (camBuf.get()) {
                stream = camBuf->getOwner();
                stream->captureDone(camBuf, comingReq);
                mProcessingInputBufs.erase(mProcessingInputBufs.begin());
                LOGD("%s: reprocess request %d done, remaining %d requests", __FUNCTION__,
                    comingReq->getId(), mProcessingPostProcBufs.size());
            } else {
                LOGE("@%s: reprocess inputBuffer should not be NULL", __FUNCTION__);
            }
            //check if there are some disorder requests need to handle
            //this will happen in CTS:testMandatoryReprocessConfigurations
            //and will not occur in normal zsl capture case
            for (int i = 0; i < mProcessingPostProcBufs.size(); ++i) {
                processingReq = *mProcessingRequests.begin();
                std::shared_ptr<PostProcBuffer> buffer = mProcessingPostProcBufs.at(i);
                if(processingReq->getId() == buffer->request->getId()) {
                    LOGD("@%s %d: stored request %d bufferdone", __FUNCTION__, __LINE__, processingReq->getId());
                    mProcessingPostProcBufs.erase(mProcessingPostProcBufs.begin()+i);
                    bufferDone(buffer);
                }
            }
        }
    } else if(processingReq->getId() < comingReq->getId()) {
        LOGD("%s, request %d are processing, store the coming request %d", __FUNCTION__, processingReq->getId(), comingReq->getId());
        // store the buffer arrived ahead of time
        mProcessingPostProcBufs.push_back(buf);
    } else {
        LOGE("@%s: request %d are processing, coming request %d should be already done, is a BUG!!", __FUNCTION__,
            processingReq->getId(), comingReq->getId());
        return UNKNOWN_ERROR;
    }

    return OK;
}

status_t
RKISP2InputFrameWorker::notifyNewFrame(const std::shared_ptr<PostProcBuffer>& buf,
                                  const std::shared_ptr<RKISP2ProcUnitSettings>& settings,
                                  int err)
{
    ALOGE("RKISP2InputFrameWorker::notifyNewFrame");
    std::unique_lock<std::mutex> l(mBufDoneLock);
    status_t status = bufferDone(buf);

    return status;
}

status_t RKISP2InputFrameWorker::configure(bool configChanged)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    FrameInfo sourceFmt;
    sourceFmt.width = mStream->width;
    sourceFmt.height = mStream->height;
    /* TODO: not used now, set to 0 */
    sourceFmt.size = 0;
    sourceFmt.format = mStream->format;
    sourceFmt.stride = mStream->width;

    std::vector<camera3_stream_t*>::iterator iter =
        mOutputStreams.begin();
    for (; iter != mOutputStreams.end(); iter++)
        if ((*iter)->stream_type == CAMERA3_STREAM_BIDIRECTIONAL)
            break;
    if (iter != mOutputStreams.begin() && iter != mOutputStreams.end())
        mOutputStreams.erase(iter);

    mPostPipeline->prepare(sourceFmt, mOutputStreams, mNeedPostProcess);
    mPostPipeline->start();

    return OK;
}

status_t RKISP2InputFrameWorker::prepareRun(std::shared_ptr<DeviceMessage> msg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    mMsg = msg;
    status_t status = NO_ERROR;
    std::shared_ptr<CameraBuffer> inBuf;
    std::vector<std::shared_ptr<CameraBuffer>> outBufs;

    if (!mStream)
        return NO_ERROR;

    Camera3Request* request = mMsg->cbMetadataMsg.request;
    request->setSequenceId(-1);

    inBuf = findInputBuffer(request, mStream);
    outBufs = findOutputBuffers(request);
    if (inBuf.get() && outBufs.size() > 0) {
        // Work for mStream
        status = prepareBuffer(inBuf);
        if (status != NO_ERROR) {
            LOGE("prepare buffer error!");
            goto exit;
        }

        // If Input format is something else than
        // NV21 or Android flexible YCbCr 4:2:0, return
        if (inBuf->format() != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
            inBuf->format() != HAL_PIXEL_FORMAT_YCbCr_420_888 &&
            inBuf->format() != HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED &&
            inBuf->format() != HAL_PIXEL_FORMAT_BLOB)  {
            LOGE("Bad format %d", inBuf->format());
            status = BAD_TYPE;
            goto exit;
        }

        for (auto buf : outBufs) {
            status = prepareBuffer(buf);
            if (status != NO_ERROR) {
                LOGE("prepare buffer error!");
                goto exit;
            }

            // If Input format is something else than
            // NV21 or Android flexible YCbCr 4:2:0, return
            if (buf->format() != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
                buf->format() != HAL_PIXEL_FORMAT_YCbCr_420_888 &&
                buf->format() != HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED &&
                buf->format() != HAL_PIXEL_FORMAT_BLOB)  {
                LOGE("Bad format %d", buf->format());
                status = BAD_TYPE;
                goto exit;
            }
        }
        mProcessingInputBufs.push_back(inBuf);
    } else {
        LOGD("No work for this worker mStream: %p", mStream);
        return NO_ERROR;
    }

    LOGI("%s:%d:instance(%p), requestId(%d)", __FUNCTION__, __LINE__, this, request->getId());

exit:
    if (status < 0)
        returnBuffers();

    return status < 0 ? status : OK;
}

status_t RKISP2InputFrameWorker::run()
{
    status_t status = NO_ERROR;
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    struct timespec ts;

    // Update request sequence if needed
    Camera3Request* request = mMsg->cbMetadataMsg.request;
    const CameraMetadata* settings = request->getSettings();
    camera_metadata_ro_entry entry;

    entry = settings->find(ANDROID_SENSOR_TIMESTAMP);
    if (entry.count == 1) {
        ts.tv_sec = entry.data.i64[0] / 1000000000;
        ts.tv_nsec = entry.data.i64[0] % 1000000000;
    } else {
        LOGW("@%s %d: input buffer settings do not contain sensor timestamp", __FUNCTION__, __LINE__);
        clock_gettime(CLOCK_MONOTONIC, &ts);
    }

    ICaptureEventListener::CaptureMessage outMsg;
    outMsg.data.event.reqId = request->getId();
    outMsg.id = ICaptureEventListener::CAPTURE_MESSAGE_ID_EVENT;
    outMsg.data.event.type = ICaptureEventListener::CAPTURE_EVENT_SHUTTER;
    outMsg.data.event.timestamp.tv_sec = ts.tv_sec;
    outMsg.data.event.timestamp.tv_usec = ts.tv_nsec / 1000;
    outMsg.data.event.sequence = request->sequenceId();
    notifyListeners(&outMsg);

    LOGD("%s:%d:instance(%p), frame_id(%d), requestId(%d)", __FUNCTION__, __LINE__, this, request->sequenceId(), request->getId());

    return (status < 0) ? status : OK;
}

status_t RKISP2InputFrameWorker::postRun()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    status_t status = OK;
    CameraStream *stream;
    Camera3Request* request = nullptr;
    std::vector<std::shared_ptr<PostProcBuffer>> outBufs;
    std::shared_ptr<PostProcBuffer> postOutBuf;
    std::shared_ptr<PostProcBuffer> inBuf = std::make_shared<PostProcBuffer> ();
    std::vector<std::shared_ptr<CameraBuffer>> camBufs;
    std::shared_ptr<CameraBuffer> inCamBuf;
    std::unique_lock<std::mutex> l(mBufDoneLock);
    int stream_type;

    if (mMsg == nullptr) {
        LOGE("Message null - Fix the bug");
        status = UNKNOWN_ERROR;
        goto exit;
    }

    request = mMsg->cbMetadataMsg.request;
    if (request == nullptr) {
        LOGE("No request provided for captureDone");
        status = UNKNOWN_ERROR;
        goto exit;
    }
    mProcessingRequests.push_back(request);

    camBufs = findOutputBuffers(request);

    for(auto buf : camBufs) {
        postOutBuf = std::make_shared<PostProcBuffer> ();
        postOutBuf->cambuf = buf;
        postOutBuf->request = request;
        outBufs.push_back(postOutBuf);
        postOutBuf.reset();
    }

    inCamBuf = findInputBuffer(request, mStream);
    inBuf->cambuf = inCamBuf;
    inBuf->request = request;

    mPostPipeline->processFrame(inBuf, outBufs, mMsg->pMsg.processingSettings);

exit:
    /* Prevent from using old data */
    mMsg = nullptr;
    if (status != OK)
        returnBuffers();

    return status;
}

void RKISP2InputFrameWorker::returnBuffers()
{
    if (!mMsg || !mMsg->cbMetadataMsg.request)
        return;

    Camera3Request* request = mMsg->cbMetadataMsg.request;
    std::shared_ptr<CameraBuffer> buffer;

    buffer = findInputBuffer(request, mStream);
    if (buffer.get() && buffer->isRegistered())
        buffer->getOwner()->captureDone(buffer, request);
    std::vector<std::shared_ptr<CameraBuffer>> outBufs =
        findOutputBuffers(request);
    for(auto buf : outBufs) {
        if (buf.get() && buf->isRegistered())
            buf->getOwner()->captureDone(buf, request);
    }
}

status_t
RKISP2InputFrameWorker::prepareBuffer(std::shared_ptr<CameraBuffer>& buffer)
{
    CheckError((buffer.get() == nullptr), UNKNOWN_ERROR, "null buffer!");

    status_t status = NO_ERROR;
    if (!buffer->isLocked()) {
        status = buffer->lock();
        if (CC_UNLIKELY(status != NO_ERROR)) {
            LOGE("Could not lock the buffer error %d", status);
            return UNKNOWN_ERROR;
        }
    }
    status = buffer->waitOnAcquireFence();
    if (CC_UNLIKELY(status != NO_ERROR)) {
        LOGW("Wait on fence for buffer %p timed out", buffer.get());
    }
    return status;
}

std::shared_ptr<CameraBuffer>
RKISP2InputFrameWorker::findInputBuffer(Camera3Request* request,
                              camera3_stream_t* stream)
{
    CheckError((request == nullptr || stream == nullptr), nullptr,
                "null request/stream!");

    CameraStream *s = nullptr;
    std::shared_ptr<CameraBuffer> buffer = nullptr;
    const std::vector<camera3_stream_buffer>* inputBufs =
                                        request->getInputBuffers();

    for (camera3_stream_buffer InputBuffer : *inputBufs) {
        s = reinterpret_cast<CameraStream *>(InputBuffer.stream->priv);
        if (s->getStream() == stream) {
            buffer = request->findBuffer(s, false);
            if (CC_UNLIKELY(buffer == nullptr)) {
                LOGW("buffer not found for stream");
            }
            break;
        }
    }

    if (buffer.get() == nullptr) {
        LOGI("No buffer for stream %p in req %d", stream, request->getId());
    }
    return buffer;
}

std::vector<std::shared_ptr<CameraBuffer>>
RKISP2InputFrameWorker::findOutputBuffers(Camera3Request* request)
{
    CameraStream *s = nullptr;
    std::vector<std::shared_ptr<CameraBuffer>> buffers;
    std::shared_ptr<CameraBuffer> buf;
    const std::vector<camera3_stream_buffer>* outBufs =
        request->getOutputBuffers();

    for (camera3_stream_buffer OutputBuffer : *outBufs) {
        s = reinterpret_cast<CameraStream *>(OutputBuffer.stream->priv);
        buf = request->findBuffer(s, false);
        if (CC_UNLIKELY(buf == nullptr)) {
            LOGW("buffer not found for stream");
        }
        buffers.push_back(buf);
    }

    return buffers;
}

} /* namespace rkisp2 */
} /* namespace camera2 */
} /* namespace android */
