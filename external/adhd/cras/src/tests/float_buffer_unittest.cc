// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "float_buffer.h"

#include <gtest/gtest.h>

namespace {
TEST(FloatBuffer, ReadWrite) {
  unsigned int readable = 10;
  struct float_buffer* b = float_buffer_create(10, 2);
  EXPECT_EQ(10, float_buffer_writable(b));

  // (w, r)=(8, 0)
  float_buffer_written(b, 8);
  EXPECT_EQ(8, float_buffer_level(b));

  float_buffer_read_pointer(b, 0, &readable);
  EXPECT_EQ(8, readable);
  EXPECT_EQ(2, float_buffer_writable(b));

  readable = 10;
  float_buffer_read_pointer(b, 3, &readable);
  EXPECT_EQ(5, readable);

  // (w, r)=(8, 6)
  float_buffer_read(b, 6);
  EXPECT_EQ(2, float_buffer_writable(b));

  // (w, r)=(0, 6)
  float_buffer_written(b, 2);
  EXPECT_EQ(6, float_buffer_writable(b));

  // (w, r)=(3, 6)
  readable = 10;
  float_buffer_written(b, 3);
  float_buffer_read_pointer(b, 0, &readable);
  EXPECT_EQ(4, readable);

  readable = 10;
  float_buffer_read_pointer(b, 1, &readable);
  EXPECT_EQ(3, readable);

  float_buffer_destroy(&b);
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
