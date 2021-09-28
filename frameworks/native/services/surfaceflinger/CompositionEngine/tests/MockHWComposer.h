/*
 * Copyright 2018 The Android Open Source Project
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

#pragma once

#include <compositionengine/Output.h>
#include <gmock/gmock.h>

// TODO(b/129481165): remove the #pragma below and fix conversion issues
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"

#include "DisplayHardware/HWComposer.h"

// TODO(b/129481165): remove the #pragma below and fix conversion issues
#pragma clang diagnostic pop // ignored "-Wconversion"

namespace android {
namespace mock {

namespace hal = android::hardware::graphics::composer::hal;

class HWComposer : public android::HWComposer {
public:
    HWComposer();
    ~HWComposer() override;

    MOCK_METHOD2(setConfiguration, void(HWC2::ComposerCallback*, int32_t));
    MOCK_CONST_METHOD3(getDisplayIdentificationData,
                       bool(hal::HWDisplayId, uint8_t*, DisplayIdentificationData*));
    MOCK_CONST_METHOD1(hasCapability, bool(hal::Capability));
    MOCK_CONST_METHOD2(hasDisplayCapability, bool(DisplayId, hal::DisplayCapability));

    MOCK_METHOD3(allocateVirtualDisplay,
                 std::optional<DisplayId>(uint32_t, uint32_t, ui::PixelFormat*));
    MOCK_METHOD2(allocatePhysicalDisplay, void(hal::HWDisplayId, DisplayId));
    MOCK_METHOD1(createLayer, HWC2::Layer*(DisplayId));
    MOCK_METHOD2(destroyLayer, void(DisplayId, HWC2::Layer*));
    MOCK_METHOD3(getDeviceCompositionChanges,
                 status_t(DisplayId, bool,
                          std::optional<android::HWComposer::DeviceRequestedChanges>*));
    MOCK_METHOD5(setClientTarget,
                 status_t(DisplayId, uint32_t, const sp<Fence>&, const sp<GraphicBuffer>&,
                          ui::Dataspace));
    MOCK_METHOD1(presentAndGetReleaseFences, status_t(DisplayId));
    MOCK_METHOD2(setPowerMode, status_t(DisplayId, hal::PowerMode));
    MOCK_METHOD2(setActiveConfig, status_t(DisplayId, size_t));
    MOCK_METHOD2(setColorTransform, status_t(DisplayId, const mat4&));
    MOCK_METHOD1(disconnectDisplay, void(DisplayId));
    MOCK_CONST_METHOD1(hasDeviceComposition, bool(const std::optional<DisplayId>&));
    MOCK_CONST_METHOD1(getPresentFence, sp<Fence>(DisplayId));
    MOCK_CONST_METHOD2(getLayerReleaseFence, sp<Fence>(DisplayId, HWC2::Layer*));
    MOCK_METHOD3(setOutputBuffer, status_t(DisplayId, const sp<Fence>&, const sp<GraphicBuffer>&));
    MOCK_METHOD1(clearReleaseFences, void(DisplayId));
    MOCK_METHOD2(getHdrCapabilities, status_t(DisplayId, HdrCapabilities*));
    MOCK_CONST_METHOD1(getSupportedPerFrameMetadata, int32_t(DisplayId));
    MOCK_CONST_METHOD2(getRenderIntents, std::vector<ui::RenderIntent>(DisplayId, ui::ColorMode));
    MOCK_METHOD2(getDataspaceSaturationMatrix, mat4(DisplayId, ui::Dataspace));
    MOCK_METHOD4(getDisplayedContentSamplingAttributes,
                 status_t(DisplayId, ui::PixelFormat*, ui::Dataspace*, uint8_t*));
    MOCK_METHOD4(setDisplayContentSamplingEnabled, status_t(DisplayId, bool, uint8_t, uint64_t));
    MOCK_METHOD4(getDisplayedContentSample,
                 status_t(DisplayId, uint64_t, uint64_t, DisplayedFrameStats*));
    MOCK_METHOD2(setDisplayBrightness, std::future<status_t>(DisplayId, float));
    MOCK_METHOD2(getDisplayBrightnessSupport, status_t(DisplayId, bool*));

    MOCK_METHOD2(onHotplug,
                 std::optional<DisplayIdentificationInfo>(hal::HWDisplayId, hal::Connection));
    MOCK_METHOD2(onVsync, bool(hal::HWDisplayId, int64_t));
    MOCK_METHOD2(setVsyncEnabled, void(DisplayId, hal::Vsync));
    MOCK_CONST_METHOD1(getRefreshTimestamp, nsecs_t(DisplayId));
    MOCK_CONST_METHOD1(isConnected, bool(DisplayId));
    MOCK_CONST_METHOD1(getConfigs,
                       std::vector<std::shared_ptr<const HWC2::Display::Config>>(DisplayId));
    MOCK_CONST_METHOD1(getActiveConfig, std::shared_ptr<const HWC2::Display::Config>(DisplayId));
    MOCK_CONST_METHOD1(getActiveConfigIndex, int(DisplayId));
    MOCK_CONST_METHOD1(getColorModes, std::vector<ui::ColorMode>(DisplayId));
    MOCK_METHOD3(setActiveColorMode, status_t(DisplayId, ui::ColorMode, ui::RenderIntent));
    MOCK_CONST_METHOD0(isUsingVrComposer, bool());
    MOCK_CONST_METHOD1(getDisplayConnectionType, DisplayConnectionType(DisplayId));
    MOCK_CONST_METHOD1(isVsyncPeriodSwitchSupported, bool(DisplayId));
    MOCK_CONST_METHOD1(getDisplayVsyncPeriod, nsecs_t(DisplayId));
    MOCK_METHOD4(setActiveConfigWithConstraints,
                 status_t(DisplayId, size_t, const hal::VsyncPeriodChangeConstraints&,
                          hal::VsyncPeriodChangeTimeline*));
    MOCK_METHOD2(setAutoLowLatencyMode, status_t(DisplayId, bool));
    MOCK_METHOD2(getSupportedContentTypes, status_t(DisplayId, std::vector<hal::ContentType>*));
    MOCK_METHOD2(setContentType, status_t(DisplayId, hal::ContentType));
    MOCK_CONST_METHOD0(getSupportedLayerGenericMetadata,
                       const std::unordered_map<std::string, bool>&());

    MOCK_CONST_METHOD1(dump, void(std::string&));
    MOCK_CONST_METHOD0(getComposer, android::Hwc2::Composer*());
    MOCK_CONST_METHOD1(getHwcDisplayId, std::optional<hal::HWDisplayId>(int32_t));
    MOCK_CONST_METHOD0(getInternalHwcDisplayId, std::optional<hal::HWDisplayId>());
    MOCK_CONST_METHOD0(getExternalHwcDisplayId, std::optional<hal::HWDisplayId>());
    MOCK_CONST_METHOD1(toPhysicalDisplayId, std::optional<DisplayId>(hal::HWDisplayId));
    MOCK_CONST_METHOD1(fromPhysicalDisplayId, std::optional<hal::HWDisplayId>(DisplayId));
};

} // namespace mock
} // namespace android
