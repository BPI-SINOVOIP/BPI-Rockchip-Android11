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

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <android-base/format.h>
#include <android-base/stringprintf.h>
#include <android-base/test_utils.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Experiments.h"

namespace android::net {

class ExperimentsTest : public ::testing::Test {
  public:
    ExperimentsTest() : mExperiments(fakeGetExperimentFlagInt) {}

  protected:
    static int fakeGetExperimentFlagInt(const std::string& key, int defaultValue) {
        auto it = sFakeFlagsMapInt.find(key);
        if (it != sFakeFlagsMapInt.end()) {
            return it->second;
        }
        return defaultValue;
    }

    void setupFakeMap(int value) {
        for (const auto& key : Experiments::kExperimentFlagKeyList) {
            sFakeFlagsMapInt[key] = value;
        }
    }

    void setupExperimentsMap(int value) {
        setupFakeMap(value);
        std::lock_guard guard(mExperiments.mMutex);
        mExperiments.mFlagsMapInt = sFakeFlagsMapInt;
    }

    void expectFlagsMapInt() {
        std::lock_guard guard(mExperiments.mMutex);
        EXPECT_THAT(mExperiments.mFlagsMapInt, ::testing::ContainerEq(sFakeFlagsMapInt));
    }

    void expectFlagsMapIntDefault() {
        std::lock_guard guard(mExperiments.mMutex);
        for (const auto& [key, value] : mExperiments.mFlagsMapInt) {
            EXPECT_EQ(value, Experiments::kFlagIntDefault);
        }
    }

    void expectGetDnsExperimentFlagIntDefault(int value) {
        for (const auto& key : Experiments::kExperimentFlagKeyList) {
            EXPECT_EQ(mExperiments.getFlag(key, value), value);
        }
    }

    void expectGetDnsExperimentFlagInt() {
        std::unordered_map<std::string_view, int> tempMap;
        for (const auto& key : Experiments::kExperimentFlagKeyList) {
            tempMap[key] = mExperiments.getFlag(key, 0);
        }
        EXPECT_THAT(tempMap, ::testing::ContainerEq(sFakeFlagsMapInt));
    }

    void expectDumpOutput() {
        netdutils::DumpWriter dw(STDOUT_FILENO);
        CapturedStdout captured;
        mExperiments.dump(dw);
        const std::string dumpString = captured.str();
        const std::string title = "Experiments list:";
        EXPECT_EQ(dumpString.find(title), 0U);
        size_t startPos = title.size();
        std::lock_guard guard(mExperiments.mMutex);
        for (const auto& [key, value] : mExperiments.mFlagsMapInt) {
            std::string flagDump = fmt::format("{}: {}", key, value);
            if (value == Experiments::kFlagIntDefault) {
                flagDump = fmt::format("{}: UNSET", key);
            }
            SCOPED_TRACE(flagDump);
            size_t pos = dumpString.find(flagDump, startPos);
            EXPECT_NE(pos, std::string::npos);
            startPos = pos + flagDump.size();
        }
        EXPECT_EQ(startPos + 1, dumpString.size());
        EXPECT_EQ(dumpString.substr(startPos), "\n");
    }

    static std::unordered_map<std::string_view, int> sFakeFlagsMapInt;
    Experiments mExperiments;
};

std::unordered_map<std::string_view, int> ExperimentsTest::sFakeFlagsMapInt;

TEST_F(ExperimentsTest, update) {
    std::vector<int> testValues = {50, 3, 5, 0};
    for (int testValue : testValues) {
        setupFakeMap(testValue);
        mExperiments.update();
        expectFlagsMapInt();
    }
}

TEST_F(ExperimentsTest, getDnsExperimentFlagInt) {
    std::vector<int> testValues = {5, 1, 6, 0};
    for (int testValue : testValues) {
        setupExperimentsMap(testValue);
        expectGetDnsExperimentFlagInt();
    }
}

TEST_F(ExperimentsTest, getDnsExperimentFlagIntDefaultValue) {
    // Clear the map and make mExperiments initialized with our default int value.
    sFakeFlagsMapInt.clear();
    mExperiments.update();
    expectFlagsMapIntDefault();
    std::vector<int> testValues = {100, 50, 30, 5};
    for (int testValue : testValues) {
        expectGetDnsExperimentFlagIntDefault(testValue);
    }
}

TEST_F(ExperimentsTest, dump) {
    std::vector<int> testValues = {100, 37, 0, 30};
    for (int testValue : testValues) {
        setupFakeMap(testValue);
        mExperiments.update();
        expectDumpOutput();
    }
    sFakeFlagsMapInt.clear();
    mExperiments.update();
    expectDumpOutput();
}

}  // namespace android::net
