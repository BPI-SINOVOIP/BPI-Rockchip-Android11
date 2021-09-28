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

#include <android/hardware/automotive/sv/1.0/ISurroundViewService.h>
#include <android/hardware/automotive/sv/1.0/ISurroundViewStream.h>

#include <ui/GraphicBuffer.h>

#include <mutex>
#include <thread>

using namespace android::hardware::automotive::sv::V1_0;

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

class MockSurroundViewCallback : public ISurroundViewStream {
public:
    MockSurroundViewCallback(android::sp<ISurroundViewSession> pSession);

    // Methods from ::android::hardware::automotive::sv::V1_0::ISurroundViewStream.
    android::hardware::Return<void> notify(SvEvent svEvent) override;
    android::hardware::Return<void> receiveFrames(const SvFramesDesc& svFramesDesc) override;

    // Methods to get and clear the mReceivedFramesCount.
    int getReceivedFramesCount();
    void clearReceivedFramesCount();
private:
    std::mutex mAccessLock;
    android::sp<ISurroundViewSession> mSession;

    // Keeps a count of the number of calls made to receiveFrames().
    int mReceivedFramesCount GUARDED_BY(mAccessLock) = 0;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
