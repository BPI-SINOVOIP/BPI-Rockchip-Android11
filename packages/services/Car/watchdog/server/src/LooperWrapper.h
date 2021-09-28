/**
 * Copyright (c) 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WATCHDOG_SERVER_SRC_LOOPERWRAPPER_H_
#define WATCHDOG_SERVER_SRC_LOOPERWRAPPER_H_

#include <utils/Looper.h>
#include <utils/RefBase.h>
#include <utils/Timers.h>

namespace android {
namespace automotive {
namespace watchdog {

// LooperWrapper is a wrapper around the actual looper implementation so tests can stub this wrapper
// to deterministically poll the underlying looper.
// Refer to utils/Looper.h for the class and methods descriptions.
class LooperWrapper : public RefBase {
public:
    LooperWrapper() : mLooper(nullptr) {}
    virtual ~LooperWrapper() {}

    void setLooper(sp<Looper> looper) { mLooper = looper; }
    void wake();
    virtual nsecs_t now() { return systemTime(SYSTEM_TIME_MONOTONIC); }
    virtual int pollAll(int timeoutMillis);
    virtual void sendMessage(const android::sp<MessageHandler>& handler, const Message& message);
    virtual void sendMessageAtTime(nsecs_t uptime, const android::sp<MessageHandler>& handler,
                                   const Message& message);
    virtual void removeMessages(const android::sp<MessageHandler>& handler);

protected:
    android::sp<Looper> mLooper;
};

}  // namespace watchdog
}  // namespace automotive
}  // namespace android

#endif  //  WATCHDOG_SERVER_SRC_LOOPERWRAPPER_H_
