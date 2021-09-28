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

#ifndef WATCHDOG_SERVER_SRC_SERVICEMANAGER_H_
#define WATCHDOG_SERVER_SRC_SERVICEMANAGER_H_

#include <android-base/result.h>
#include <utils/Looper.h>
#include <utils/StrongPointer.h>

#include "IoPerfCollection.h"
#include "WatchdogBinderMediator.h"
#include "WatchdogProcessService.h"

namespace android {
namespace automotive {
namespace watchdog {

class ServiceManager {
public:
    static android::base::Result<void> startServices(const android::sp<Looper>& looper);
    static android::base::Result<void> startBinderMediator();
    static void terminateServices();

private:
    static android::base::Result<void> startProcessAnrMonitor(const android::sp<Looper>& looper);
    static android::base::Result<void> startIoPerfCollection();

    static android::sp<WatchdogProcessService> sWatchdogProcessService;
    static android::sp<IoPerfCollection> sIoPerfCollection;
    static android::sp<WatchdogBinderMediator> sWatchdogBinderMediator;
};

}  // namespace watchdog
}  // namespace automotive
}  // namespace android

#endif  // WATCHDOG_SERVER_SRC_SERVICEMANAGER_H_
