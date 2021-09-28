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

#include <fstream>
#include <string>

#include <android-base/properties.h>
#include <android/api-level.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

namespace android {
namespace kernel {

class KernelHeadersTest : public ::testing::Test {
 protected:
  std::string version_;
  const int first_api_level_;
  KernelHeadersTest()
      : first_api_level_(std::stoi(
            android::base::GetProperty("ro.product.first_api_level", "0"))) {
  }

  bool should_run(struct utsname buf) const {
    int kernel_version_major, kernel_version_minor, ret;
    char dummy;
    bool kver_pass = false;

    ret = sscanf(buf.release, "%d.%d%c", &kernel_version_major, &kernel_version_minor, &dummy);

    // If kernel version format changes in future, run it anyway
    if (ret < 2)
      kver_pass = true;
    else if (kernel_version_major > 4 || (kernel_version_major == 4 && kernel_version_minor >= 14))
      kver_pass = true;

    return ((first_api_level_ > __ANDROID_API_Q__) && (kver_pass == true));
  }
};

TEST_F(KernelHeadersTest, UnameWorks) {
  struct utsname buf;

  // Make sure uname works since we need it in this test
  ASSERT_EQ(0, uname(&buf));
}

TEST_F(KernelHeadersTest, KheadersExist) {
  struct stat st;
  struct utsname buf;
  std::string path = "/sys/kernel/kheaders.tar.xz";

  uname(&buf);
  if (!should_run(buf)) return;

  // Make sure the kheaders are available
  errno = 0;
  stat(path.c_str(), &st);
  ASSERT_EQ(0, (errno == ENOENT));
}

}  // namespace kernel
}  // namespace android
