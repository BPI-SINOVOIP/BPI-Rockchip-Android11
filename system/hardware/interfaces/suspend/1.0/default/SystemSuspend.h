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

#ifndef ANDROID_SYSTEM_SYSTEM_SUSPEND_V1_0_H
#define ANDROID_SYSTEM_SYSTEM_SUSPEND_V1_0_H

#include <android-base/result.h>
#include <android-base/unique_fd.h>
#include <android/system/suspend/1.0/ISystemSuspend.h>
#include <hidl/HidlTransportSupport.h>

#include <condition_variable>
#include <mutex>
#include <string>

#include "SuspendControlService.h"
#include "WakeLockEntryList.h"

namespace android {
namespace system {
namespace suspend {
namespace V1_0 {

using ::android::base::Result;
using ::android::base::unique_fd;
using ::android::hardware::hidl_string;
using ::android::hardware::interfacesEqual;
using ::android::hardware::Return;

using namespace std::chrono_literals;

class SystemSuspend;

struct SuspendStats {
    int success = 0;
    int fail = 0;
    int failedFreeze = 0;
    int failedPrepare = 0;
    int failedSuspend = 0;
    int failedSuspendLate = 0;
    int failedSuspendNoirq = 0;
    int failedResume = 0;
    int failedResumeEarly = 0;
    int failedResumeNoirq = 0;
    std::string lastFailedDev;
    int lastFailedErrno = 0;
    std::string lastFailedStep;
};

std::string readFd(int fd);

class WakeLock : public IWakeLock {
   public:
    WakeLock(SystemSuspend* systemSuspend, const std::string& name, int pid);
    ~WakeLock();

    Return<void> release();

   private:
    inline void releaseOnce();
    std::once_flag mReleased;

    SystemSuspend* mSystemSuspend;
    std::string mName;
    int mPid;
};

class SystemSuspend : public ISystemSuspend {
   public:
    SystemSuspend(unique_fd wakeupCountFd, unique_fd stateFd, unique_fd suspendStatsFd,
                  size_t maxNativeStatsEntries, unique_fd kernelWakelockStatsFd,
                  std::chrono::milliseconds baseSleepTime,
                  const sp<SuspendControlService>& controlService, bool useSuspendCounter = true);
    Return<sp<IWakeLock>> acquireWakeLock(WakeLockType type, const hidl_string& name) override;
    void incSuspendCounter(const std::string& name);
    void decSuspendCounter(const std::string& name);
    bool enableAutosuspend();
    bool forceSuspend();

    const WakeLockEntryList& getStatsList() const;
    void updateWakeLockStatOnRelease(const std::string& name, int pid, TimestampType timeNow);
    void updateStatsNow();
    Result<SuspendStats> getSuspendStats();

   private:
    void initAutosuspend();

    std::mutex mCounterLock;
    std::condition_variable mCounterCondVar;
    uint32_t mSuspendCounter;
    unique_fd mWakeupCountFd;
    unique_fd mStateFd;

    unique_fd mSuspendStatsFd;

    // Amount of sleep time between consecutive iterations of the suspend loop.
    std::chrono::milliseconds mBaseSleepTime;
    std::chrono::milliseconds mSleepTime;
    // Updates sleep time depending on the result of suspend attempt.
    void updateSleepTime(bool success);

    sp<SuspendControlService> mControlService;

    WakeLockEntryList mStatsList;

    // If true, use mSuspendCounter to keep track of native wake locks. Otherwise, rely on
    // /sys/power/wake_lock interface to block suspend.
    // TODO(b/128923994): remove dependency on /sys/power/wake_lock interface.
    bool mUseSuspendCounter;
    unique_fd mWakeLockFd;
    unique_fd mWakeUnlockFd;
};

}  // namespace V1_0
}  // namespace suspend
}  // namespace system
}  // namespace android

#endif  // ANDROID_SYSTEM_SYSTEM_SUSPEND_V1_0_H
