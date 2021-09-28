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

#define LOG_TAG "SurroundView2dSessionTests"

#include "mock-evs/MockEvsEnumerator.h"
#include "mock-evs/MockSurroundViewCallback.h"

#include "IOModule.h"
#include "SurroundView2dSession.h"

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

using android::hardware::automotive::sv::V1_0::Sv2dMappingInfo;
using android::hardware::automotive::sv::V1_0::SvQuality;

// Sv 2D output height and width set by the config file.
const int kSv2dWidth = 768;
const int kSv2dHeight = 1024;

class SurroundView2dSessionTests : public ::testing::Test {
protected:
    void SetUp() override {
        sp<IEvsEnumerator> fakeEvs = new MockEvsEnumerator();
        mIoModule = new IOModule(kSvConfigFilename);
        EXPECT_EQ(mIoModule->initialize(), IOStatus::OK);

        mIoModule->getConfig(&mIoModuleConfig);

        mSv2dSession = new SurroundView2dSession(fakeEvs, &mIoModuleConfig);
        EXPECT_TRUE(mSv2dSession->initialize());
    }

    IOModule* mIoModule;
    IOModuleConfig mIoModuleConfig;
    sp<SurroundView2dSession> mSv2dSession;
};

TEST_F(SurroundView2dSessionTests, startAndStopSurroundView2dSession) {
    sp<MockSurroundViewCallback> sv2dCallback =
            new MockSurroundViewCallback(mSv2dSession);

    EXPECT_EQ(mSv2dSession->startStream(sv2dCallback), SvResult::OK);

    sleep(5);

    mSv2dSession->stopStream();
}

TEST_F(SurroundView2dSessionTests, get2dMappingInfoSuccess) {
    Sv2dMappingInfo sv2dMappingInfo;
    mSv2dSession->get2dMappingInfo(
        [&sv2dMappingInfo](const Sv2dMappingInfo& mappingInfo) {
            sv2dMappingInfo = mappingInfo;
        });

    EXPECT_NE(sv2dMappingInfo.width, 0);
    EXPECT_NE(sv2dMappingInfo.height, 0);
    EXPECT_EQ(sv2dMappingInfo.center.x, 0.0f);
    EXPECT_EQ(sv2dMappingInfo.center.y, 0.0f);
}

TEST_F(SurroundView2dSessionTests, get2dConfigSuccess) {
    Sv2dConfig sv2dConfig;
    mSv2dSession->get2dConfig(
        [&sv2dConfig](const Sv2dConfig& config) {
            sv2dConfig = config;
        });

    EXPECT_EQ(sv2dConfig.width, kSv2dWidth);
    EXPECT_EQ(sv2dConfig.blending, SvQuality::HIGH);
}

// Sets a different config and checks of the received config matches.
TEST_F(SurroundView2dSessionTests, setAndGet2dConfigSuccess) {
    // Set config.
    Sv2dConfig sv2dConfigSet = {kSv2dWidth / 2, SvQuality::LOW};
    EXPECT_EQ(mSv2dSession->set2dConfig(sv2dConfigSet), SvResult::OK);

    // Get config.
    Sv2dConfig sv2dConfigReceived;
    mSv2dSession->get2dConfig(
        [&sv2dConfigReceived](const Sv2dConfig& config) {
            sv2dConfigReceived = config;
        });

    EXPECT_EQ(sv2dConfigReceived.width, sv2dConfigSet.width);
    EXPECT_EQ(sv2dConfigReceived.blending, sv2dConfigSet.blending);
}

// Projects center of each cameras and checks if valid projected point is received.
TEST_F(SurroundView2dSessionTests, projectPoints2dSuccess) {
    hidl_vec<Point2dInt> points2dCamera = {
        /*Center point*/{.x = kSv2dWidth / 2, .y = kSv2dHeight /2}
    };

    std::vector<hidl_string> cameraIds = {"/dev/video60", "/dev/video61", "/dev/video62" ,
            "/dev/video63"};

    for (int i = 0; i < cameraIds.size(); i++) {
        mSv2dSession->projectCameraPoints(points2dCamera, cameraIds[i],
            [](const hidl_vec<Point2dFloat>& projectedPoints) {
                EXPECT_TRUE(projectedPoints[0].isValid);
            });
    }
}

}  // namespace
}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
