/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef WATCHDOG_TESTCLIENT_SRC_WATCHDOGCLIENT_H_
#define WATCHDOG_TESTCLIENT_SRC_WATCHDOGCLIENT_H_

#include <aidl/android/automotive/watchdog/BnCarWatchdog.h>
#include <aidl/android/automotive/watchdog/BnCarWatchdogClient.h>
#include <utils/Looper.h>
#include <utils/Mutex.h>
#include <utils/String16.h>

namespace aidl {
namespace android {
namespace automotive {
namespace watchdog {

struct CommandParam {
    std::string timeout;
    int inactiveAfterInSec;
    int terminateAfterInSec;
    bool forcedKill;
    bool verbose;
};

struct HealthCheckSession {
    HealthCheckSession(int32_t sessionId = -1,
                       TimeoutLength sessionTimeout = TimeoutLength::TIMEOUT_NORMAL) :
          id(sessionId),
          timeout(sessionTimeout) {}

    int32_t id;
    TimeoutLength timeout;
};

class WatchdogClient : public BnCarWatchdogClient {
public:
    explicit WatchdogClient(const ::android::sp<::android::Looper>& handlerLooper);

    ndk::ScopedAStatus checkIfAlive(int32_t sessionId, TimeoutLength timeout) override;
    ndk::ScopedAStatus prepareProcessTermination() override;

    bool initialize(const CommandParam& param);
    void finalize();

private:
    class MessageHandlerImpl : public ::android::MessageHandler {
    public:
        explicit MessageHandlerImpl(WatchdogClient* client);
        void handleMessage(const ::android::Message& message) override;

    private:
        WatchdogClient* mClient;
    };

private:
    void respondToWatchdog();
    void becomeInactive();
    void terminateProcess();
    void registerClient(const std::string& timeout);
    void unregisterClient();

private:
    ::android::sp<::android::Looper> mHandlerLooper;
    ::android::sp<MessageHandlerImpl> mMessageHandler;
    bool mForcedKill;
    bool mVerbose;
    ::android::Mutex mMutex;
    std::shared_ptr<ICarWatchdog> mWatchdogServer GUARDED_BY(mMutex);
    std::shared_ptr<ICarWatchdogClient> mTestClient GUARDED_BY(mMutex);
    bool mIsClientActive GUARDED_BY(mMutex);
    HealthCheckSession mSession GUARDED_BY(mMutex);
};

}  // namespace watchdog
}  // namespace automotive
}  // namespace android
}  // namespace aidl

#endif  // WATCHDOG_TESTCLIENT_SRC_WATCHDOGCLIENT_H_
