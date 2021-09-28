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

#include "MockSurroundViewCallback.h"

#include <android-base/logging.h>

#include <thread>

using ::android::sp;
using ::android::hardware::Return;

using ::std::thread;

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

MockSurroundViewCallback::MockSurroundViewCallback(
        sp<ISurroundViewSession> pSession) :
        mSession(pSession) {}

Return<void> MockSurroundViewCallback::notify(SvEvent svEvent) {
    LOG(INFO) << __FUNCTION__ << "SvEvent received: " << (int)svEvent;
    return {};
}

Return<void> MockSurroundViewCallback::receiveFrames(
        const SvFramesDesc& svFramesDesc) {
    LOG(INFO) << __FUNCTION__ << svFramesDesc.svBuffers.size()
              << " frames are received";

    // Increment the count of received frames.
    {
        std::scoped_lock<std::mutex> lock(mAccessLock);
        mReceivedFramesCount++;
    }

    // Create a separate thread to return the frames to the session. This
    // simulates the behavior of oneway HIDL method call.
    thread mockHidlThread([this, &svFramesDesc]() {
        mSession->doneWithFrames(svFramesDesc);
    });
    mockHidlThread.detach();
    return {};
}

int MockSurroundViewCallback::getReceivedFramesCount() {
    {
        std::scoped_lock<std::mutex> lock(mAccessLock);
        return mReceivedFramesCount;
    }
}

void MockSurroundViewCallback::clearReceivedFramesCount() {
    {
        std::scoped_lock<std::mutex> lock(mAccessLock);
        mReceivedFramesCount = 0;
    }
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
