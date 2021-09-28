/*
 * Copyright (C) 2019, The Android Open Source Project
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

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>

#include <unistd.h>

#include <android/hidl/manager/1.2/IServiceManager.h>
#include <gtest/gtest.h>
#include <hidl-util/FqInstance.h>
#include <hidl/HidlSupport.h>
#include <hidl/HidlTransportSupport.h>
#include <hidl/HidlTransportUtils.h>
#include <hwbinder/IPCThreadState.h>

using ::android::FqInstance;
using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::IPCThreadState;
using ::android::hidl::base::V1_0::IBase;
using ::android::hidl::manager::V1_2::IServiceManager;

static FqInstance gInstance;

sp<IBase> getHal() {
    return ::android::hardware::details::getRawServiceInternal(gInstance.getFqName().string(),
                                                               gInstance.getInstance(),
                                                               true /*retry*/, false /*getStub*/);
}

class HidlLazyTest : public ::testing::Test {
  protected:
    sp<IServiceManager> manager;

    void SetUp() override {
        manager = IServiceManager::getService();
        ASSERT_NE(manager, nullptr);

        ASSERT_FALSE(isServiceRunning())
                << "Service '" << gInstance.string() << "' is already running. Please ensure this "
                << "service is implemented as a lazy HAL, then kill all "
                << "clients of this service and try again.";
    }

    static constexpr size_t SHUTDOWN_WAIT_TIME = 10;
    void TearDown() override {
        std::cout << "Waiting " << SHUTDOWN_WAIT_TIME << " seconds before checking that the "
                  << "service has shut down." << std::endl;
        IPCThreadState::self()->flushCommands();
        sleep(SHUTDOWN_WAIT_TIME);
        ASSERT_FALSE(isServiceRunning()) << "Service failed to shutdown.";
    }

    bool isServiceRunning() {
        bool isRunning = false;
        EXPECT_TRUE(
                manager->listByInterface(gInstance.getFqName().string(),
                                         [&isRunning](const hidl_vec<hidl_string>& instanceNames) {
                                             for (const hidl_string& name : instanceNames) {
                                                 if (name == gInstance.getInstance()) {
                                                     isRunning = true;
                                                     break;
                                                 }
                                             }
                                         })
                        .isOk());
        return isRunning;
    }
};

static constexpr size_t NUM_IMMEDIATE_GET_UNGETS = 100;
TEST_F(HidlLazyTest, GetUnget) {
    for (size_t i = 0; i < NUM_IMMEDIATE_GET_UNGETS; i++) {
        IPCThreadState::self()->flushCommands();
        sp<IBase> hal = getHal();
        ASSERT_NE(hal.get(), nullptr);
        EXPECT_TRUE(hal->ping().isOk());
    }
}

static std::vector<size_t> waitTimes(size_t numTimes, size_t maxWait) {
    std::vector<size_t> times(numTimes);
    for (size_t i = 0; i < numTimes; i++) {
        times[i] = (size_t)(rand() % (maxWait + 1));
    }
    return times;
}

static void testWithTimes(const std::vector<size_t>& waitTimes) {
    std::cout << "Note runtime expected from sleeps: "
              << std::accumulate(waitTimes.begin(), waitTimes.end(), 0) << " second(s)."
              << std::endl;

    for (size_t sleepTime : waitTimes) {
        IPCThreadState::self()->flushCommands();
        std::cout << "Thread waiting " << sleepTime << " while not holding HAL." << std::endl;
        sleep(sleepTime);
        sp<IBase> hal = getHal();
        ASSERT_NE(hal.get(), nullptr);
        ASSERT_TRUE(hal->ping().isOk());
    }
}

static constexpr size_t NUM_TIMES_GET_UNGET = 5;
static constexpr size_t MAX_WAITING_DURATION = 10;
static constexpr size_t NUM_CONCURRENT_THREADS = 5;
TEST_F(HidlLazyTest, GetWithWaitConcurrent) {
    std::vector<std::vector<size_t>> threadWaitTimes(NUM_CONCURRENT_THREADS);

    for (size_t i = 0; i < threadWaitTimes.size(); i++) {
        threadWaitTimes[i] = waitTimes(NUM_TIMES_GET_UNGET, MAX_WAITING_DURATION);
    }

    std::vector<std::thread> threads(NUM_CONCURRENT_THREADS);
    for (size_t i = 0; i < threads.size(); i++) {
        threads[i] = std::thread(testWithTimes, threadWaitTimes[i]);
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    srand(time(nullptr));

    std::string fqInstance;

    if (argc == 1) {
        fqInstance = "android.hardware.tests.lazy@1.0::ILazy/default";
    } else if (argc == 2) {
        fqInstance = argv[1];
    } else {
        std::cerr << "Usage: lazy_test fqinstance" << std::endl;
        return 1;
    }

    if (!gInstance.setTo(fqInstance)) {
        std::cerr << "Invalid fqinstance: " << fqInstance << std::endl;
        return 1;
    }

    return RUN_ALL_TESTS();
}
