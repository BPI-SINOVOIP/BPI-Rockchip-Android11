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
#ifndef __HDRPLUSCLIENTLISTENERHANDLERTHREAD__
#define __HDRPLUSCLIENTLISTENERHANDLERTHREAD__

// System dependencies
#include <utils/Thread.h>
#include <queue>

#include "EaselManagerClient.h"
#include "HdrPlusClient.h"

using ::android::hardware::camera::common::V1_0::helper::CameraMetadata;
using namespace android;

namespace qcamera {

/*
 * A thread to handle callbacks from HDR+ client. When a callback from HDR+ client is invoked,
 * HDR+ client callback thread will return and the threadloop of QCamera3HdrPlusListenerThread
 * will call the callback handlers in QCamera3HWI, to avoid deadlock in HDR+ client callback thread.
 */
class QCamera3HdrPlusListenerThread : public HdrPlusClientListener, public Thread
{
public:
    // listener is an HdrPlusClientListener to forward the callbacks in the thread loop.
    QCamera3HdrPlusListenerThread(HdrPlusClientListener *listener);
    virtual ~QCamera3HdrPlusListenerThread();

    // Request the thread to exit.
    void requestExit() override;

private:
    // HDR+ client callbacks.
    void onOpened(std::unique_ptr<HdrPlusClient> client) override;
    void onOpenFailed(status_t err) override;
    void onFatalError() override;
    void onCaptureResult(pbcamera::CaptureResult *result,
            const camera_metadata_t &resultMetadata) override;
    void onFailedCaptureResult(pbcamera::CaptureResult *failedResult) override;
    void onShutter(uint32_t requestId, int64_t apSensorTimestampNs) override;
    void onNextCaptureReady(uint32_t requestId) override;
    void onPostview(uint32_t requestId, std::unique_ptr<std::vector<uint8_t>> postview,
            uint32_t width, uint32_t height, uint32_t stride, int32_t format) override;

    bool threadLoop() override;

    // The following functions handle the pending callbacks by calling the callback handlers
    // in QCamera3HWI.
    bool hasPendingEventsLocked();
    void handlePendingClient();
    void handleNextCaptureReady();
    void handleCaptureResult();
    void handleFatalError();
    void handleOpenError();
    void handleShutter();
    void handlePostview();

    struct PendingResult {
        pbcamera::CaptureResult result;
        camera_metadata_t *metadata;
        bool isFailed;
    };

    struct PendingPostview {
        uint32_t requestId;
        std::unique_ptr<std::vector<uint8_t>> postview;
        uint32_t width;
        uint32_t height;
        uint32_t stride;
        int32_t format;
    };

    enum CallbackType {
        CALLBACK_TYPE_OPENED = 0,
        CALLBACK_TYPE_OPENFAILED,
        CALLBACK_TYPE_FATAL_ERROR,
        CALLBACK_TYPE_CAPTURE_RESULT,
        CALLBACK_TYPE_SHUTTER,
        CALLBACK_TYPE_NEXT_CAPTURE_READY,
        CALLBACK_TYPE_POSTVIEW,
    };

    HdrPlusClientListener *mListener;

    std::mutex mCallbackLock;

    // Condition for a new callback. Protected by mCallbackLock.
    std::condition_variable mCallbackCond;
    // If exit has been requested. Protected by mCallbackLock.
    bool mExitRequested;

    // The following variables store pending callbacks. Protected by mCallbackLock.
    std::unique_ptr<HdrPlusClient> mClient;
    std::queue<uint32_t> mNextCaptureReadyIds;
    std::queue<PendingResult> mResults;
    bool mFatalError;
    status_t mOpenError;
    std::queue<std::pair<uint32_t, int64_t>> mShutters;
    std::queue<PendingPostview> mPostviews;

    // A queue of pending callback types, in the same order as invoked by HDR+ client.
    // Protected by mCallbackLock.
    std::queue<CallbackType> mPendingCallbacks;
};

}; // namespace qcamera

#endif /* __HDRPLUSCLIENTLISTENERHANDLERTHREAD__ */