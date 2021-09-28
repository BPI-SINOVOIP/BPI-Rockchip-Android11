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

#include "DeviceMatrixTest.h"

#include <android-base/properties.h>
#include <vintf/VintfObject.h>

using android::base::GetProperty;

namespace android {
namespace vintf {
namespace testing {

const string kVndkVersionProp{"ro.vndk.version"};

void DeviceMatrixTest::SetUp() {
  VtsTrebleVintfTestBase::SetUp();

  vendor_matrix_ = VintfObject::GetDeviceCompatibilityMatrix();
  ASSERT_NE(nullptr, vendor_matrix_)
      << "Failed to get device compatibility matrix." << endl;
}

TEST_F(DeviceMatrixTest, VndkVersion) {
  if (GetShippingApiLevel() < 28) {
    GTEST_SKIP()
        << "VNDK version doesn't need to be set on devices before Android P";
  }

  std::string syspropVndkVersion = GetProperty(kVndkVersionProp, "");
  ASSERT_NE("", syspropVndkVersion)
      << kVndkVersionProp << " must not be empty.";
  std::string vintfVndkVersion = vendor_matrix_->getVendorNdkVersion();
  ASSERT_NE("", vintfVndkVersion)
      << "Device compatibility matrix does not declare proper VNDK version.";

  EXPECT_EQ(syspropVndkVersion, vintfVndkVersion)
      << "VNDK version does not match: " << kVndkVersionProp << "="
      << syspropVndkVersion << ", device compatibility matrix requires "
      << vintfVndkVersion << ".";
}

}  // namespace testing
}  // namespace vintf
}  // namespace android
