/******************************************************************************
 *
 *  Copyright 2020 The Android Open Source Project
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include <base/strings/string_number_conversions.h>
#include "hci/le_security_interface.h"
#include "os/log.h"
#include "security/ecc/p_256_ecc_pp.h"
#include "security/ecdh_keys.h"
#include "security/test/mocks.h"

using namespace std::chrono_literals;

namespace bluetooth {
namespace security {

class EcdhKeysTest : public testing::Test {
 protected:
  void SetUp() {}

  void TearDown() {}

 public:
};

/* This test generates two pairs of keys, computes the Diffie–Hellman key using both, and verifies that they match */
TEST_F(EcdhKeysTest, test_generated) {
  std::srand(std::time(nullptr));

  auto [private_key_a, public_key_a] = GenerateECDHKeyPair();
  auto [private_key_b, public_key_b] = GenerateECDHKeyPair();

  std::array<uint8_t, 32> dhkeya = ComputeDHKey(private_key_a, public_key_b);
  std::array<uint8_t, 32> dhkeyb = ComputeDHKey(private_key_b, public_key_a);

  EXPECT_EQ(dhkeya, dhkeyb);

  if (dhkeya != dhkeyb) {
    LOG_ERROR("private key a : %s", base::HexEncode(private_key_a.data(), private_key_a.size()).c_str());
    LOG_ERROR("public key a.x : %s", base::HexEncode(public_key_a.x.data(), public_key_a.x.size()).c_str());
    LOG_ERROR("public key a.y : %s", base::HexEncode(public_key_a.y.data(), public_key_a.y.size()).c_str());

    LOG_ERROR("private key b : %s", base::HexEncode(private_key_b.data(), private_key_b.size()).c_str());
    LOG_ERROR("public key b.x : %s", base::HexEncode(public_key_b.x.data(), public_key_b.x.size()).c_str());
    LOG_ERROR("public key b.y : %s", base::HexEncode(public_key_b.y.data(), public_key_b.y.size()).c_str());

    LOG_ERROR("dhkeya : %s", base::HexEncode(dhkeya.data(), dhkeya.size()).c_str());
    LOG_ERROR("dhkeyb : %s", base::HexEncode(dhkeyb.data(), dhkeyb.size()).c_str());
  }
}

/* This test uses two fixed pairs of keys, computes the Diffie–Hellman key using both, and verifies that they match
precomputed value.

This code is also useful during debuging - one can replace fixed values with values from failed exchagne to verify which
side did bad computation. */
TEST_F(EcdhKeysTest, test_static) {
  std::array<uint8_t, 32> private_key_a = {0x3E, 0xC8, 0x2A, 0x32, 0xB3, 0x75, 0x76, 0xBA, 0x7D, 0xB8, 0xB4,
                                           0x7B, 0xA0, 0x8A, 0xA3, 0xC3, 0xF2, 0x03, 0x1A, 0x53, 0xF6, 0x52,
                                           0x26, 0x32, 0xB6, 0xAE, 0x57, 0x3F, 0x13, 0x15, 0x29, 0x51};
  bluetooth::security::EcdhPublicKey public_key_a;
  uint8_t ax[32] = {0xDC, 0x88, 0xD0, 0xE5, 0x59, 0x73, 0xF2, 0x41, 0x88, 0x6C, 0xB4, 0x45, 0x8B, 0x61, 0x3B, 0x10,
                    0xF5, 0xD4, 0xD2, 0x5B, 0x4E, 0xA1, 0x7F, 0x94, 0xE3, 0xA9, 0x38, 0xF8, 0x84, 0xD4, 0x98, 0x10};
  uint8_t ay[32] = {0x3D, 0x13, 0x76, 0x4F, 0xD1, 0x29, 0x6E, 0xEC, 0x8D, 0xF6, 0x70, 0x33, 0x8B, 0xA7, 0x18, 0xEA,
                    0x84, 0x15, 0xE8, 0x8C, 0x4A, 0xC8, 0x76, 0x45, 0x90, 0x98, 0xBA, 0x52, 0x8B, 0x00, 0x69, 0xAF};
  memcpy(public_key_a.x.data(), ax, 32);
  memcpy(public_key_a.y.data(), ay, 32);

  std::array<uint8_t, 32> private_key_b = {0xDD, 0x53, 0x84, 0x91, 0xC8, 0xFA, 0x4B, 0x45, 0xB2, 0xFF, 0xC0,
                                           0x53, 0x89, 0x64, 0x16, 0x7B, 0x67, 0x30, 0xCE, 0x5D, 0x82, 0xF4,
                                           0x8F, 0x38, 0xA2, 0xE6, 0x78, 0xB6, 0xFB, 0xA1, 0x07, 0xD8};
  bluetooth::security::EcdhPublicKey public_key_b;
  uint8_t bx[32] = {0x23, 0x1A, 0xEC, 0xFE, 0x7D, 0xC1, 0x20, 0x2F, 0x03, 0x3E, 0x9A, 0xAA, 0x99, 0x55, 0x78, 0x86,
                    0x58, 0xCB, 0x37, 0x68, 0x7D, 0xE1, 0xFF, 0x19, 0x33, 0xF8, 0xCB, 0x7A, 0x17, 0xAB, 0x0B, 0x73};
  uint8_t by[32] = {0x4C, 0x25, 0xE2, 0x42, 0x3C, 0x69, 0x0E, 0x3B, 0xC0, 0xEF, 0x94, 0x09, 0x4D, 0x3F, 0x96, 0xBB,
                    0x18, 0xF2, 0x55, 0x81, 0x71, 0x5A, 0xDE, 0xC4, 0x3E, 0xF9, 0x6F, 0xA9, 0xAF, 0x04, 0x4E, 0x86};
  memcpy(public_key_b.x.data(), bx, 32);
  memcpy(public_key_b.y.data(), by, 32);

  std::array<uint8_t, 32> dhkey;
  uint8_t dhkey_val[32] = {0x3B, 0xF8, 0xDF, 0x33, 0x99, 0x94, 0x66, 0x55, 0x4F, 0x2C, 0x4A,
                           0x78, 0x2B, 0x51, 0xD1, 0x49, 0x0F, 0xF1, 0x96, 0x63, 0x51, 0x75,
                           0x9E, 0x65, 0x7F, 0x3C, 0xFE, 0x77, 0xB4, 0x3F, 0x7A, 0x93};
  memcpy(dhkey.data(), dhkey_val, 32);

  std::array<uint8_t, 32> dhkey_a = ComputeDHKey(private_key_a, public_key_b);
  std::array<uint8_t, 32> dhkey_b = ComputeDHKey(private_key_b, public_key_a);

  EXPECT_EQ(dhkey_a, dhkey);
  EXPECT_EQ(dhkey_b, dhkey);
}

}  // namespace security
}  // namespace bluetooth
