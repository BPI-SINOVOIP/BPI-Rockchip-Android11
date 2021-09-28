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

#include "utils/checksum.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

TEST(LuhnTest, CorrectlyHandlesSimpleCases) {
  EXPECT_TRUE(VerifyLuhnChecksum("3782 8224 6310 005"));
  EXPECT_FALSE(VerifyLuhnChecksum("0"));
  EXPECT_FALSE(VerifyLuhnChecksum("1"));
  EXPECT_FALSE(VerifyLuhnChecksum("0A"));
}

TEST(LuhnTest, CorrectlyVerifiesPaymentCardNumbers) {
  // Fake test numbers.
  EXPECT_TRUE(VerifyLuhnChecksum("3782 8224 6310 005"));
  EXPECT_TRUE(VerifyLuhnChecksum("371449635398431"));
  EXPECT_TRUE(VerifyLuhnChecksum("5610591081018250"));
  EXPECT_TRUE(VerifyLuhnChecksum("38520000023237"));
  EXPECT_TRUE(VerifyLuhnChecksum("6011000990139424"));
  EXPECT_TRUE(VerifyLuhnChecksum("3566002020360505"));
  EXPECT_TRUE(VerifyLuhnChecksum("5105105105105100"));
  EXPECT_TRUE(VerifyLuhnChecksum("4012 8888 8888 1881"));
}

TEST(LuhnTest, HandlesWhitespace) {
  EXPECT_TRUE(
      VerifyLuhnChecksum("3782 8224 6310 005 ", /*ignore_whitespace=*/true));
  EXPECT_FALSE(
      VerifyLuhnChecksum("3782 8224 6310 005 ", /*ignore_whitespace=*/false));
}

TEST(LuhnTest, HandlesEdgeCases) {
  EXPECT_FALSE(VerifyLuhnChecksum("    ", /*ignore_whitespace=*/true));
  EXPECT_FALSE(VerifyLuhnChecksum("    ", /*ignore_whitespace=*/false));
  EXPECT_FALSE(VerifyLuhnChecksum("", /*ignore_whitespace=*/true));
  EXPECT_FALSE(VerifyLuhnChecksum("", /*ignore_whitespace=*/false));
}

}  // namespace
}  // namespace libtextclassifier3
