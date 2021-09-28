// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bsdiff/bz2_decompressor.h"

#include <memory>

#include <gtest/gtest.h>

namespace {

// echo -n "Hello!" | bzip2 -9 | hexdump -v -e '"    " 11/1 "0x%02x, " "\n"'
constexpr uint8_t kBZ2Hello[] = {
    0x42, 0x5a, 0x68, 0x39, 0x31, 0x41, 0x59, 0x26, 0x53, 0x59, 0x1a,
    0xea, 0x74, 0xba, 0x00, 0x00, 0x00, 0x95, 0x00, 0x20, 0x00, 0x00,
    0x40, 0x02, 0x04, 0xa0, 0x00, 0x21, 0x83, 0x41, 0x9a, 0x02, 0x5c,
    0x2e, 0x2e, 0xe4, 0x8a, 0x70, 0xa1, 0x20, 0x35, 0xd4, 0xe9, 0x74,
};

}  // namespace

namespace bsdiff {

class BZ2DecompressorTest : public testing::Test {
 protected:
  void SetUp() {
    decompressor_.reset(new BZ2Decompressor());
    EXPECT_NE(nullptr, decompressor_.get());
  }

  std::unique_ptr<BZ2Decompressor> decompressor_;
};

TEST_F(BZ2DecompressorTest, ReadingFromEmptyFileTest) {
  uint8_t data = 0;
  EXPECT_TRUE(decompressor_->SetInputData(&data, 0));

  uint8_t output_data[10];
  EXPECT_FALSE(decompressor_->Read(output_data, sizeof(output_data)));
}

// Check that we fail to read from a truncated file.
TEST_F(BZ2DecompressorTest, ReadingFromTruncatedFileTest) {
  // We feed only half of the compressed file.
  EXPECT_TRUE(decompressor_->SetInputData(kBZ2Hello, sizeof(kBZ2Hello) / 2));
  uint8_t output_data[6];
  EXPECT_FALSE(decompressor_->Read(output_data, sizeof(output_data)));
}

// Check that we fail to read more than it is available in the file.
TEST_F(BZ2DecompressorTest, ReadingMoreThanAvailableTest) {
  EXPECT_TRUE(decompressor_->SetInputData(kBZ2Hello, sizeof(kBZ2Hello)));
  uint8_t output_data[1000];
  EXPECT_FALSE(decompressor_->Read(output_data, sizeof(output_data)));
}

}  // namespace bsdiff
