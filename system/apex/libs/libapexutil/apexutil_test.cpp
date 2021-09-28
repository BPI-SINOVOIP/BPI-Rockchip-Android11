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

#include "apexutil.h"

#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

#include <android-base/file.h>
#include <apex_manifest.pb.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace std::literals;

using ::android::apex::GetActivePackages;
using ::android::base::WriteStringToFile;
using ::apex::proto::ApexManifest;
using ::testing::Contains;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace {

ApexManifest CreateApexManifest(std::string apex_name, int version) {
  ApexManifest manifest;
  manifest.set_name(apex_name);
  manifest.set_version(version);
  return manifest;
}

void Mkdir(std::string dir_path) {
  if (access(dir_path.c_str(), F_OK) == 0)
    return;
  ASSERT_NE(-1, mkdir(dir_path.c_str(), 0755) == -1)
      << "Failed to create a directory: " << dir_path;
}

void WriteFile(std::string file_path, std::string content) {
  ASSERT_TRUE(WriteStringToFile(content, file_path))
      << "Failed to write a file: " << file_path;
}

} // namespace

namespace apex {
namespace proto {

// simple implementation for testing purpose
bool operator==(const ApexManifest &lhs, const ApexManifest &rhs) {
  return lhs.SerializeAsString() == rhs.SerializeAsString();
}

} // namespace proto
} // namespace apex

TEST(ApexUtil, GetActivePackages) {
  TemporaryDir td;

  // com.android.foo: valid
  auto foo_path = td.path + "/com.android.foo"s;
  auto foo_manifest = CreateApexManifest("com.android.foo", 1);
  Mkdir(foo_path);
  WriteFile(foo_path + "/apex_manifest.pb", foo_manifest.SerializeAsString());

  // com.android.foo@1: ignore versioned
  Mkdir(foo_path + "@1");
  WriteFile(foo_path + "@1/apex_manifest.pb", foo_manifest.SerializeAsString());

  // com.android.baz: valid
  auto bar_path = td.path + "/com.android.bar"s;
  auto bar_manifest = CreateApexManifest("com.android.bar", 2);
  Mkdir(bar_path);
  WriteFile(bar_path + "/apex_manifest.pb", bar_manifest.SerializeAsString());

  // invalid: a directory without apex_manifest.pb
  Mkdir(td.path + "/com.android.baz"s);
  // invalid: a file
  WriteFile(td.path + "/com.android.qux.apex"s, "");

  auto apexes = GetActivePackages(td.path);
  ASSERT_EQ(2U, apexes.size());

  ASSERT_THAT(apexes, UnorderedElementsAre(Pair(foo_path, foo_manifest),
                                           Pair(bar_path, bar_manifest)));
}