/*
 * Copyright 2020 The Android Open Source Project
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

#include "UidIoStats.h"

#include <android-base/file.h>

#include <unordered_map>

#include "gmock/gmock.h"

namespace android {
namespace automotive {
namespace watchdog {

using ::android::base::WriteStringToFile;

TEST(UidIoStatsTest, TestValidStatFile) {
    // Format: uid fgRdChar fgWrChar fgRdBytes fgWrBytes bgRdChar bgWrChar bgRdBytes bgWrBytes
    // fgFsync bgFsync
    constexpr char firstSnapshot[] =
        "1001234 5000 1000 3000 500 0 0 0 0 20 0\n"
        "1005678 500 100 30 50 300 400 100 200 45 60\n"
        "1009 0 0 0 0 40000 50000 20000 30000 0 300\n"
        "1001000 4000 3000 2000 1000 400 300 200 100 50 10\n";
    std::unordered_map<uint32_t, UidIoUsage> expectedFirstUsage = {
        {1001234,
         {.uid = 1001234,
          .ios = {/*fgRdBytes=*/3000, /*bgRdBytes=*/0, /*fgWrBytes=*/500,
                  /*bgWrBytes=*/0, /*fgFsync=*/20, /*bgFsync=*/0}}},
        {1005678, {.uid = 1005678, .ios = {30, 100, 50, 200, 45, 60}}},
        {1009, {.uid = 1009, .ios = {0, 20000, 0, 30000, 0, 300}}},
        {1001000, {.uid = 1001000, .ios = {2000, 200, 1000, 100, 50, 10}}},
    };
    TemporaryFile tf;
    ASSERT_NE(tf.fd, -1);
    ASSERT_TRUE(WriteStringToFile(firstSnapshot, tf.path));

    UidIoStats uidIoStats(tf.path);
    ASSERT_TRUE(uidIoStats.enabled()) << "Temporary file is inaccessible";

    const auto& actualFirstUsage = uidIoStats.collect();
    EXPECT_TRUE(actualFirstUsage) << actualFirstUsage.error();
    EXPECT_EQ(expectedFirstUsage.size(), actualFirstUsage->size());
    for (const auto& it : expectedFirstUsage) {
        if (actualFirstUsage->find(it.first) == actualFirstUsage->end()) {
            ADD_FAILURE() << "Expected uid " << it.first << " not found in the first snapshot";
        }
        const UidIoUsage& expected = it.second;
        const UidIoUsage& actual = actualFirstUsage->at(it.first);
        EXPECT_EQ(expected.uid, actual.uid);
        EXPECT_EQ(expected.ios, actual.ios)
            << "Unexpected I/O usage for uid " << it.first << " in first snapshot.\nExpected:\n"
            << expected.ios.toString() << "\nActual:\n"<< actual.ios.toString();
    }

    constexpr char secondSnapshot[] =
        "1001234 10000 2000 7000 950 0 0 0 0 45 0\n"
        "1005678 600 100 40 50 1000 1000 1000 600 50 70\n"
        "1003456 300 500 200 300 0 0 0 0 50 0\n"
        "1001000 400 300 200 100 40 30 20 10 5 1\n";
    std::unordered_map<uint32_t, UidIoUsage> expectedSecondUsage = {
        {1001234,
         {.uid = 1001234,
          .ios = {/*fgRdBytes=*/4000, /*bgRdBytes=*/0,
                  /*fgWrBytes=*/450, /*bgWrBytes=*/0, /*fgFsync=*/25,
                  /*bgFsync=*/0}}},
        {1005678, {.uid = 1005678, .ios = {10, 900, 0, 400, 5, 10}}},
        {1003456, {.uid = 1003456, .ios = {200, 0, 300, 0, 50, 0}}},
        {1001000, {.uid = 1001000, .ios = {0, 0, 0, 0, 0, 0}}},
    };
    ASSERT_TRUE(WriteStringToFile(secondSnapshot, tf.path));
    const auto& actualSecondUsage = uidIoStats.collect();
    EXPECT_TRUE(actualSecondUsage) << actualSecondUsage.error();
    EXPECT_EQ(expectedSecondUsage.size(), actualSecondUsage->size());
    for (const auto& it : expectedSecondUsage) {
        if (actualSecondUsage->find(it.first) == actualSecondUsage->end()) {
            ADD_FAILURE() << "Expected uid " << it.first << " not found in the second snapshot";
        }
        const UidIoUsage& expected = it.second;
        const UidIoUsage& actual = actualSecondUsage->at(it.first);
        EXPECT_EQ(expected.uid, actual.uid);
        EXPECT_EQ(expected.ios, actual.ios)
            << "Unexpected I/O usage for uid " << it.first << " in second snapshot:.\nExpected:\n"
            << expected.ios.toString() << "\nActual:\n"<< actual.ios.toString();
    }
}

TEST(UidIoStatsTest, TestErrorOnInvalidStatFile) {
    // Format: uid fgRdChar fgWrChar fgRdBytes fgWrBytes bgRdChar bgWrChar bgRdBytes bgWrBytes
    // fgFsync bgFsync
    constexpr char contents[] =
        "1001234 5000 1000 3000 500 0 0 0 0 20 0\n"
        "1005678 500 100 30 50 300 400 100 200 45 60\n"
        "1009012 0 0 0 0 40000 50000 20000 30000 0 300\n"
        "1001000 4000 3000 2000 1000 CORRUPTED DATA\n";
    TemporaryFile tf;
    ASSERT_NE(tf.fd, -1);
    ASSERT_TRUE(WriteStringToFile(contents, tf.path));

    UidIoStats uidIoStats(tf.path);
    ASSERT_TRUE(uidIoStats.enabled()) << "Temporary file is inaccessible";
    EXPECT_FALSE(uidIoStats.collect()) << "No error returned for invalid file";
}

}  // namespace watchdog
}  // namespace automotive
}  // namespace android
