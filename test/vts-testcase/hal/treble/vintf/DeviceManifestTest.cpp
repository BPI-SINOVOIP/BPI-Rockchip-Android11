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

#include "DeviceManifestTest.h"

#include <android-base/properties.h>
#include <vintf/VintfObject.h>
#include "SingleManifestTest.h"

namespace android {
namespace vintf {
namespace testing {

void DeviceManifestTest::SetUp() {
  VtsTrebleVintfTestBase::SetUp();

  vendor_manifest_ = VintfObject::GetDeviceHalManifest();
  ASSERT_NE(vendor_manifest_, nullptr)
      << "Failed to get vendor HAL manifest." << endl;
}

// Tests that Shipping FCM Version in the device manifest is at least the
// minimum Shipping FCM Version as required by Shipping API level.
TEST_F(DeviceManifestTest, ShippingFcmVersion) {
  uint64_t shipping_api_level = GetShippingApiLevel();
  ASSERT_NE(shipping_api_level, 0u)
      << "Device's shipping API level cannot be determined.";

  Level shipping_fcm_version = VintfObject::GetDeviceHalManifest()->level();
  if (shipping_fcm_version == Level::UNSPECIFIED) {
    // O / O-MR1 vendor image doesn't have shipping FCM version declared and
    // shipping FCM version is inferred from Shipping API level, hence it always
    // meets the requirement.
    return;
  }

  ASSERT_GE(shipping_api_level, kFcm2ApiLevelMap.begin()->first /* 25 */)
      << "Pre-N devices should not run this test.";

  auto it = kFcm2ApiLevelMap.find(shipping_api_level);
  ASSERT_TRUE(it != kFcm2ApiLevelMap.end())
      << "No launch requirement is set yet for Shipping API level "
      << shipping_api_level << ". Please update the test.";

  Level required_fcm_version = it->second;

  ASSERT_GE(shipping_fcm_version, required_fcm_version)
      << "Shipping API level == " << shipping_api_level
      << " requires Shipping FCM Version >= " << required_fcm_version
      << " (but is " << shipping_fcm_version << ")";
}

TEST_F(DeviceManifestTest, KernelFcmVersion) {
  Level shipping_fcm_version = VintfObject::GetDeviceHalManifest()->level();
  Level kernel_fcm_version = VintfObject::GetRuntimeInfo()->kernelLevel();

  if (shipping_fcm_version == Level::UNSPECIFIED ||
      shipping_fcm_version < Level::R) {
    GTEST_SKIP() << "Kernel FCM version not enforced on target FCM version "
                 << shipping_fcm_version;
  }
  ASSERT_NE(Level::UNSPECIFIED, kernel_fcm_version)
      << "Kernel FCM version must be specified for target FCM version "
      << shipping_fcm_version;
  ASSERT_GE(kernel_fcm_version, shipping_fcm_version)
      << "Kernel FCM version " << kernel_fcm_version
      << " must be greater or equal to target FCM version "
      << shipping_fcm_version;
}

// Tests that deprecated HALs are not in the manifest, unless a higher,
// non-deprecated minor version is in the manifest.
TEST_F(DeviceManifestTest, NoDeprecatedHalsOnManifest) {
  string error;
  EXPECT_EQ(android::vintf::NO_DEPRECATED_HALS,
            VintfObject::GetInstance()->checkDeprecation(
                HidlInterfaceMetadata::all(), &error))
      << error;
}

// Tests that devices launching R support mapper@4.0.  Go devices are exempt
// from this requirement, so we use this test to enforce instead of the
// compatibility matrix.
TEST_F(DeviceManifestTest, GrallocHalVersionCompatibility) {
  Level shipping_fcm_version = vendor_manifest_->level();
  bool is_go_device =
      android::base::GetBoolProperty("ro.config.low_ram", false);
  if (shipping_fcm_version == Level::UNSPECIFIED ||
      shipping_fcm_version < Level::R || is_go_device) {
    GTEST_SKIP() << "Gralloc4 is only required on launching R devices";
  }

  ASSERT_TRUE(vendor_manifest_->hasHidlInstance(
      "android.hardware.graphics.mapper", {4, 0}, "IMapper", "default"));
  ASSERT_FALSE(vendor_manifest_->hasHidlInstance(
      "android.hardware.graphics.mapper", {2, 0}, "IMapper", "default"));
  ASSERT_FALSE(vendor_manifest_->hasHidlInstance(
      "android.hardware.graphics.mapper", {2, 1}, "IMapper", "default"));
}

static std::vector<HalManifestPtr> GetTestManifests() {
  return {
      VintfObject::GetDeviceHalManifest(),
  };
}

INSTANTIATE_TEST_CASE_P(DeviceManifest, SingleManifestTest,
                        ::testing::ValuesIn(GetTestManifests()));

}  // namespace testing
}  // namespace vintf
}  // namespace android
