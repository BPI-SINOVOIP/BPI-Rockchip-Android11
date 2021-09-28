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

#pragma once

#include <android/hardware/automotive/evs/1.1/IEvsCamera.h>
#include <android/hardware/automotive/evs/1.1/IEvsDisplay.h>
#include <android/hardware/automotive/evs/1.1/IEvsEnumerator.h>
#include <android/hardware/camera/device/3.2/ICameraDevice.h>
#include <system/camera_metadata.h>

#include <ConfigManager.h>

using namespace ::android::hardware::automotive::evs::V1_1;
using ::android::hardware::camera::device::V3_2::Stream;

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

using EvsDisplayState = ::android::hardware::automotive::evs::V1_0::DisplayState;
using IEvsCamera_1_0 = ::android::hardware::automotive::evs::V1_0::IEvsCamera;
using IEvsCamera_1_1 = ::android::hardware::automotive::evs::V1_1::IEvsCamera;
using IEvsDisplay_1_0 = ::android::hardware::automotive::evs::V1_0::IEvsDisplay;
using IEvsDisplay_1_1 = ::android::hardware::automotive::evs::V1_1::IEvsDisplay;
using IEvsEnumerator_1_1 = ::android::hardware::automotive::evs::V1_1::IEvsEnumerator;

class MockEvsEnumerator : public IEvsEnumerator_1_1 {
public:
    MockEvsEnumerator();

    // Methods from ::android::hardware::automotive::evs::V1_0::IEvsEnumerator follow.
    Return<void> getCameraList(getCameraList_cb _hidl_cb) override;
    Return<sp<IEvsCamera_1_0>> openCamera(const hidl_string& cameraId) override;
    Return<void> closeCamera(const ::android::sp<IEvsCamera_1_0>& virtualCamera) override;
    Return<sp<IEvsDisplay_1_0>> openDisplay() override;
    Return<void> closeDisplay(const ::android::sp<IEvsDisplay_1_0>& display) override;
    Return<EvsDisplayState> getDisplayState() override;

    // Methods from ::android::hardware::automotive::evs::V1_1::IEvsEnumerator follow.
    Return<void> getCameraList_1_1(getCameraList_1_1_cb _hidl_cb) override;
    Return<sp<IEvsCamera_1_1>> openCamera_1_1(const hidl_string& cameraId,
                                              const Stream& streamCfg) override;
    Return<bool> isHardware() override { return false; }
    Return<void> getDisplayIdList(getDisplayIdList_cb _list_cb) override;
    Return<sp<IEvsDisplay_1_1>> openDisplay_1_1(uint8_t id) override;
    Return<void> getUltrasonicsArrayList(getUltrasonicsArrayList_cb _hidl_cb) override;
    Return<sp<IEvsUltrasonicsArray>> openUltrasonicsArray(
            const hidl_string& ultrasonicsArrayId) override;
    Return<void> closeUltrasonicsArray(
            const ::android::sp<IEvsUltrasonicsArray>& evsUltrasonicsArray) override;

    // Methods from ::android.hidl.base::V1_0::IBase follow.
    Return<void> debug(const hidl_handle& fd, const hidl_vec<hidl_string>& options) override;

private:
    std::unique_ptr<ConfigManager> mConfigManager;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
