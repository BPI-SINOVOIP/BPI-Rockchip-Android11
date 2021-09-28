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

#ifndef WATCHDOG_SERVER_SRC_WATCHDOGBINDERMEDIATOR_H_
#define WATCHDOG_SERVER_SRC_WATCHDOGBINDERMEDIATOR_H_

#include <android-base/result.h>
#include <android/automotive/watchdog/BnCarWatchdog.h>
#include <android/automotive/watchdog/StateType.h>
#include <binder/IBinder.h>
#include <binder/Status.h>
#include <gtest/gtest_prod.h>
#include <utils/Errors.h>
#include <utils/String16.h>
#include <utils/StrongPointer.h>
#include <utils/Vector.h>

#include "IoPerfCollection.h"
#include "WatchdogProcessService.h"

namespace android {
namespace automotive {
namespace watchdog {

class ServiceManager;

// WatchdogBinderMediator implements the carwatchdog binder APIs such that it forwards the calls
// either to process ANR service or I/O performance data collection.
class WatchdogBinderMediator : public BnCarWatchdog, public IBinder::DeathRecipient {
public:
    WatchdogBinderMediator() : mWatchdogProcessService(nullptr), mIoPerfCollection(nullptr) {}

    status_t dump(int fd, const Vector<String16>& args) override;
    binder::Status registerClient(const sp<ICarWatchdogClient>& client,
                                  TimeoutLength timeout) override {
        return mWatchdogProcessService->registerClient(client, timeout);
    }
    binder::Status unregisterClient(const sp<ICarWatchdogClient>& client) override {
        return mWatchdogProcessService->unregisterClient(client);
    }
    binder::Status registerMediator(const sp<ICarWatchdogClient>& mediator) override;
    binder::Status unregisterMediator(const sp<ICarWatchdogClient>& mediator) override;
    binder::Status registerMonitor(const sp<ICarWatchdogMonitor>& monitor) override;
    binder::Status unregisterMonitor(const sp<ICarWatchdogMonitor>& monitor) override;
    binder::Status tellClientAlive(const sp<ICarWatchdogClient>& client,
                                   int32_t sessionId) override {
        return mWatchdogProcessService->tellClientAlive(client, sessionId);
    }
    binder::Status tellMediatorAlive(const sp<ICarWatchdogClient>& mediator,
                                     const std::vector<int32_t>& clientsNotResponding,
                                     int32_t sessionId) override;
    binder::Status tellDumpFinished(const android::sp<ICarWatchdogMonitor>& monitor,
                                    int32_t pid) override;
    binder::Status notifySystemStateChange(StateType type, int32_t arg1, int32_t arg2) override;

protected:
    android::base::Result<void> init(android::sp<WatchdogProcessService> watchdogProcessService,
                                     android::sp<IoPerfCollection> ioPerfCollection);
    void terminate() {
        mWatchdogProcessService = nullptr;
        mIoPerfCollection = nullptr;
    }

private:
    void binderDied(const android::wp<IBinder>& who) override {
        return mWatchdogProcessService->binderDied(who);
    }
    bool dumpHelpText(int fd, std::string errorMsg);

    android::sp<WatchdogProcessService> mWatchdogProcessService;
    android::sp<IoPerfCollection> mIoPerfCollection;

    friend class ServiceManager;
    friend class WatchdogBinderMediatorTest;
    FRIEND_TEST(WatchdogBinderMediatorTest, TestErrorOnNullptrDuringInit);
    FRIEND_TEST(WatchdogBinderMediatorTest, TestHandlesEmptyDumpArgs);
};

}  // namespace watchdog
}  // namespace automotive
}  // namespace android

#endif  // WATCHDOG_SERVER_SRC_WATCHDOGBINDERMEDIATOR_H_
