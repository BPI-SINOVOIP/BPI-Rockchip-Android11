/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <string>

#include <android-base/expected.h>
#include <gtest/gtest.h>
#include <netdutils/ThreadUtil.h>

namespace android::netdutils {

namespace {

android::base::expected<std::string, int> getThreadName() {
    char name[16] = {};
    if (const int ret = pthread_getname_np(pthread_self(), name, sizeof(name)); ret != 0) {
        return android::base::unexpected(ret);
    }
    return std::string(name);
}

class NoopRun {
  public:
    explicit NoopRun(const std::string& name = "") : mName(name) { instanceNum++; }

    // Destructor happens in the thread.
    ~NoopRun() {
        if (checkName) {
            auto expected = getThreadName();
            EXPECT_TRUE(expected.has_value());
            EXPECT_EQ(mExpectedName, expected.value());
        }
        instanceNum--;
    }

    void run() {}

    std::string threadName() { return mName; }

    // Set the expected thread name which will be used to check if it matches the actual thread
    // name which is returned from the system call. The check will happen in the destructor.
    void setExpectedName(const std::string& expectedName) {
        checkName = true;
        mExpectedName = expectedName;
    }

    static bool waitForAllReleased(int timeoutMs) {
        constexpr int intervalMs = 20;
        int limit = timeoutMs / intervalMs;
        for (int i = 1; i < limit; i++) {
            if (instanceNum == 0) {
                return true;
            }
            usleep(intervalMs * 1000);
        }
        return false;
    }

    // To track how many instances are alive.
    static std::atomic<int> instanceNum;

  private:
    std::string mName;
    std::string mExpectedName;
    bool checkName = false;
};

std::atomic<int> NoopRun::instanceNum;

}  // namespace

TEST(ThreadUtilTest, objectReleased) {
    NoopRun::instanceNum = 0;
    NoopRun* obj = new NoopRun();
    EXPECT_EQ(1, NoopRun::instanceNum);
    threadLaunch(obj);

    // Wait for the object released along with the thread exited.
    EXPECT_TRUE(NoopRun::waitForAllReleased(1000));
    EXPECT_EQ(0, NoopRun::instanceNum);
}

TEST(ThreadUtilTest, SetThreadName) {
    NoopRun::instanceNum = 0;

    // Test thread name empty.
    NoopRun* obj1 = new NoopRun();
    obj1->setExpectedName("");

    // Test normal case.
    NoopRun* obj2 = new NoopRun("TestName");
    obj2->setExpectedName("TestName");

    // Test thread name too long.
    std::string name("TestNameTooooLong");
    NoopRun* obj3 = new NoopRun(name);
    obj3->setExpectedName(name.substr(0, 15));

    // Thread names are examined in their destructors.
    EXPECT_EQ(3, NoopRun::instanceNum);
    threadLaunch(obj1);
    threadLaunch(obj2);
    threadLaunch(obj3);

    EXPECT_TRUE(NoopRun::waitForAllReleased(1000));
    EXPECT_EQ(0, NoopRun::instanceNum);
}

}  // namespace android::netdutils
