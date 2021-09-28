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

#define LOG_TAG "carwatchdogd"

#include "ServiceManager.h"

#include <android-base/chrono_utils.h>
#include <android-base/properties.h>
#include <android-base/result.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <log/log.h>
#include <signal.h>
#include <utils/Looper.h>

#include <thread>

using android::IPCThreadState;
using android::Looper;
using android::ProcessState;
using android::sp;
using android::automotive::watchdog::ServiceManager;
using android::base::Result;

namespace {

const size_t kMaxBinderThreadCount = 16;

void sigHandler(int sig) {
    IPCThreadState::self()->stopProcess();
    ServiceManager::terminateServices();
    ALOGW("car watchdog server terminated on receiving signal %d.", sig);
    exit(1);
}

void registerSigHandler() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = sigHandler;
    sigaction(SIGQUIT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

}  // namespace

int main(int /*argc*/, char** /*argv*/) {
    // Set up the looper
    sp<Looper> looper(Looper::prepare(/*opts=*/0));

    // Start the services
    auto result = ServiceManager::startServices(looper);
    if (!result) {
        ALOGE("Failed to start services: %s", result.error().message().c_str());
        ServiceManager::terminateServices();
        exit(result.error().code());
    }

    registerSigHandler();

    // Wait for the service manager before starting binder mediator.
    while (android::base::GetProperty("init.svc.servicemanager", "") != "running") {
        // Poll frequent enough so the CarWatchdogDaemonHelper can connect to the daemon during
        // system boot up.
        std::this_thread::sleep_for(250ms);
    }

    // Set up the binder
    sp<ProcessState> ps(ProcessState::self());
    ps->setThreadPoolMaxThreadCount(kMaxBinderThreadCount);
    ps->startThreadPool();
    ps->giveThreadPoolName();
    IPCThreadState::self()->disableBackgroundScheduling(true);

    result = ServiceManager::startBinderMediator();
    if (!result) {
        ALOGE("Failed to start binder mediator: %s", result.error().message().c_str());
        ServiceManager::terminateServices();
        exit(result.error().code());
    }

    // Loop forever -- the health check runs on this thread in a handler, and the binder calls
    // remain responsive in their pool of threads.
    while (true) {
        looper->pollAll(/*timeoutMillis=*/-1);
    }
    ALOGW("Car watchdog server escaped from its loop.");

    return 0;
}
