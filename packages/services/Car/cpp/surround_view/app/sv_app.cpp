/*
 * Copyright 2020 The Android Open Source Project
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

#include <android-base/logging.h>
#include <android/hardware/automotive/evs/1.1/IEvsEnumerator.h>
#include <android/hardware/automotive/sv/1.0/ISurroundViewService.h>
#include <android/hardware/automotive/sv/1.0/ISurroundView2dSession.h>
#include <android/hardware/automotive/sv/1.0/ISurroundView3dSession.h>
#include <hidl/HidlTransportSupport.h>
#include <stdio.h>
#include <utils/StrongPointer.h>
#include <utils/Log.h>
#include <thread>

#include "SurroundViewServiceCallback.h"

// libhidl:
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

using android::sp;
using android::hardware::Return;
using android::hardware::automotive::evs::V1_0::EvsResult;

using BufferDesc_1_0  = android::hardware::automotive::evs::V1_0::BufferDesc;
using DisplayState = android::hardware::automotive::evs::V1_0::DisplayState;

using namespace android::hardware::automotive::sv::V1_0;
using namespace android::hardware::automotive::evs::V1_1;

const int kLowResolutionWidth = 120;
const int kLowResolutionHeight = 90;

enum DemoMode {
    UNKNOWN,
    DEMO_2D,
    DEMO_3D,
};

const float kHorizontalFov = 90;

// Number of views to generate.
const uint32_t kPoseCount = 16;

// Set of pose rotations expressed in quaternions.
// Views are generated about a circle at a height about the car, point towards the center.
const float kPoseRot[kPoseCount][4] = {
    {-0.251292, -0.251292, -0.660948, 0.660948},
    {0.197439, 0.295488, 0.777193, -0.519304},
    {0.135998, 0.328329, 0.86357, -0.357702},
    {0.0693313, 0.348552, 0.916761, -0.182355},
    {-7.76709e-09, 0.355381, 0.934722, 2.0429e-08},
    {-0.0693313, 0.348552, 0.916761, 0.182355},
    {-0.135998, 0.328329, 0.86357, 0.357702},
    {-0.197439, 0.295488, 0.777193, 0.519304},
    {-0.251292, 0.251292, 0.660948, 0.660948},
    {-0.295488, 0.197439, 0.519304, 0.777193},
    {-0.328329, 0.135998, 0.357702, 0.86357},
    {-0.348552, 0.0693313, 0.182355, 0.916761},
    {-0.355381, -2.11894e-09, -5.57322e-09, 0.934722},
    {-0.348552, -0.0693313, -0.182355, 0.916761},
    {-0.328329, -0.135998, -0.357702, 0.86357},
    {-0.295488, -0.197439, -0.519304, 0.777193}
};

// Set of pose translations i.e. positions of the views.
// Views are generated about a circle at a height about the car, point towards the center.
const float kPoseTrans[kPoseCount][4] = {
    {4, 0, 2.5},
    {3.69552, 1.53073, 2.5},
    {2.82843, 2.82843, 2.5},
    {1.53073, 3.69552, 2.5},
    {-1.74846e-07, 4, 2.5},
    {-1.53073, 3.69552, 2.5},
    {-2.82843, 2.82843, 2.5},
    {-3.69552, 1.53073, 2.5},
    {-4, -3.49691e-07, 2.5},
    {-3.69552, -1.53073, 2.5},
    {-2.82843, -2.82843, 2.5},
    {-1.53073, -3.69552, 2.5},
    {4.76995e-08, -4, 2.5},
    {1.53073, -3.69552, 2.5},
    {2.82843, -2.82843, 2.5},
    {3.69552, -1.53073, 2.5}
};

bool run2dSurroundView(sp<ISurroundViewService> pSurroundViewService,
                       sp<IEvsDisplay> pDisplay) {
    LOG(INFO) << "Run 2d Surround View demo";

    // Call HIDL API "start2dSession"
    sp<ISurroundView2dSession> surroundView2dSession;

    SvResult svResult;
    pSurroundViewService->start2dSession(
        [&surroundView2dSession, &svResult](
            const sp<ISurroundView2dSession>& session, SvResult result) {
        surroundView2dSession = session;
        svResult = result;
    });

    if (surroundView2dSession == nullptr || svResult != SvResult::OK) {
        LOG(ERROR) << "Failed to start2dSession";
        return false;
    } else {
        LOG(INFO) << "start2dSession succeeded";
    }

    sp<SurroundViewServiceCallback> sv2dCallback
        = new SurroundViewServiceCallback(pDisplay, surroundView2dSession);

    // Start 2d stream with callback
    if (surroundView2dSession->startStream(sv2dCallback) != SvResult::OK) {
        LOG(ERROR) << "Failed to start 2d stream";
        return false;
    }

    // Let the SV algorithm run for 10 seconds for HIGH_QUALITY
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Switch to low quality and lower resolution
    Sv2dConfig config;
    config.width = kLowResolutionWidth;
    config.blending = SvQuality::LOW;
    if (surroundView2dSession->set2dConfig(config) != SvResult::OK) {
        LOG(ERROR) << "Failed to set2dConfig";
        return false;
    }

    // Let the SV algorithm run for 10 seconds for LOW_QUALITY
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // TODO(b/150412555): wait for the last frame
    // Stop the 2d stream and session
    surroundView2dSession->stopStream();

    pSurroundViewService->stop2dSession(surroundView2dSession);
    surroundView2dSession = nullptr;

    LOG(INFO) << "SV 2D session finished.";

    return true;
};

// Given a valid sv 3d session and pose, viewid and hfov parameters, sets the view.
bool setView(sp<ISurroundView3dSession> surroundView3dSession, uint32_t viewId,
        uint32_t poseIndex, float hfov)
{
    const View3d view3d = {
        .viewId = viewId,
        .pose = {
            .rotation = {.x=kPoseRot[poseIndex][0], .y=kPoseRot[poseIndex][1],
                    .z=kPoseRot[poseIndex][2], .w=kPoseRot[poseIndex][3]},
            .translation = {.x=kPoseTrans[poseIndex][0], .y=kPoseTrans[poseIndex][1],
                    .z=kPoseTrans[poseIndex][2]},
        },
        .horizontalFov = hfov,
    };

    const std::vector<View3d> views = {view3d};
    if (surroundView3dSession->setViews(views) != SvResult::OK) {
        return false;
    }
    return true;
}

bool run3dSurroundView(sp<ISurroundViewService> pSurroundViewService,
                       sp<IEvsDisplay> pDisplay) {
    LOG(INFO) << "Run 3d Surround View demo";

    // Call HIDL API "start3dSession"
    sp<ISurroundView3dSession> surroundView3dSession;

    SvResult svResult;
    pSurroundViewService->start3dSession(
        [&surroundView3dSession, &svResult](
            const sp<ISurroundView3dSession>& session, SvResult result) {
        surroundView3dSession = session;
        svResult = result;
    });

    if (surroundView3dSession == nullptr || svResult != SvResult::OK) {
        LOG(ERROR) << "Failed to start3dSession";
        return false;
    } else {
        LOG(INFO) << "start3dSession succeeded";
    }

    sp<SurroundViewServiceCallback> sv3dCallback
        = new SurroundViewServiceCallback(pDisplay, surroundView3dSession);

    // A view must be set before the 3d stream is started.
    if (!setView(surroundView3dSession, 0, 0, kHorizontalFov)) {
        LOG(ERROR) << "Failed to setView of pose index :" << 0;
        return false;
    }

    // Start 3d stream with callback
    if (surroundView3dSession->startStream(sv3dCallback) != SvResult::OK) {
        LOG(ERROR) << "Failed to start 3d stream";
        return false;
    }

    // Let the SV algorithm run for 10 seconds for HIGH_QUALITY
    const int totalViewingTimeSecs = 10;
    const std::chrono::milliseconds
            perPoseSleepTimeMs(totalViewingTimeSecs * 1000 / kPoseCount);
    for(uint32_t i = 1; i < kPoseCount; i++) {
        if (!setView(surroundView3dSession, i, i, kHorizontalFov)) {
            LOG(WARNING) << "Failed to setView of pose index :" << i;
        }
        std::this_thread::sleep_for(perPoseSleepTimeMs);
    }

    // Switch to low quality and lower resolution
    Sv3dConfig config;
    config.width = kLowResolutionWidth;
    config.height = kLowResolutionHeight;
    config.carDetails = SvQuality::LOW;
    if (surroundView3dSession->set3dConfig(config) != SvResult::OK) {
        LOG(ERROR) << "Failed to set3dConfig";
        return false;
    }

    // Let the SV algorithm run for 10 seconds for LOW_QUALITY
    for(uint32_t i = 0; i < kPoseCount; i++) {
        if(!setView(surroundView3dSession, i + kPoseCount, i, kHorizontalFov)) {
            LOG(WARNING) << "Failed to setView of pose index :" << i;
        }
        std::this_thread::sleep_for(perPoseSleepTimeMs);
    }

    // TODO(b/150412555): wait for the last frame
    // Stop the 3d stream and session
    surroundView3dSession->stopStream();

    pSurroundViewService->stop3dSession(surroundView3dSession);
    surroundView3dSession = nullptr;

    LOG(DEBUG) << "SV 3D session finished.";

    return true;
};

// Main entry point
int main(int argc, char** argv) {
    // Start up
    LOG(INFO) << "SV app starting";

    DemoMode mode = UNKNOWN;
    for (int i=1; i< argc; i++) {
        if (strcmp(argv[i], "--use2d") == 0) {
            mode = DEMO_2D;
        } else if (strcmp(argv[i], "--use3d") == 0) {
            mode = DEMO_3D;
        } else {
            LOG(WARNING) << "Ignoring unrecognized command line arg: "
                         << argv[i];
        }
    }

    if (mode == UNKNOWN) {
        LOG(ERROR) << "No demo mode is specified. Exiting";
        return EXIT_FAILURE;
    }

    // Set thread pool size to one to avoid concurrent events from the HAL.
    // This pool will handle the SurroundViewStream callbacks.
    configureRpcThreadpool(1, false /* callerWillJoin */);

    // Try to connect to EVS service
    LOG(INFO) << "Acquiring EVS Enumerator";
    sp<IEvsEnumerator> evs = IEvsEnumerator::getService();
    if (evs == nullptr) {
        LOG(ERROR) << "getService(default) returned NULL.  Exiting.";
        return EXIT_FAILURE;
    }

    // Try to connect to SV service
    LOG(INFO) << "Acquiring SV Service";
    android::sp<ISurroundViewService> surroundViewService
        = ISurroundViewService::getService("default");

    if (surroundViewService == nullptr) {
        LOG(ERROR) << "getService(default) returned NULL.";
        return EXIT_FAILURE;
    } else {
        LOG(INFO) << "Get ISurroundViewService default";
    }

    // Connect to evs display
    int displayId;
    evs->getDisplayIdList([&displayId](auto idList) {
        displayId = idList[0];
    });

    LOG(INFO) << "Acquiring EVS Display with ID: "
              << displayId;
    sp<IEvsDisplay> display = evs->openDisplay_1_1(displayId);
    if (display == nullptr) {
        LOG(ERROR) << "EVS Display unavailable.  Exiting.";
        return EXIT_FAILURE;
    }

    if (mode == DEMO_2D) {
        if (!run2dSurroundView(surroundViewService, display)) {
            LOG(ERROR) << "Something went wrong in 2d surround view demo. "
                       << "Exiting.";
            return EXIT_FAILURE;
        }
    } else if (mode == DEMO_3D) {
        if (!run3dSurroundView(surroundViewService, display)) {
            LOG(ERROR) << "Something went wrong in 3d surround view demo. "
                       << "Exiting.";
            return EXIT_FAILURE;
        }
    }

    evs->closeDisplay(display);

    LOG(DEBUG) << "SV sample app finished running successfully";
    return EXIT_SUCCESS;
}
