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

#include "AnalyzeUseCase.h"
#include "ConfigManager.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

namespace android {
namespace automotive {
namespace evs {
namespace support {

AnalyzeUseCase::AnalyzeUseCase(string cameraId, BaseAnalyzeCallback* callback)
              : BaseUseCase(vector<string>(1, cameraId)),
                mAnalyzeCallback(callback) {}

AnalyzeUseCase::~AnalyzeUseCase() {}

bool AnalyzeUseCase::initialize() {
    // TODO(b/130246434): Move the following ConfigManager and thread pool
    // logic into ResourceManager, for both display and analyze use case.

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

    ALOGD("Requesting camera list");
    for (auto&& info : config.getCameras()) {
        // This use case is currently a single camera use case.
        // Only one element is available in the camera id list.
        string cameraId = mCameraIds[0];
        if (cameraId == info.cameraId) {
            mStreamHandler =
                mResourceManager->obtainStreamHandler(cameraId);
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

bool AnalyzeUseCase::startVideoStream() {
    ALOGD("AnalyzeUseCase::startVideoStream");

    // Initialize the use case.
    if (!mIsInitialized && !initialize()) {
        ALOGE("There is an error while initializing the use case. Exiting");
        return false;
    }

    ALOGD("Attach callback to StreamHandler");
    if (mAnalyzeCallback != nullptr) {
        mStreamHandler->attachAnalyzeCallback(mAnalyzeCallback);
    }

    mStreamHandler->startStream();

    return true;
}

void AnalyzeUseCase::stopVideoStream() {
    ALOGD("AnalyzeUseCase::stopVideoStream");

    if (mStreamHandler == nullptr) {
        ALOGE("Failed to detach render callback since stream handler is null");

        // Something may go wrong. Instead of to return this method right away,
        // we want to finish the remaining logic of this method to try to
        // release other resources.
    } else {
        mStreamHandler->detachAnalyzeCallback();
    }

    if (mResourceManager == nullptr) {
        ALOGE("Failed to release resources since resource manager is null");
    } else {
        mResourceManager->releaseStreamHandler(mCameraIds[0]);
    }

    mStreamHandler = nullptr;

    // TODO(b/130246434): with the current logic, the initialize method will
    // be triggered every time when a pair of
    // stopVideoStream/startVideoStream is called. We might want to move
    // some heavy work away from initialize method so increase the
    // performance.

    // Sets mIsInitialzed to false so the initialize method will be
    // triggered when startVideoStream is called again.
    mIsInitialized = false;
}

// TODO(b/130246434): For both Analyze use case and Display use case, return a
// pointer instead of an object.
AnalyzeUseCase AnalyzeUseCase::createDefaultUseCase(
    string cameraId, BaseAnalyzeCallback* callback) {
    return AnalyzeUseCase(cameraId, callback);
}

}  // namespace support
}  // namespace evs
}  // namespace automotive
}  // namespace android

