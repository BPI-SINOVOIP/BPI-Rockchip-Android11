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

#include <array>
#include <cstdio>
#include <fstream>
#include <string>

#include <android-base/properties.h>
#include <android/api-level.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace android {
namespace kernel {

class KernelLoopConfigTest : public ::testing::Test {
 protected:
  const int first_api_level_;
  KernelLoopConfigTest()
      : first_api_level_(std::stoi(
            android::base::GetProperty("ro.product.first_api_level", "0"))) {}
  bool should_run() const {
    // TODO check for APEX support (for upgrading devices)
    return first_api_level_ >= __ANDROID_API_Q__;
  }
};

TEST_F(KernelLoopConfigTest, ValidLoopConfig) {
  if (!should_run()) return;

  static constexpr const char* kCmd =
      "zcat /proc/config.gz | grep CONFIG_BLK_DEV_LOOP_MIN_COUNT";
  std::array<char, 256> line;

  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(kCmd, "r"), pclose);
  ASSERT_NE(pipe, nullptr);

  auto read = fgets(line.data(), line.size(), pipe.get());
  ASSERT_NE(read, nullptr);

  auto minCountStr = std::string(read);

  auto pos = minCountStr.find("=");
  ASSERT_NE(pos, std::string::npos);
  ASSERT_GE(minCountStr.length(), pos + 1);

  int minCountValue = std::stoi(minCountStr.substr(pos + 1));
  ASSERT_GE(minCountValue, 16);
}

TEST_F(KernelLoopConfigTest, ValidLoopParameters) {
  if (!should_run()) return;

  std::ifstream max_loop("/sys/module/loop/parameters/max_loop");
  std::ifstream max_part("/sys/module/loop/parameters/max_part");

  std::string max_loop_str;
  std::string max_part_str;

  std::getline(max_loop, max_loop_str);
  std::getline(max_part, max_part_str);

  int max_part_value = std::stoi(max_part_str);
  EXPECT_LE(max_part_value, 7);

  int max_loop_value = std::stoi(max_loop_str);
  EXPECT_EQ(0, max_loop_value);
}

}  // namespace kernel
}  // namespace android
