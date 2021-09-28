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

#ifndef EVS_MANAGER_1_1_TEST_FUZZER_MOCKHWDISPLAY_H_
#define EVS_MANAGER_1_1_TEST_FUZZER_MOCKHWDISPLAY_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace android {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {

class MockHWDisplay : public IEvsDisplay_1_1 {
public:
    MockHWDisplay() = default;

    Return<void> getDisplayInfo(getDisplayInfo_cb _hidl_cb) override { return {}; }
    Return<EvsResult> setDisplayState(EvsDisplayState state) override { return EvsResult::OK; }
    Return<EvsDisplayState> getDisplayState() override { return EvsDisplayState::VISIBLE; }
    Return<void> getTargetBuffer(getTargetBuffer_cb _hidl_cb) override { return {}; }
    Return<EvsResult> returnTargetBufferForDisplay(const BufferDesc_1_0& buffer) override {
        return EvsResult::OK;
    }
    Return<void> getDisplayInfo_1_1(getDisplayInfo_1_1_cb _info_cb) override {
        return {};
    }
};

}  // namespace implementation
}  // namespace V1_1
}  // namespace evs
}  // namespace automotive
}  // namespace android

#endif  // EVS_MANAGER_1_1_TEST_FUZZER_MOCKHWDISPLAY_H_
