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

// TODO(b/129481165): remove the #pragma below and fix conversion issues
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"

#undef LOG_TAG
#define LOG_TAG "LibSurfaceFlingerUnittests"

#include <vector>

#include <gmock/gmock.h>
#include <gui/LayerMetadata.h>
#include <log/log.h>

#include "DisplayHardware/HWComposer.h"
#include "mock/DisplayHardware/MockComposer.h"

// TODO(b/129481165): remove the #pragma below and fix conversion issues
#pragma clang diagnostic pop // ignored "-Wconversion"

namespace android {
namespace {

namespace hal = android::hardware::graphics::composer::hal;

using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAreArray;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

struct MockHWC2ComposerCallback : public HWC2::ComposerCallback {
    ~MockHWC2ComposerCallback() = default;

    MOCK_METHOD3(onHotplugReceived,
                 void(int32_t sequenceId, hal::HWDisplayId display, hal::Connection connection));
    MOCK_METHOD2(onRefreshReceived, void(int32_t sequenceId, hal::HWDisplayId display));
    MOCK_METHOD4(onVsyncReceived,
                 void(int32_t sequenceId, hal::HWDisplayId display, int64_t timestamp,
                      std::optional<hal::VsyncPeriodNanos> vsyncPeriod));
    MOCK_METHOD3(onVsyncPeriodTimingChangedReceived,
                 void(int32_t sequenceId, hal::HWDisplayId display,
                      const hal::VsyncPeriodChangeTimeline& updatedTimeline));
    MOCK_METHOD2(onSeamlessPossible, void(int32_t sequenceId, hal::HWDisplayId display));
};

struct HWComposerTest : public testing::Test {
    Hwc2::mock::Composer* mHal = new StrictMock<Hwc2::mock::Composer>();
};

struct HWComposerSetConfigurationTest : public HWComposerTest {
    StrictMock<MockHWC2ComposerCallback> mCallback;
};

TEST_F(HWComposerSetConfigurationTest, loadsLayerMetadataSupport) {
    const std::string kMetadata1Name = "com.example.metadata.1";
    constexpr bool kMetadata1Mandatory = false;
    const std::string kMetadata2Name = "com.example.metadata.2";
    constexpr bool kMetadata2Mandatory = true;

    EXPECT_CALL(*mHal, getMaxVirtualDisplayCount()).WillOnce(Return(0));
    EXPECT_CALL(*mHal, getCapabilities()).WillOnce(Return(std::vector<hal::Capability>{}));
    EXPECT_CALL(*mHal, getLayerGenericMetadataKeys(_))
            .WillOnce(DoAll(SetArgPointee<0>(std::vector<hal::LayerGenericMetadataKey>{
                                    {kMetadata1Name, kMetadata1Mandatory},
                                    {kMetadata2Name, kMetadata2Mandatory},
                            }),
                            Return(hardware::graphics::composer::V2_4::Error::NONE)));
    EXPECT_CALL(*mHal, registerCallback(_));
    EXPECT_CALL(*mHal, isVsyncPeriodSwitchSupported()).WillOnce(Return(false));

    impl::HWComposer hwc{std::unique_ptr<Hwc2::Composer>(mHal)};
    hwc.setConfiguration(&mCallback, 123);

    const auto& supported = hwc.getSupportedLayerGenericMetadata();
    EXPECT_EQ(2u, supported.size());
    EXPECT_EQ(1u, supported.count(kMetadata1Name));
    EXPECT_EQ(kMetadata1Mandatory, supported.find(kMetadata1Name)->second);
    EXPECT_EQ(1u, supported.count(kMetadata2Name));
    EXPECT_EQ(kMetadata2Mandatory, supported.find(kMetadata2Name)->second);
}

TEST_F(HWComposerSetConfigurationTest, handlesUnsupportedCallToGetLayerGenericMetadataKeys) {
    EXPECT_CALL(*mHal, getMaxVirtualDisplayCount()).WillOnce(Return(0));
    EXPECT_CALL(*mHal, getCapabilities()).WillOnce(Return(std::vector<hal::Capability>{}));
    EXPECT_CALL(*mHal, getLayerGenericMetadataKeys(_))
            .WillOnce(Return(hardware::graphics::composer::V2_4::Error::UNSUPPORTED));
    EXPECT_CALL(*mHal, registerCallback(_));
    EXPECT_CALL(*mHal, isVsyncPeriodSwitchSupported()).WillOnce(Return(false));

    impl::HWComposer hwc{std::unique_ptr<Hwc2::Composer>(mHal)};
    hwc.setConfiguration(&mCallback, 123);

    const auto& supported = hwc.getSupportedLayerGenericMetadata();
    EXPECT_EQ(0u, supported.size());
}

struct HWComposerLayerTest : public testing::Test {
    static constexpr hal::HWDisplayId kDisplayId = static_cast<hal::HWDisplayId>(1001);
    static constexpr hal::HWLayerId kLayerId = static_cast<hal::HWLayerId>(1002);

    HWComposerLayerTest(const std::unordered_set<hal::Capability>& capabilities)
          : mCapabilies(capabilities) {}

    ~HWComposerLayerTest() override { EXPECT_CALL(*mHal, destroyLayer(kDisplayId, kLayerId)); }

    std::unique_ptr<Hwc2::mock::Composer> mHal{new StrictMock<Hwc2::mock::Composer>()};
    const std::unordered_set<hal::Capability> mCapabilies;
    HWC2::impl::Layer mLayer{*mHal, mCapabilies, kDisplayId, kLayerId};
};

struct HWComposerLayerGenericMetadataTest : public HWComposerLayerTest {
    static const std::string kLayerGenericMetadata1Name;
    static constexpr bool kLayerGenericMetadata1Mandatory = false;
    static const std::vector<uint8_t> kLayerGenericMetadata1Value;
    static const std::string kLayerGenericMetadata2Name;
    static constexpr bool kLayerGenericMetadata2Mandatory = true;
    static const std::vector<uint8_t> kLayerGenericMetadata2Value;

    HWComposerLayerGenericMetadataTest() : HWComposerLayerTest({}) {}
};

const std::string HWComposerLayerGenericMetadataTest::kLayerGenericMetadata1Name =
        "com.example.metadata.1";

const std::vector<uint8_t> HWComposerLayerGenericMetadataTest::kLayerGenericMetadata1Value = {1u,
                                                                                              2u,
                                                                                              3u};

const std::string HWComposerLayerGenericMetadataTest::kLayerGenericMetadata2Name =
        "com.example.metadata.2";

const std::vector<uint8_t> HWComposerLayerGenericMetadataTest::kLayerGenericMetadata2Value = {45u,
                                                                                              67u};

TEST_F(HWComposerLayerGenericMetadataTest, forwardsSupportedMetadata) {
    EXPECT_CALL(*mHal,
                setLayerGenericMetadata(kDisplayId, kLayerId, kLayerGenericMetadata1Name,
                                        kLayerGenericMetadata1Mandatory,
                                        kLayerGenericMetadata1Value))
            .WillOnce(Return(hardware::graphics::composer::V2_4::Error::NONE));
    auto result = mLayer.setLayerGenericMetadata(kLayerGenericMetadata1Name,
                                                 kLayerGenericMetadata1Mandatory,
                                                 kLayerGenericMetadata1Value);
    EXPECT_EQ(hal::Error::NONE, result);

    EXPECT_CALL(*mHal,
                setLayerGenericMetadata(kDisplayId, kLayerId, kLayerGenericMetadata2Name,
                                        kLayerGenericMetadata2Mandatory,
                                        kLayerGenericMetadata2Value))
            .WillOnce(Return(hardware::graphics::composer::V2_4::Error::UNSUPPORTED));
    result = mLayer.setLayerGenericMetadata(kLayerGenericMetadata2Name,
                                            kLayerGenericMetadata2Mandatory,
                                            kLayerGenericMetadata2Value);
    EXPECT_EQ(hal::Error::UNSUPPORTED, result);
}

} // namespace
} // namespace android
