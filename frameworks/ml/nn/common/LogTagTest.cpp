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

#define LOG_TAG "MainFileTag"

#include <android-base/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "Utils.h"

namespace log_tag_test {

// In an extra file with different log tag
void generateLogOtherTag();

bool generateLog() {
    NN_RET_CHECK_FAIL() << "Forcing failure to validate log tag";
}

class LogTagTest : public ::testing::Test {
   protected:
    ~LogTagTest() override { ::android::base::SetLogger(::android::base::LogdLogger()); }
};

TEST_F(LogTagTest, NnRetCheckFailMacroReturnsFalse) {
    EXPECT_FALSE(generateLog());
}

TEST_F(LogTagTest, EachFileLogTagIsCaptured) {
    android::base::SetLogger([](android::base::LogId /* logId */,
                                android::base::LogSeverity /* logSeverity */, const char* tag,
                                const char* /*file*/, unsigned int /*line*/,
                                const char* /*message*/) {
        EXPECT_STREQ(tag, "MainFileTag") << "Tag for this file has not been used";
    });
    generateLog();

    android::base::SetLogger([](android::base::LogId /* logId */,
                                android::base::LogSeverity /* logSeverity */, const char* tag,
                                const char* /*file*/, unsigned int /*line*/,
                                const char* /*message*/) {
        EXPECT_STREQ(tag, "SecondFileTag") << "Tag for the second test file has not been used";
    });
    generateLogOtherTag();
}

TEST_F(LogTagTest, LogIsAtErrorLevel) {
    android::base::SetLogger(
            [](android::base::LogId /* logId */, android::base::LogSeverity logSeverity,
               const char* /* tag */, const char* /* file */, unsigned int /*line*/,
               const char* /* message */) { EXPECT_EQ(logSeverity, ::android::base::ERROR); });

    generateLog();
}

TEST_F(LogTagTest, LogContainsCommonMessage) {
    android::base::SetLogger([](android::base::LogId /* logId */,
                                android::base::LogSeverity /* logSeverity */, const char* /* tag */,
                                const char* /* file */, unsigned int /*line*/,
                                const char* message) {
        EXPECT_THAT(message, testing::MatchesRegex("NN_RET_CHECK failed.+"));
    });

    generateLog();
}

TEST_F(LogTagTest, ErrnoIsRestoredAfterLogging) {
    android::base::SetLogger([](android::base::LogId, android::base::LogSeverity,
                                const char* /*tag*/, const char* /*file*/, unsigned int /*line*/,
                                const char* /*message*/) { errno = -1; });

    const int kTestErrno = 56;
    errno = kTestErrno;
    generateLog();
    EXPECT_EQ(errno, kTestErrno);
}
}  // namespace log_tag_test
