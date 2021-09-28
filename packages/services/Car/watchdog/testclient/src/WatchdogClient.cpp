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

#define LOG_TAG "carwatchdog_testclient"

#include "WatchdogClient.h"

#include <android-base/file.h>
#include <android/binder_manager.h>

#include <unordered_map>

namespace aidl {
namespace android {
namespace automotive {
namespace watchdog {

using ::android::Looper;
using ::android::Message;
using ::android::Mutex;
using ::android::sp;

namespace {

enum { WHAT_CHECK_ALIVE = 1, WHAT_BECOME_INACTIVE = 2, WHAT_TERMINATE = 3 };

const std::unordered_map<std::string, TimeoutLength> kTimeoutMap =
        {{"critical", TimeoutLength::TIMEOUT_CRITICAL},
         {"moderate", TimeoutLength::TIMEOUT_MODERATE},
         {"normal", TimeoutLength::TIMEOUT_NORMAL}};

}  // namespace

WatchdogClient::WatchdogClient(const sp<Looper>& handlerLooper) : mHandlerLooper(handlerLooper) {
    mMessageHandler = new MessageHandlerImpl(this);
}

ndk::ScopedAStatus WatchdogClient::checkIfAlive(int32_t sessionId, TimeoutLength timeout) {
    if (mVerbose) {
        ALOGI("Pinged by car watchdog daemon: session id = %d", sessionId);
    }
    Mutex::Autolock lock(mMutex);
    mHandlerLooper->removeMessages(mMessageHandler, WHAT_CHECK_ALIVE);
    mSession = HealthCheckSession(sessionId, timeout);
    mHandlerLooper->sendMessage(mMessageHandler, Message(WHAT_CHECK_ALIVE));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus WatchdogClient::prepareProcessTermination() {
    ALOGI("This process is being terminated by car watchdog");
    return ndk::ScopedAStatus::ok();
}

bool WatchdogClient::initialize(const CommandParam& param) {
    ndk::SpAIBinder binder(
            AServiceManager_getService("android.automotive.watchdog.ICarWatchdog/default"));
    if (binder.get() == nullptr) {
        ALOGE("Getting carwatchdog daemon failed");
        return false;
    }
    std::shared_ptr<ICarWatchdog> server = ICarWatchdog::fromBinder(binder);
    if (server == nullptr) {
        ALOGE("Failed to connect to carwatchdog daemon");
        return false;
    }
    {
        Mutex::Autolock lock(mMutex);
        mWatchdogServer = server;
        mIsClientActive = true;
    }
    mForcedKill = param.forcedKill;
    mVerbose = param.verbose;
    registerClient(param.timeout);

    if (param.inactiveAfterInSec >= 0) {
        mHandlerLooper->sendMessageDelayed(seconds_to_nanoseconds(param.inactiveAfterInSec),
                                           mMessageHandler, Message(WHAT_BECOME_INACTIVE));
    }
    if (param.terminateAfterInSec >= 0) {
        mHandlerLooper->sendMessageDelayed(seconds_to_nanoseconds(param.terminateAfterInSec),
                                           mMessageHandler, Message(WHAT_TERMINATE));
    }
    return true;
}

void WatchdogClient::finalize() {
    {
        Mutex::Autolock lock(mMutex);
        if (!mWatchdogServer) {
            return;
        }
    }
    unregisterClient();
}

void WatchdogClient::respondToWatchdog() {
    int sessionId;
    std::shared_ptr<ICarWatchdog> watchdogServer;
    std::shared_ptr<ICarWatchdogClient> testClient;
    {
        Mutex::Autolock lock(mMutex);
        if (!mIsClientActive || mTestClient == nullptr || mWatchdogServer == nullptr) {
            return;
        }
        watchdogServer = mWatchdogServer;
        testClient = mTestClient;
        sessionId = mSession.id;
    }
    ndk::ScopedAStatus status = watchdogServer->tellClientAlive(testClient, sessionId);
    if (!status.isOk()) {
        ALOGE("Failed to call binder interface: %d", status.getStatus());
        return;
    }
    if (mVerbose) {
        ALOGI("Sent response to car watchdog daemon: session id = %d", sessionId);
    }
}

void WatchdogClient::becomeInactive() {
    Mutex::Autolock lock(mMutex);
    mIsClientActive = false;
    if (mVerbose) {
        ALOGI("Became inactive");
    }
}

void WatchdogClient::terminateProcess() {
    if (!mForcedKill) {
        unregisterClient();
    }
    raise(SIGKILL);
}

void WatchdogClient::registerClient(const std::string& timeout) {
    ndk::SpAIBinder binder = this->asBinder();
    if (binder.get() == nullptr) {
        ALOGW("Failed to get car watchdog client binder object");
        return;
    }
    std::shared_ptr<ICarWatchdogClient> client = ICarWatchdogClient::fromBinder(binder);
    if (client == nullptr) {
        ALOGW("Failed to get ICarWatchdogClient from binder");
        return;
    }
    std::shared_ptr<ICarWatchdog> watchdogServer;
    {
        Mutex::Autolock lock(mMutex);
        if (mWatchdogServer == nullptr) {
            return;
        }
        watchdogServer = mWatchdogServer;
        mTestClient = client;
    }
    watchdogServer->registerClient(client, kTimeoutMap.at(timeout));
    ALOGI("Successfully registered the client to car watchdog server");
}

void WatchdogClient::unregisterClient() {
    std::shared_ptr<ICarWatchdog> watchdogServer;
    std::shared_ptr<ICarWatchdogClient> testClient;
    {
        Mutex::Autolock lock(mMutex);
        if (mWatchdogServer == nullptr || mTestClient == nullptr) {
            return;
        }
        watchdogServer = mWatchdogServer;
        testClient = mTestClient;
        mTestClient = nullptr;
    }
    watchdogServer->unregisterClient(testClient);
    ALOGI("Successfully unregistered the client from car watchdog server");
}

WatchdogClient::MessageHandlerImpl::MessageHandlerImpl(WatchdogClient* client) : mClient(client) {}

void WatchdogClient::MessageHandlerImpl::handleMessage(const Message& message) {
    switch (message.what) {
        case WHAT_CHECK_ALIVE:
            mClient->respondToWatchdog();
            break;
        case WHAT_BECOME_INACTIVE:
            mClient->becomeInactive();
            break;
        case WHAT_TERMINATE:
            mClient->terminateProcess();
            break;
        default:
            ALOGW("Unknown message: %d", message.what);
    }
}

}  // namespace watchdog
}  // namespace automotive
}  // namespace android
}  // namespace aidl
