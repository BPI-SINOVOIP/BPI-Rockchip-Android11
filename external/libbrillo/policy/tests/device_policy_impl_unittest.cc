// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "policy/device_policy_impl.h"

#include "bindings/chrome_device_policy.pb.h"
#include "install_attributes/mock_install_attributes_reader.h"

namespace em = enterprise_management;

using testing::ElementsAre;

namespace policy {

class DevicePolicyImplTest : public testing::Test, public DevicePolicyImpl {
 protected:
  void InitializePolicy(const char* device_mode,
                        const em::ChromeDeviceSettingsProto& proto) {
    device_policy_.set_policy_for_testing(proto);
    device_policy_.set_install_attributes_for_testing(
        std::make_unique<MockInstallAttributesReader>(
            device_mode, true /* initialized */));
  }

  DevicePolicyImpl device_policy_;
};

// Enterprise managed.
TEST_F(DevicePolicyImplTest, GetOwner_Managed) {
  em::PolicyData policy_data;
  policy_data.set_username("user@example.com");
  policy_data.set_management_mode(em::PolicyData::ENTERPRISE_MANAGED);
  device_policy_.set_policy_data_for_testing(policy_data);

  std::string owner("something");
  EXPECT_TRUE(device_policy_.GetOwner(&owner));
  EXPECT_TRUE(owner.empty());
}

// Consumer owned.
TEST_F(DevicePolicyImplTest, GetOwner_Consumer) {
  em::PolicyData policy_data;
  policy_data.set_username("user@example.com");
  policy_data.set_management_mode(em::PolicyData::LOCAL_OWNER);
  policy_data.set_request_token("codepath-must-ignore-dmtoken");
  device_policy_.set_policy_data_for_testing(policy_data);

  std::string owner;
  EXPECT_TRUE(device_policy_.GetOwner(&owner));
  EXPECT_EQ("user@example.com", owner);
}

// Consumer owned, username is missing.
TEST_F(DevicePolicyImplTest, GetOwner_ConsumerMissingUsername) {
  em::PolicyData policy_data;
  device_policy_.set_policy_data_for_testing(policy_data);

  std::string owner("something");
  EXPECT_FALSE(device_policy_.GetOwner(&owner));
  EXPECT_EQ("something", owner);
}

// Enterprise managed, denoted by management_mode.
TEST_F(DevicePolicyImplTest, IsEnterpriseManaged_ManagementModeManaged) {
  em::PolicyData policy_data;
  policy_data.set_management_mode(em::PolicyData::ENTERPRISE_MANAGED);
  device_policy_.set_policy_data_for_testing(policy_data);

  EXPECT_TRUE(device_policy_.IsEnterpriseManaged());
}

// Enterprise managed, fallback to DM token.
TEST_F(DevicePolicyImplTest, IsEnterpriseManaged_DMTokenManaged) {
  em::PolicyData policy_data;
  policy_data.set_request_token("abc");
  device_policy_.set_policy_data_for_testing(policy_data);

  EXPECT_TRUE(device_policy_.IsEnterpriseManaged());
}

// Consumer owned, denoted by management_mode.
TEST_F(DevicePolicyImplTest, IsEnterpriseManaged_ManagementModeConsumer) {
  em::PolicyData policy_data;
  policy_data.set_management_mode(em::PolicyData::LOCAL_OWNER);
  policy_data.set_request_token("codepath-must-ignore-dmtoken");
  device_policy_.set_policy_data_for_testing(policy_data);

  EXPECT_FALSE(device_policy_.IsEnterpriseManaged());
}

// Consumer owned, fallback to interpreting absence of DM token.
TEST_F(DevicePolicyImplTest, IsEnterpriseManaged_DMTokenConsumer) {
  em::PolicyData policy_data;
  device_policy_.set_policy_data_for_testing(policy_data);

  EXPECT_FALSE(device_policy_.IsEnterpriseManaged());
}

// RollbackAllowedMilestones is not set.
TEST_F(DevicePolicyImplTest, GetRollbackAllowedMilestones_NotSet) {
  device_policy_.set_install_attributes_for_testing(
      std::make_unique<MockInstallAttributesReader>(
          InstallAttributesReader::kDeviceModeEnterprise, true));

  int value = -1;
  ASSERT_TRUE(device_policy_.GetRollbackAllowedMilestones(&value));
  EXPECT_EQ(0, value);
}

// RollbackAllowedMilestones is set to a valid value.
TEST_F(DevicePolicyImplTest, GetRollbackAllowedMilestones_Set) {
  em::ChromeDeviceSettingsProto device_policy_proto;
  em::AutoUpdateSettingsProto* auto_update_settings =
      device_policy_proto.mutable_auto_update_settings();
  auto_update_settings->set_rollback_allowed_milestones(3);
  InitializePolicy(InstallAttributesReader::kDeviceModeEnterprise,
                   device_policy_proto);

  int value = -1;
  ASSERT_TRUE(device_policy_.GetRollbackAllowedMilestones(&value));
  EXPECT_EQ(3, value);
}

// RollbackAllowedMilestones is set to a valid value, using AD.
TEST_F(DevicePolicyImplTest, GetRollbackAllowedMilestones_SetAD) {
  em::ChromeDeviceSettingsProto device_policy_proto;
  em::AutoUpdateSettingsProto* auto_update_settings =
      device_policy_proto.mutable_auto_update_settings();
  auto_update_settings->set_rollback_allowed_milestones(3);
  InitializePolicy(InstallAttributesReader::kDeviceModeEnterpriseAD,
                   device_policy_proto);
  int value = -1;
  ASSERT_TRUE(device_policy_.GetRollbackAllowedMilestones(&value));
  EXPECT_EQ(3, value);
}

// RollbackAllowedMilestones is set to a valid value, but it's not an enterprise
// device.
TEST_F(DevicePolicyImplTest, GetRollbackAllowedMilestones_SetConsumer) {
  em::ChromeDeviceSettingsProto device_policy_proto;
  em::AutoUpdateSettingsProto* auto_update_settings =
      device_policy_proto.mutable_auto_update_settings();
  auto_update_settings->set_rollback_allowed_milestones(3);
  InitializePolicy(InstallAttributesReader::kDeviceModeConsumer,
                   device_policy_proto);

  int value = -1;
  ASSERT_FALSE(device_policy_.GetRollbackAllowedMilestones(&value));
}

// RollbackAllowedMilestones is set to an invalid value.
TEST_F(DevicePolicyImplTest, GetRollbackAllowedMilestones_SetTooLarge) {
  em::ChromeDeviceSettingsProto device_policy_proto;
  em::AutoUpdateSettingsProto* auto_update_settings =
      device_policy_proto.mutable_auto_update_settings();
  auto_update_settings->set_rollback_allowed_milestones(10);
  InitializePolicy(InstallAttributesReader::kDeviceModeEnterprise,
                   device_policy_proto);

  int value = -1;
  ASSERT_TRUE(device_policy_.GetRollbackAllowedMilestones(&value));
  EXPECT_EQ(4, value);
}

// RollbackAllowedMilestones is set to an invalid value.
TEST_F(DevicePolicyImplTest, GetRollbackAllowedMilestones_SetTooSmall) {
  em::ChromeDeviceSettingsProto device_policy_proto;
  em::AutoUpdateSettingsProto* auto_update_settings =
      device_policy_proto.mutable_auto_update_settings();
  auto_update_settings->set_rollback_allowed_milestones(-1);
  InitializePolicy(InstallAttributesReader::kDeviceModeEnterprise,
                   device_policy_proto);

  int value = -1;
  ASSERT_TRUE(device_policy_.GetRollbackAllowedMilestones(&value));
  EXPECT_EQ(0, value);
}

// Update staging schedule has no values
TEST_F(DevicePolicyImplTest, GetDeviceUpdateStagingSchedule_NoValues) {
  em::ChromeDeviceSettingsProto device_policy_proto;
  em::AutoUpdateSettingsProto *auto_update_settings =
      device_policy_proto.mutable_auto_update_settings();
  auto_update_settings->set_staging_schedule("[]");
  InitializePolicy(InstallAttributesReader::kDeviceModeEnterprise,
                   device_policy_proto);

  std::vector<DayPercentagePair> staging_schedule;
  ASSERT_TRUE(device_policy_.GetDeviceUpdateStagingSchedule(&staging_schedule));
  EXPECT_EQ(0, staging_schedule.size());
}

// Update staging schedule has valid values
TEST_F(DevicePolicyImplTest, GetDeviceUpdateStagingSchedule_Valid) {
  em::ChromeDeviceSettingsProto device_policy_proto;
  em::AutoUpdateSettingsProto *auto_update_settings =
      device_policy_proto.mutable_auto_update_settings();
  auto_update_settings->set_staging_schedule(
      "[{\"days\": 4, \"percentage\": 40}, {\"days\": 10, \"percentage\": "
      "100}]");
  InitializePolicy(InstallAttributesReader::kDeviceModeEnterprise,
                   device_policy_proto);

  std::vector<DayPercentagePair> staging_schedule;
  ASSERT_TRUE(device_policy_.GetDeviceUpdateStagingSchedule(&staging_schedule));
  EXPECT_THAT(staging_schedule, ElementsAre(DayPercentagePair{4, 40},
                                            DayPercentagePair{10, 100}));
}

// Update staging schedule has valid values, set using AD.
TEST_F(DevicePolicyImplTest, GetDeviceUpdateStagingSchedule_Valid_AD) {
  em::ChromeDeviceSettingsProto device_policy_proto;
  em::AutoUpdateSettingsProto *auto_update_settings =
      device_policy_proto.mutable_auto_update_settings();
  auto_update_settings->set_staging_schedule(
      "[{\"days\": 4, \"percentage\": 40}, {\"days\": 10, \"percentage\": "
      "100}]");
  InitializePolicy(InstallAttributesReader::kDeviceModeEnterpriseAD,
                   device_policy_proto);

  std::vector<DayPercentagePair> staging_schedule;
  ASSERT_TRUE(device_policy_.GetDeviceUpdateStagingSchedule(&staging_schedule));
  EXPECT_THAT(staging_schedule, ElementsAre(DayPercentagePair{4, 40},
                                            DayPercentagePair{10, 100}));
}

// Update staging schedule has values with values set larger than the max
// allowed days/percentage and smaller than the min allowed days/percentage.
TEST_F(DevicePolicyImplTest,
       GetDeviceUpdateStagingSchedule_SetOutsideAllowable) {
  em::ChromeDeviceSettingsProto device_policy_proto;
  em::AutoUpdateSettingsProto *auto_update_settings =
      device_policy_proto.mutable_auto_update_settings();
  auto_update_settings->set_staging_schedule(
      "[{\"days\": -1, \"percentage\": -10}, {\"days\": 30, \"percentage\": "
      "110}]");
  InitializePolicy(InstallAttributesReader::kDeviceModeEnterprise,
                   device_policy_proto);

  std::vector<DayPercentagePair> staging_schedule;
  ASSERT_TRUE(device_policy_.GetDeviceUpdateStagingSchedule(&staging_schedule));
  EXPECT_THAT(staging_schedule, ElementsAre(DayPercentagePair{1, 0},
                                            DayPercentagePair{28, 100}));
}

}  // namespace policy
