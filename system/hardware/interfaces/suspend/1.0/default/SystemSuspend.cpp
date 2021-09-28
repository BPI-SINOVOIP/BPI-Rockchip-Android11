/*
 * Copyright 2018 The Android Open Source Project
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

#include "SystemSuspend.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <fcntl.h>
#include <hidl/Status.h>
#include <hwbinder/IPCThreadState.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <thread>

using ::android::base::Error;
using ::android::base::ReadFdToString;
using ::android::base::WriteStringToFd;
using ::android::hardware::Void;
using ::std::string;

namespace android {
namespace system {
namespace suspend {
namespace V1_0 {

static const char kSleepState[] = "mem";
// TODO(b/128923994): we only need /sys/power/wake_[un]lock to export debugging info via
// /sys/kernel/debug/wakeup_sources.
static constexpr char kSysPowerWakeLock[] = "/sys/power/wake_lock";
static constexpr char kSysPowerWakeUnlock[] = "/sys/power/wake_unlock";

// This function assumes that data in fd is small enough that it can be read in one go.
// We use this function instead of the ones available in libbase because it doesn't block
// indefinitely when reading from socket streams which are used for testing.
string readFd(int fd) {
    char buf[BUFSIZ];
    ssize_t n = TEMP_FAILURE_RETRY(read(fd, &buf[0], sizeof(buf)));
    if (n < 0) return "";
    return string{buf, static_cast<size_t>(n)};
}

static inline int getCallingPid() {
    return ::android::hardware::IPCThreadState::self()->getCallingPid();
}

WakeLock::WakeLock(SystemSuspend* systemSuspend, const string& name, int pid)
    : mReleased(), mSystemSuspend(systemSuspend), mName(name), mPid(pid) {
    mSystemSuspend->incSuspendCounter(mName);
}

WakeLock::~WakeLock() {
    releaseOnce();
}

Return<void> WakeLock::release() {
    releaseOnce();
    return Void();
}

void WakeLock::releaseOnce() {
    std::call_once(mReleased, [this]() {
        mSystemSuspend->decSuspendCounter(mName);
        mSystemSuspend->updateWakeLockStatOnRelease(mName, mPid, getTimeNow());
    });
}

SystemSuspend::SystemSuspend(unique_fd wakeupCountFd, unique_fd stateFd, unique_fd suspendStatsFd,
                             size_t maxNativeStatsEntries, unique_fd kernelWakelockStatsFd,
                             std::chrono::milliseconds baseSleepTime,
                             const sp<SuspendControlService>& controlService,
                             bool useSuspendCounter)
    : mSuspendCounter(0),
      mWakeupCountFd(std::move(wakeupCountFd)),
      mStateFd(std::move(stateFd)),
      mSuspendStatsFd(std::move(suspendStatsFd)),
      mBaseSleepTime(baseSleepTime),
      mSleepTime(baseSleepTime),
      mControlService(controlService),
      mStatsList(maxNativeStatsEntries, std::move(kernelWakelockStatsFd)),
      mUseSuspendCounter(useSuspendCounter),
      mWakeLockFd(-1),
      mWakeUnlockFd(-1) {
    mControlService->setSuspendService(this);

    if (!mUseSuspendCounter) {
        mWakeLockFd.reset(TEMP_FAILURE_RETRY(open(kSysPowerWakeLock, O_CLOEXEC | O_RDWR)));
        if (mWakeLockFd < 0) {
            PLOG(ERROR) << "error opening " << kSysPowerWakeLock;
        }
        mWakeUnlockFd.reset(TEMP_FAILURE_RETRY(open(kSysPowerWakeUnlock, O_CLOEXEC | O_RDWR)));
        if (mWakeUnlockFd < 0) {
            PLOG(ERROR) << "error opening " << kSysPowerWakeUnlock;
        }
    }
}

bool SystemSuspend::enableAutosuspend() {
    static bool initialized = false;
    if (initialized) {
        LOG(ERROR) << "Autosuspend already started.";
        return false;
    }

    initAutosuspend();
    initialized = true;
    return true;
}

bool SystemSuspend::forceSuspend() {
    //  We are forcing the system to suspend. This particular call ignores all
    //  existing wakelocks (full or partial). It does not cancel the wakelocks
    //  or reset mSuspendCounter, it just ignores them.  When the system
    //  returns from suspend, the wakelocks and SuspendCounter will not have
    //  changed.
    auto counterLock = std::unique_lock(mCounterLock);
    bool success = WriteStringToFd(kSleepState, mStateFd);
    counterLock.unlock();

    if (!success) {
        PLOG(VERBOSE) << "error writing to /sys/power/state for forceSuspend";
    }
    return success;
}

Return<sp<IWakeLock>> SystemSuspend::acquireWakeLock(WakeLockType /* type */,
                                                     const hidl_string& name) {
    auto pid = getCallingPid();
    auto timeNow = getTimeNow();
    IWakeLock* wl = new WakeLock{this, name, pid};
    mStatsList.updateOnAcquire(name, pid, timeNow);
    return wl;
}

void SystemSuspend::incSuspendCounter(const string& name) {
    auto l = std::lock_guard(mCounterLock);
    if (mUseSuspendCounter) {
        mSuspendCounter++;
    } else {
        if (!WriteStringToFd(name, mWakeLockFd)) {
            PLOG(ERROR) << "error writing " << name << " to " << kSysPowerWakeLock;
        }
    }
}

void SystemSuspend::decSuspendCounter(const string& name) {
    auto l = std::lock_guard(mCounterLock);
    if (mUseSuspendCounter) {
        if (--mSuspendCounter == 0) {
            mCounterCondVar.notify_one();
        }
    } else {
        if (!WriteStringToFd(name, mWakeUnlockFd)) {
            PLOG(ERROR) << "error writing " << name << " to " << kSysPowerWakeUnlock;
        }
    }
}

void SystemSuspend::initAutosuspend() {
    std::thread autosuspendThread([this] {
        while (true) {
            std::this_thread::sleep_for(mSleepTime);
            lseek(mWakeupCountFd, 0, SEEK_SET);
            const string wakeupCount = readFd(mWakeupCountFd);
            if (wakeupCount.empty()) {
                PLOG(ERROR) << "error reading from /sys/power/wakeup_count";
                continue;
            }

            auto counterLock = std::unique_lock(mCounterLock);
            mCounterCondVar.wait(counterLock, [this] { return mSuspendCounter == 0; });
            // The mutex is locked and *MUST* remain locked until we write to /sys/power/state.
            // Otherwise, a WakeLock might be acquired after we check mSuspendCounter and before we
            // write to /sys/power/state.

            if (!WriteStringToFd(wakeupCount, mWakeupCountFd)) {
                PLOG(VERBOSE) << "error writing from /sys/power/wakeup_count";
                continue;
            }
            bool success = WriteStringToFd(kSleepState, mStateFd);
            counterLock.unlock();

            if (!success) {
                PLOG(VERBOSE) << "error writing to /sys/power/state";
            }

            mControlService->notifyWakeup(success);

            updateSleepTime(success);
        }
    });
    autosuspendThread.detach();
    LOG(INFO) << "automatic system suspend enabled";
}

void SystemSuspend::updateSleepTime(bool success) {
    static constexpr std::chrono::milliseconds kMaxSleepTime = 1min;
    if (success) {
        mSleepTime = mBaseSleepTime;
        return;
    }
    // Double sleep time after each failure up to one minute.
    mSleepTime = std::min(mSleepTime * 2, kMaxSleepTime);
}

void SystemSuspend::updateWakeLockStatOnRelease(const std::string& name, int pid,
                                                TimestampType timeNow) {
    mStatsList.updateOnRelease(name, pid, timeNow);
}

const WakeLockEntryList& SystemSuspend::getStatsList() const {
    return mStatsList;
}

void SystemSuspend::updateStatsNow() {
    mStatsList.updateNow();
}

/**
 * Returns suspend stats.
 */
Result<SuspendStats> SystemSuspend::getSuspendStats() {
    SuspendStats stats;
    std::unique_ptr<DIR, decltype(&closedir)> dp(fdopendir(dup(mSuspendStatsFd.get())), &closedir);
    if (!dp) {
        return stats;
    }

    // rewinddir, else subsequent calls will not get any suspend_stats
    rewinddir(dp.get());

    struct dirent* de;

    // Grab a wakelock before reading suspend stats,
    // to ensure a consistent snapshot.
    sp<IWakeLock> suspendStatsLock = acquireWakeLock(WakeLockType::PARTIAL, "suspend_stats_lock");

    while ((de = readdir(dp.get()))) {
        std::string statName(de->d_name);
        if ((statName == ".") || (statName == "..")) {
            continue;
        }

        unique_fd statFd{TEMP_FAILURE_RETRY(
            openat(mSuspendStatsFd.get(), statName.c_str(), O_CLOEXEC | O_RDONLY))};
        if (statFd < 0) {
            return Error() << "Failed to open " << statName;
        }

        std::string valStr;
        if (!ReadFdToString(statFd.get(), &valStr)) {
            return Error() << "Failed to read " << statName;
        }

        // Trim newline
        valStr.erase(std::remove(valStr.begin(), valStr.end(), '\n'), valStr.end());

        if (statName == "last_failed_dev") {
            stats.lastFailedDev = valStr;
        } else if (statName == "last_failed_step") {
            stats.lastFailedStep = valStr;
        } else {
            int statVal = std::stoi(valStr);
            if (statName == "success") {
                stats.success = statVal;
            } else if (statName == "fail") {
                stats.fail = statVal;
            } else if (statName == "failed_freeze") {
                stats.failedFreeze = statVal;
            } else if (statName == "failed_prepare") {
                stats.failedPrepare = statVal;
            } else if (statName == "failed_suspend") {
                stats.failedSuspend = statVal;
            } else if (statName == "failed_suspend_late") {
                stats.failedSuspendLate = statVal;
            } else if (statName == "failed_suspend_noirq") {
                stats.failedSuspendNoirq = statVal;
            } else if (statName == "failed_resume") {
                stats.failedResume = statVal;
            } else if (statName == "failed_resume_early") {
                stats.failedResumeEarly = statVal;
            } else if (statName == "failed_resume_noirq") {
                stats.failedResumeNoirq = statVal;
            } else if (statName == "last_failed_errno") {
                stats.lastFailedErrno = statVal;
            }
        }
    }

    return stats;
}

}  // namespace V1_0
}  // namespace suspend
}  // namespace system
}  // namespace android
