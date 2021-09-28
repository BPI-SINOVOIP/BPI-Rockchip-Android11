/*
 * Copyright 2017 The Android Open Source Project
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

#include "QCamera3HdrPlusListenerThread.h"


using ::android::hardware::camera::common::V1_0::helper::CameraMetadata;
using namespace android;

namespace qcamera {

QCamera3HdrPlusListenerThread::QCamera3HdrPlusListenerThread(
        HdrPlusClientListener *listener) : mListener(listener), mExitRequested(false),
        mFatalError(false)
{
}

QCamera3HdrPlusListenerThread::~QCamera3HdrPlusListenerThread()
{
    requestExit();

    while (mResults.size() > 0) {
        PendingResult result = mResults.front();
        mResults.pop();

        if (result.metadata != nullptr) {
            free_camera_metadata(result.metadata);
        }
    }
}

void QCamera3HdrPlusListenerThread::onOpened(std::unique_ptr<HdrPlusClient> client)
{
    std::unique_lock<std::mutex> l(mCallbackLock);
    if (mClient != nullptr) {
        ALOGW("%s: An old client exists and will be destroyed.", __FUNCTION__);
    }
    mClient = std::move(client);
    mPendingCallbacks.push(CALLBACK_TYPE_OPENED);
    mCallbackCond.notify_one();
}

void QCamera3HdrPlusListenerThread::onOpenFailed(status_t err)
{
    std::unique_lock<std::mutex> l(mCallbackLock);
    if (mOpenError != OK) {
        ALOGW("%s: An old open failure exists and will be ignored: %s (%d)", __FUNCTION__,
                strerror(-mOpenError), mOpenError);
    }
    mOpenError = err;
    mPendingCallbacks.push(CALLBACK_TYPE_OPENFAILED);
    mCallbackCond.notify_one();
}

void QCamera3HdrPlusListenerThread::onFatalError()
{
    std::unique_lock<std::mutex> l(mCallbackLock);
    if (mFatalError) {
        ALOGW("%s: An old fatal failure exists.", __FUNCTION__);
    }
    mFatalError = true;
    mPendingCallbacks.push(CALLBACK_TYPE_FATAL_ERROR);
    mCallbackCond.notify_one();
}

void QCamera3HdrPlusListenerThread::onCaptureResult(pbcamera::CaptureResult *result,
            const camera_metadata_t &resultMetadata)
{
    std::unique_lock<std::mutex> l(mCallbackLock);

    PendingResult pendingResult = {};
    pendingResult.result = *result;
    pendingResult.metadata = clone_camera_metadata(&resultMetadata);
    pendingResult.isFailed = false;
    mResults.push(pendingResult);

    mPendingCallbacks.push(CALLBACK_TYPE_CAPTURE_RESULT);
    mCallbackCond.notify_one();
}

void QCamera3HdrPlusListenerThread::onFailedCaptureResult(pbcamera::CaptureResult *failedResult)
{
    std::unique_lock<std::mutex> l(mCallbackLock);

    PendingResult result = {};
    result.result = *failedResult;
    result.metadata = nullptr;
    result.isFailed = true;
    mResults.push(result);

    mPendingCallbacks.push(CALLBACK_TYPE_CAPTURE_RESULT);
    mCallbackCond.notify_one();
}

void QCamera3HdrPlusListenerThread::onShutter(uint32_t requestId, int64_t apSensorTimestampNs)
{
    std::unique_lock<std::mutex> l(mCallbackLock);

    std::pair<uint32_t, int64_t> shutter(requestId, apSensorTimestampNs);
    mShutters.push(shutter);

    mPendingCallbacks.push(CALLBACK_TYPE_SHUTTER);
    mCallbackCond.notify_one();
}

void QCamera3HdrPlusListenerThread::onNextCaptureReady(uint32_t requestId)
{
    std::unique_lock<std::mutex> l(mCallbackLock);
    mNextCaptureReadyIds.push(requestId);

    mPendingCallbacks.push(CALLBACK_TYPE_NEXT_CAPTURE_READY);
    mCallbackCond.notify_one();

}

void QCamera3HdrPlusListenerThread::onPostview(uint32_t requestId,
        std::unique_ptr<std::vector<uint8_t>> postview, uint32_t width, uint32_t height,
        uint32_t stride, int32_t format)
{
    std::unique_lock<std::mutex> l(mCallbackLock);

    PendingPostview pendingPostview = {};
    pendingPostview.requestId = requestId;
    pendingPostview.postview = std::move(postview);
    pendingPostview.width = width;
    pendingPostview.height = height;
    pendingPostview.stride = stride;
    pendingPostview.format = format;
    mPostviews.push(std::move(pendingPostview));

    mPendingCallbacks.push(CALLBACK_TYPE_POSTVIEW);
    mCallbackCond.notify_one();

}

void QCamera3HdrPlusListenerThread::requestExit()
{
    std::unique_lock<std::mutex> l(mCallbackLock);
    mExitRequested = true;
    mCallbackCond.notify_one();
}

void QCamera3HdrPlusListenerThread::handleFatalError()
{
    bool fatalError;
    {
        std::unique_lock<std::mutex> lock(mCallbackLock);
        if (!mFatalError) {
            ALOGW("%s: There is no fatal error.", __FUNCTION__);
            return;
        }

        fatalError = mFatalError;
    }

    mListener->onFatalError();
}

void QCamera3HdrPlusListenerThread::handlePendingClient()
{
    std::unique_ptr<HdrPlusClient> client;
    {
        std::unique_lock<std::mutex> lock(mCallbackLock);
        if (mClient == nullptr) {
            ALOGW("%s: There is no pending client.", __FUNCTION__);
            return;
        }

        client = std::move(mClient);
    }

    mListener->onOpened(std::move(client));
}

void QCamera3HdrPlusListenerThread::handleOpenError()
{
    status_t err = OK;
    {
        std::unique_lock<std::mutex> lock(mCallbackLock);
        if (mOpenError == OK) {
            ALOGW("%s: There is no pending open failure.", __FUNCTION__);
            return;
        }

        err = mOpenError;
        mOpenError = OK;
    }

    mListener->onOpenFailed(err);
}

void QCamera3HdrPlusListenerThread::handleNextCaptureReady()
{
    uint32_t requestId = 0;
    {
        std::unique_lock<std::mutex> l(mCallbackLock);
        if (mNextCaptureReadyIds.size() == 0) {
            ALOGW("%s: There is no NextCaptureReady.", __FUNCTION__);
            return;
        }
        requestId = mNextCaptureReadyIds.front();
        mNextCaptureReadyIds.pop();
    }
    mListener->onNextCaptureReady(requestId);
}

void QCamera3HdrPlusListenerThread::handleCaptureResult()
{
    PendingResult result = {};
    {
        std::unique_lock<std::mutex> l(mCallbackLock);
        if (mResults.size() == 0) {
            ALOGW("%s: There is no capture result.", __FUNCTION__);
            return;
        }
        result = mResults.front();
        mResults.pop();
    }

    if (result.isFailed) {
        mListener->onFailedCaptureResult(&result.result);
    } else {
        mListener->onCaptureResult(&result.result, *result.metadata);
    }

    if (result.metadata != nullptr) {
        free_camera_metadata(result.metadata);
    }
}

void QCamera3HdrPlusListenerThread::handleShutter()
{
    uint32_t requestId;
    int64_t apSensorTimestampNs;

    {
        std::unique_lock<std::mutex> l(mCallbackLock);
        if (mShutters.size() == 0) {
            ALOGW("%s: There is no shutter.", __FUNCTION__);
            return;;
        }

        auto shutter = mShutters.front();
        requestId = shutter.first;
        apSensorTimestampNs = shutter.second;
        mShutters.pop();
    }

    mListener->onShutter(requestId, apSensorTimestampNs);
}

void QCamera3HdrPlusListenerThread::handlePostview()
{
    PendingPostview postview = {};

    {
        std::unique_lock<std::mutex> l(mCallbackLock);
        if (mPostviews.size() == 0) {
            ALOGW("%s: There is no postview.", __FUNCTION__);
            return;;
        }

        postview = std::move(mPostviews.front());
        mPostviews.pop();
    }

    mListener->onPostview(postview.requestId, std::move(postview.postview), postview.width,
            postview.height, postview.stride, postview.format);
}

bool QCamera3HdrPlusListenerThread::threadLoop()
{
    if (mListener == nullptr) {
        ALOGE("%s: mListener is nullptr.", __FUNCTION__);
        return false;
    }

    while (1) {
        CallbackType nextCallback;

        {
            std::unique_lock<std::mutex> lock(mCallbackLock);
            if (!mExitRequested && mPendingCallbacks.size() == 0) {
                mCallbackCond.wait(lock,
                        [&] { return mExitRequested || mPendingCallbacks.size() > 0; });
            }

            if (mExitRequested) {
                return false;
            } else {
                nextCallback = mPendingCallbacks.front();
                mPendingCallbacks.pop();
            }
        }

        switch (nextCallback) {
            case CALLBACK_TYPE_OPENED:
                handlePendingClient();
                break;
            case CALLBACK_TYPE_OPENFAILED:
                handleOpenError();
                break;
            case CALLBACK_TYPE_FATAL_ERROR:
                handleFatalError();
                break;
            case CALLBACK_TYPE_CAPTURE_RESULT:
                handleCaptureResult();
                break;
            case CALLBACK_TYPE_SHUTTER:
                handleShutter();
                break;
            case CALLBACK_TYPE_NEXT_CAPTURE_READY:
                handleNextCaptureReady();
                break;
            case CALLBACK_TYPE_POSTVIEW:
                handlePostview();
                break;
            default:
                ALOGE("%s: Unknown callback type %d", __FUNCTION__, nextCallback);
                break;
        }
    }

    return false;
}

}; // namespace qcamera
