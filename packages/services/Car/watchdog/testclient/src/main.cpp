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

#include <android-base/parseint.h>
#include <android-base/result.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <utils/Looper.h>

using aidl::android::automotive::watchdog::CommandParam;
using aidl::android::automotive::watchdog::WatchdogClient;
using android::Looper;
using android::sp;
using android::base::Error;
using android::base::ParseInt;
using android::base::Result;

Result<CommandParam> checkArgument(int argc, char** argv) {
    CommandParam param;
    if (argc < 4) {
        return Error() << "Invalid syntax";
    }
    if (strcmp(argv[1], "critical") && strcmp(argv[1], "moderate") && strcmp(argv[1], "normal")) {
        return Error() << "Invalid timeout";
    }
    param.timeout = argv[1];
    std::string strValue = argv[2];
    if (!ParseInt(strValue, &param.inactiveAfterInSec)) {
        return Error() << "Invalid inactive after time";
    }
    strValue = argv[3];
    if (!ParseInt(strValue, &param.terminateAfterInSec)) {
        return Error() << "Invalid terminate after time";
    }
    param.forcedKill = false;
    param.verbose = false;
    for (int i = 4; i < argc; i++) {
        if (!strcmp(argv[i], "--forcedkill")) {
            param.forcedKill = true;
        } else if (!strcmp(argv[i], "--verbose")) {
            param.verbose = true;
        } else {
            return Error() << "Invalid option";
        }
    }
    return param;
}
/**
 * Usage: carwatchdog_testclient [timeout] [inactive_after] [terminate_after] [--forcedkill]
 *                               [--verbose]
 * timeout: critical|moderate|normal
 * inactive_after: number in seconds. -1 for never being inactive.
 * terminate_after: number in seconds. -1 for running forever.
 * --forcedkill: terminate without unregistering from car watchdog daemon.
 * --verbose: output verbose logs.
 */
int main(int argc, char** argv) {
    sp<Looper> looper(Looper::prepare(/*opts=*/0));

    ABinderProcess_setThreadPoolMaxThreadCount(1);
    ABinderProcess_startThreadPool();
    std::shared_ptr<WatchdogClient> service = ndk::SharedRefBase::make<WatchdogClient>(looper);

    auto param = checkArgument(argc, argv);
    if (!param.ok()) {
        ALOGE("%s: use \"carwatchdog_testclient timeout inactive_after terminate_after "
              "[--forcedkill]\"",
              param.error().message().c_str());
        ALOGE("timeout: critical|moderate|normal");
        ALOGE("inactive_after: number in seconds (-1 for never being inactive)");
        ALOGE("terminate_after: number in seconds (-1 for running forever)");
        ALOGE("--forcedkill: terminate without unregistering from car watchdog daemon");
        ALOGE("--verbose: output verbose logs");
        return 1;
    }
    if (!service->initialize(*param)) {
        ALOGE("Failed to initialize watchdog client");
        return 1;
    }

    while (true) {
        looper->pollAll(/*timeoutMillis=*/-1);
    }

    return 0;
}
