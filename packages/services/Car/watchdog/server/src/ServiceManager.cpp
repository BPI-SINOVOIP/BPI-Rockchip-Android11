/*
 * Copyright (c) 2020 The Android Open Source Project
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

namespace android {
namespace automotive {
namespace watchdog {

using android::sp;
using android::String16;
using android::automotive::watchdog::WatchdogProcessService;
using android::base::Error;
using android::base::Result;

sp<WatchdogProcessService> ServiceManager::sWatchdogProcessService = nullptr;
sp<IoPerfCollection> ServiceManager::sIoPerfCollection = nullptr;
sp<WatchdogBinderMediator> ServiceManager::sWatchdogBinderMediator = nullptr;

Result<void> ServiceManager::startServices(const sp<Looper>& looper) {
    if (sWatchdogProcessService != nullptr || sIoPerfCollection != nullptr ||
        sWatchdogBinderMediator != nullptr) {
        return Error(INVALID_OPERATION) << "Cannot start services more than once";
    }
    auto result = startProcessAnrMonitor(looper);
    if (!result.ok()) {
        return result;
    }
    result = startIoPerfCollection();
    if (!result.ok()) {
        return result;
    }
    return {};
}

void ServiceManager::terminateServices() {
    if (sWatchdogProcessService != nullptr) {
        sWatchdogProcessService->terminate();
        sWatchdogProcessService = nullptr;
    }
    if (sIoPerfCollection != nullptr) {
        sIoPerfCollection->terminate();
        sIoPerfCollection = nullptr;
    }
    if (sWatchdogBinderMediator != nullptr) {
        sWatchdogBinderMediator->terminate();
        sWatchdogBinderMediator = nullptr;
    }
}

Result<void> ServiceManager::startProcessAnrMonitor(const sp<Looper>& looper) {
    sWatchdogProcessService = new WatchdogProcessService(looper);
    return {};
}

Result<void> ServiceManager::startIoPerfCollection() {
    sp<IoPerfCollection> service = new IoPerfCollection();
    const auto& result = service->start();
    if (!result.ok()) {
        return Error(result.error().code())
                << "Failed to start I/O performance collection: " << result.error();
    }
    sIoPerfCollection = service;
    return {};
}

Result<void> ServiceManager::startBinderMediator() {
    sWatchdogBinderMediator = new WatchdogBinderMediator();
    const auto& result = sWatchdogBinderMediator->init(sWatchdogProcessService, sIoPerfCollection);
    if (!result.ok()) {
        return Error(result.error().code())
                << "Failed to start binder mediator: " << result.error();
    }
    return {};
}

}  // namespace watchdog
}  // namespace automotive
}  // namespace android
