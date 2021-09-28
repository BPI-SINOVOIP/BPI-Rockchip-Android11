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

#include <gtest/gtest.h>

#include "security/ecc/p_256_ecc_pp.h"

namespace bluetooth {
namespace security {
namespace ecc {

// Test ECC point validation
TEST(SmpEccValidationTest, test_valid_points) {
  Point p;

  // Test data from Bluetooth Core Specification
  // Version 5.0 | Vol 2, Part G | 7.1.2

  // Sample 1
  p.x[7] = 0x20b003d2;
  p.x[6] = 0xf297be2c;
  p.x[5] = 0x5e2c83a7;
  p.x[4] = 0xe9f9a5b9;
  p.x[3] = 0xeff49111;
  p.x[2] = 0xacf4fddb;
  p.x[1] = 0xcc030148;
  p.x[0] = 0x0e359de6;

  p.y[7] = 0xdc809c49;
  p.y[6] = 0x652aeb6d;
  p.y[5] = 0x63329abf;
  p.y[4] = 0x5a52155c;
  p.y[3] = 0x766345c2;
  p.y[2] = 0x8fed3024;
  p.y[1] = 0x741c8ed0;
  p.y[0] = 0x1589d28b;

  EXPECT_TRUE(ECC_ValidatePoint(p));

  // Sample 2
  p.x[7] = 0x2c31a47b;
  p.x[6] = 0x5779809e;
  p.x[5] = 0xf44cb5ea;
  p.x[4] = 0xaf5c3e43;
  p.x[3] = 0xd5f8faad;
  p.x[2] = 0x4a8794cb;
  p.x[1] = 0x987e9b03;
  p.x[0] = 0x745c78dd;

  p.y[7] = 0x91951218;
  p.y[6] = 0x3898dfbe;
  p.y[5] = 0xcd52e240;
  p.y[4] = 0x8e43871f;
  p.y[3] = 0xd0211091;
  p.y[2] = 0x17bd3ed4;
  p.y[1] = 0xeaf84377;
  p.y[0] = 0x43715d4f;

  EXPECT_TRUE(ECC_ValidatePoint(p));
}

TEST(SmpEccValidationTest, test_invalid_points) {
  Point p;
  multiprecision_init(p.x);
  multiprecision_init(p.y);

  EXPECT_FALSE(ECC_ValidatePoint(p));

  // Sample 1
  p.x[7] = 0x20b003d2;
  p.x[6] = 0xf297be2c;
  p.x[5] = 0x5e2c83a7;
  p.x[4] = 0xe9f9a5b9;
  p.x[3] = 0xeff49111;
  p.x[2] = 0xacf4fddb;
  p.x[1] = 0xcc030148;
  p.x[0] = 0x0e359de6;

  EXPECT_FALSE(ECC_ValidatePoint(p));

  p.y[7] = 0xdc809c49;
  p.y[6] = 0x652aeb6d;
  p.y[5] = 0x63329abf;
  p.y[4] = 0x5a52155c;
  p.y[3] = 0x766345c2;
  p.y[2] = 0x8fed3024;
  p.y[1] = 0x741c8ed0;
  p.y[0] = 0x1589d28b;

  p.y[0]--;

  EXPECT_FALSE(ECC_ValidatePoint(p));
}

}  // namespace ecc
}  // namespace security
}  // namespace bluetooth
