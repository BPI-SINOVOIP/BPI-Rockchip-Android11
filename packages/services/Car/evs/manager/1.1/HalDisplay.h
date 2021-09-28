/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef ANDROID_AUTOMOTIVE_EVS_V1_1_DISPLAYPROXY_H
#define ANDROID_AUTOMOTIVE_EVS_V1_1_DISPLAYPROXY_H

#include <limits>

#include <android/hardware/automotive/evs/1.1/types.h>
#include <android/hardware/automotive/evs/1.1/IEvsDisplay.h>

using namespace ::android::hardware::automotive::evs::V1_1;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_handle;
using ::android::hardware::automotive::evs::V1_0::EvsResult;
using BufferDesc_1_0 = ::android::hardware::automotive::evs::V1_0::BufferDesc;
using EvsDisplayState = ::android::hardware::automotive::evs::V1_0::DisplayState;
using IEvsDisplay_1_0 = ::android::hardware::automotive::evs::V1_0::IEvsDisplay;
using IEvsDisplay_1_1 = ::android::hardware::automotive::evs::V1_1::IEvsDisplay;

namespace android {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {

// TODO(129284474): This class has been defined to wrap the IEvsDisplay object the driver
// returns because of b/129284474 and represents an EVS display to the client
// application.  With a proper bug fix, we may remove this class and update the
// manager directly to use the IEvsDisplay object the driver provides.
class HalDisplay : public IEvsDisplay_1_1 {
public:
    explicit HalDisplay(sp<IEvsDisplay_1_0> display,
                        int32_t port = std::numeric_limits<int32_t>::min());
    virtual ~HalDisplay() override;

    inline void         shutdown();
    sp<IEvsDisplay_1_0> getHwDisplay();

    // Methods from ::android::hardware::automotive::evs::V1_0::IEvsDisplay follow.
    Return<void>            getDisplayInfo(getDisplayInfo_cb _hidl_cb)  override;
    Return<EvsResult>       setDisplayState(EvsDisplayState state)  override;
    Return<EvsDisplayState> getDisplayState()  override;
    Return<void>            getTargetBuffer(getTargetBuffer_cb _hidl_cb)  override;
    Return<EvsResult>       returnTargetBufferForDisplay(const BufferDesc_1_0& buffer)  override;

    // Methods from ::android::hardware::automotive::evs::V1_1::IEvsDisplay follow.
    Return<void>            getDisplayInfo_1_1(getDisplayInfo_1_1_cb _info_cb) override;

    // Returns a string showing the current status
    std::string toString(const char* indent = "");

private:
    sp<IEvsDisplay_1_0>     mHwDisplay; // The low level display interface that backs this proxy
    int32_t                 mId; // Display identifier
};

} // namespace implementation
} // namespace V1_1
} // namespace evs
} // namespace automotive
} // namespace android

#endif  // ANDROID_AUTOMOTIVE_EVS_V1_1_DISPLAYPROXY_H
