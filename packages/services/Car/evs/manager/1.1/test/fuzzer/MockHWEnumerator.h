// Copyright 2020 The Android Open Source Project
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

#ifndef EVS_MANAGER_1_1_TEST_FUZZER_MOCKHWENUMERATOR_H_
#define EVS_MANAGER_1_1_TEST_FUZZER_MOCKHWENUMERATOR_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "MockHWDisplay.h"
#include "MockHWCamera.h"

using ::android::hardware::automotive::evs::V1_0::DisplayState;

namespace android {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {

class MockHWEnumerator : public IEvsEnumerator_1_1 {
public:
    MockHWEnumerator() {
        // four cameras
        for (uint64_t i = startMockHWCameraId; i < endMockHWCameraId; i++) {
            sp<MockHWCamera> camera = new MockHWCamera();
            mHWCameras[i] = camera;
        }

        // two displays
        for (uint8_t i = static_cast<uint8_t>(startMockHWDisplayId);
             i < static_cast<uint8_t>(endMockHWDisplayId); i++) {
            sp<MockHWDisplay> display = new MockHWDisplay();
            mHWDisplays[i] = display;
            mDisplayPortList[i] = i;
        }
    }

    // Methods from ::android::hardware::automotive::evs::V1_0::IEvsEnumerator follow.
    MOCK_METHOD(Return<void>, getCameraList, (getCameraList_cb _hidl_cb), (override));
    Return<sp<IEvsCamera_1_0>> openCamera(const hidl_string& cameraId) override {
        uint64_t idx = stoi(cameraId);
        if (mHWCameras.find(idx) != mHWCameras.end()) {
            return mHWCameras[idx];
        }
        return nullptr;
    }
    MOCK_METHOD(Return<void>, closeCamera, (const sp<IEvsCamera_1_0>& carCamera), (override));
    Return<sp<IEvsDisplay_1_0>> openDisplay() override {
        return mHWDisplays.begin()->second;
    }
    Return<void> closeDisplay(const sp<IEvsDisplay_1_0>& display) override {
        return {};
    }
    MOCK_METHOD(Return<DisplayState>, getDisplayState, (), (override));

    // Methods from ::android::hardware::automotive::evs::V1_1::IEvsEnumerator follow.
    Return<void> getCameraList_1_1(getCameraList_1_1_cb _hidl_cb) {
        return {};
    }
    Return<sp<IEvsCamera_1_1>> openCamera_1_1(const hidl_string& cameraId,
                                              const Stream& streamCfg) override {
        uint64_t idx = stoi(cameraId);
        if (mHWCameras.find(idx) != mHWCameras.end()) {
            return mHWCameras[idx];
        }
        return nullptr;
    }
    MOCK_METHOD(Return<bool>, isHardware, (), (override));
    Return<void> getDisplayIdList(getDisplayIdList_cb _list_cb) override {
        vector<uint8_t> ids;

        ids.resize(mDisplayPortList.size());
        unsigned i = 0;
        for (const auto& [port, id] : mDisplayPortList) {
            ids[i++] = port;
        }

        _list_cb(ids);
        return {};
    }
    Return<sp<IEvsDisplay_1_1>> openDisplay_1_1(uint8_t port) override {
        auto iter = mDisplayPortList.find(port);
        if (iter != mDisplayPortList.end()) {
            uint64_t id = iter->second;
            auto it = mHWDisplays.find(id);
            if (it != mHWDisplays.end()) {
                return it->second;
            }
            return nullptr;
        }
        return nullptr;
    }
    MOCK_METHOD(Return<void>, getUltrasonicsArrayList, (getUltrasonicsArrayList_cb _hidl_cb),
                (override));
    MOCK_METHOD(Return<sp<IEvsUltrasonicsArray>>, openUltrasonicsArray,
                (const hidl_string& ultrasonicsArrayId), (override));
    MOCK_METHOD(Return<void>, closeUltrasonicsArray,
                (const sp<IEvsUltrasonicsArray>& evsUltrasonicsArray), (override));

private:
    std::map<uint64_t, sp<MockHWDisplay>> mHWDisplays;
    std::map<uint64_t, sp<MockHWCamera>> mHWCameras;
    std::map<uint8_t, uint64_t> mDisplayPortList;
};

}  // namespace implementation
}  // namespace V1_1
}  // namespace evs
}  // namespace automotive
}  // namespace android

#endif  // EVS_MANAGER_1_1_TEST_FUZZER_MOCKHWENUMERATOR_H_
