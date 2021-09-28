// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "policy/libpolicy.h"

#include <memory>
#include <utility>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <gtest/gtest.h>

#include "bindings/chrome_device_policy.pb.h"
#include "install_attributes/mock_install_attributes_reader.h"
#include "policy/device_policy_impl.h"

namespace policy {

static const char kPolicyFileAllSet[] = "policy/tests/whitelist/policy_all";
static const char kPolicyFileNoneSet[] = "policy/tests/whitelist/policy_none";
static const char kKeyFile[] = "policy/tests/whitelist/owner.key";
static const char kNonExistingFile[] = "file-does-not-exist";

// Creates the DevicePolicyImpl with given parameters for test.
std::unique_ptr<DevicePolicyImpl> CreateDevicePolicyImpl(
    std::unique_ptr<InstallAttributesReader> install_attributes_reader,
    const base::FilePath& policy_path,
    const base::FilePath& keyfile_path,
    bool verify_files) {
  std::unique_ptr<DevicePolicyImpl> device_policy(new DevicePolicyImpl());
  device_policy->set_install_attributes_for_testing(
      std::move(install_attributes_reader));
  device_policy->set_policy_path_for_testing(policy_path);
  device_policy->set_key_file_path_for_testing(keyfile_path);
  device_policy->set_verify_root_ownership_for_testing(verify_files);

  return device_policy;
}

// Test that a policy file can be verified and parsed correctly. The file
// contains all possible fields, so reading should succeed for all.
TEST(PolicyTest, DevicePolicyAllSetTest) {
  base::FilePath policy_file(kPolicyFileAllSet);
  base::FilePath key_file(kKeyFile);
  PolicyProvider provider;
  provider.SetDevicePolicyForTesting(CreateDevicePolicyImpl(
      std::make_unique<MockInstallAttributesReader>(
          InstallAttributesReader::kDeviceModeEnterprise, true),
      policy_file, key_file, false));
  provider.Reload();

  // Ensure we successfully loaded the device policy file.
  ASSERT_TRUE(provider.device_policy_is_loaded());

  const DevicePolicy& policy = provider.GetDevicePolicy();

  // Check that we can read out all fields of the sample protobuf.
  int int_value = -1;
  ASSERT_TRUE(policy.GetPolicyRefreshRate(&int_value));
  EXPECT_EQ(100, int_value);

  std::vector<std::string> list_value;
  ASSERT_TRUE(policy.GetUserWhitelist(&list_value));
  ASSERT_EQ(3, list_value.size());
  EXPECT_EQ("me@here.com", list_value[0]);
  EXPECT_EQ("you@there.com", list_value[1]);
  EXPECT_EQ("*@monsters.com", list_value[2]);

  bool bool_value = true;
  ASSERT_TRUE(policy.GetGuestModeEnabled(&bool_value));
  EXPECT_FALSE(bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetCameraEnabled(&bool_value));
  EXPECT_FALSE(bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetShowUserNames(&bool_value));
  EXPECT_FALSE(bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetDataRoamingEnabled(&bool_value));
  EXPECT_FALSE(bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetAllowNewUsers(&bool_value));
  EXPECT_FALSE(bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetMetricsEnabled(&bool_value));
  EXPECT_FALSE(bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetReportVersionInfo(&bool_value));
  EXPECT_FALSE(bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetReportActivityTimes(&bool_value));
  EXPECT_FALSE(bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetReportBootMode(&bool_value));
  EXPECT_FALSE(bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetEphemeralUsersEnabled(&bool_value));
  EXPECT_FALSE(bool_value);

  std::string string_value;
  ASSERT_TRUE(policy.GetReleaseChannel(&string_value));
  EXPECT_EQ("stable-channel", string_value);

  bool_value = false;
  ASSERT_TRUE(policy.GetReleaseChannelDelegated(&bool_value));
  EXPECT_TRUE(bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetUpdateDisabled(&bool_value));
  EXPECT_FALSE(bool_value);

  int64_t int64_value = -1LL;
  ASSERT_TRUE(policy.GetScatterFactorInSeconds(&int64_value));
  EXPECT_EQ(17LL, int64_value);

  ASSERT_TRUE(policy.GetTargetVersionPrefix(&string_value));
  EXPECT_EQ("42.0.", string_value);

  int_value = -1;
  ASSERT_TRUE(policy.GetRollbackToTargetVersion(&int_value));
  EXPECT_EQ(enterprise_management::AutoUpdateSettingsProto::
                ROLLBACK_WITH_FULL_POWERWASH,
            int_value);

  int_value = -1;
  ASSERT_TRUE(policy.GetRollbackAllowedMilestones(&int_value));
  EXPECT_EQ(3, int_value);

  std::set<std::string> types;
  ASSERT_TRUE(policy.GetAllowedConnectionTypesForUpdate(&types));
  EXPECT_TRUE(types.end() != types.find("ethernet"));
  EXPECT_TRUE(types.end() != types.find("wifi"));
  EXPECT_EQ(2, types.size());

  ASSERT_TRUE(policy.GetOpenNetworkConfiguration(&string_value));
  EXPECT_EQ("{}", string_value);

  ASSERT_TRUE(policy.GetOwner(&string_value));
  EXPECT_EQ("", string_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetHttpDownloadsEnabled(&bool_value));
  EXPECT_FALSE(bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetAuP2PEnabled(&bool_value));
  EXPECT_FALSE(bool_value);

  bool_value = true;
  ASSERT_TRUE(policy.GetAllowKioskAppControlChromeVersion(&bool_value));
  EXPECT_FALSE(bool_value);

  std::vector<DevicePolicy::UsbDeviceId> list_device;
  ASSERT_TRUE(policy.GetUsbDetachableWhitelist(&list_device));
  EXPECT_EQ(2, list_device.size());
  EXPECT_EQ(0x413c, list_device[0].vendor_id);
  EXPECT_EQ(0x2105, list_device[0].product_id);
  EXPECT_EQ(0x0403, list_device[1].vendor_id);
  EXPECT_EQ(0x6001, list_device[1].product_id);

  ASSERT_TRUE(policy.GetAutoLaunchedKioskAppId(&string_value));
  EXPECT_EQ("my_kiosk_app", string_value);

  int_value = -1;
  ASSERT_TRUE(policy.GetSecondFactorAuthenticationMode(&int_value));
  EXPECT_EQ(2, int_value);

  std::vector<DevicePolicy::WeeklyTimeInterval> intervals;
  ASSERT_TRUE(policy.GetDisallowedTimeIntervals(&intervals));
  ASSERT_EQ(2, intervals.size());
  EXPECT_EQ(4, intervals[0].start_day_of_week);
  EXPECT_EQ(base::TimeDelta::FromMinutes(30) + base::TimeDelta::FromHours(12),
            intervals[0].start_time);
  EXPECT_EQ(6, intervals[0].end_day_of_week);
  EXPECT_EQ(base::TimeDelta::FromMinutes(15) + base::TimeDelta::FromHours(3),
            intervals[0].end_time);
  EXPECT_EQ(1, intervals[1].start_day_of_week);
  EXPECT_EQ(base::TimeDelta::FromMinutes(10) + base::TimeDelta::FromHours(20),
            intervals[1].start_time);
  EXPECT_EQ(3, intervals[1].end_day_of_week);
  EXPECT_EQ(base::TimeDelta::FromMinutes(20), intervals[1].end_time);

  ASSERT_TRUE(policy.GetAutoLaunchedKioskAppId(&string_value));
  ASSERT_EQ("my_kiosk_app", string_value);

  // Reloading the protobuf should succeed.
  EXPECT_TRUE(provider.Reload());
}

// Test that a policy file can be verified and parsed correctly. The file
// contains none of the possible fields, so reading should fail for all.
TEST(PolicyTest, DevicePolicyNoneSetTest) {
  base::FilePath policy_file(kPolicyFileNoneSet);
  base::FilePath key_file(kKeyFile);

  PolicyProvider provider;
  provider.SetDevicePolicyForTesting(CreateDevicePolicyImpl(
      std::make_unique<MockInstallAttributesReader>(
          InstallAttributesReader::kDeviceModeEnterprise, true),
      policy_file, key_file, false));
  provider.Reload();

  // Ensure we successfully loaded the device policy file.
  ASSERT_TRUE(provider.device_policy_is_loaded());

  const DevicePolicy& policy = provider.GetDevicePolicy();

  // Check that we cannot read any fields out of the sample protobuf.
  int int_value;
  int64_t int64_value;
  std::vector<std::string> list_value;
  bool bool_value;
  std::string string_value;
  std::vector<DevicePolicy::UsbDeviceId> list_device;
  std::vector<DevicePolicy::WeeklyTimeInterval> intervals;

  EXPECT_FALSE(policy.GetPolicyRefreshRate(&int_value));
  EXPECT_FALSE(policy.GetUserWhitelist(&list_value));
  EXPECT_FALSE(policy.GetGuestModeEnabled(&bool_value));
  EXPECT_FALSE(policy.GetCameraEnabled(&bool_value));
  EXPECT_FALSE(policy.GetShowUserNames(&bool_value));
  EXPECT_FALSE(policy.GetDataRoamingEnabled(&bool_value));
  EXPECT_FALSE(policy.GetAllowNewUsers(&bool_value));
  EXPECT_FALSE(policy.GetMetricsEnabled(&bool_value));
  EXPECT_FALSE(policy.GetReportVersionInfo(&bool_value));
  EXPECT_FALSE(policy.GetReportActivityTimes(&bool_value));
  EXPECT_FALSE(policy.GetReportBootMode(&bool_value));
  EXPECT_FALSE(policy.GetEphemeralUsersEnabled(&bool_value));
  EXPECT_FALSE(policy.GetReleaseChannel(&string_value));
  EXPECT_FALSE(policy.GetUpdateDisabled(&bool_value));
  EXPECT_FALSE(policy.GetTargetVersionPrefix(&string_value));
  EXPECT_FALSE(policy.GetRollbackToTargetVersion(&int_value));
  // RollbackAllowedMilestones has the default value of 0 for enterprise
  // devices.
  ASSERT_TRUE(policy.GetRollbackAllowedMilestones(&int_value));
  EXPECT_EQ(0, int_value);
  EXPECT_FALSE(policy.GetScatterFactorInSeconds(&int64_value));
  EXPECT_FALSE(policy.GetOpenNetworkConfiguration(&string_value));
  EXPECT_FALSE(policy.GetHttpDownloadsEnabled(&bool_value));
  EXPECT_FALSE(policy.GetAuP2PEnabled(&bool_value));
  EXPECT_FALSE(policy.GetAllowKioskAppControlChromeVersion(&bool_value));
  EXPECT_FALSE(policy.GetUsbDetachableWhitelist(&list_device));
  EXPECT_FALSE(policy.GetSecondFactorAuthenticationMode(&int_value));
  EXPECT_FALSE(policy.GetDisallowedTimeIntervals(&intervals));
}

// Verify that the library will correctly recognize and signal missing files.
TEST(PolicyTest, DevicePolicyFailure) {
  LOG(INFO) << "Errors expected.";
  // Try loading non-existing protobuf should fail.
  base::FilePath policy_file(kNonExistingFile);
  base::FilePath key_file(kNonExistingFile);
  PolicyProvider provider;
  provider.SetDevicePolicyForTesting(
      CreateDevicePolicyImpl(std::make_unique<MockInstallAttributesReader>(
                                 cryptohome::SerializedInstallAttributes()),
                             policy_file, key_file, true));

  // Even after reload the policy should still be not loaded.
  ASSERT_FALSE(provider.Reload());
  EXPECT_FALSE(provider.device_policy_is_loaded());
}

// Verify that signature verification is waived for a device in enterprise_ad
// mode.
TEST(PolicyTest, SkipSignatureForEnterpriseAD) {
  base::FilePath policy_file(kPolicyFileAllSet);
  base::FilePath key_file(kNonExistingFile);
  PolicyProvider provider;
  provider.SetDevicePolicyForTesting(CreateDevicePolicyImpl(
      std::make_unique<MockInstallAttributesReader>(
          InstallAttributesReader::kDeviceModeEnterpriseAD, true),
      policy_file, key_file, false));
  provider.Reload();

  // Ensure we successfully loaded the device policy file.
  EXPECT_TRUE(provider.device_policy_is_loaded());
}

// Ensure that signature verification is enforced for a device in vanilla
// enterprise mode.
TEST(PolicyTest, DontSkipSignatureForEnterprise) {
  base::FilePath policy_file(kPolicyFileAllSet);
  base::FilePath key_file(kNonExistingFile);

  PolicyProvider provider;
  provider.SetDevicePolicyForTesting(CreateDevicePolicyImpl(
      std::make_unique<MockInstallAttributesReader>(
          InstallAttributesReader::kDeviceModeEnterprise, true),
      policy_file, key_file, false));
  provider.Reload();

  // Ensure that unverifed policy is not loaded.
  EXPECT_FALSE(provider.device_policy_is_loaded());
}

// Ensure that signature verification is enforced for a device in consumer mode.
TEST(PolicyTest, DontSkipSignatureForConsumer) {
  base::FilePath policy_file(kPolicyFileAllSet);
  base::FilePath key_file(kNonExistingFile);
  cryptohome::SerializedInstallAttributes install_attributes;

  PolicyProvider provider;
  provider.SetDevicePolicyForTesting(CreateDevicePolicyImpl(
      std::make_unique<MockInstallAttributesReader>(install_attributes),
      policy_file, key_file, false));
  provider.Reload();

  // Ensure that unverifed policy is not loaded.
  EXPECT_FALSE(provider.device_policy_is_loaded());
}

// Checks return value of IsConsumerDevice when it's a still in OOBE.
TEST(PolicyTest, IsConsumerDeviceOobe) {
  PolicyProvider provider;
  provider.SetInstallAttributesReaderForTesting(
      std::make_unique<MockInstallAttributesReader>("", false));
  EXPECT_FALSE(provider.IsConsumerDevice());
}

// Checks return value of IsConsumerDevice when it's a consumer device.
TEST(PolicyTest, IsConsumerDeviceConsumer) {
  PolicyProvider provider;
  provider.SetInstallAttributesReaderForTesting(
      std::make_unique<MockInstallAttributesReader>("", true));
  EXPECT_TRUE(provider.IsConsumerDevice());
}

// Checks return value of IsConsumerDevice when it's an enterprise device.
TEST(PolicyTest, IsConsumerDeviceEnterprise) {
  PolicyProvider provider;
  provider.SetInstallAttributesReaderForTesting(
      std::make_unique<MockInstallAttributesReader>(
          InstallAttributesReader::kDeviceModeEnterprise, true));
  EXPECT_FALSE(provider.IsConsumerDevice());
}

// Checks return value of IsConsumerDevice when it's an enterprise AD device.
TEST(PolicyTest, IsConsumerDeviceEnterpriseAd) {
  PolicyProvider provider;
  provider.SetInstallAttributesReaderForTesting(
      std::make_unique<MockInstallAttributesReader>(
          InstallAttributesReader::kDeviceModeEnterpriseAD, true));
  EXPECT_FALSE(provider.IsConsumerDevice());
}

}  // namespace policy

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
