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

#ifndef EVS_MANAGER_1_1_TEST_FUZZER_MOCKHWCAMERA_H_
#define EVS_MANAGER_1_1_TEST_FUZZER_MOCKHWCAMERA_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace android {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {

class MockHWCamera : public IEvsCamera_1_1 {
public:
    MockHWCamera() = default;

    // v1.0 methods
    Return<void> getCameraInfo(getCameraInfo_cb _hidl_cb) override { return {}; }
    Return<EvsResult> setMaxFramesInFlight(uint32_t bufferCount) override {
        if (bufferCount > 1024) {
            return EvsResult::INVALID_ARG;
        }
        return EvsResult::OK;
    }
    Return<EvsResult> startVideoStream(const ::android::sp<IEvsCameraStream_1_0>& stream) override {
        return EvsResult::OK;
    }
    Return<void> doneWithFrame(const BufferDesc_1_0& buffer) override { return {}; }
    Return<void> stopVideoStream() override { return {}; }
    Return<int32_t> getExtendedInfo(uint32_t opaqueIdentifier) override {
        if (mExtendedInfo.find(opaqueIdentifier) != mExtendedInfo.end()) {
            return mExtendedInfo[opaqueIdentifier];
        }
        return 0;
    }
    Return<EvsResult> setExtendedInfo(uint32_t opaqueIdentifier, int32_t opaqueValue) override {
        mExtendedInfo[opaqueIdentifier] = opaqueValue;
        return EvsResult::OK;
    }

    // v1.1 methods
    Return<void> getCameraInfo_1_1(getCameraInfo_1_1_cb _hidl_cb) override { return {}; }
    MOCK_METHOD(Return<void>, getPhysicalCameraInfo,
                (const hidl_string& deviceId, getPhysicalCameraInfo_cb _hidl_cb), (override));
    MOCK_METHOD(Return<EvsResult>, pauseVideoStream, (), (override));
    MOCK_METHOD(Return<EvsResult>, resumeVideoStream, (), (override));
    Return<EvsResult> doneWithFrame_1_1(const hardware::hidl_vec<BufferDesc_1_1>& buffer) override {
        return EvsResult::OK;
    }
    MOCK_METHOD(Return<EvsResult>, setMaster, (), (override));
    MOCK_METHOD(Return<EvsResult>, forceMaster, (const sp<IEvsDisplay_1_0>& display), (override));
    MOCK_METHOD(Return<EvsResult>, unsetMaster, (), (override));
    Return<void> getParameterList(getParameterList_cb _hidl_cb) override { return {}; }
    Return<void> getIntParameterRange(CameraParam id, getIntParameterRange_cb _hidl_cb) override {
        return {};
    }
    Return<void> setIntParameter(CameraParam id, int32_t value,
                                 setIntParameter_cb _hidl_cb) override {
        // FIXME in default implementation, it's _hidl_cb(EvsResult::INVALID_ARG, 0); which will
        // always cause a segfault what's the predefined behavior of _hidl_cb?
        std::vector<int32_t> vec;
        vec.push_back(0);
        _hidl_cb(EvsResult::INVALID_ARG, vec);
        return {};
    }

    Return<void> getIntParameter(CameraParam id, getIntParameter_cb _hidl_cb) {
        _hidl_cb(EvsResult::INVALID_ARG, 0);
        return {};
    }

    Return<void> getExtendedInfo_1_1(uint32_t opaqueIdentifier,
                                     getExtendedInfo_1_1_cb _hidl_cb) override {
        return {};
    }
    Return<EvsResult> setExtendedInfo_1_1(uint32_t opaqueIdentifier,
                                          const hidl_vec<uint8_t>& opaqueValue) override {
        return EvsResult::OK;
    }
    Return<void> importExternalBuffers(const hidl_vec<BufferDesc_1_1>& buffers,
                                       importExternalBuffers_cb _hidl_cb) override {
        return {};
    }

private:
    std::map<uint32_t, int32_t> mExtendedInfo;
    std::map<uint32_t, int32_t> mExtendedInfo_1_1;
};

}  // namespace implementation
}  // namespace V1_1
}  // namespace evs
}  // namespace automotive
}  // namespace android

#endif  // EVS_MANAGER_1_1_TEST_FUZZER_MOCKHWCAMERA_H_
