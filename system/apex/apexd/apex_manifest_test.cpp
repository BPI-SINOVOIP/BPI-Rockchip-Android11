/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <algorithm>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <gtest/gtest.h>

#include "apex_manifest.h"

namespace android {
namespace apex {

namespace {

std::string ToString(const ApexManifest& manifest) {
  std::string out;
  manifest.SerializeToString(&out);
  return out;
}

}  // namespace

TEST(ApexManifestTest, SimpleTest) {
  ApexManifest manifest;
  manifest.set_name("com.android.example.apex");
  manifest.set_version(1);
  auto apex_manifest = ParseManifest(ToString(manifest));
  ASSERT_RESULT_OK(apex_manifest);
  EXPECT_EQ("com.android.example.apex", std::string(apex_manifest->name()));
  EXPECT_EQ(1u, apex_manifest->version());
  EXPECT_FALSE(apex_manifest->nocode());
}

TEST(ApexManifestTest, NameMissing) {
  ApexManifest manifest;
  manifest.set_version(1);
  auto apex_manifest = ParseManifest(ToString(manifest));
  ASSERT_FALSE(apex_manifest.ok());
  EXPECT_EQ(apex_manifest.error().message(),
            std::string("Missing required field \"name\" from APEX manifest."))
      << apex_manifest.error();
}

TEST(ApexManifestTest, VersionMissing) {
  ApexManifest manifest;
  manifest.set_name("com.android.example.apex");
  auto apex_manifest = ParseManifest(ToString(manifest));
  ASSERT_FALSE(apex_manifest.ok());
  EXPECT_EQ(
      apex_manifest.error().message(),
      std::string("Missing required field \"version\" from APEX manifest."))
      << apex_manifest.error();
}

TEST(ApexManifestTest, NoPreInstallHook) {
  ApexManifest manifest;
  manifest.set_name("com.android.example.apex");
  manifest.set_version(1);
  auto apex_manifest = ParseManifest(ToString(manifest));
  ASSERT_RESULT_OK(apex_manifest);
  EXPECT_EQ("", std::string(apex_manifest->preinstallhook()));
}

TEST(ApexManifestTest, PreInstallHook) {
  ApexManifest manifest;
  manifest.set_name("com.android.example.apex");
  manifest.set_version(1);
  manifest.set_preinstallhook("bin/preInstallHook");
  auto apex_manifest = ParseManifest(ToString(manifest));
  ASSERT_RESULT_OK(apex_manifest);
  EXPECT_EQ("bin/preInstallHook", std::string(apex_manifest->preinstallhook()));
}

TEST(ApexManifestTest, NoPostInstallHook) {
  ApexManifest manifest;
  manifest.set_name("com.android.example.apex");
  manifest.set_version(1);
  auto apex_manifest = ParseManifest(ToString(manifest));
  ASSERT_RESULT_OK(apex_manifest);
  EXPECT_EQ("", std::string(apex_manifest->postinstallhook()));
}

TEST(ApexManifestTest, PostInstallHook) {
  ApexManifest manifest;
  manifest.set_name("com.android.example.apex");
  manifest.set_version(1);
  manifest.set_postinstallhook("bin/postInstallHook");
  auto apex_manifest = ParseManifest(ToString(manifest));
  ASSERT_RESULT_OK(apex_manifest);
  EXPECT_EQ("bin/postInstallHook",
            std::string(apex_manifest->postinstallhook()));
}

TEST(ApexManifestTest, UnparsableManifest) {
  auto apex_manifest = ParseManifest("This is an invalid pony");
  ASSERT_FALSE(apex_manifest.ok());
  EXPECT_EQ(apex_manifest.error().message(),
            std::string("Can't parse APEX manifest."))
      << apex_manifest.error();
}

TEST(ApexManifestTest, NoCode) {
  ApexManifest manifest;
  manifest.set_name("com.android.example.apex");
  manifest.set_version(1);
  manifest.set_nocode(true);
  auto apex_manifest = ParseManifest(ToString(manifest));
  ASSERT_RESULT_OK(apex_manifest);
  EXPECT_TRUE(apex_manifest->nocode());
}

}  // namespace apex
}  // namespace android
