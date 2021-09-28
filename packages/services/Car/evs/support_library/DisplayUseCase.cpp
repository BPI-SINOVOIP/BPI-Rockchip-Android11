/*
 * Copyright (C) 2019 The Android Open Source Project
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
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>
#include <utils/SystemClock.h>

#include "DisplayUseCase.h"
#include "RenderDirectView.h"
#include "Utils.h"

namespace android {
namespace automotive {
namespace evs {
namespace support {

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

// TODO(b/130246434): since we don't support multi-display use case, there
// should only be one DisplayUseCase. Add the logic to prevent more than
// one DisplayUseCases running at the same time.
DisplayUseCase::DisplayUseCase(string cameraId, BaseRenderCallback* callback)
              : BaseUseCase(vector<string>(1, cameraId)) {
    mRenderCallback = callback;
}

DisplayUseCase::~DisplayUseCase() {
    if (mCurrentRenderer != nullptr) {
        mCurrentRenderer->deactivate();
        mCurrentRenderer = nullptr;  // It's a smart pointer, so destructs on assignment to null
    }

    mIsReadyToRun = false;
    if (mWorkerThread.joinable()) {
        mWorkerThread.join();
    }
}

bool DisplayUseCase::initialize() {
    // Load our configuration information
    ConfigManager config;
    if (!config.initialize("/system/etc/automotive/evs_support_lib/camera_config.json")) {
        ALOGE("Missing or improper configuration for the EVS application.  Exiting.");
        return false;
    }

    // Set thread pool size to one to avoid concurrent events from the HAL.
    // This pool will handle the EvsCameraStream callbacks.
    // Note:  This _will_ run in parallel with the EvsListener run() loop below which
    // runs the application logic that reacts to the async events.
    configureRpcThreadpool(1, false /* callerWillJoin */);

    mResourceManager = ResourceManager::getInstance();
    if (mResourceManager == nullptr) {
        ALOGE("Failed to get resource manager instance. Initialization failed.");
        return false;
    }

    // Request exclusive access to the EVS display
    ALOGI("Acquiring EVS Display");

    mDisplay = mResourceManager->openDisplay();
    if (mDisplay.get() == nullptr) {
        ALOGE("EVS Display unavailable.  Exiting.");
        return false;
    }

    ALOGD("Requesting camera list");
    for (auto&& info : config.getCameras()) {
        // This use case is currently a single camera use case.
        // Only one element is available in the camera id list.
        string cameraId = mCameraIds[0];
        if (cameraId == info.cameraId) {
            mStreamHandler = mResourceManager->obtainStreamHandler(cameraId);
            if (mStreamHandler.get() == nullptr) {
                ALOGE("Failed to get a valid StreamHandler for %s",
                      cameraId.c_str());
                return false;
            }

            mIsInitialized = true;
            return true;
        }
    }

    ALOGE("Cannot find a match camera. Exiting");
    return false;
}

// TODO(b/130246434): if user accidentally call this function twice, there is
// no logic to handle that and it will causes issues. For example, the
// mWorkerThread will be assigned twice and cause unexpected behavior.
// We need to fix this issue.
bool DisplayUseCase::startVideoStream() {
    // Initialize the use case.
    if (!mIsInitialized && !initialize()) {
        ALOGE("There is an error while initializing the use case. Exiting");
        return false;
    }

    ALOGD("Attach use case to StreamHandler");
    if (mRenderCallback != nullptr) {
        mStreamHandler->attachRenderCallback(mRenderCallback);
    }

    ALOGD("Start video streaming using worker thread");
    mIsReadyToRun = true;
    mWorkerThread = std::thread([this]() {
        // We have a camera assigned to this state for direct view
        mCurrentRenderer = std::make_unique<RenderDirectView>();
        if (!mCurrentRenderer) {
            ALOGE("Failed to construct direct renderer. Exiting.");
            mIsReadyToRun = false;
            return;
        }

        // Now set the display state based on whether we have a video feed to show
        // Start the camera stream
        ALOGD("EvsStartCameraStreamTiming start time: %" PRId64 "ms", android::elapsedRealtime());
        if (!mCurrentRenderer->activate()) {
            ALOGE("New renderer failed to activate. Exiting");
            mIsReadyToRun = false;
            return;
        }

        // Activate the display
        ALOGD("EvsActivateDisplayTiming start time: %" PRId64 "ms", android::elapsedRealtime());
        Return<EvsResult> result = mDisplay->setDisplayState(DisplayState::VISIBLE_ON_NEXT_FRAME);
        if (result != EvsResult::OK) {
            ALOGE("setDisplayState returned an error (%d). Exiting.", (EvsResult)result);
            mIsReadyToRun = false;
            return;
        }

        if (!mStreamHandler->startStream()) {
            ALOGE("failed to start stream handler");
            mIsReadyToRun = false;
            return;
        }

        while (mIsReadyToRun && streamFrame());

        ALOGD("Worker thread stops.");
    });

    return true;
}

void DisplayUseCase::stopVideoStream() {
    ALOGD("Stop video streaming in worker thread.");
    mIsReadyToRun = false;

    if (mStreamHandler == nullptr) {
        ALOGE("Failed to detach render callback since stream handler is null");

        // Something may go wrong. Instead of to return this method right away,
        // we want to finish the remaining logic of this method to try to
        // release other resources.
    } else {
        mStreamHandler->detachRenderCallback();
    }

    if (mResourceManager == nullptr) {
        ALOGE("Failed to release resources since resource manager is null");
    } else {
        mResourceManager->releaseStreamHandler(mCameraIds[0]);
        mStreamHandler = nullptr;

        mResourceManager->closeDisplay(mDisplay);
        mDisplay = nullptr;

        // TODO(b/130246434): with the current logic, the initialize method will
        // be triggered every time when a pair of
        // stopVideoStream/startVideoStream is called. We might want to move
        // some heavy work away from initialize method so increase the
        // performance.

        // Sets mIsInitialzed to false so the initialize method will be
        // triggered when startVideoStream is called again.
        mIsInitialized = false;
    }
    return;
}

bool DisplayUseCase::streamFrame() {
    // Get the output buffer we'll use to display the imagery
    BufferDesc tgtBuffer = {};
    mDisplay->getTargetBuffer([&tgtBuffer](const BufferDesc& buff) { tgtBuffer = buff; });

    // TODO(b/130246434): if there is no new display frame available, shall we
    // still get display buffer? Shall we just skip and keep the display
    // un-refreshed?
    // We should explore this option.

    // If there is no display buffer available, skip it.
    if (tgtBuffer.memHandle == nullptr) {
        ALOGW("Didn't get requested output buffer -- skipping this frame.");

        // Return true since it won't affect next call.
        return true;
    } else {
        // If there is no new display frame available, re-use the old (held)
        // frame for display.
        // Otherwise, return the old (held) frame, fetch the newly available
        // frame from stream handler, and use the new frame for display
        // purposes.
        if (!mStreamHandler->newDisplayFrameAvailable()) {
            ALOGD("No new display frame is available. Re-use the old frame.");
        } else {
            ALOGD("Get new display frame, refreshing");

            // If we already hold a camera image for display purposes, it's
            // time to return it to evs camera driver.
            if (mImageBuffer.memHandle.getNativeHandle() != nullptr) {
                mStreamHandler->doneWithFrame(mImageBuffer);
            }

            // Get the new image we want to use as our display content
            mImageBuffer = mStreamHandler->getNewDisplayFrame();
        }

        // Render the image buffer to the display buffer
        bool result = mCurrentRenderer->drawFrame(tgtBuffer, mImageBuffer);

        // Send the finished display buffer back to display driver
        // Even if the rendering fails, we still want to return the display
        // buffer.
        mDisplay->returnTargetBufferForDisplay(tgtBuffer);

        return result;
    }
}

DisplayUseCase DisplayUseCase::createDefaultUseCase(string cameraId, BaseRenderCallback* callback) {
    return DisplayUseCase(cameraId, callback);
}

}  // namespace support
}  // namespace evs
}  // namespace automotive
}  // namespace android
