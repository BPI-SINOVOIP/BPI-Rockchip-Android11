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

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/result.h>
#include <android-base/unique_fd.h>
#include <android/system/suspend/BnSuspendCallback.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <cutils/native_handle.h>
#include <ftw.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <hidl/HidlTransportSupport.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <future>
#include <string>
#include <thread>

#include "SuspendControlService.h"
#include "SystemSuspend.h"

using android::sp;
using android::base::Result;
using android::base::Socketpair;
using android::base::unique_fd;
using android::base::WriteStringToFd;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::Return;
using android::hardware::Void;
using android::system::suspend::BnSuspendCallback;
using android::system::suspend::ISuspendControlService;
using android::system::suspend::WakeLockInfo;
using android::system::suspend::V1_0::getTimeNow;
using android::system::suspend::V1_0::ISystemSuspend;
using android::system::suspend::V1_0::IWakeLock;
using android::system::suspend::V1_0::readFd;
using android::system::suspend::V1_0::SuspendControlService;
using android::system::suspend::V1_0::SuspendStats;
using android::system::suspend::V1_0::SystemSuspend;
using android::system::suspend::V1_0::TimestampType;
using android::system::suspend::V1_0::WakeLockType;
using namespace std::chrono_literals;

namespace android {

static constexpr char kServiceName[] = "TestService";
static constexpr char kControlServiceName[] = "TestControlService";

static bool isReadBlocked(int fd, int timeout_ms = 20) {
    struct pollfd pfd {
        .fd = fd, .events = POLLIN,
    };
    return poll(&pfd, 1, timeout_ms) == 0;
}

class SystemSuspendTest : public ::testing::Test {
   protected:
    static void registerTestService() {
        std::thread testService([] {
            configureRpcThreadpool(1, true /* callerWillJoin */);

            sp<SuspendControlService> suspendControl = new SuspendControlService();
            auto controlStatus = ::android::defaultServiceManager()->addService(
                android::String16(kControlServiceName), suspendControl);
            if (android::OK != controlStatus) {
                LOG(FATAL) << "Unable to register service " << kControlServiceName << controlStatus;
            }

            // Create non-HW binder threadpool for SuspendControlService.
            sp<android::ProcessState> ps{android::ProcessState::self()};
            ps->startThreadPool();

            sp<ISystemSuspend> suspend = new SystemSuspend(
                std::move(wakeupCountFds[1]), std::move(stateFds[1]),
                unique_fd(-1) /*suspendStatsFd*/, 1 /* maxNativeStatsEntries */,
                unique_fd(-1) /* kernelWakelockStatsFd */, 0ms /* baseSleepTime */, suspendControl);
            status_t status = suspend->registerAsService(kServiceName);
            if (android::OK != status) {
                LOG(FATAL) << "Unable to register service: " << status;
            }

            joinRpcThreadpool();
        });
        testService.detach();
    }

    static void SetUpTestSuite() {
        Socketpair(SOCK_STREAM, &wakeupCountFds[0], &wakeupCountFds[1]);
        Socketpair(SOCK_STREAM, &stateFds[0], &stateFds[1]);

        registerTestService();
        ::android::hardware::details::waitForHwService(ISystemSuspend::descriptor, kServiceName);
        sp<ISystemSuspend> suspendService = ISystemSuspend::getService(kServiceName);
        ASSERT_NE(suspendService, nullptr) << "failed to get suspend service";

        sp<IBinder> control =
            android::defaultServiceManager()->getService(android::String16(kControlServiceName));
        ASSERT_NE(control, nullptr) << "failed to get the suspend control service";
        sp<ISuspendControlService> controlService = interface_cast<ISuspendControlService>(control);

        // Start auto-suspend.
        bool enabled = false;
        controlService->enableAutosuspend(&enabled);
        ASSERT_EQ(enabled, true) << "failed to start autosuspend";
    }

   public:
    virtual void SetUp() override {
        ::android::hardware::details::waitForHwService(ISystemSuspend::descriptor, kServiceName);
        suspendService = ISystemSuspend::getService(kServiceName);
        ASSERT_NE(suspendService, nullptr) << "failed to get suspend service";

        sp<IBinder> control =
            android::defaultServiceManager()->getService(android::String16(kControlServiceName));
        ASSERT_NE(control, nullptr) << "failed to get the suspend control service";
        controlService = interface_cast<ISuspendControlService>(control);

        wakeupCountFd = wakeupCountFds[0];
        stateFd = stateFds[0];

        // SystemSuspend HAL should not have written back to wakeupCountFd or stateFd yet.
        ASSERT_TRUE(isReadBlocked(wakeupCountFd));
        ASSERT_TRUE(isReadBlocked(stateFd));
    }

    virtual void TearDown() override {
        if (!isReadBlocked(wakeupCountFd)) readFd(wakeupCountFd);
        if (!isReadBlocked(stateFd)) readFd(stateFd).empty();
        ASSERT_TRUE(isReadBlocked(wakeupCountFd));
        ASSERT_TRUE(isReadBlocked(stateFd));
    }

    void unblockSystemSuspendFromWakeupCount() {
        std::string wakeupCount = std::to_string(rand());
        ASSERT_TRUE(WriteStringToFd(wakeupCount, wakeupCountFd));
    }

    bool isSystemSuspendBlocked(int timeout_ms = 20) { return isReadBlocked(stateFd, timeout_ms); }

    sp<IWakeLock> acquireWakeLock(const std::string& name = "TestLock") {
        return suspendService->acquireWakeLock(WakeLockType::PARTIAL, name);
    }

    size_t getActiveWakeLockCount() {
        std::vector<WakeLockInfo> wlStats;
        controlService->getWakeLockStats(&wlStats);
        return count_if(wlStats.begin(), wlStats.end(), [](auto entry) { return entry.isActive; });
    }

    void checkLoop(int numIter) {
        for (int i = 0; i < numIter; i++) {
            // Mock value for /sys/power/wakeup_count.
            std::string wakeupCount = std::to_string(rand());
            ASSERT_TRUE(WriteStringToFd(wakeupCount, wakeupCountFd));
            ASSERT_EQ(readFd(wakeupCountFd), wakeupCount)
                << "wakeup count value written by SystemSuspend is not equal to value given to it";
            ASSERT_EQ(readFd(stateFd), "mem")
                << "SystemSuspend failed to write correct sleep state.";
        }
    }
    sp<ISystemSuspend> suspendService;
    sp<ISuspendControlService> controlService;
    static unique_fd wakeupCountFds[2];
    static unique_fd stateFds[2];
    static int wakeupCountFd;
    static int stateFd;
};

// SystemSuspendTest test suite resources
unique_fd SystemSuspendTest::wakeupCountFds[2];
unique_fd SystemSuspendTest::stateFds[2];
int SystemSuspendTest::wakeupCountFd;
int SystemSuspendTest::stateFd;

// Tests that autosuspend thread can only be enabled once.
TEST_F(SystemSuspendTest, OnlyOneEnableAutosuspend) {
    bool enabled = false;
    controlService->enableAutosuspend(&enabled);
    ASSERT_EQ(enabled, false);
}

TEST_F(SystemSuspendTest, AutosuspendLoop) {
    checkLoop(5);
}

// Tests that upon WakeLock destruction SystemSuspend HAL is unblocked.
TEST_F(SystemSuspendTest, WakeLockDestructor) {
    {
        sp<IWakeLock> wl = acquireWakeLock();
        ASSERT_NE(wl, nullptr);
        unblockSystemSuspendFromWakeupCount();
        ASSERT_TRUE(isSystemSuspendBlocked());
    }
    ASSERT_FALSE(isSystemSuspendBlocked());
}

// Tests that upon WakeLock::release() SystemSuspend HAL is unblocked.
TEST_F(SystemSuspendTest, WakeLockRelease) {
    sp<IWakeLock> wl = acquireWakeLock();
    ASSERT_NE(wl, nullptr);
    unblockSystemSuspendFromWakeupCount();
    ASSERT_TRUE(isSystemSuspendBlocked());
    wl->release();
    ASSERT_FALSE(isSystemSuspendBlocked());
}

// Tests that multiple WakeLocks correctly block SystemSuspend HAL.
TEST_F(SystemSuspendTest, MultipleWakeLocks) {
    {
        sp<IWakeLock> wl1 = acquireWakeLock();
        ASSERT_NE(wl1, nullptr);
        ASSERT_TRUE(isSystemSuspendBlocked());
        unblockSystemSuspendFromWakeupCount();
        {
            sp<IWakeLock> wl2 = acquireWakeLock();
            ASSERT_NE(wl2, nullptr);
            ASSERT_TRUE(isSystemSuspendBlocked());
        }
        ASSERT_TRUE(isSystemSuspendBlocked());
    }
    ASSERT_FALSE(isSystemSuspendBlocked());
}

// Tests that upon thread deallocation WakeLock is destructed and SystemSuspend HAL is unblocked.
TEST_F(SystemSuspendTest, ThreadCleanup) {
    std::thread clientThread([this] {
        sp<IWakeLock> wl = acquireWakeLock();
        ASSERT_NE(wl, nullptr);
        unblockSystemSuspendFromWakeupCount();
        ASSERT_TRUE(isSystemSuspendBlocked());
    });
    clientThread.join();
    ASSERT_FALSE(isSystemSuspendBlocked());
}

// Test that binder driver correctly deallocates acquired WakeLocks, even if the client processs
// is terminated without ability to do clean up.
TEST_F(SystemSuspendTest, CleanupOnAbort) {
    ASSERT_EXIT(
        {
            sp<IWakeLock> wl = acquireWakeLock();
            ASSERT_NE(wl, nullptr);
            std::abort();
        },
        ::testing::KilledBySignal(SIGABRT), "");
    ASSERT_TRUE(isSystemSuspendBlocked());
    unblockSystemSuspendFromWakeupCount();
    // Timing of the wake lock clean-up after process death is scheduler-dependent.
    // Increase the timeout to avoid flakes.
    ASSERT_FALSE(isSystemSuspendBlocked(200));
}

// Stress test acquiring/releasing WakeLocks.
TEST_F(SystemSuspendTest, WakeLockStressTest) {
    // numThreads threads will acquire/release numLocks locks each.
    constexpr int numThreads = 10;
    constexpr int numLocks = 10000;
    std::thread tds[numThreads];

    for (int i = 0; i < numThreads; i++) {
        tds[i] = std::thread([this] {
            for (int j = 0; j < numLocks; j++) {
                sp<IWakeLock> wl1 = acquireWakeLock();
                sp<IWakeLock> wl2 = acquireWakeLock();
                wl2->release();
            }
        });
    }
    for (int i = 0; i < numThreads; i++) {
        tds[i].join();
    }
    ASSERT_EQ(getActiveWakeLockCount(), 0);
}

// Callbacks are passed around as sp<>. However, mock expectations are verified when mock objects
// are destroyed, i.e. the test needs to control lifetime of the mock object.
// MockCallbackImpl can be destroyed independently of its wrapper MockCallback which is passed to
// SystemSuspend.
struct MockCallbackImpl {
    MOCK_METHOD1(notifyWakeup, binder::Status(bool));
};

class MockCallback : public BnSuspendCallback {
   public:
    MockCallback(MockCallbackImpl* impl) : mImpl(impl), mDisabled(false) {}
    binder::Status notifyWakeup(bool x) {
        return mDisabled ? binder::Status::ok() : mImpl->notifyWakeup(x);
    }
    // In case we pull the rug from under MockCallback, but SystemSuspend still has an sp<> to the
    // object.
    void disable() { mDisabled = true; }

   private:
    MockCallbackImpl* mImpl;
    bool mDisabled;
};

// Tests that nullptr can't be registered as callbacks.
TEST_F(SystemSuspendTest, RegisterInvalidCallback) {
    bool retval = false;
    controlService->registerCallback(nullptr, &retval);
    ASSERT_FALSE(retval);
}

// Tests that SystemSuspend HAL correctly notifies wakeup events.
TEST_F(SystemSuspendTest, CallbackNotifyWakeup) {
    constexpr int numWakeups = 5;
    MockCallbackImpl impl;
    // SystemSuspend should suspend numWakeup + 1 times. However, it might
    // only be able to notify numWakeup times. The test case might have
    // finished by the time last notification completes.
    EXPECT_CALL(impl, notifyWakeup).Times(testing::AtLeast(numWakeups));
    sp<MockCallback> cb = new MockCallback(&impl);
    bool retval = false;
    controlService->registerCallback(cb, &retval);
    ASSERT_TRUE(retval);
    checkLoop(numWakeups + 1);
    cb->disable();
}

// Tests that SystemSuspend HAL correctly deals with a dead callback.
TEST_F(SystemSuspendTest, DeadCallback) {
    ASSERT_EXIT(
        {
            sp<MockCallback> cb = new MockCallback(nullptr);
            bool retval = false;
            controlService->registerCallback(cb, &retval);
            ASSERT_TRUE(retval);
            std::exit(0);
        },
        ::testing::ExitedWithCode(0), "");

    // Dead process callback must still be dealt with either by unregistering it
    // or checking isOk() on every call.
    checkLoop(3);
}

// Callback that registers another callback.
class CbRegisteringCb : public BnSuspendCallback {
   public:
    CbRegisteringCb(sp<ISuspendControlService> controlService) : mControlService(controlService) {}
    binder::Status notifyWakeup(bool x) {
        sp<MockCallback> cb = new MockCallback(nullptr);
        cb->disable();
        bool retval = false;
        mControlService->registerCallback(cb, &retval);
        return binder::Status::ok();
    }

   private:
    sp<ISuspendControlService> mControlService;
};

// Tests that callback registering another callback doesn't result in a deadlock.
TEST_F(SystemSuspendTest, CallbackRegisterCallbackNoDeadlock) {
    sp<CbRegisteringCb> cb = new CbRegisteringCb(controlService);
    bool retval = false;
    controlService->registerCallback(cb, &retval);
    ASSERT_TRUE(retval);
    checkLoop(3);
}

class SystemSuspendSameThreadTest : public ::testing::Test {
   public:
    sp<IWakeLock> acquireWakeLock(const std::string& name = "TestLock") {
        return suspendService->acquireWakeLock(WakeLockType::PARTIAL, name);
    }

    /**
     * Returns true if wake lock is found else false.
     */
    bool findWakeLockInfoByName(const std::vector<WakeLockInfo>& wlStats, const std::string& name,
                                WakeLockInfo* info) {
        auto it = std::find_if(wlStats.begin(), wlStats.end(),
                               [&name](const auto& x) { return x.name == name; });
        if (it != wlStats.end()) {
            *info = *it;
            return true;
        }
        return false;
    }

    bool writeStatToFile(int statDirFd, const std::string& fileName, const std::string& stat) {
        unique_fd statFd{TEMP_FAILURE_RETRY(
            openat(statDirFd, fileName.c_str(), O_CREAT | O_CLOEXEC | O_RDWR, S_IRWXU))};
        if (statFd < 0) {
            PLOG(ERROR) << "SystemSuspend: Error opening " << fileName;
            return false;
        }

        if (!WriteStringToFd(stat, statFd.get())) {
            PLOG(ERROR) << "SystemSuspend: Error writing stat to " << fileName;
            return false;
        }

        return true;
    }

    bool writeStatToFile(int statDirFd, const std::string& fileName, int64_t stat) {
        return writeStatToFile(statDirFd, fileName, std::to_string(stat));
    }

    /**
     * Creates a kernel wakelock directory and stats files.
     * Returns true on success else false.
     */
    bool addKernelWakelock(const std::string& name, int64_t activeCount = 42,
                           int64_t activeTime = 42, int64_t eventCount = 42,
                           int64_t expireCount = 42, int64_t lastChange = 42, int64_t maxTime = 42,
                           int64_t preventSuspendTime = 42, int64_t totalTime = 42,
                           int64_t wakeupCount = 42) {
        static int id = 0;
        std::string kwlId = "wakeup" + std::to_string(id++);

        if ((mkdirat(kernelWakelockStatsFd, kwlId.c_str(), S_IRWXU)) < 0) {
            PLOG(ERROR) << "SystemSuspend: Error creating directory for " << kwlId;
            return false;
        }

        unique_fd kernelWakelockFd{TEMP_FAILURE_RETRY(
            openat(kernelWakelockStatsFd, kwlId.c_str(), O_DIRECTORY | O_CLOEXEC | O_RDONLY))};
        if (kernelWakelockFd < 0) {
            PLOG(ERROR) << "SystemSuspend: Error opening " << kwlId;
            return false;
        }

        int fd = kernelWakelockFd.get();

        return writeStatToFile(fd, "name", name) &&
               writeStatToFile(fd, "active_count", activeCount) &&
               writeStatToFile(fd, "active_time_ms", activeTime) &&
               writeStatToFile(fd, "event_count", eventCount) &&
               writeStatToFile(fd, "expire_count", expireCount) &&
               writeStatToFile(fd, "last_change_ms", lastChange) &&
               writeStatToFile(fd, "max_time_ms", maxTime) &&
               writeStatToFile(fd, "prevent_suspend_time_ms", preventSuspendTime) &&
               writeStatToFile(fd, "total_time_ms", totalTime) &&
               writeStatToFile(fd, "wakeup_count", wakeupCount);
    }

    /**
     * Adds Suspend stats files to suspendStatDir.
     * Returns true on success else false.
     */
    bool addSuspendStats(int64_t success = 42, int64_t fail = 42, int64_t failedFreeze = 42,
                         int64_t failedPrepare = 42, int64_t failedSuspend = 42,
                         int64_t failedSuspendLate = 42, int64_t failedSuspendNoirq = 42,
                         int64_t failedResume = 42, int64_t failedResumeEarly = 42,
                         int64_t failedResumeNoirq = 42,
                         const std::string& lastFailedDev = "fakeDev", int64_t lastFailedErrno = 42,
                         const std::string& lastFailedStep = "fakeStep") {
        int fd = suspendStatsFd.get();

        return writeStatToFile(fd, "success", success) && writeStatToFile(fd, "fail", fail) &&
               writeStatToFile(fd, "failed_freeze", failedFreeze) &&
               writeStatToFile(fd, "failed_prepare", failedPrepare) &&
               writeStatToFile(fd, "failed_suspend", failedSuspend) &&
               writeStatToFile(fd, "failed_suspend_late", failedSuspendLate) &&
               writeStatToFile(fd, "failed_suspend_noirq", failedSuspendNoirq) &&
               writeStatToFile(fd, "failed_resume", failedResume) &&
               writeStatToFile(fd, "failed_resume_early", failedResumeEarly) &&
               writeStatToFile(fd, "failed_resume_noirq", failedResumeNoirq) &&
               writeStatToFile(fd, "last_failed_dev", lastFailedDev) &&
               writeStatToFile(fd, "last_failed_errno", lastFailedErrno) &&
               writeStatToFile(fd, "last_failed_step", lastFailedStep);
    }

    bool removeDirectoryEntry(const std::string& path) {
        auto callback = [](const char* child, const struct stat*, int file_type,
                           struct FTW*) -> int {
            switch (file_type) {
                case FTW_D:
                case FTW_DP:
                case FTW_DNR:
                    if (rmdir(child) == -1) {
                        PLOG(ERROR) << "rmdir " << child;
                    }
                    break;
                case FTW_NS:
                default:
                    if (rmdir(child) != -1) break;
                    // FALLTHRU (for gcc, lint, pcc, etc; and following for clang)
                    FALLTHROUGH_INTENDED;
                case FTW_F:
                case FTW_SL:
                case FTW_SLN:
                    if (unlink(child) == -1) {
                        PLOG(ERROR) << "unlink " << child;
                    }
                    break;
            }
            return 0;
        };
        return nftw(path.c_str(), callback, 128, FTW_DEPTH | FTW_MOUNT | FTW_PHYS) == 0;
    }

    /**
     * Removes all entries from directory.
     * Returns true on success else false.
     */
    bool clearDirectory(const std::string& dirPath) {
        std::unique_ptr<DIR, decltype(&closedir)> dp(opendir(dirPath.c_str()), &closedir);
        if (!dp) {
            return false;
        }

        rewinddir(dp.get());
        struct dirent* de;
        while ((de = readdir(dp.get()))) {
            std::string name(de->d_name);
            if ((name == ".") || (name == "..")) {
                continue;
            }
            if (!removeDirectoryEntry(dirPath + "/" + name)) {
                PLOG(ERROR) << "SystemSuspend: Failed to remove " << name;
                return false;
            }
        }

        return true;
    }

    /**
     * Returns wakelock stats.
     */
    std::vector<WakeLockInfo> getWakelockStats() {
        std::vector<WakeLockInfo> wlStats;
        controlService->getWakeLockStats(&wlStats);
        return wlStats;
    }

    /**
     * Returns suspend stats.
     */
    Result<SuspendStats> getSuspendStats() {
        return reinterpret_cast<SystemSuspend*>(suspendService.get())->getSuspendStats();
    }

    virtual void SetUp() override {
        kernelWakelockStatsFd = unique_fd(TEMP_FAILURE_RETRY(
            open(kernelWakelockStatsDir.path, O_DIRECTORY | O_CLOEXEC | O_RDONLY)));
        if (kernelWakelockStatsFd < 0) {
            PLOG(FATAL) << "SystemSuspend: Failed to open kernel wakelock stats directory";
        }

        suspendStatsFd = unique_fd(
            TEMP_FAILURE_RETRY(open(suspendStatsDir.path, O_DIRECTORY | O_CLOEXEC | O_RDONLY)));
        if (suspendStatsFd < 0) {
            PLOG(FATAL) << "SystemSuspend: Failed to open suspend_stats directory";
        }

        // Set up same thread suspend services
        sp<SuspendControlService> suspendControl = new SuspendControlService();
        controlService = suspendControl;
        suspendService = new SystemSuspend(
            unique_fd(-1) /* wakeupCountFd */, unique_fd(-1) /* stateFd */,
            unique_fd(dup(suspendStatsFd)), 1 /* maxNativeStatsEntries */,
            unique_fd(dup(kernelWakelockStatsFd.get())), 0ms /* baseSleepTime */, suspendControl);
    }

    virtual void TearDown() override {
        ASSERT_TRUE(clearDirectory(kernelWakelockStatsDir.path));
        ASSERT_TRUE(clearDirectory(suspendStatsDir.path));
    }

    sp<ISystemSuspend> suspendService;
    sp<ISuspendControlService> controlService;
    unique_fd kernelWakelockStatsFd;
    unique_fd suspendStatsFd;
    TemporaryDir kernelWakelockStatsDir;
    TemporaryDir suspendStatsDir;
};

// Test that getWakeLockStats has correct information about Native WakeLocks.
TEST_F(SystemSuspendSameThreadTest, GetNativeWakeLockStats) {
    std::string fakeWlName = "FakeLock";
    {
        sp<IWakeLock> fakeLock = acquireWakeLock(fakeWlName);
        std::vector<WakeLockInfo> wlStats = getWakelockStats();
        ASSERT_EQ(wlStats.size(), 1);

        WakeLockInfo nwlInfo;
        ASSERT_TRUE(findWakeLockInfoByName(wlStats, fakeWlName, &nwlInfo));
        ASSERT_EQ(nwlInfo.name, fakeWlName);
        ASSERT_EQ(nwlInfo.activeCount, 1);
        ASSERT_EQ(nwlInfo.isActive, true);
        ASSERT_FALSE(nwlInfo.isKernelWakelock);

        ASSERT_EQ(nwlInfo.pid, getpid());

        ASSERT_EQ(nwlInfo.eventCount, 0);
        ASSERT_EQ(nwlInfo.expireCount, 0);
        ASSERT_EQ(nwlInfo.preventSuspendTime, 0);
        ASSERT_EQ(nwlInfo.wakeupCount, 0);

        // We sleep so that the wake lock stats entry get updated with a different timestamp.
        std::this_thread::sleep_for(1s);
    }
    std::vector<WakeLockInfo> wlStats = getWakelockStats();
    ASSERT_EQ(wlStats.size(), 1);

    WakeLockInfo nwlInfo;
    ASSERT_TRUE(findWakeLockInfoByName(wlStats, fakeWlName, &nwlInfo));
    ASSERT_EQ(nwlInfo.name, fakeWlName);
    ASSERT_EQ(nwlInfo.activeCount, 1);
    ASSERT_GE(nwlInfo.maxTime, 1000);
    ASSERT_GE(nwlInfo.totalTime, 1000);
    ASSERT_EQ(nwlInfo.isActive, false);
    ASSERT_EQ(nwlInfo.activeTime, 0);  // No longer active
    ASSERT_FALSE(nwlInfo.isKernelWakelock);

    ASSERT_EQ(nwlInfo.pid, getpid());

    ASSERT_EQ(nwlInfo.eventCount, 0);
    ASSERT_EQ(nwlInfo.expireCount, 0);
    ASSERT_EQ(nwlInfo.preventSuspendTime, 0);
    ASSERT_EQ(nwlInfo.wakeupCount, 0);
}

// Test that getWakeLockStats has correct information about Kernel WakeLocks.
TEST_F(SystemSuspendSameThreadTest, GetKernelWakeLockStats) {
    std::string fakeKwlName1 = "fakeKwl1";
    std::string fakeKwlName2 = "fakeKwl2";
    addKernelWakelock(fakeKwlName1);
    addKernelWakelock(fakeKwlName2, 10 /* activeCount */);

    std::vector<WakeLockInfo> wlStats = getWakelockStats();

    ASSERT_EQ(wlStats.size(), 2);

    WakeLockInfo kwlInfo1;
    ASSERT_TRUE(findWakeLockInfoByName(wlStats, fakeKwlName1, &kwlInfo1));
    ASSERT_EQ(kwlInfo1.name, fakeKwlName1);
    ASSERT_EQ(kwlInfo1.activeCount, 42);
    ASSERT_EQ(kwlInfo1.lastChange, 42);
    ASSERT_EQ(kwlInfo1.maxTime, 42);
    ASSERT_EQ(kwlInfo1.totalTime, 42);
    ASSERT_EQ(kwlInfo1.isActive, true);
    ASSERT_EQ(kwlInfo1.activeTime, 42);
    ASSERT_TRUE(kwlInfo1.isKernelWakelock);

    ASSERT_EQ(kwlInfo1.pid, -1);

    ASSERT_EQ(kwlInfo1.eventCount, 42);
    ASSERT_EQ(kwlInfo1.expireCount, 42);
    ASSERT_EQ(kwlInfo1.preventSuspendTime, 42);
    ASSERT_EQ(kwlInfo1.wakeupCount, 42);

    WakeLockInfo kwlInfo2;
    ASSERT_TRUE(findWakeLockInfoByName(wlStats, fakeKwlName2, &kwlInfo2));
    ASSERT_EQ(kwlInfo2.name, fakeKwlName2);
    ASSERT_EQ(kwlInfo2.activeCount, 10);
    ASSERT_EQ(kwlInfo2.lastChange, 42);
    ASSERT_EQ(kwlInfo2.maxTime, 42);
    ASSERT_EQ(kwlInfo2.totalTime, 42);
    ASSERT_EQ(kwlInfo2.isActive, true);
    ASSERT_EQ(kwlInfo2.activeTime, 42);
    ASSERT_TRUE(kwlInfo2.isKernelWakelock);

    ASSERT_EQ(kwlInfo2.pid, -1);

    ASSERT_EQ(kwlInfo2.eventCount, 42);
    ASSERT_EQ(kwlInfo2.expireCount, 42);
    ASSERT_EQ(kwlInfo2.preventSuspendTime, 42);
    ASSERT_EQ(kwlInfo2.wakeupCount, 42);
}

// Test that getWakeLockStats has correct information about Native AND Kernel WakeLocks.
TEST_F(SystemSuspendSameThreadTest, GetNativeAndKernelWakeLockStats) {
    std::string fakeNwlName = "fakeNwl";
    std::string fakeKwlName = "fakeKwl";

    addKernelWakelock(fakeKwlName);

    {
        sp<IWakeLock> fakeLock = acquireWakeLock(fakeNwlName);
        std::vector<WakeLockInfo> wlStats = getWakelockStats();
        ASSERT_EQ(wlStats.size(), 2);

        // Native Wakelock Stats
        WakeLockInfo nwlInfo;
        ASSERT_TRUE(findWakeLockInfoByName(wlStats, fakeNwlName, &nwlInfo));
        ASSERT_EQ(nwlInfo.name, fakeNwlName);
        ASSERT_EQ(nwlInfo.activeCount, 1);
        ASSERT_EQ(nwlInfo.isActive, true);
        ASSERT_FALSE(nwlInfo.isKernelWakelock);

        ASSERT_EQ(nwlInfo.pid, getpid());

        ASSERT_EQ(nwlInfo.eventCount, 0);
        ASSERT_EQ(nwlInfo.expireCount, 0);
        ASSERT_EQ(nwlInfo.preventSuspendTime, 0);
        ASSERT_EQ(nwlInfo.wakeupCount, 0);

        // Kernel Wakelock Stats
        WakeLockInfo kwlInfo;
        ASSERT_TRUE(findWakeLockInfoByName(wlStats, fakeKwlName, &kwlInfo));
        ASSERT_EQ(kwlInfo.name, fakeKwlName);
        ASSERT_EQ(kwlInfo.activeCount, 42);
        ASSERT_EQ(kwlInfo.lastChange, 42);
        ASSERT_EQ(kwlInfo.maxTime, 42);
        ASSERT_EQ(kwlInfo.totalTime, 42);
        ASSERT_EQ(kwlInfo.isActive, true);
        ASSERT_EQ(kwlInfo.activeTime, 42);
        ASSERT_TRUE(kwlInfo.isKernelWakelock);

        ASSERT_EQ(kwlInfo.pid, -1);

        ASSERT_EQ(kwlInfo.eventCount, 42);
        ASSERT_EQ(kwlInfo.expireCount, 42);
        ASSERT_EQ(kwlInfo.preventSuspendTime, 42);
        ASSERT_EQ(kwlInfo.wakeupCount, 42);

        // We sleep so that the wake lock stats entry get updated with a different timestamp.
        std::this_thread::sleep_for(1s);
    }
    std::vector<WakeLockInfo> wlStats = getWakelockStats();
    ASSERT_EQ(wlStats.size(), 2);

    // Native Wakelock Stats
    WakeLockInfo nwlInfo;
    ASSERT_TRUE(findWakeLockInfoByName(wlStats, fakeNwlName, &nwlInfo));
    ASSERT_EQ(nwlInfo.name, fakeNwlName);
    ASSERT_EQ(nwlInfo.activeCount, 1);
    ASSERT_GE(nwlInfo.maxTime, 1000);
    ASSERT_GE(nwlInfo.totalTime, 1000);
    ASSERT_EQ(nwlInfo.isActive, false);
    ASSERT_EQ(nwlInfo.activeTime, 0);  // No longer active
    ASSERT_FALSE(nwlInfo.isKernelWakelock);

    ASSERT_EQ(nwlInfo.pid, getpid());

    ASSERT_EQ(nwlInfo.eventCount, 0);
    ASSERT_EQ(nwlInfo.expireCount, 0);
    ASSERT_EQ(nwlInfo.preventSuspendTime, 0);
    ASSERT_EQ(nwlInfo.wakeupCount, 0);

    // Kernel Wakelock Stats (No changes expected here)
    WakeLockInfo kwlInfo;
    ASSERT_TRUE(findWakeLockInfoByName(wlStats, fakeKwlName, &kwlInfo));
    ASSERT_EQ(kwlInfo.name, fakeKwlName);
    ASSERT_EQ(kwlInfo.activeCount, 42);
    ASSERT_EQ(kwlInfo.lastChange, 42);
    ASSERT_EQ(kwlInfo.maxTime, 42);
    ASSERT_EQ(kwlInfo.totalTime, 42);
    ASSERT_EQ(kwlInfo.isActive, true);
    ASSERT_EQ(kwlInfo.activeTime, 42);
    ASSERT_TRUE(kwlInfo.isKernelWakelock);

    ASSERT_EQ(kwlInfo.pid, -1);

    ASSERT_EQ(kwlInfo.eventCount, 42);
    ASSERT_EQ(kwlInfo.expireCount, 42);
    ASSERT_EQ(kwlInfo.preventSuspendTime, 42);
    ASSERT_EQ(kwlInfo.wakeupCount, 42);
}

// Test that the least recently used native wake lock stats entry is evicted after a given
// threshold.
TEST_F(SystemSuspendSameThreadTest, NativeWakeLockStatsLruEviction) {
    std::string fakeWlName1 = "FakeLock1";
    std::string fakeWlName2 = "FakeLock2";

    acquireWakeLock(fakeWlName1);
    acquireWakeLock(fakeWlName2);

    std::vector<WakeLockInfo> wlStats = getWakelockStats();

    // Max number of native stats entries was set to 1 in SystemSuspend constructor.
    ASSERT_EQ(wlStats.size(), 1);
    ASSERT_EQ(wlStats.begin()->name, fakeWlName2);

    WakeLockInfo wlInfo;
    ASSERT_TRUE(findWakeLockInfoByName(wlStats, fakeWlName2, &wlInfo));
    ASSERT_FALSE(findWakeLockInfoByName(wlStats, fakeWlName1, &wlInfo));  // Evicted
}

// Test that GetSuspendStats has correct information.
TEST_F(SystemSuspendSameThreadTest, GetSuspendStats) {
    addSuspendStats();

    Result<SuspendStats> res = getSuspendStats();
    ASSERT_RESULT_OK(res);

    SuspendStats stats = res.value();

    ASSERT_EQ(stats.success, 42);
    ASSERT_EQ(stats.fail, 42);
    ASSERT_EQ(stats.failedFreeze, 42);
    ASSERT_EQ(stats.failedPrepare, 42);
    ASSERT_EQ(stats.failedSuspend, 42);
    ASSERT_EQ(stats.failedSuspendLate, 42);
    ASSERT_EQ(stats.failedSuspendNoirq, 42);
    ASSERT_EQ(stats.failedResume, 42);
    ASSERT_EQ(stats.failedResumeEarly, 42);
    ASSERT_EQ(stats.failedResumeNoirq, 42);
    ASSERT_EQ(stats.lastFailedDev, "fakeDev");
    ASSERT_EQ(stats.lastFailedErrno, 42);
    ASSERT_EQ(stats.lastFailedStep, "fakeStep");
}

}  // namespace android

int main(int argc, char** argv) {
    setenv("TREBLE_TESTING_OVERRIDE", "true", true);
    ::testing::InitGoogleMock(&argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
