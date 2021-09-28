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

#include <unordered_map>

#include <gtest/gtest.h>

#include "hci/address.h"
#include "hci/address_with_type.h"
#include "hci/hci_packets.h"

namespace bluetooth {
namespace hci {

TEST(AddressWithTypeTest, AddressWithTypeSameValueSameOrder) {
  Address addr1{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
  AddressType type1 = AddressType::PUBLIC_DEVICE_ADDRESS;
  AddressWithType address_with_type_1(addr1, type1);
  Address addr2{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
  AddressType type2 = AddressType::PUBLIC_DEVICE_ADDRESS;
  AddressWithType address_with_type_2(addr2, type2);
  // Test if two address with type with same byte value have the same hash
  struct std::hash<bluetooth::hci::AddressWithType> hasher;
  EXPECT_EQ(hasher(address_with_type_1), hasher(address_with_type_2));
  // Test if two address with type with the same hash and the same value, they will
  // still map to the same value
  std::unordered_map<AddressWithType, int> data = {};
  data[address_with_type_1] = 5;
  data[address_with_type_2] = 8;
  EXPECT_EQ(data[address_with_type_1], data[address_with_type_2]);
}

TEST(AddressWithTypeTest, HashDifferentDiffAddrSameType) {
  Address addr{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
  AddressType type = AddressType::PUBLIC_IDENTITY_ADDRESS;
  AddressWithType address_with_type(addr, type);
  struct std::hash<AddressWithType> hasher;
  EXPECT_NE(hasher(address_with_type), hasher(AddressWithType(Address::kEmpty, AddressType::PUBLIC_IDENTITY_ADDRESS)));
}

TEST(AddressWithTypeTest, HashDifferentSameAddressDiffType) {
  Address addr1{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
  AddressType type1 = AddressType::PUBLIC_DEVICE_ADDRESS;
  AddressWithType address_with_type_1(addr1, type1);
  Address addr2{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
  AddressType type2 = AddressType::PUBLIC_IDENTITY_ADDRESS;
  AddressWithType address_with_type_2(addr2, type2);
  struct std::hash<bluetooth::hci::AddressWithType> hasher;
  EXPECT_NE(hasher(address_with_type_1), hasher(address_with_type_2));
}

TEST(AddressWithTypeTest, IsRpa) {
  // Public address can't be RPA
  EXPECT_FALSE(
      AddressWithType(Address{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}}, AddressType::PUBLIC_IDENTITY_ADDRESS).IsRpa());

  // Must have proper Most Significant Bit configuration
  EXPECT_FALSE(
      AddressWithType(Address{{0x30, 0x02, 0x03, 0x04, 0x05, 0x06}}, AddressType::RANDOM_DEVICE_ADDRESS).IsRpa());
  EXPECT_TRUE(
      AddressWithType(Address{{0x40, 0x02, 0x03, 0x04, 0x05, 0x03}}, AddressType::RANDOM_DEVICE_ADDRESS).IsRpa());
  EXPECT_TRUE(
      AddressWithType(Address{{0x50, 0x02, 0x03, 0x04, 0x05, 0x06}}, AddressType::RANDOM_DEVICE_ADDRESS).IsRpa());
  EXPECT_TRUE(
      AddressWithType(Address{{0x60, 0x02, 0x03, 0x04, 0x05, 0x06}}, AddressType::RANDOM_DEVICE_ADDRESS).IsRpa());
  EXPECT_TRUE(
      AddressWithType(Address{{0x70, 0x02, 0x03, 0x04, 0x05, 0x06}}, AddressType::RANDOM_DEVICE_ADDRESS).IsRpa());
  EXPECT_FALSE(
      AddressWithType(Address{{0x80, 0x02, 0x03, 0x04, 0x05, 0x06}}, AddressType::RANDOM_DEVICE_ADDRESS).IsRpa());
}

TEST(AddressWithTypeTest, IsRpaThatMatchesIrk) {
  // Public address can't be RPA
  AddressWithType address_1 =
      AddressWithType(Address{{0x50, 0x02, 0x03, 0xC9, 0x12, 0xDE}}, AddressType::RANDOM_DEVICE_ADDRESS);
  AddressWithType address_2 =
      AddressWithType(Address{{0x50, 0x02, 0x03, 0xC9, 0x12, 0xDD}}, AddressType::RANDOM_DEVICE_ADDRESS);
  crypto_toolbox::Octet16 irk_1{0x90, 0x5e, 0x60, 0x59, 0xc9, 0x11, 0x43, 0x7b,
                                0x04, 0x09, 0x6a, 0x53, 0x28, 0xe6, 0x59, 0x6d};

  EXPECT_TRUE(address_1.IsRpaThatMatchesIrk(irk_1));
  EXPECT_FALSE(address_2.IsRpaThatMatchesIrk(irk_1));
}

}  // namespace hci
}  // namespace bluetooth