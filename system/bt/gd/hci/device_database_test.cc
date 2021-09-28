/******************************************************************************
 *
 *  Copyright 2019 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#include "device_database.h"
#include "classic_device.h"

#include <gtest/gtest.h>

using namespace bluetooth::hci;

namespace bluetooth::hci {
namespace {

Address address({0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
std::string address_str = "06:05:04:03:02:01";
class DeviceDatabaseTest : public ::testing::Test {
 protected:
  DeviceDatabaseTest() = default;

  void SetUp() override {}

  void TearDown() override {}

  DeviceDatabase device_database_;
};

TEST_F(DeviceDatabaseTest, create_classic_device) {
  auto classic_device = device_database_.CreateClassicDevice(address);
  ASSERT_TRUE(classic_device);
  ASSERT_EQ(CLASSIC, classic_device->GetDeviceType());
  ASSERT_EQ(address_str, classic_device->GetUuid());
}

TEST_F(DeviceDatabaseTest, create_le_device) {
  auto le_device = device_database_.CreateLeDevice(address);
  ASSERT_TRUE(le_device);
  ASSERT_EQ(LE, le_device->GetDeviceType());
  ASSERT_EQ(address_str, le_device->GetUuid());
}

TEST_F(DeviceDatabaseTest, create_dual_device) {
  auto dual_device = device_database_.CreateDualDevice(address);
  ASSERT_TRUE(dual_device);
  ASSERT_EQ(DUAL, dual_device->GetDeviceType());
  ASSERT_EQ(DUAL, dual_device->GetClassicDevice()->GetDeviceType());
  ASSERT_EQ(DUAL, dual_device->GetLeDevice()->GetDeviceType());
  ASSERT_EQ(address_str, dual_device->GetUuid());
}

// Shouldn't fail when creating twice.  Should just get back a s_ptr the same device
TEST_F(DeviceDatabaseTest, create_classic_device_twice) {
  auto classic_device = device_database_.CreateClassicDevice(address);
  ASSERT_TRUE(classic_device);
  ASSERT_EQ(CLASSIC, classic_device->GetDeviceType());
  ASSERT_EQ(address_str, classic_device->GetUuid());
  ASSERT_TRUE(device_database_.CreateClassicDevice(address));
}

TEST_F(DeviceDatabaseTest, create_le_device_twice) {
  auto le_device = device_database_.CreateLeDevice(address);
  ASSERT_TRUE(le_device);
  ASSERT_EQ(LE, le_device->GetDeviceType());
  ASSERT_EQ(address_str, le_device->GetUuid());
  ASSERT_TRUE(device_database_.CreateLeDevice(address));
}

TEST_F(DeviceDatabaseTest, create_dual_device_twice) {
  auto dual_device = device_database_.CreateDualDevice(address);
  ASSERT_TRUE(dual_device);

  // Dual
  ASSERT_EQ(DUAL, dual_device->GetDeviceType());
  ASSERT_EQ(address_str, dual_device->GetUuid());

  // Classic
  ASSERT_EQ(DUAL, dual_device->GetClassicDevice()->GetDeviceType());
  ASSERT_EQ(address_str, dual_device->GetClassicDevice()->GetUuid());

  // LE
  ASSERT_EQ(DUAL, dual_device->GetLeDevice()->GetDeviceType());
  ASSERT_EQ(address_str, dual_device->GetLeDevice()->GetUuid());

  ASSERT_TRUE(device_database_.CreateDualDevice(address));
}

TEST_F(DeviceDatabaseTest, remove_device) {
  std::shared_ptr<Device> created_device = device_database_.CreateClassicDevice(address);
  ASSERT_TRUE(created_device);
  ASSERT_TRUE(device_database_.RemoveDevice(created_device));
  ASSERT_TRUE(device_database_.CreateClassicDevice(address));
}

TEST_F(DeviceDatabaseTest, remove_device_twice) {
  std::shared_ptr<Device> created_device = device_database_.CreateClassicDevice(address);
  ASSERT_TRUE(device_database_.RemoveDevice(created_device));
  ASSERT_FALSE(device_database_.RemoveDevice(created_device));
}

TEST_F(DeviceDatabaseTest, get_nonexistent_device) {
  std::shared_ptr<Device> device_ptr = device_database_.GetClassicDevice(address_str);
  ASSERT_FALSE(device_ptr);
}

TEST_F(DeviceDatabaseTest, address_modification_check) {
  std::shared_ptr<Device> created_device = device_database_.CreateClassicDevice(address);
  std::shared_ptr<Device> gotten_device = device_database_.GetClassicDevice(address.ToString());
  ASSERT_TRUE(created_device);
  ASSERT_TRUE(gotten_device);
  ASSERT_EQ(address_str, created_device->GetAddress().ToString());
  ASSERT_EQ(address_str, gotten_device->GetAddress().ToString());
  device_database_.UpdateDeviceAddress(created_device, Address({0x01, 0x01, 0x01, 0x01, 0x01, 0x01}));
  ASSERT_EQ("01:01:01:01:01:01", created_device->GetAddress().ToString());
  ASSERT_EQ("01:01:01:01:01:01", gotten_device->GetAddress().ToString());
  std::shared_ptr<Device> gotten_modified_device = device_database_.GetClassicDevice("01:01:01:01:01:01");
  ASSERT_TRUE(gotten_modified_device);
  ASSERT_TRUE(device_database_.RemoveDevice(gotten_modified_device));
  ASSERT_FALSE(device_database_.GetClassicDevice("01:01:01:01:01:01"));
}
}  // namespace
}  // namespace bluetooth::hci
