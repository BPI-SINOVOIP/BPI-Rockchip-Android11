// Copyright (C) 2019 The Android Open Source Project
// Copyright (C) 2019 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <gtest/gtest.h>

#include "android/base/synchronization/AndroidConditionVariable.h"
#include "android/base/synchronization/AndroidLock.h"
#include "android/base/threads/AndroidWorkPool.h"

#include <atomic>
#include <vector>

namespace android {
namespace base {
namespace guest {

// Tests basic default construction/deconstruction.
TEST(WorkPool, Basic) {
    WorkPool p;
}

// Tests sending one task.
TEST(WorkPool, One) {
    WorkPool p;

    WorkPool::Task task = [] {
        fprintf(stderr, "do something\n");
    };

    std::vector<WorkPool::Task> tasks { task };

    p.schedule(tasks);
}

// Tests sending two tasks.
TEST(WorkPool, Two) {
    WorkPool p;

    std::vector<WorkPool::Task> tasks {
        [] { fprintf(stderr, "do something 1\n"); },
        [] { fprintf(stderr, "do something 2\n"); },
    };

    p.schedule(tasks);
}

// Tests sending eight tasks (can require spawning more threads)
TEST(WorkPool, Eight) {
    WorkPool p;

    std::vector<WorkPool::Task> tasks {
        [] { fprintf(stderr, "do something 1\n"); },
        [] { fprintf(stderr, "do something 2\n"); },
        [] { fprintf(stderr, "do something 3\n"); },
        [] { fprintf(stderr, "do something 4\n"); },
        [] { fprintf(stderr, "do something 5\n"); },
        [] { fprintf(stderr, "do something 6\n"); },
        [] { fprintf(stderr, "do something 7\n"); },
        [] { fprintf(stderr, "do something 8\n"); },
    };

    p.schedule(tasks);
}

// Tests waitAny primitive; if at least one of the tasks successfully run,
// at least one of them will read 0 and store back 1 in |x|, or more,
// so check that x >= 1.
TEST(WorkPool, WaitAny) {
    WorkPool p;
    int x = 0;
    WorkPool::WaitGroupHandle handle = 0;

    {
        std::vector<WorkPool::Task> tasks;

        for (int i = 0; i < 8; ++i) {
            tasks.push_back([&x] { ++x; });
        }

        handle = p.schedule(tasks);
    }


    p.waitAny(handle, -1);

    EXPECT_GE(x, 1);

    // Prevent use after scope after test finish
    p.waitAll(handle);
}

// Tests waitAll primitive; each worker increments the atomic int once,
// so we expect it to end up at 8 (8 workers).
TEST(WorkPool, WaitAll) {
    WorkPool p;
    std::atomic<int> x { 0 };

    std::vector<WorkPool::Task> tasks;

    for (int i = 0; i < 8; ++i) {
        tasks.push_back([&x] { ++x; });
    }

    auto handle = p.schedule(tasks);

    p.waitAll(handle, -1);

    EXPECT_EQ(x, 8);
}

// Tests waitAll primitive with two concurrent wait groups in flight.
// The second wait group is scheduled after the first, but
// we wait on the second wait group first. This is to ensure that
// order of submission does not enforce order of waiting / completion.
TEST(WorkPool, WaitAllTwoWaitGroups) {
    WorkPool p;
    std::atomic<int> x { 0 };
    std::atomic<int> y { 0 };

    std::vector<WorkPool::Task> tasks1;
    std::vector<WorkPool::Task> tasks2;

    for (int i = 0; i < 8; ++i) {
        tasks1.push_back([&x] { ++x; });
        tasks2.push_back([&y] { ++y; });
    }

    auto handle1 = p.schedule(tasks1);
    auto handle2 = p.schedule(tasks2);

    p.waitAll(handle2, -1);
    p.waitAll(handle1, -1);

    EXPECT_EQ(x, 8);
    EXPECT_EQ(y, 8);
}

// Tests waitAll primitive with two concurrent wait groups.
// The first wait group waits on what the second wait group will signal.
// This is to ensure that we can send blocking tasks to WorkPool
// without causing a deadlock.
TEST(WorkPool, WaitAllWaitSignal) {
    WorkPool p;
    Lock lock;
    ConditionVariable cv;
    // Similar to a timeline semaphore object;
    // one process waits on a particular value to get reached,
    // while other processes gradually increment it.
    std::atomic<int> x { 0 };

    std::vector<WorkPool::Task> tasks1 = {
        [&lock, &cv, &x] {
            AutoLock l(lock);
            while (x < 8) {
                cv.wait(&lock);
            }
        },
    };

    std::vector<WorkPool::Task> tasks2;

    for (int i = 0; i < 8; ++i) {
        tasks2.push_back([&lock, &cv, &x] {
            AutoLock l(lock);
            ++x;
            cv.signal();
        });
    }

    auto handle1 = p.schedule(tasks1);
    auto handle2 = p.schedule(tasks2);

    p.waitAll(handle1, -1);

    EXPECT_EQ(8, x);
}

// Tests waitAll primitive with some kind of timeout.
// We don't expect x to be anything in particular..
TEST(WorkPool, WaitAllTimeout) {
    WorkPool p;
    Lock lock;
    ConditionVariable cv;
    std::atomic<int> x { 0 };

    std::vector<WorkPool::Task> tasks1 = {
        [&lock, &cv, &x] {
            AutoLock l(lock);
            while (x < 8) {
                cv.wait(&lock);
            }
        },
    };

    std::vector<WorkPool::Task> tasks2;

    for (int i = 0; i < 8; ++i) {
        tasks2.push_back([&lock, &cv, &x] {
            AutoLock l(lock);
            ++x;
            cv.signal();
        });
    }

    auto handle1 = p.schedule(tasks1);
    auto handle2 = p.schedule(tasks2);

    p.waitAll(handle1, 10);
}

// Tests waitAny primitive with some kind of timeout.
// We don't expect x to be anything in particular..
TEST(WorkPool, WaitAnyTimeout) {
    WorkPool p;
    Lock lock;
    ConditionVariable cv;
    std::atomic<int> x { 0 };

    std::vector<WorkPool::Task> tasks1 = {
        [&lock, &cv, &x] {
            AutoLock l(lock);
            while (x < 8) {
                cv.wait(&lock);
            }
        },
    };

    std::vector<WorkPool::Task> tasks2;

    for (int i = 0; i < 8; ++i) {
        tasks2.push_back([&lock, &cv, &x] {
            AutoLock l(lock);
            ++x;
            cv.signal();
        });
    }

    auto handle1 = p.schedule(tasks1);
    auto handle2 = p.schedule(tasks2);

    p.waitAny(handle1, 10);
}

// Nesting waitAll inside another task.
TEST(WorkPool, NestedWaitAll) {
    WorkPool p;
    std::atomic<int> x { 0 };
    std::atomic<int> y { 0 };

    std::vector<WorkPool::Task> tasks1;

    for (int i = 0; i < 8; ++i) {
        tasks1.push_back([&x] {
            ++x;
        });
    }

    auto waitGroupHandle = p.schedule(tasks1);

    std::vector<WorkPool::Task> tasks2 = {
        [&p, waitGroupHandle, &x, &y] {
            p.waitAll(waitGroupHandle);
            EXPECT_EQ(8, x);
            ++y;
        },
    };

    auto handle2 = p.schedule(tasks2);

    p.waitAll(handle2);

    EXPECT_EQ(1, y);
}

} // namespace android
} // namespace base
} // namespace guest
