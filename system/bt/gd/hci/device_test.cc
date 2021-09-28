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
#include "device.h"
#include "classic_device.h"

#include <gtest/gtest.h>

using namespace bluetooth::hci;

static const char* test_addr_str = "bc:9a:78:56:34:12";
static const uint8_t test_addr[] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};
static const Address address(test_addr);

namespace bluetooth::hci {
namespace {
class TestableDevice : public Device {
 public:
  explicit TestableDevice(Address a) : Device(a, CLASSIC) {}

  void SetTheAddress() {
    Address a({0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    this->SetAddress(a);
  }
  void SetTheClassOfDevice() {
    ClassOfDevice class_of_device({0x01, 0x02, 0x03});
    this->SetClassOfDevice(class_of_device);
  }
  void SetTheName() {
    std::string name = "Some Name";
    this->SetName(name);
  }
  void SetTheIsBonded() {
    this->SetIsBonded(true);
  }
};
class DeviceTest : public ::testing::Test {
 public:
  DeviceTest() : device_(Address(test_addr)) {}

 protected:
  void SetUp() override {}

  void TearDown() override {}
  TestableDevice device_;
};

TEST_F(DeviceTest, initial_integrity) {
  ASSERT_STREQ(test_addr_str, device_.GetAddress().ToString().c_str());
  ASSERT_STREQ(test_addr_str, device_.GetUuid().c_str());
  ASSERT_EQ(DeviceType::CLASSIC, device_.GetDeviceType());
  ASSERT_EQ("", device_.GetName());
}

TEST_F(DeviceTest, set_get_class_of_device) {
  ClassOfDevice class_of_device({0x01, 0x02, 0x03});
  ASSERT_NE(class_of_device, device_.GetClassOfDevice());
  device_.SetTheClassOfDevice();
  ASSERT_EQ(class_of_device, device_.GetClassOfDevice());
}

TEST_F(DeviceTest, set_get_name) {
  std::string name = "Some Name";
  ASSERT_EQ("", device_.GetName());
  device_.SetTheName();
  ASSERT_EQ(name, device_.GetName());
}

TEST_F(DeviceTest, operator_iseq) {
  TestableDevice d(address);
  EXPECT_EQ(device_, d);
}

TEST_F(DeviceTest, set_address) {
  ASSERT_EQ(test_addr_str, device_.GetAddress().ToString());
  device_.SetTheAddress();
  ASSERT_EQ("06:05:04:03:02:01", device_.GetAddress().ToString());
}

TEST_F(DeviceTest, set_bonded) {
  ASSERT_FALSE(device_.IsBonded());
  device_.SetTheIsBonded();
  ASSERT_TRUE(device_.IsBonded());
}

}  // namespace
}  // namespace bluetooth::hci
