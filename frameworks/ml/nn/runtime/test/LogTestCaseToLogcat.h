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

#ifndef ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_LOG_TEST_CASE_TO_LOGCAT_H
#define ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_LOG_TEST_CASE_TO_LOGCAT_H

#include <android-base/logging.h>
#include <gtest/gtest.h>

namespace android::nn {

class LogTestCaseToLogcat : public ::testing::EmptyTestEventListener {
    virtual void OnTestStart(const ::testing::TestInfo& test_info) {
        LOG(INFO) << "[Test Case] " << test_info.test_suite_name() << "." << test_info.name()
                  << " BEGIN";
    }

    virtual void OnTestEnd(const ::testing::TestInfo& test_info) {
        LOG(INFO) << "[Test Case] " << test_info.test_suite_name() << "." << test_info.name()
                  << " END";
    }
};

}  // namespace android::nn

#endif  // ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_LOG_TEST_CASE_TO_LOGCAT_H
