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

#include "WatchdogBinderMediator.h"

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android/automotive/watchdog/BootPhase.h>
#include <android/automotive/watchdog/PowerCycle.h>
#include <android/automotive/watchdog/UserState.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <cutils/multiuser.h>
#include <log/log.h>
#include <private/android_filesystem_config.h>

namespace android {
namespace automotive {
namespace watchdog {

using android::defaultServiceManager;
using android::base::Error;
using android::base::Join;
using android::base::ParseUint;
using android::base::Result;
using android::base::StringPrintf;
using android::base::WriteStringToFd;
using android::binder::Status;

namespace {

constexpr const char* kHelpFlag = "--help";
constexpr const char* kHelpShortFlag = "-h";
constexpr const char* kHelpText =
        "CarWatchdog daemon dumpsys help page:\n"
        "Format: dumpsys android.automotive.watchdog.ICarWatchdog/default [options]\n\n"
        "%s or %s: Displays this help text.\n"
        "When no options are specified, carwatchdog report is generated.\n";

Status checkSystemUser() {
    if (IPCThreadState::self()->getCallingUid() != AID_SYSTEM) {
        return Status::fromExceptionCode(Status::EX_SECURITY,
                                         "Calling process does not have proper privilege");
    }
    return Status::ok();
}

Status fromExceptionCode(int32_t exceptionCode, std::string message) {
    ALOGW("%s", message.c_str());
    return Status::fromExceptionCode(exceptionCode, message.c_str());
}

}  // namespace

Result<void> WatchdogBinderMediator::init(sp<WatchdogProcessService> watchdogProcessService,
                                          sp<IoPerfCollection> ioPerfCollection) {
    if (watchdogProcessService == nullptr || ioPerfCollection == nullptr) {
        return Error(INVALID_OPERATION)
                << "Must initialize both process and I/O perf collection service before starting "
                << "carwatchdog binder mediator";
    }
    if (mWatchdogProcessService != nullptr || mIoPerfCollection != nullptr) {
        return Error(INVALID_OPERATION)
                << "Cannot initialize carwatchdog binder mediator more than once";
    }
    mWatchdogProcessService = watchdogProcessService;
    mIoPerfCollection = ioPerfCollection;
    status_t status =
            defaultServiceManager()
                    ->addService(String16("android.automotive.watchdog.ICarWatchdog/default"),
                                 this);
    if (status != OK) {
        return Error(status) << "Failed to start carwatchdog binder mediator";
    }
    return {};
}

status_t WatchdogBinderMediator::dump(int fd, const Vector<String16>& args) {
    int numArgs = args.size();
    if (numArgs == 1 && (args[0] == String16(kHelpFlag) || args[0] == String16(kHelpShortFlag))) {
        if (!dumpHelpText(fd, "")) {
            ALOGW("Failed to write help text to fd");
            return FAILED_TRANSACTION;
        }
        return OK;
    }
    if (numArgs >= 1 &&
        (args[0] == String16(kStartCustomCollectionFlag) ||
         args[0] == String16(kEndCustomCollectionFlag))) {
        auto ret = mIoPerfCollection->onCustomCollection(fd, args);
        if (!ret.ok()) {
            std::string mode = args[0] == String16(kStartCustomCollectionFlag) ? "start" : "end";
            std::string errorMsg = StringPrintf("Failed to %s custom I/O perf collection: %s",
                                                mode.c_str(), ret.error().message().c_str());
            if (ret.error().code() == BAD_VALUE) {
                dumpHelpText(fd, errorMsg);
            } else {
                ALOGW("%s", errorMsg.c_str());
            }
            return ret.error().code();
        }
        return OK;
    }

    if (numArgs > 0) {
        ALOGW("Car watchdog cannot recognize the given option(%s). Dumping the current state...",
              Join(args, " ").c_str());
    }

    auto ret = mWatchdogProcessService->dump(fd, args);
    if (!ret.ok()) {
        ALOGW("Failed to dump carwatchdog process service: %s", ret.error().message().c_str());
        return ret.error().code();
    }
    ret = mIoPerfCollection->onDump(fd);
    if (!ret.ok()) {
        ALOGW("Failed to dump I/O perf collection: %s", ret.error().message().c_str());
        return ret.error().code();
    }
    return OK;
}

bool WatchdogBinderMediator::dumpHelpText(int fd, std::string errorMsg) {
    if (!errorMsg.empty()) {
        ALOGW("Error: %s", errorMsg.c_str());
        if (!WriteStringToFd(StringPrintf("Error: %s\n\n", errorMsg.c_str()), fd)) {
            ALOGW("Failed to write error message to fd");
            return false;
        }
    }

    return WriteStringToFd(StringPrintf(kHelpText, kHelpFlag, kHelpShortFlag), fd) &&
            mIoPerfCollection->dumpHelpText(fd);
}

Status WatchdogBinderMediator::registerMediator(const sp<ICarWatchdogClient>& mediator) {
    Status status = checkSystemUser();
    if (!status.isOk()) {
        return status;
    }
    return mWatchdogProcessService->registerMediator(mediator);
}

Status WatchdogBinderMediator::unregisterMediator(const sp<ICarWatchdogClient>& mediator) {
    Status status = checkSystemUser();
    if (!status.isOk()) {
        return status;
    }
    return mWatchdogProcessService->unregisterMediator(mediator);
}
Status WatchdogBinderMediator::registerMonitor(const sp<ICarWatchdogMonitor>& monitor) {
    Status status = checkSystemUser();
    if (!status.isOk()) {
        return status;
    }
    return mWatchdogProcessService->registerMonitor(monitor);
}
Status WatchdogBinderMediator::unregisterMonitor(const sp<ICarWatchdogMonitor>& monitor) {
    Status status = checkSystemUser();
    if (!status.isOk()) {
        return status;
    }
    return mWatchdogProcessService->unregisterMonitor(monitor);
}

Status WatchdogBinderMediator::tellMediatorAlive(const sp<ICarWatchdogClient>& mediator,
                                                 const std::vector<int32_t>& clientsNotResponding,
                                                 int32_t sessionId) {
    Status status = checkSystemUser();
    if (!status.isOk()) {
        return status;
    }
    return mWatchdogProcessService->tellMediatorAlive(mediator, clientsNotResponding, sessionId);
}
Status WatchdogBinderMediator::tellDumpFinished(const android::sp<ICarWatchdogMonitor>& monitor,
                                                int32_t pid) {
    Status status = checkSystemUser();
    if (!status.isOk()) {
        return status;
    }
    return mWatchdogProcessService->tellDumpFinished(monitor, pid);
}

Status WatchdogBinderMediator::notifySystemStateChange(StateType type, int32_t arg1, int32_t arg2) {
    Status status = checkSystemUser();
    if (!status.isOk()) {
        return status;
    }
    switch (type) {
        case StateType::POWER_CYCLE: {
            PowerCycle powerCycle = static_cast<PowerCycle>(static_cast<uint32_t>(arg1));
            if (powerCycle >= PowerCycle::NUM_POWER_CYLES) {
                return fromExceptionCode(Status::EX_ILLEGAL_ARGUMENT,
                                         StringPrintf("Invalid power cycle %d", powerCycle));
            }
            return mWatchdogProcessService->notifyPowerCycleChange(powerCycle);
        }
        case StateType::USER_STATE: {
            userid_t userId = static_cast<userid_t>(arg1);
            UserState userState = static_cast<UserState>(static_cast<uint32_t>(arg2));
            if (userState >= UserState::NUM_USER_STATES) {
                return fromExceptionCode(Status::EX_ILLEGAL_ARGUMENT,
                                         StringPrintf("Invalid user state %d", userState));
            }
            return mWatchdogProcessService->notifyUserStateChange(userId, userState);
        }
        case StateType::BOOT_PHASE: {
            BootPhase phase = static_cast<BootPhase>(static_cast<uint32_t>(arg1));
            if (phase >= BootPhase::BOOT_COMPLETED) {
                auto ret = mIoPerfCollection->onBootFinished();
                if (!ret.ok()) {
                    return fromExceptionCode(ret.error().code(), ret.error().message());
                }
            }
            return Status::ok();
        }
    }
    return fromExceptionCode(Status::EX_ILLEGAL_ARGUMENT,
                             StringPrintf("Invalid state change type %d", type));
}

}  // namespace watchdog
}  // namespace automotive
}  // namespace android
