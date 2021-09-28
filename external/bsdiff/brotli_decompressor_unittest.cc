// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bsdiff/brotli_decompressor.h"

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

// echo -n "Hello!" | brotli -9 | hexdump -v -e '"    " 11/1 "0x%02x, " "\n"'
constexpr uint8_t kBrotliHello[] = {
    0x8b, 0x02, 0x80, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x21, 0x03,
};

}  // namespace

namespace bsdiff {

class BrotliDecompressorTest : public testing::Test {
 protected:
  void SetUp() {
    decompressor_.reset(new BrotliDecompressor());
    EXPECT_NE(nullptr, decompressor_.get());
  }

  std::unique_ptr<BrotliDecompressor> decompressor_;
};

TEST_F(BrotliDecompressorTest, SmokeTest) {
  EXPECT_TRUE(decompressor_->SetInputData(kBrotliHello, sizeof(kBrotliHello)));
  std::vector<uint8_t> output_data(6);
  EXPECT_TRUE(decompressor_->Read(output_data.data(), output_data.size()));
  std::string hello = "Hello!";
  EXPECT_EQ(std::vector<uint8_t>(hello.begin(), hello.end()), output_data);
}

TEST_F(BrotliDecompressorTest, ReadingFromEmptyFileTest) {
  uint8_t data = 0;
  EXPECT_TRUE(decompressor_->SetInputData(&data, 0));

  uint8_t output_data[10];
  EXPECT_FALSE(decompressor_->Read(output_data, sizeof(output_data)));
}

// Check that we fail to read from a truncated file.
TEST_F(BrotliDecompressorTest, ReadingFromTruncatedFileTest) {
  // We feed only half of the compressed file.
  EXPECT_TRUE(
      decompressor_->SetInputData(kBrotliHello, sizeof(kBrotliHello) / 2));
  uint8_t output_data[6];
  EXPECT_FALSE(decompressor_->Read(output_data, sizeof(output_data)));
}

// Check that we fail to read more than it is available in the file.
TEST_F(BrotliDecompressorTest, ReadingMoreThanAvailableTest) {
  EXPECT_TRUE(decompressor_->SetInputData(kBrotliHello, sizeof(kBrotliHello)));
  uint8_t output_data[1000];
  EXPECT_FALSE(decompressor_->Read(output_data, sizeof(output_data)));
}

}  // namespace bsdiff
