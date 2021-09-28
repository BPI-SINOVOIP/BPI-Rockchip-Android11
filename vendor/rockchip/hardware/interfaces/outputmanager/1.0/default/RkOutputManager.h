/*
 * Copyright (C) 2016 The Android Open Source Project
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
#ifndef ROCKCHIP_HARDWARE_OUTPUTMANAGER_V1_0_RKCOMPOSER_H
#define ROCKCHIP_HARDWARE_OUTPUTMANAGER_V1_0_RKCOMPOSER_H

#include <rockchip/hardware/outputmanager/1.0/IRkOutputManager.h>
#include <hidl/Status.h>
#include <hardware/hwcomposer.h>

#include <hidl/MQDescriptor.h>
#include <hardware/hw_output.h>

namespace rockchip {
namespace hardware {
namespace outputmanager {
namespace V1_0 {
namespace implementation {

using namespace android;
using namespace rockchip::hardware::outputmanager::V1_0;

using ::rockchip::hardware::outputmanager::V1_0::IRkOutputManager;
using ::rockchip::hardware::outputmanager::V1_0::Result;
using ::rockchip::hardware::outputmanager::V1_0::RkDrmMode;

using android::hardware::hidl_handle;
using android::hardware::hidl_string;
using android::hardware::hidl_vec;
using android::hardware::Return;
using android::hardware::Void;
using ::android::sp;

struct RkOutputManager : public IRkOutputManager {
    RkOutputManager(hw_output_device* hw_module);
    ~RkOutputManager();

    Return<void> initial()  override;
    Return<Result> setMode(Display display, const hidl_string& mode)  override;
    Return<Result> setGamma(Display display, uint32_t size, const hidl_vec<uint16_t>& r, const hidl_vec<uint16_t>& g, const hidl_vec<uint16_t>& b) override;
    Return<Result> setBrightness(Display display, uint32_t value) override;
    Return<Result> setContrast(Display display, uint32_t value) override;
    Return<Result> setSaturation(Display display, uint32_t value) override;
    Return<Result> setHue(Display display, uint32_t value) override;
    Return<Result> setScreenScale(Display display, uint32_t direction, uint32_t value) override;
    Return<Result> setHdrMode(Display display, uint32_t hdrmode) override;
    Return<Result> setColorMode(Display display, const hidl_string& mode)  override;

    Return<void> getCurCorlorMode(Display display, getCurCorlorMode_cb hidl_cb) override;
    Return<void> getCurMode(Display display, getCurMode_cb _hidl_cb) override;
    Return<void> getNumConnectors(Display display, getNumConnectors_cb _hidl_cb) override;
    Return<void> getConnectState(Display display, getConnectState_cb _hidl_cb) override;
    Return<void> getBuiltIn(Display display, getBuiltIn_cb _hidl_cb) override;
    Return<void> getCorlorModeConfigs(Display display, getCorlorModeConfigs_cb _hidl_cb) override;
    Return<void> getOverscan(Display display, getOverscan_cb _hidl_cb) override;
    Return<void> getBcsh(Display display, getBcsh_cb _hidl_cb) override;
    Return<void> getDisplayModes(Display display, getDisplayModes_cb _hidl_cb) override;
    Return<void> saveConfig() override;
    Return<void> hotPlug() override;
    Return<Result> set3DMode(const hidl_string& mode)  override;
    Return<Result> set3DLut(Display display, uint32_t size, const hidl_vec<uint16_t>& r, const hidl_vec<uint16_t>& g, const hidl_vec<uint16_t>& b) override;
    Return<void> getConnectorInfo(getConnectorInfo_cb _hidl_cb) override;
    Return<Result> updateDispHeader() override;
private:
    hw_output_device* mHwOutput;
};

extern "C" IRkOutputManager* HIDL_FETCH_IRkOutputManager(const char* name);

}  // namespace implementation
}  // namespace V1_0
}  // namespace composer
}  // namespace hardware
}  // namespace rockchip

#endif  // ANDROID_HARDWARE_TV_INPUT_V1_0_TVINPUT_H
