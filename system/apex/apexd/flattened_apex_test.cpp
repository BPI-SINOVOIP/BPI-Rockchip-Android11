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

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#include <gtest/gtest.h>
#include <filesystem>
#include <memory>

#include <ApexProperties.sysprop.h>
#include "apex_constants.h"
#include "apex_shim.h"
#include "apexd_utils.h"
#include "string_log.h"

namespace android {
namespace apex {

TEST(FlattenedApexTest, SysPropIsFalse) {
  bool value = android::sysprop::ApexProperties::updatable().value_or(false);
  ASSERT_FALSE(value);
}

TEST(FlattenedApexTest, ApexFilesAreFlattened) {
  namespace fs = std::filesystem;
  auto assert_is_dir = [&](const fs::directory_entry& entry) {
    if (entry.path().filename() == shim::kSystemShimApexName) {
      return;
    }
    std::error_code ec;
    bool is_dir = entry.is_directory(ec);
    ASSERT_FALSE(ec) << ec.message();
    ASSERT_TRUE(is_dir) << entry.path() << " is not a directory";
  };
  WalkDir(kApexPackageSystemDir, assert_is_dir);
}

TEST(FlattenedApexTest, MountsAreCorrect) {
  using android::base::ReadFileToString;
  using android::base::Split;
  std::string mounts;
  ASSERT_TRUE(ReadFileToString("/proc/self/mountinfo", &mounts));
  bool has_apex_mount = false;
  for (const auto& mount : Split(mounts, "\n")) {
    const std::vector<std::string>& tokens = Split(mount, " ");
    // line format:
    // mnt_id parent_mnt_id major:minor source target option propagation_type
    if (tokens.size() < 7) {
      continue;
    }
    const std::string& source = tokens[3];
    const std::string& target = tokens[4];
    if (source == kApexPackageSystemDir && target == "/apex") {
      has_apex_mount = true;
    }
  }
  ASSERT_TRUE(has_apex_mount) << "Failed to find apex mount point";
}

TEST(FlattenedApexTest, ApexdIsNotRunning) {
  constexpr const char* kCmd = "pidof -s apexd";
  std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(kCmd, "r"), pclose);
  ASSERT_NE(nullptr, pipe) << "Failed to open pipe for: " << kCmd;
  char buf[1024];
  if (fgets(buf, 1024, pipe.get()) != nullptr) {
    FAIL() << "apexd is running and has pid " << buf;
  }
}

}  // namespace apex
}  // namespace android

int main(int argc, char** argv) {
  android::base::InitLogging(argv, &android::base::StderrLogger);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
