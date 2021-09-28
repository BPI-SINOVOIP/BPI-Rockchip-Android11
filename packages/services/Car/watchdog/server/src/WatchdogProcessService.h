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

#ifndef WATCHDOG_SERVER_SRC_WATCHDOGPROCESSSERVICE_H_
#define WATCHDOG_SERVER_SRC_WATCHDOGPROCESSSERVICE_H_

#include <android-base/result.h>
#include <android/automotive/watchdog/BnCarWatchdog.h>
#include <android/automotive/watchdog/PowerCycle.h>
#include <android/automotive/watchdog/UserState.h>
#include <binder/IBinder.h>
#include <binder/Status.h>
#include <cutils/multiuser.h>
#include <utils/Looper.h>
#include <utils/Mutex.h>
#include <utils/String16.h>
#include <utils/StrongPointer.h>
#include <utils/Vector.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace android {
namespace automotive {
namespace watchdog {

class WatchdogProcessService : public IBinder::DeathRecipient {
public:
    explicit WatchdogProcessService(const android::sp<Looper>& handlerLooper);

    virtual android::base::Result<void> dump(int fd, const Vector<String16>& args);

    virtual binder::Status registerClient(const sp<ICarWatchdogClient>& client,
                                          TimeoutLength timeout);
    virtual binder::Status unregisterClient(const sp<ICarWatchdogClient>& client);
    virtual binder::Status registerMediator(const sp<ICarWatchdogClient>& mediator);
    virtual binder::Status unregisterMediator(const sp<ICarWatchdogClient>& mediator);
    virtual binder::Status registerMonitor(const sp<ICarWatchdogMonitor>& monitor);
    virtual binder::Status unregisterMonitor(const sp<ICarWatchdogMonitor>& monitor);
    virtual binder::Status tellClientAlive(const sp<ICarWatchdogClient>& client, int32_t sessionId);
    virtual binder::Status tellMediatorAlive(const sp<ICarWatchdogClient>& mediator,
                                             const std::vector<int32_t>& clientsNotResponding,
                                             int32_t sessionId);
    virtual binder::Status tellDumpFinished(const android::sp<ICarWatchdogMonitor>& monitor,
                                            int32_t pid);
    virtual binder::Status notifyPowerCycleChange(PowerCycle cycle);
    virtual binder::Status notifyUserStateChange(userid_t userId, UserState state);
    virtual void binderDied(const android::wp<IBinder>& who);

    void doHealthCheck(int what);
    void terminate();

private:
    enum ClientType {
        Regular,
        Mediator,
    };

    struct ClientInfo {
        ClientInfo(const android::sp<ICarWatchdogClient>& client, pid_t pid, userid_t userId,
                   ClientType type) :
              client(client),
              pid(pid),
              userId(userId),
              type(type) {}
        std::string toString();

        android::sp<ICarWatchdogClient> client;
        pid_t pid;
        userid_t userId;
        int sessionId;
        ClientType type;
    };

    typedef std::unordered_map<int, ClientInfo> PingedClientMap;

    class MessageHandlerImpl : public MessageHandler {
    public:
        explicit MessageHandlerImpl(const android::sp<WatchdogProcessService>& service);

        void handleMessage(const Message& message) override;

    private:
        android::sp<WatchdogProcessService> mService;
    };

private:
    binder::Status registerClientLocked(const android::sp<ICarWatchdogClient>& client,
                                        TimeoutLength timeout, ClientType clientType);
    binder::Status unregisterClientLocked(const std::vector<TimeoutLength>& timeouts,
                                          android::sp<IBinder> binder, ClientType clientType);
    bool isRegisteredLocked(const android::sp<ICarWatchdogClient>& client);
    binder::Status tellClientAliveLocked(const android::sp<ICarWatchdogClient>& client,
                                         int32_t sessionId);
    base::Result<void> startHealthCheckingLocked(TimeoutLength timeout);
    base::Result<void> dumpAndKillClientsIfNotResponding(TimeoutLength timeout);
    base::Result<void> dumpAndKillAllProcesses(const std::vector<int32_t>& processesNotResponding);
    int32_t getNewSessionId();
    bool isWatchdogEnabled();

    using Processor =
            std::function<void(std::vector<ClientInfo>&, std::vector<ClientInfo>::const_iterator)>;
    bool findClientAndProcessLocked(const std::vector<TimeoutLength> timeouts,
                                    const android::sp<IBinder> binder, const Processor& processor);

private:
    sp<Looper> mHandlerLooper;
    android::sp<MessageHandlerImpl> mMessageHandler;
    Mutex mMutex;
    std::unordered_map<TimeoutLength, std::vector<ClientInfo>> mClients GUARDED_BY(mMutex);
    std::unordered_map<TimeoutLength, PingedClientMap> mPingedClients GUARDED_BY(mMutex);
    std::unordered_set<userid_t> mStoppedUserId GUARDED_BY(mMutex);
    android::sp<ICarWatchdogMonitor> mMonitor GUARDED_BY(mMutex);
    bool mWatchdogEnabled GUARDED_BY(mMutex);
    // mLastSessionId is accessed only within main thread. No need for mutual-exclusion.
    int32_t mLastSessionId;
};

}  // namespace watchdog
}  // namespace automotive
}  // namespace android

#endif  // WATCHDOG_SERVER_SRC_WATCHDOGPROCESSSERVICE_H_
