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
#include "dual_device.h"
#include "device.h"

#include <gtest/gtest.h>

using namespace bluetooth::hci;

static const char* test_addr_str = "bc:9a:78:56:34:12";
static const uint8_t test_addr[] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};
static const Address address(test_addr);

namespace bluetooth::hci {
namespace {
class TestableClassicDevice : public ClassicDevice {
 public:
  explicit TestableClassicDevice(Address a) : ClassicDevice(a) {}
};
class TestableLeDevice : public LeDevice {
 public:
  explicit TestableLeDevice(Address a) : LeDevice(a) {}
};
class TestableDevice : public DualDevice {
 public:
  TestableDevice(Address a, std::shared_ptr<TestableClassicDevice>& classic_device,
                 std::shared_ptr<TestableLeDevice>& le_device)
      : DualDevice(a, classic_device, le_device) {}

  void SetTheAddress() {
    Address a({0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    this->SetAddress(a);
  }
};
std::shared_ptr<TestableClassicDevice> classic_device = std::make_shared<TestableClassicDevice>(address);
std::shared_ptr<TestableLeDevice> le_device = std::make_shared<TestableLeDevice>(address);
class DualDeviceTest : public ::testing::Test {
 public:
  DualDeviceTest() : device_(Address(test_addr), classic_device, le_device) {}

 protected:
  void SetUp() override {}

  void TearDown() override {}
  TestableDevice device_;
};

TEST_F(DualDeviceTest, initial_integrity) {
  Address a = device_.GetAddress();
  ASSERT_EQ(test_addr_str, a.ToString());

  ASSERT_EQ(DUAL, device_.GetClassicDevice()->GetDeviceType());
  ASSERT_EQ(a.ToString(), device_.GetClassicDevice()->GetAddress().ToString());

  ASSERT_EQ(DUAL, device_.GetLeDevice()->GetDeviceType());
  ASSERT_EQ(a.ToString(), device_.GetLeDevice()->GetAddress().ToString());

  device_.SetTheAddress();

  ASSERT_EQ("06:05:04:03:02:01", device_.GetAddress().ToString());
  ASSERT_EQ("06:05:04:03:02:01", device_.GetClassicDevice()->GetAddress().ToString());
  ASSERT_EQ("06:05:04:03:02:01", device_.GetLeDevice()->GetAddress().ToString());
}

}  // namespace
}  // namespace bluetooth::hci
