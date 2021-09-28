/*
 * Copyright 2019 The Android Open Source Project
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

#include <gtest/gtest.h>
#include <cstdint>

#include "l2cap/signal_id.h"

namespace bluetooth {
namespace l2cap {

TEST(L2capSignalIdTest, valid_values) {
  int valid = 0;
  uint8_t i = 0;
  while (++i != 0) {
    SignalId signal_id(i);
    if (signal_id.IsValid()) {
      valid++;
    }
  }
  ASSERT_TRUE(valid == 255);
}

TEST(L2capSignalIdTest, zero_invalid) {
  SignalId signal_id(0);
  ASSERT_FALSE(signal_id.IsValid());
}

TEST(L2capSignalIdTest, pre_increment) {
  SignalId signal_id(0);
  ASSERT_FALSE(signal_id.IsValid());

  for (uint8_t i = 0; i != 0xff; ++i, ++signal_id) {
    ASSERT_TRUE(i == signal_id.Value());
  }
}

TEST(L2capSignalIdTest, post_increment) {
  SignalId signal_id(0);
  ASSERT_FALSE(signal_id.IsValid());

  for (uint8_t i = 0; i != 0xff; i++, signal_id++) {
    ASSERT_TRUE(i == signal_id.Value());
  }
}

TEST(L2capSignalIdTest, almost_wrap_up) {
  SignalId signal_id(0);
  ASSERT_FALSE(signal_id.IsValid());

  for (int i = 0; i < 255; i++) {
    signal_id++;
  }
  ASSERT_EQ(0xff, signal_id.Value());
}

TEST(L2capSignalIdTest, wrap_up) {
  SignalId signal_id(0);
  ASSERT_FALSE(signal_id.IsValid());

  for (int i = 0; i < 256; i++) {
    signal_id++;
  }
  ASSERT_EQ(1, signal_id.Value());
}

TEST(L2capSignalIdTest, pre_decrement) {
  SignalId signal_id(0);
  ASSERT_FALSE(signal_id.IsValid());

  for (uint8_t i = 0; i != 0xff; --i, --signal_id) {
    ASSERT_TRUE(i == signal_id.Value());
  }
}

TEST(L2capSignalIdTest, post_decrement) {
  SignalId signal_id(0);
  ASSERT_FALSE(signal_id.IsValid());

  for (uint8_t i = 0; i != 0xff; i--, signal_id--) {
    ASSERT_TRUE(i == signal_id.Value());
  }
}

TEST(L2capSignalIdTest, almost_wrap_down) {
  SignalId signal_id(0);
  ASSERT_FALSE(signal_id.IsValid());

  for (int i = 0; i < 255; i++) {
    signal_id--;
  }
  ASSERT_EQ(1, signal_id.Value());
}

TEST(L2capSignalIdTest, wrap_down) {
  SignalId signal_id(0);
  ASSERT_FALSE(signal_id.IsValid());

  for (int i = 0; i < 256; i++) {
    signal_id--;
  }
  ASSERT_EQ(0xff, signal_id.Value());
}

}  // namespace l2cap
}  // namespace bluetooth
