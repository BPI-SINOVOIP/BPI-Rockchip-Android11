//
// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#define LOG_TAG "VtsHalAutomotiveDisplayTest"
#include <android-base/logging.h>

#include <android/frameworks/automotive/display/1.0/IAutomotiveDisplayProxyService.h>
#include <android/hardware/graphics/bufferqueue/2.0/IGraphicBufferProducer.h>
#include <ui/DisplayConfig.h>
#include <ui/DisplayState.h>
#include <utils/Log.h>

#include <gtest/gtest.h>
#include <hidl/GtestPrinter.h>
#include <hidl/ServiceManagement.h>

using namespace ::android::frameworks::automotive::display::V1_0;
using ::android::hardware::graphics::bufferqueue::V2_0::IGraphicBufferProducer;
using ::android::sp;

// The main test class for Automotive Display Service
class AutomotiveDisplayHidlTest : public ::testing::TestWithParam<std::string> {
public:
    virtual void SetUp() override {
        // Make sure we can connect to the service
        mDisplayProxy = IAutomotiveDisplayProxyService::getService(GetParam());
        ASSERT_NE(mDisplayProxy.get(), nullptr);
    }

    virtual void TearDown() override {}

    sp<IAutomotiveDisplayProxyService> mDisplayProxy;    // Every test needs access to the service
};

TEST_P(AutomotiveDisplayHidlTest, getIGBP) {
    ALOGI("Test getIGraphicBufferProducer method");

    android::hardware::hidl_vec<uint64_t> displayIdList;
    mDisplayProxy->getDisplayIdList([&displayIdList](const auto& list) {
        displayIdList.resize(list.size());
        for (auto i = 0; i < list.size(); ++i) {
            displayIdList[i] = list[i];
        }
    });

    for (const auto& id : displayIdList) {
        // Get a display info
        mDisplayProxy->getDisplayInfo(id, [](const auto& cfg, const auto& /*state*/) {
            android::DisplayConfig* pConfig = (android::DisplayConfig*)cfg.data();
            ASSERT_GT(pConfig->resolution.getWidth(), 0);
            ASSERT_GT(pConfig->resolution.getHeight(), 0);
        });

        sp<IGraphicBufferProducer> igbp = mDisplayProxy->getIGraphicBufferProducer(id);
        ASSERT_NE(igbp, nullptr);
    }
}

TEST_P(AutomotiveDisplayHidlTest, showWindow) {
    ALOGI("Test showWindow method");

    android::hardware::hidl_vec<uint64_t> displayIdList;
    mDisplayProxy->getDisplayIdList([&displayIdList](const auto& list) {
        displayIdList.resize(list.size());
        for (auto i = 0; i < list.size(); ++i) {
            displayIdList[i] = list[i];
        }
    });

    for (const auto& id : displayIdList) {
        ASSERT_EQ(mDisplayProxy->showWindow(id), true);
    }
}

TEST_P(AutomotiveDisplayHidlTest, hideWindow) {
    ALOGI("Test hideWindow method");

    android::hardware::hidl_vec<uint64_t> displayIdList;
    mDisplayProxy->getDisplayIdList([&displayIdList](const auto& list) {
        displayIdList.resize(list.size());
        for (auto i = 0; i < list.size(); ++i) {
            displayIdList[i] = list[i];
        }
    });

    for (const auto& id : displayIdList) {
        ASSERT_EQ(mDisplayProxy->hideWindow(id), true);
    }
}

INSTANTIATE_TEST_SUITE_P(
    PerInstance,
    AutomotiveDisplayHidlTest,
    testing::ValuesIn(
        android::hardware::getAllHalInstanceNames(IAutomotiveDisplayProxyService::descriptor)
    ),
    android::hardware::PrintInstanceNameToString
);
