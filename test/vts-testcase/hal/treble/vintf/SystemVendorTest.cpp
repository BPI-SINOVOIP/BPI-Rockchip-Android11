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

// SystemVendorTest test cases that runs on P+ vendor.

#include "SystemVendorTest.h"

#include <android-base/logging.h>
#include <android-base/strings.h>
#include <vintf/VintfObject.h>
#include <iostream>

#include "SingleManifestTest.h"

namespace android {
namespace vintf {
namespace testing {

using std::endl;

// Tests that device manifest and framework compatibility matrix are compatible.
TEST_F(SystemVendorTest, DeviceManifestFrameworkMatrixCompatibility) {
  auto device_manifest = VintfObject::GetDeviceHalManifest();
  ASSERT_NE(device_manifest, nullptr) << "Failed to get device HAL manifest.";
  auto fwk_matrix = VintfObject::GetFrameworkCompatibilityMatrix();
  ASSERT_NE(fwk_matrix, nullptr)
      << "Failed to get framework compatibility matrix.";

  string error;
  EXPECT_TRUE(device_manifest->checkCompatibility(*fwk_matrix, &error))
      << error;
}

// Tests that framework manifest and device compatibility matrix are compatible.
TEST_F(SystemVendorTest, FrameworkManifestDeviceMatrixCompatibility) {
  auto fwk_manifest = VintfObject::GetFrameworkHalManifest();
  ASSERT_NE(fwk_manifest, nullptr) << "Failed to get framework HAL manifest.";
  auto device_matrix = VintfObject::GetDeviceCompatibilityMatrix();
  ASSERT_NE(device_matrix, nullptr)
      << "Failed to get device compatibility matrix.";

  string error;
  EXPECT_TRUE(fwk_manifest->checkCompatibility(*device_matrix, &error))
      << error;
}

// Tests that framework compatibility matrix and runtime info are compatible.
// AVB version is not a compliance requirement.
TEST_F(SystemVendorTest, FrameworkMatrixDeviceRuntimeCompatibility) {
  auto fwk_matrix = VintfObject::GetFrameworkCompatibilityMatrix();
  ASSERT_NE(fwk_matrix, nullptr)
      << "Failed to get framework compatibility matrix.";
  auto runtime_info = VintfObject::GetRuntimeInfo();
  ASSERT_NE(nullptr, runtime_info) << "Failed to get runtime info.";

  string error;
  EXPECT_TRUE(runtime_info->checkCompatibility(
      *fwk_matrix, &error,
      ::android::vintf::CheckFlags::ENABLE_ALL_CHECKS.disableAvb()
          .disableKernel()))
      << error;
}

// Tests that runtime kernel matches requirements in compatibility matrix.
// This includes testing kernel version and kernel configurations.
TEST_F(SystemVendorTest, KernelCompatibility) {
  auto fwk_matrix = VintfObject::GetFrameworkCompatibilityMatrix();
  ASSERT_NE(fwk_matrix, nullptr)
      << "Failed to get framework compatibility matrix.";
  auto runtime_info = VintfObject::GetRuntimeInfo();
  ASSERT_NE(nullptr, runtime_info) << "Failed to get runtime info.";

  string error;
  EXPECT_TRUE(runtime_info->checkCompatibility(
      *fwk_matrix, &error,
      ::android::vintf::CheckFlags::DISABLE_ALL_CHECKS.enableKernel()))
      << error;
}

// Tests that vendor and framework are compatible.
// If any of the other tests in SystemVendorTest fails, this test will fail as
// well. This is a sanity check in case the sub-tests do not cover some
// checks.
// AVB version is not a compliance requirement.
TEST_F(SystemVendorTest, VendorFrameworkCompatibility) {
  string error;
  EXPECT_EQ(
      android::vintf::COMPATIBLE,
      VintfObject::GetInstance()->checkCompatibility(
          &error, ::android::vintf::CheckFlags::ENABLE_ALL_CHECKS.disableAvb()))
      << error;
}

template <typename D, typename S>
static void insert(D *d, const S &s) {
  d->insert(s.begin(), s.end());
}

// This needs to be tested besides
// SingleManifestTest.ServedHwbinderHalsAreInManifest because some HALs may
// refuse to provide its PID, and the partition cannot be inferred.
TEST_F(SystemVendorTest, ServedHwbinderHalsAreInManifest) {
  auto device_manifest = VintfObject::GetDeviceHalManifest();
  ASSERT_NE(device_manifest, nullptr) << "Failed to get device HAL manifest.";
  auto fwk_manifest = VintfObject::GetFrameworkHalManifest();
  ASSERT_NE(fwk_manifest, nullptr) << "Failed to get framework HAL manifest.";

  std::set<std::string> manifest_hwbinder_hals;

  insert(&manifest_hwbinder_hals, GetHwbinderHals(fwk_manifest));
  insert(&manifest_hwbinder_hals, GetHwbinderHals(device_manifest));

  Return<void> ret = default_manager_->list([&](const auto &list) {
    for (const auto &name : list) {
      // TODO(b/73774955): use standardized parsing code for fqinstancename
      if (std::string(name).find(IBase::descriptor) == 0) continue;

      EXPECT_NE(manifest_hwbinder_hals.find(name), manifest_hwbinder_hals.end())
          << name << " is being served, but it is not in a manifest.";
    }
  });
  EXPECT_TRUE(ret.isOk());
}

static std::vector<HalManifestPtr> GetTestManifests() {
  return {
      VintfObject::GetFrameworkHalManifest(),
  };
}

INSTANTIATE_TEST_CASE_P(FrameworkManifest, SingleManifestTest,
                        ::testing::ValuesIn(GetTestManifests()));

}  // namespace testing
}  // namespace vintf
}  // namespace android
