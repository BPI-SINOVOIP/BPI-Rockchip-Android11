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

#define LOG_TAG "carwatchdogd_ioperf_test"

#include "LooperStub.h"

#include <android-base/chrono_utils.h>
#include <utils/Looper.h>

#include <future>

namespace android {
namespace automotive {
namespace watchdog {
namespace testing {

using android::base::Error;
using android::base::Result;

// As the messages, which are to be polled immediately, are enqueued in the underlying looper
// handler before calling its poll method, the looper handler doesn't have to wait for any new
// messages.
const std::chrono::milliseconds kLooperPollTimeout = 0ms;

// Maximum timeout before giving up on the underlying looper handler. This doesn't block the test
// as long as the underlying looper handler processes the enqueued messages quickly and updates
// |mShouldPoll|.
const std::chrono::milliseconds kStubPollCheckTimeout = 3min;

int LooperStub::pollAll(int /*timeoutMillis*/) {
    {
        Mutex::Autolock lock(mMutex);
        if (!mShouldPoll) {
            return 0;
        }
        mElapsedTime = mTimer;
        while (!mCache.empty() && mCache.front().empty()) {
            mTimer += 1s;  // Each empty entry in the cache is a second elapsed.
            mCache.erase(mCache.begin());
        }
        mElapsedTime = mTimer - mElapsedTime;
        if (mCache.empty()) {
            mShouldPoll = false;
            return 0;
        }
        // Send messages from the top of the cache and poll them immediately.
        const auto messages = mCache.front();
        for (const auto& m : messages) {
            mLooper->sendMessage(mHandler, m);
        }
        mCache.erase(mCache.begin());
    }
    int result = mLooper->pollAll(kLooperPollTimeout.count());
    Mutex::Autolock lock(mMutex);
    mShouldPoll = false;
    return result;
}

void LooperStub::sendMessage(const android::sp<MessageHandler>& handler, const Message& message) {
    sendMessageAtTime(now(), handler, message);
}

void LooperStub::sendMessageAtTime(nsecs_t uptime, const android::sp<MessageHandler>& handler,
                                   const Message& message) {
    Mutex::Autolock lock(mMutex);
    mHandler = handler;
    nsecs_t uptimeDelay = uptime - now();
    size_t pos = static_cast<size_t>(ns2s(uptimeDelay));
    while (mCache.size() < pos + 1) {
        mCache.emplace_back(LooperStub::CacheEntry());
    }
    mCache[pos].emplace_back(message);
}

void LooperStub::removeMessages(const android::sp<MessageHandler>& handler) {
    Mutex::Autolock lock(mMutex);
    mCache.clear();
    mLooper->removeMessages(handler);
}

Result<void> LooperStub::pollCache() {
    {
        Mutex::Autolock lock(mMutex);
        mShouldPoll = true;
    }
    auto checkPollCompleted = std::async([&]() {
        bool shouldPoll = true;
        while (shouldPoll) {
            Mutex::Autolock lock(mMutex);
            shouldPoll = mShouldPoll;
        }
    });
    if (checkPollCompleted.wait_for(kStubPollCheckTimeout) != std::future_status::ready) {
        mShouldPoll = false;
        return Error() << "Poll didn't complete before " << kStubPollCheckTimeout.count()
                       << " milliseconds";
    }
    return {};
}

}  // namespace testing
}  // namespace watchdog
}  // namespace automotive
}  // namespace android
