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

#define LOG_TAG "ResultProcessor"

#include "ResultProcessor.h"
#include "Camera3Request.h"
#include "RequestThread.h"
#include "PlatformData.h"
#include "PerformanceTraces.h"

NAMESPACE_DECLARATION {

ResultProcessor::ResultProcessor(RequestThread * aReqThread,
                                 const camera3_callback_ops_t * cbOps) :
    mRequestThread(aReqThread),
    mMessageQueue("ResultProcessor", MESSAGE_ID_MAX),
    mMessageThread(new MessageThread(this,"ResultProcessor")),
    mCallbackOps(cbOps),
    mThreadRunning(true),
    mPartialResultCount(0),
    mNextRequestId(0)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    mReqStatePool.init(MAX_REQUEST_IN_TRANSIT);
    mMessageThread->run();
}

ResultProcessor::~ResultProcessor()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    if (mMessageThread != nullptr) {
        mMessageThread.reset();
        mMessageThread = nullptr;
    }
    mRequestsPendingMetaReturn.clear();
    mRequestsInTransit.clear();
}

/**********************************************************************
 * Public methods
 */
/**********************************************************************
 * Thread methods
 */
status_t ResultProcessor::requestExitAndWait(void)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    status_t status = mMessageQueue.send(&msg, MESSAGE_ID_EXIT);
    status |= mMessageThread->requestExitAndWait();
    return status;
}
status_t ResultProcessor::handleMessageExit(void)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    while ((mRequestsInTransit.size()) != 0) {
        recycleRequest((mRequestsInTransit.begin()->second)->request);
    }
    mThreadRunning = false;
    mMessageQueue.reply(MESSAGE_ID_EXIT, OK);
    return NO_ERROR;
}

/**
 * registerRequest
 *
 * Present a request to the ResultProcessor.
 * This call is used to inform the result processor that a new request
 * has been sent to the PSL. RequestThread uses this method
 * ResultProcessor will store its state in an internal vector to track
 * the different events during the lifetime of the request.
 *
 * Once the request has been completed ResultProcessor returns the request
 * to the RequestThread for recycling, using the method:
 * RequestThread::returnRequest();
 *
 * \param request [IN] item to register
 * \return NO_ERROR
 */
status_t ResultProcessor::registerRequest(Camera3Request *request)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    Message msg;
    msg.id = MESSAGE_ID_REGISTER_REQUEST;
    msg.request = request;
    return mMessageQueue.send(&msg, MESSAGE_ID_REGISTER_REQUEST);
}

status_t ResultProcessor::handleRegisterRequest(Message &msg)
{
    status_t status = NO_ERROR;
    RequestState_t* reqState;
    int reqId = msg.request->getId();
    /**
     * check if the request was not already register. we may receive registration
     * request duplicated in case of request that are held by the PSL
     */
    if (getRequestsInTransit(&reqState, reqId) == NO_ERROR) {
        return NO_ERROR;
    }

    status = mReqStatePool.acquireItem(&reqState);
    if (status != NO_ERROR) {
        LOGE("Could not acquire an empty reqState from the pool");
        return status;
    }

    reqState->init(msg.request);
    mRequestsInTransit.insert(RequestsInTransitPair(reqState->reqId, reqState));
    LOGD("<request %d> camera id %d registered @ ResultProcessor", reqState->reqId, msg.request->getCameraId());
    /**
     * get the number of partial results the request may return, this is not
     *  going to change once the camera is open, so do it only once.
     *  We initialize the value to 0, the minimum value should be 1
     */
    if (CC_UNLIKELY(mPartialResultCount == 0)) {
        mPartialResultCount = msg.request->getpartialResultCount();
    }
    return status;
}

void ResultProcessor::messageThreadLoop(void)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    mThreadRunning = true;
    while (mThreadRunning) {
        status_t status = NO_ERROR;
        Message msg;
        mMessageQueue.receive(&msg);
        PERFORMANCE_HAL_ATRACE_PARAM1("msg", msg.id);
        switch (msg.id) {
        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;
        case MESSAGE_ID_SHUTTER_DONE:
            status = handleShutterDone(msg);
            break;
        case MESSAGE_ID_METADATA_DONE:
            status = handleMetadataDone(msg);
            break;
        case MESSAGE_ID_BUFFER_DONE:
            status = handleBufferDone(msg);
            break;
        case MESSAGE_ID_REGISTER_REQUEST:
            status = handleRegisterRequest(msg);
            break;
        case MESSAGE_ID_DEVICE_ERROR:
            handleDeviceError();
            break;
        default:
           LOGE("Wrong message id %d", msg.id);
           status = BAD_VALUE;
           break;
        }
        mMessageQueue.reply(msg.id, status);
    }
}

status_t ResultProcessor::shutterDone(Camera3Request* request,
                                      int64_t timestamp)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    Message msg;
    msg.id = MESSAGE_ID_SHUTTER_DONE;
    msg.request = request;
    msg.data.shutter.time = timestamp;

    return mMessageQueue.send(&msg);
}

status_t ResultProcessor::handleShutterDone(Message &msg)
{
    status_t status = NO_ERROR;
    int reqId = 0;
    Camera3Request* request = msg.request;

    reqId = request->getId();
    LOGD("%s for <Request : %d", __FUNCTION__, reqId);
    PERFORMANCE_HAL_ATRACE_PARAM1("reqId", reqId);
    PERFORMANCE_ATRACE_NAME_SNPRINTF("handleShutterDone - %d", reqId);
    PERFORMANCE_ATRACE_ASYNC_BEGIN("Shutter2Alldone", reqId);

    RequestState_t *reqState = nullptr;
    if (getRequestsInTransit(&reqState, reqId) == BAD_VALUE) {
        LOGE("Request %d was not registered find the bug", reqId);
        return BAD_VALUE;
    }

    reqState->shutterTime = msg.data.shutter.time;
    if (mNextRequestId != reqId) {
        LOGW("shutter done received ahead of time, expecting %d got %d Or discontinuities requests received.",
                mNextRequestId, reqId);
        reqState->shutterReceived = true;
    }

    returnShutterDone(reqState);

    if (!reqState->pendingBuffers.empty()) {
        returnPendingBuffers(reqState);
    }

    unsigned int resultsReceived = reqState->pendingPartialResults.size();
    bool allMetaReceived = (resultsReceived == mPartialResultCount);

    if (allMetaReceived) {
        returnPendingPartials(reqState);
    }

    bool allMetaDone = (reqState->partialResultReturned == mPartialResultCount);
    bool allBuffersDone = (reqState->buffersReturned == reqState->buffersToReturn);
    if (allBuffersDone && allMetaDone) {
        status = recycleRequest(request);
    }

    return status;
}

/**
 * returnShutterDone
 * Signal to the client that shutter event was received
 * \param reqState [IN/OUT] state of the request
 */
void ResultProcessor::returnShutterDone(RequestState_t* reqState)
{
    if (reqState->isShutterDone)
        return;

    camera3_notify_msg shutter;
    shutter.type = CAMERA3_MSG_SHUTTER;
    shutter.message.shutter.frame_number = reqState->reqId;
    shutter.message.shutter.timestamp =reqState->shutterTime;
    mCallbackOps->notify(mCallbackOps, &shutter);
    reqState->isShutterDone = true;
    mNextRequestId = reqState->nextReqId;
    LOGD("<Request %d> camera id %d shutter done", reqState->reqId, reqState->request->getCameraId());
}

status_t ResultProcessor::metadataDone(Camera3Request* request,
                                       int resultIndex)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    Message msg;
    msg.id = MESSAGE_ID_METADATA_DONE;
    msg.request = request;
    msg.data.meta.resultIndex = resultIndex;

    return mMessageQueue.send(&msg);
}

status_t ResultProcessor::handleMetadataDone(Message &msg)
{
    status_t status = NO_ERROR;
    Camera3Request* request = msg.request;
    int reqId = request->getId();
    LOGD("%s for <Request %d>", __FUNCTION__, reqId);
    PERFORMANCE_HAL_ATRACE_PARAM1("reqId", reqId);
    PERFORMANCE_ATRACE_NAME_SNPRINTF("handleMetadataDone - %d", reqId);

    RequestState_t *reqState = nullptr;
    if (getRequestsInTransit(&reqState, reqId) == BAD_VALUE) {
        LOGE("Request %d was not registered:find the bug", reqId);
        return BAD_VALUE;
    }

    if (!reqState->pendingBuffers.empty())
        returnPendingBuffers(reqState);

    if (msg.data.meta.resultIndex >= 0) {
        /**
         * New Partial metadata result path. The result buffer is not the
         * settings but a separate buffer stored in the request.
         * The resultIndex indicates which one.
         * This can be returned straight away now that we have declared 3.2
         * device version. No need to enforce the order between shutter events
         * result and buffers. We do not need to store the partials either.
         * we can return them directly
         */
        status = returnResult(reqState, msg.data.meta.resultIndex);

        bool allMetadataDone = (reqState->partialResultReturned == mPartialResultCount);
        bool allBuffersDone = (reqState->buffersReturned == reqState->buffersToReturn);
        if (allBuffersDone && allMetadataDone) {
           status = recycleRequest(request);
        }
        return status;
    }

    reqState->pendingPartialResults.emplace_back(request->getSettings());
    LOGD(" <Request %d> camera id %d Metadata arrived %zu/%d", reqId, reqState->request->getCameraId(),
            reqState->pendingPartialResults.size(),mPartialResultCount);

    if (!reqState->isShutterDone) {
        LOGD("metadata arrived before shutter, storing");
        return NO_ERROR;
    }

    unsigned int resultsReceived = reqState->pendingPartialResults.size();
    bool allMetaReceived = (resultsReceived == mPartialResultCount);

    if (allMetaReceived) {
        returnPendingPartials(reqState);
    }

    bool allMetadataDone = (reqState->partialResultReturned == mPartialResultCount);
    bool allBuffersDone = (reqState->buffersReturned == reqState->buffersToReturn);
    if (allBuffersDone && allMetadataDone) {
        status = recycleRequest(request);
    }

    /**
     * if the metadata done for the next request is available then send it.
     *
     */
    if (allMetadataDone) {
        returnStoredPartials();
    }

    return status;
}

/**
 * returnStoredPartials
 * return the all stored pending metadata
 */
status_t ResultProcessor::returnStoredPartials()
{
    status_t status = NO_ERROR;

    while (mRequestsPendingMetaReturn.size() > 0) {
        int reqId = mRequestsPendingMetaReturn.front();
        LOGD("stored metadata req size:%zu, first reqid:%d", mRequestsPendingMetaReturn.size(), reqId);
        RequestState_t *reqState = nullptr;

        if (getRequestsInTransit(&reqState, reqId) == BAD_VALUE) {
            LOGE("Request %d was not registered:find the bug", reqId);
            mRequestsPendingMetaReturn.pop_front();
            return BAD_VALUE;
        }

        returnPendingPartials(reqState);
        bool allMetadataDone = (reqState->partialResultReturned == mPartialResultCount);
        bool allBuffersDone = (reqState->buffersReturned == reqState->buffersToReturn);
        if (allBuffersDone && allMetadataDone) {
            status = recycleRequest(reqState->request);
        }

        mRequestsPendingMetaReturn.pop_front();
    }
    return status;
}


status_t ResultProcessor::bufferDone(Camera3Request* request,
                                     std::shared_ptr<CameraBuffer> buffer)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    Message msg;
    msg.id = MESSAGE_ID_BUFFER_DONE;
    msg.request = request;
    msg.buffer = buffer;

    return  mMessageQueue.send(&msg);
}

/**
 * handleBufferDone
 *
 * Try to return the buffer provided by PSL to client
 * This method checks whether we can return the buffer straight to client or
 * we need to hold it until shutter event has been received.
 * \param msg [IN] Contains the buffer produced by PSL
 */
status_t ResultProcessor::handleBufferDone(Message &msg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = NO_ERROR;
    Camera3Request* request = msg.request;
    std::shared_ptr<CameraBuffer> buffer = msg.buffer;

    int reqId = request->getId();
    if (buffer.get() && buffer->getOwner()) {
        PERFORMANCE_HAL_ATRACE_PARAM1(
            "streamAndReqId", reqId | (buffer->getOwner()->seqNo() << 28));
    } else {
        PERFORMANCE_HAL_ATRACE_PARAM1("reqId", reqId);
    }
    PERFORMANCE_ATRACE_NAME_SNPRINTF("handleBufferDone - %d", reqId);

    RequestState_t *reqState = nullptr;
    if (getRequestsInTransit(&reqState, reqId) == BAD_VALUE) {
        LOGE("Request %d was not registered find the bug", reqId);
        return BAD_VALUE;
    }

    LOGD("<Request %d> camera id %d buffer received from PSL",
        reqId, reqState->request->getCameraId());
    reqState->pendingBuffers.emplace_back(buffer);
    if (!reqState->isShutterDone) {
        LOGD("Buffer arrived before shutter req %d, queue it",reqId);
        return NO_ERROR;
    }

    returnPendingBuffers(reqState);

    if (!reqState->pendingPartialResults.empty()) {
        returnPendingPartials(reqState);
    }

    bool allMetaDone = (reqState->partialResultReturned == mPartialResultCount);
    bool allBuffersDone = (reqState->buffersReturned == reqState->buffersToReturn);
    if (allBuffersDone && allMetaDone) {
        status = recycleRequest(request);
    }
    return status;
}

void ResultProcessor::returnPendingBuffers(RequestState_t* reqState)
{
    LOGD("@%s - req-%d  %zu buffs", __FUNCTION__, reqState->reqId,
                                  reqState->pendingBuffers.size());
    unsigned int i;
    camera3_capture_result_t result;
    camera3_stream_buffer_t buf;
    std::shared_ptr<CameraBuffer> pendingBuf;
    Camera3Request* request = reqState->request;

    /**
     * protection against duplicated calls when all buffers have been returned
     */
    if(reqState->buffersReturned == reqState->buffersToReturn) {
        LOGW("trying to return buffers twice. Check PSL implementation");
        return;
    }

    for (i = 0; i < reqState->pendingBuffers.size(); i++) {
        CLEAR(buf);
        CLEAR(result);

        pendingBuf = reqState->pendingBuffers[i];
        if (!request->isInputBuffer(pendingBuf)) {
            result.num_output_buffers = 1;
        }
        result.frame_number = reqState->reqId;
        // Force drop buffers when request error
        buf.status = request->getError() ? CAMERA3_BUFFER_STATUS_ERROR :
                        pendingBuf->status();
        buf.stream = pendingBuf->getOwner()->getStream();
        /* framework check the handle point other than handle */
        /* buf.buffer = pendingBuf->getBufferHandle(); */
        buf.buffer = pendingBuf->getBufferHandlePtr();
        pendingBuf->getFence(&buf);
        result.result = nullptr;
        if (request->isInputBuffer(pendingBuf)) {
            result.input_buffer = &buf;
            LOGD(" <Request %d> return an input buffer", reqState->reqId);
        } else {
            result.output_buffers = &buf;
        }

        processCaptureResult(reqState, &result);
        pendingBuf->getOwner()->decOutBuffersInHal();
        reqState->buffersReturned += 1;
        LOGD(" <Request %d> camera id %d buffer done %d/%d ", reqState->reqId,
            reqState->request->getCameraId(), reqState->buffersReturned, reqState->buffersToReturn);
    }

    reqState->pendingBuffers.clear();
}

/**
 * Returns the single partial result stored in the vector.
 * In the future we will have more than one.
 */
void ResultProcessor::returnPendingPartials(RequestState_t* reqState)
{
    camera3_capture_result result;
    CLEAR(result);

    // it must be 1 for >= CAMERA_DEVICE_API_VERSION_3_2 if we don't support partial metadata
    result.partial_result = mPartialResultCount;

    //TODO: combine them all in one metadata buffer and return
    result.frame_number = reqState->reqId;

    // check if metadata result of the previous request is returned
    int pre_reqId = reqState->reqId - 1;
    RequestState_t *pre_reqState = nullptr;

    if (getRequestsInTransit(&pre_reqState, pre_reqId) == NO_ERROR) {
        if (pre_reqState->partialResultReturned == 0) {
            LOGD("wait the metadata of the previous request return");
            LOGD("%s add reqId %d in to pending list\n", __FUNCTION__, reqState->reqId);
            std::list<int>::iterator it;
            for (it = mRequestsPendingMetaReturn.begin();
                  (it != mRequestsPendingMetaReturn.end()) && (*it < reqState->reqId);
                  it++);
            mRequestsPendingMetaReturn.insert(it, reqState->reqId);
            return;
        }
    }

    const CameraMetadata * settings = reqState->pendingPartialResults[0];

    result.result = settings->getAndLock();
    result.num_output_buffers = 0;

    mCallbackOps->process_capture_result(mCallbackOps, &result);

    settings->unlock(result.result);

    reqState->partialResultReturned += 1;
    LOGD("<Request %d> camera id %d result cb done",reqState->reqId, reqState->request->getCameraId());
    reqState->pendingPartialResults.clear();
}

/**
 * returnResult
 *
 * Returns a partial result metadata buffer, just one.
 *
 * \param reqState[IN]: Request State control structure
 * \param returnIndex[IN]: index of the result buffer in the array of result
 *                         buffers stored in the request
 *                         -1 means null metadata
 */
status_t ResultProcessor::returnResult(RequestState_t* reqState, int returnIndex)
{
    status_t status = NO_ERROR;
    camera3_capture_result result;
    CameraMetadata *resultMetadata;
    CLEAR(result);
    if (returnIndex >= 0)
        resultMetadata = reqState->request->getPartialResultBuffer(returnIndex);
    else
        resultMetadata = nullptr;
    reqState->request->dumpResults();
    // This value should be between 1 and android.request.partialResultCount
    // The index goes between 0-partialResultCount -1
    result.partial_result = returnIndex + 1;
    result.frame_number = reqState->reqId;
    result.result = resultMetadata ? resultMetadata->getAndLock() : nullptr;
    result.num_output_buffers = 0;

    mCallbackOps->process_capture_result(mCallbackOps, &result);

    if (resultMetadata)
        resultMetadata->unlock(result.result);

    reqState->partialResultReturned += 1;
    LOGD("<Request %d> camera id %d result cb done", reqState->reqId, reqState->request->getCameraId());
    return status;
}

/**
 * getRequestsInTransit
 *
 * Returns a RequestState in the map at index.
 *
 * \param reqState[OUT]: Request State control structure
 * \param index[IN]: index of the result state, it's request Id mapped to the state
 */
status_t ResultProcessor::getRequestsInTransit(RequestState_t** reqState, int index)
{
    status_t state = NO_ERROR;
    std::map<int, RequestState_t*>::const_iterator it;

    it = mRequestsInTransit.find(index);
    if (it == mRequestsInTransit.cend()) {
        LOGI("%s, Result State not found for id %d", __FUNCTION__, index);
        state = BAD_VALUE;
    } else {
        state = NO_ERROR;
        *reqState = it->second;
    }

    return state;
}

void ResultProcessor::processCaptureResult(RequestState_t* reqState, camera3_capture_result* result)
{
    int numMetaLeft = mPartialResultCount - reqState->partialResultReturned;
    int numBufLeft = reqState->buffersToReturn - reqState->buffersReturned;

    // Report request error when it's the last result
    if (numMetaLeft + numBufLeft == 1) {
        Camera3Request* request = reqState->request;
        if (request->getError())
            returnRequestError(request->getId());
    }

    mCallbackOps->process_capture_result(mCallbackOps, result);
}

/**
 * Request is fully processed
 * send the request object back to RequestThread for recycling
 * return the request-state struct to the pool
 */
status_t ResultProcessor::recycleRequest(Camera3Request *req)
{
    status_t status = NO_ERROR;
    int id = req->getId();
    PERFORMANCE_ATRACE_ASYNC_END("Shutter2Alldone", id);
    RequestState_t *reqState = mRequestsInTransit.at(id);
    status = mReqStatePool.releaseItem(reqState);
    if (status != NO_ERROR) {
        LOGE("Request State pool failure[%d] , recycling is broken!!", status);
    }

    mRequestsInTransit.erase(id);
    mRequestThread->returnRequest(req);
    LOGD("<Request %d> camera id %d OUT from ResultProcessor",id, reqState->request->getCameraId());
    return status;
}

/**
 * The android camera framework will remove the request when receiving the
 * first result after request error. So the request error should be reported
 * right before sending the last result.
 */
void ResultProcessor::returnRequestError(int reqId)
{
    LOGD("%s for <Request : %d", __FUNCTION__, reqId);

    camera3_notify_msg msg;
    CLEAR(msg);
    msg.type = CAMERA3_MSG_ERROR;
    msg.message.error.frame_number = reqId;
    msg.message.error.error_stream = nullptr;
    msg.message.error.error_code = CAMERA3_MSG_ERROR_REQUEST;
    mCallbackOps->notify(mCallbackOps, &msg);
}

status_t ResultProcessor::deviceError(void)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    Message msg;
    msg.id = MESSAGE_ID_DEVICE_ERROR;

    return  mMessageQueue.send(&msg);
}

void ResultProcessor::handleDeviceError(void)
{
    camera3_notify_msg msg;
    CLEAR(msg);
    msg.type = CAMERA3_MSG_ERROR;
    msg.message.error.error_code = CAMERA3_MSG_ERROR_DEVICE;
    mCallbackOps->notify(mCallbackOps, &msg);
    LOGD("@%s done", __FUNCTION__);
}
//----------------------------------------------------------------------------
} NAMESPACE_DECLARATION_END
