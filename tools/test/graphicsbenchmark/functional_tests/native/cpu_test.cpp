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
 *
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cpu-features.h>
#include <iostream>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <vector>
#include <cstdlib>
#include "testutils.h"

/**
 * Call to sched_setaffinity must be repected.
 */
TEST(cpu, sched_setaffinity) {
    ASSUME_GAMECORE_CERTIFIED();

    int cpu_count =  android_getCpuCount();
    for (int cpu = 0; cpu < cpu_count; ++cpu) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(cpu, &set);
        int rc = sched_setaffinity(0, sizeof(cpu_set_t), &set);
        ASSERT_EQ(0, rc) << "sched_setaffinity failed. error = " << errno;
        ASSERT_EQ(cpu, sched_getcpu()) << "sched_setaffinity was not respected.";
    }
}

TEST(cpu, sched_setaffinity_multiple_cpu) {
    ASSUME_GAMECORE_CERTIFIED();

    int cpu_count =  android_getCpuCount();

    std::vector<std::vector<int>> data {
            {0, 1},
            {2, 3, 4, 5},
            {6, 7},
            {0, 1, 2, 3},
            {4, 5, 6, 7},
            {0, cpu_count - 1}};

    for (auto test_data : data) {
        cpu_set_t set;
        CPU_ZERO(&set);
        for (int i = 0; i < test_data.size(); ++i) {
            auto cpu = test_data[i];
            if (cpu >= cpu_count) {
                cpu = cpu_count - 1;
                test_data[i] = cpu;
            }
            cpu_set_t other_set;
            CPU_ZERO(&other_set);
            CPU_SET(cpu, &other_set);
            CPU_OR(&set, &set, &other_set);
        }
        int rc = sched_setaffinity(0, sizeof(cpu_set_t), &set);
        ASSERT_EQ(0, rc) << "sched_setaffinity failed. error = " << errno;
        ASSERT_THAT(test_data, ::testing::Contains(sched_getcpu()))
                << "sched_setaffinity was not respected.";
    }
}
