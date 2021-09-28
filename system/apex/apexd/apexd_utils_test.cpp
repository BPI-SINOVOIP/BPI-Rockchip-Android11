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

#include <filesystem>
#include <string>

#include <errno.h>

#include <android-base/file.h>
#include <android-base/result.h>
#include <android-base/stringprintf.h>
#include <gtest/gtest.h>

#include "apexd_test_utils.h"
#include "apexd_utils.h"

namespace android {
namespace apex {
namespace {

using android::apex::testing::IsOk;
using android::base::Basename;
using android::base::StringPrintf;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

// TODO(b/170327382): add unit tests for apexd_utils.h

TEST(ApexdUtilTest, FindFirstExistingDirectoryBothExist) {
  TemporaryDir first_dir;
  TemporaryDir second_dir;
  auto result = FindFirstExistingDirectory(first_dir.path, second_dir.path);
  ASSERT_TRUE(IsOk(result));
  ASSERT_EQ(*result, first_dir.path);
}

TEST(ApexdUtilTest, FindFirstExistingDirectoryOnlyFirstExist) {
  TemporaryDir first_dir;
  auto second_dir = "/data/local/tmp/does/not/exist";
  auto result = FindFirstExistingDirectory(first_dir.path, second_dir);
  ASSERT_TRUE(IsOk(result));
  ASSERT_EQ(*result, first_dir.path);
}

TEST(ApexdUtilTest, FindFirstExistingDirectoryOnlySecondExist) {
  auto first_dir = "/data/local/tmp/does/not/exist";
  TemporaryDir second_dir;
  auto result = FindFirstExistingDirectory(first_dir, second_dir.path);
  ASSERT_TRUE(IsOk(result));
  ASSERT_EQ(*result, second_dir.path);
}

TEST(ApexdUtilTest, FindFirstExistingDirectoryNoneExist) {
  auto first_dir = "/data/local/tmp/does/not/exist";
  auto second_dir = "/data/local/tmp/also/does/not/exist";
  auto result = FindFirstExistingDirectory(first_dir, second_dir);
  ASSERT_FALSE(IsOk(result));
}

TEST(ApexdUtilTest, FindFirstExistingDirectoryFirstFileSecondDir) {
  TemporaryFile first_file;
  TemporaryDir second_dir;
  auto result = FindFirstExistingDirectory(first_file.path, second_dir.path);
  ASSERT_TRUE(IsOk(result));
  ASSERT_EQ(*result, second_dir.path);
}

TEST(ApexdUtilTest, FindFirstExistingDirectoryFirstDirSecondFile) {
  TemporaryDir first_dir;
  TemporaryFile second_file;
  auto result = FindFirstExistingDirectory(first_dir.path, second_file.path);
  ASSERT_TRUE(IsOk(result));
  ASSERT_EQ(*result, first_dir.path);
}

TEST(ApexdUtilTest, FindFirstExistingDirectoryBothFiles) {
  TemporaryFile first_file;
  TemporaryFile second_file;
  auto result = FindFirstExistingDirectory(first_file.path, second_file.path);
  ASSERT_FALSE(IsOk(result));
}

TEST(ApexdUtilTest, FindFirstExistingDirectoryFirstFileSecondDoesNotExist) {
  TemporaryFile first_file;
  auto second_dir = "/data/local/tmp/does/not/exist";
  auto result = FindFirstExistingDirectory(first_file.path, second_dir);
  ASSERT_FALSE(IsOk(result));
}

TEST(ApexdUtilTest, FindFirstExistingDirectoryFirsDoesNotExistSecondFile) {
  auto first_dir = "/data/local/tmp/does/not/exist";
  TemporaryFile second_file;
  auto result = FindFirstExistingDirectory(first_dir, second_file.path);
  ASSERT_FALSE(IsOk(result));
}

TEST(ApexdUtilTest, MoveDir) {
  namespace fs = std::filesystem;

  TemporaryDir from;
  TemporaryDir to;

  TemporaryFile from_1(from.path);
  auto from_subdir = StringPrintf("%s/subdir", from.path);
  if (mkdir(from_subdir.c_str(), 07000) != 0) {
    FAIL() << "Failed to mkdir " << from_subdir << " : " << strerror(errno);
  }
  TemporaryFile from_2(from_subdir);

  auto result = MoveDir(from.path, to.path);
  ASSERT_TRUE(IsOk(result));
  ASSERT_TRUE(fs::is_empty(from.path));

  std::vector<std::string> content;
  for (const auto& it : fs::recursive_directory_iterator(to.path)) {
    content.push_back(it.path());
  }

  static const std::vector<std::string> expected = {
      StringPrintf("%s/%s", to.path, Basename(from_1.path).c_str()),
      StringPrintf("%s/subdir", to.path),
      StringPrintf("%s/subdir/%s", to.path, Basename(from_2.path).c_str()),
  };
  ASSERT_THAT(content, UnorderedElementsAreArray(expected));
}

TEST(ApexdUtilTest, MoveDirFromIsNotDirectory) {
  TemporaryFile from;
  TemporaryDir to;
  ASSERT_FALSE(IsOk(MoveDir(from.path, to.path)));
}

TEST(ApexdUtilTest, MoveDirToIsNotDirectory) {
  TemporaryDir from;
  TemporaryFile to;
  TemporaryFile from_1(from.path);
  ASSERT_FALSE(IsOk(MoveDir(from.path, to.path)));
}

TEST(ApexdUtilTest, MoveDirFromDoesNotExist) {
  TemporaryDir to;
  ASSERT_FALSE(IsOk(MoveDir("/data/local/tmp/does/not/exist", to.path)));
}

TEST(ApexdUtilTest, MoveDirToDoesNotExist) {
  namespace fs = std::filesystem;

  TemporaryDir from;
  TemporaryFile from_1(from.path);
  auto from_subdir = StringPrintf("%s/subdir", from.path);
  if (mkdir(from_subdir.c_str(), 07000) != 0) {
    FAIL() << "Failed to mkdir " << from_subdir << " : " << strerror(errno);
  }
  TemporaryFile from_2(from_subdir);

  ASSERT_FALSE(IsOk(MoveDir(from.path, "/data/local/tmp/does/not/exist")));

  // Check that |from| directory is not empty.
  std::vector<std::string> content;
  for (const auto& it : fs::recursive_directory_iterator(from.path)) {
    content.push_back(it.path());
  }

  ASSERT_THAT(content,
              UnorderedElementsAre(from_1.path, from_subdir, from_2.path));
}

}  // namespace
}  // namespace apex
}  // namespace android
