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

#define LOG_TAG "SurroundViewSessionTests"

#include "mock-evs/MockEvsEnumerator.h"
#include "mock-evs/MockSurroundViewCallback.h"

#include "IOModule.h"
#include "SurroundView2dSession.h"
#include "SurroundView3dSession.h"

#include <android/hardware/automotive/vehicle/2.0/IVehicle.h>

#include <android-base/logging.h>

#include <gtest/gtest.h>
#include <time.h>

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {
namespace {

const char* kSvConfigFilename = "vendor/etc/automotive/sv/sv_sample_config.xml";

// TODO(b/159733690): Verify the callbacks using help/mock methods
TEST(SurroundViewSessionTests, startAndStopSurroundView2dSession) {
    sp<IEvsEnumerator> fakeEvs = new MockEvsEnumerator();
    IOModule* ioModule = new IOModule(kSvConfigFilename);
    EXPECT_EQ(ioModule->initialize(), IOStatus::OK);

    IOModuleConfig config;
    ioModule->getConfig(&config);

    sp<SurroundView2dSession> sv2dSession =
            new SurroundView2dSession(fakeEvs, &config);

    EXPECT_TRUE(sv2dSession->initialize());

    sp<MockSurroundViewCallback> sv2dCallback =
            new MockSurroundViewCallback(sv2dSession);

    EXPECT_EQ(sv2dSession->startStream(sv2dCallback), SvResult::OK);

    sleep(5);

    sv2dSession->stopStream();
}

TEST(SurroundViewSessionTests, startAndStopSurroundView3dSession) {
    sp<IEvsEnumerator> fakeEvs = new MockEvsEnumerator();
    IOModule* ioModule = new IOModule(kSvConfigFilename);
    EXPECT_EQ(ioModule->initialize(), IOStatus::OK);

    IOModuleConfig config;
    ioModule->getConfig(&config);

    sp<SurroundView3dSession> sv3dSession =
            new SurroundView3dSession(fakeEvs,
                                      nullptr, /* VhalHandler */
                                      nullptr, /* AnimationModule */
                                      &config);

    EXPECT_TRUE(sv3dSession->initialize());

    sp<MockSurroundViewCallback> sv3dCallback =
            new MockSurroundViewCallback(sv3dSession);

    View3d view = {
        .viewId = 0,
        .pose = {
            .rotation = {.x=0, .y=0, .z=0, .w=1.0f},
            .translation = {.x=0, .y=0, .z=0},
        },
        .horizontalFov = 90,
    };
    vector<View3d> views;
    views.emplace_back(view);
    sv3dSession->setViews(views);

    EXPECT_EQ(sv3dSession->startStream(sv3dCallback), SvResult::OK);

    sleep(5);

    sv3dSession->stopStream();
}

}  // namespace
}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
