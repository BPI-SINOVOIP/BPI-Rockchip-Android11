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

#include <android/system/suspend/ISuspendControlService.h>
#include <binder/IServiceManager.h>
#include <gtest/gtest.h>
#include <hardware_legacy/power.h>
#include <wakelock/wakelock.h>

#include <csignal>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

using android::sp;
using android::system::suspend::ISuspendControlService;
using android::system::suspend::WakeLockInfo;
using namespace std::chrono_literals;

namespace android {

// Test acquiring/releasing WakeLocks concurrently with process exit.
TEST(LibpowerTest, ProcessExitTest) {
    std::atexit([] {
        // We want to give the other thread enough time trigger a failure and
        // dump the stack traces.
        std::this_thread::sleep_for(1s);
    });

    ASSERT_EXIT(
    {
        constexpr int numThreads = 20;
        std::vector<std::thread> tds;
        for (int i = 0; i < numThreads; i++) {
            tds.emplace_back([] {
            while (true) {
                // We want ids to be unique.
                std::string id = std::to_string(rand());
                ASSERT_EQ(acquire_wake_lock(PARTIAL_WAKE_LOCK, id.c_str()), 0);
                ASSERT_EQ(release_wake_lock(id.c_str()), 0);
            }
        });
        }
        for (auto& td : tds) {
            td.detach();
        }

        // Give some time for the threads to actually start.
        std::this_thread::sleep_for(100ms);
        std::exit(0);
    },
    ::testing::ExitedWithCode(0), "");
}

// Stress test acquiring/releasing WakeLocks.
TEST(LibpowerTest, WakeLockStressTest) {
    // numThreads threads will acquire/release numLocks locks each.
    constexpr int numThreads = 20;
    constexpr int numLocks = 1000;
    std::vector<std::thread> tds;

    for (int i = 0; i < numThreads; i++) {
        tds.emplace_back([i] {
            for (int j = 0; j < numLocks; j++) {
                // We want ids to be unique.
                std::string id = std::to_string(i) + "/" + std::to_string(j);
                ASSERT_EQ(acquire_wake_lock(PARTIAL_WAKE_LOCK, id.c_str()), 0)
                    << "id: " << id;
                ASSERT_EQ(release_wake_lock(id.c_str()), 0)
                    << "id: " << id;;
            }
        });
    }
    for (auto& td : tds) {
        td.join();
    }
}

class WakeLockTest : public ::testing::Test {
   public:
    virtual void SetUp() override {
        sp<IBinder> control =
            android::defaultServiceManager()->getService(android::String16("suspend_control"));
        ASSERT_NE(control, nullptr) << "failed to get the suspend control service";
        controlService = interface_cast<ISuspendControlService>(control);
    }

    // Returns true iff found.
    bool findWakeLockInfoByName(const sp<ISuspendControlService>& service, const std::string& name,
                                WakeLockInfo* info) {
        std::vector<WakeLockInfo> wlStats;
        service->getWakeLockStats(&wlStats);
        auto it = std::find_if(wlStats.begin(), wlStats.end(),
                               [&name](const auto& x) { return x.name == name; });
        if (it != wlStats.end()) {
            *info = *it;
            return true;
        }
        return false;
    }

    // All userspace wake locks are registered with system suspend.
    sp<ISuspendControlService> controlService;
};

// Test RAII properties of WakeLock destructor.
TEST_F(WakeLockTest, WakeLockDestructor) {
    auto name = std::to_string(rand());
    {
        android::wakelock::WakeLock wl{name};

        WakeLockInfo info;
        auto success = findWakeLockInfoByName(controlService, name, &info);
        ASSERT_TRUE(success);
        ASSERT_EQ(info.name, name);
        ASSERT_EQ(info.pid, getpid());
        ASSERT_TRUE(info.isActive);
    }

    // SystemSuspend receives wake lock release requests on hwbinder thread, while stats requests
    // come on binder thread. Sleep to make sure that stats are reported *after* wake lock release.
    std::this_thread::sleep_for(1ms);
    WakeLockInfo info;
    auto success = findWakeLockInfoByName(controlService, name, &info);
    ASSERT_TRUE(success);
    ASSERT_EQ(info.name, name);
    ASSERT_EQ(info.pid, getpid());
    ASSERT_FALSE(info.isActive);
}

}  // namespace android
