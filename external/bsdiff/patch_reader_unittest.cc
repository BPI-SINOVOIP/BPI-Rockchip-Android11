// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bsdiff/patch_reader.h"

#include <unistd.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "bsdiff/brotli_compressor.h"
#include "bsdiff/bz2_compressor.h"
#include "bsdiff/utils.h"

namespace {

void EncodeInt64(int64_t x, uint8_t* buf) {
  uint64_t y = x < 0 ? (1ULL << 63ULL) - x : x;
  for (int i = 0; i < 8; ++i) {
    buf[i] = y & 0xff;
    y /= 256;
  }
}

}  // namespace

namespace bsdiff {

class PatchReaderTest : public testing::Test {
 protected:
  void CompressData() {
    for (size_t i = 0; i < diff_data_.size(); i++) {
      uint8_t buf[24];
      EncodeInt64(diff_data_[i].size(), buf);
      EncodeInt64(extra_data_[i].size(), buf + 8);
      EncodeInt64(offset_increment_[i], buf + 16);
      EXPECT_TRUE(ctrl_stream_->Write(buf, sizeof(buf)));
      EXPECT_TRUE(diff_stream_->Write(
          reinterpret_cast<const uint8_t*>(diff_data_[i].data()),
          diff_data_[i].size()));
      EXPECT_TRUE(extra_stream_->Write(
          reinterpret_cast<const uint8_t*>(extra_data_[i].data()),
          extra_data_[i].size()));
    }
    EXPECT_TRUE(ctrl_stream_->Finish());
    EXPECT_TRUE(diff_stream_->Finish());
    EXPECT_TRUE(extra_stream_->Finish());
  }

  void ConstructPatchHeader(int64_t ctrl_size,
                            int64_t diff_size,
                            int64_t new_size,
                            std::vector<uint8_t>* patch_data) {
    EXPECT_EQ(static_cast<size_t>(8), patch_data->size());
    // Encode the header.
    uint8_t buf[24];
    EncodeInt64(ctrl_size, buf);
    EncodeInt64(diff_size, buf + 8);
    EncodeInt64(new_size, buf + 16);
    std::copy(buf, buf + sizeof(buf), std::back_inserter(*patch_data));
  }

  void ConstructPatchData(std::vector<uint8_t>* patch_data) {
    ConstructPatchHeader(ctrl_stream_->GetCompressedData().size(),
                         diff_stream_->GetCompressedData().size(),
                         new_file_size_, patch_data);

    // Concatenate the three streams into one patch.
    std::copy(ctrl_stream_->GetCompressedData().begin(),
              ctrl_stream_->GetCompressedData().end(),
              std::back_inserter(*patch_data));
    std::copy(diff_stream_->GetCompressedData().begin(),
              diff_stream_->GetCompressedData().end(),
              std::back_inserter(*patch_data));
    std::copy(extra_stream_->GetCompressedData().begin(),
              extra_stream_->GetCompressedData().end(),
              std::back_inserter(*patch_data));
  }

  void VerifyPatch(const std::vector<uint8_t>& patch_data) {
    BsdiffPatchReader patch_reader;
    EXPECT_TRUE(patch_reader.Init(patch_data.data(), patch_data.size()));
    EXPECT_EQ(new_file_size_, patch_reader.new_file_size());
    // Check that the decompressed data matches what we wrote.
    for (size_t i = 0; i < diff_data_.size(); i++) {
      ControlEntry control_entry(0, 0, 0);
      EXPECT_TRUE(patch_reader.ParseControlEntry(&control_entry));
      EXPECT_EQ(diff_data_[i].size(), control_entry.diff_size);
      EXPECT_EQ(extra_data_[i].size(), control_entry.extra_size);
      EXPECT_EQ(offset_increment_[i], control_entry.offset_increment);

      uint8_t buffer[128] = {};
      EXPECT_TRUE(patch_reader.ReadDiffStream(buffer, diff_data_[i].size()));
      EXPECT_EQ(0, memcmp(buffer, diff_data_[i].data(), diff_data_[i].size()));
      EXPECT_TRUE(patch_reader.ReadExtraStream(buffer, extra_data_[i].size()));
      EXPECT_EQ(0,
                memcmp(buffer, extra_data_[i].data(), extra_data_[i].size()));
    }
    EXPECT_TRUE(patch_reader.Finish());
  }

  // Helper function to check that invalid headers are detected. This method
  // creates a new header with the passed |ctrl_size|, |diff_size| and
  // |new_size| and appends after the header |compressed_size| bytes of extra
  // zeros. It then expects that initializing a PatchReader with this will fail.
  void InvalidHeaderTestHelper(int64_t ctrl_size,
                               int64_t diff_size,
                               int64_t new_size,
                               size_t compressed_size) {
    std::vector<uint8_t> patch_data;
    std::copy(kBSDF2MagicHeader, kBSDF2MagicHeader + 5,
              std::back_inserter(patch_data));
    patch_data.push_back(static_cast<uint8_t>(CompressorType::kBrotli));
    patch_data.push_back(static_cast<uint8_t>(CompressorType::kBrotli));
    patch_data.push_back(static_cast<uint8_t>(CompressorType::kBrotli));
    ConstructPatchHeader(ctrl_size, diff_size, new_size, &patch_data);
    patch_data.resize(patch_data.size() + compressed_size);

    BsdiffPatchReader patch_reader;
    EXPECT_FALSE(patch_reader.Init(patch_data.data(), patch_data.size()))
        << "Where ctrl_size=" << ctrl_size << " diff_size=" << diff_size
        << " new_size=" << new_size << " compressed_size=" << compressed_size;
  }

  size_t new_file_size_{500};
  std::vector<std::string> diff_data_{"HelloWorld", "BspatchPatchTest",
                                      "BspatchDiffData"};
  std::vector<std::string> extra_data_{"HelloWorld!", "BZ2PatchReaderSmoke",
                                       "BspatchExtraData"};
  std::vector<int64_t> offset_increment_{100, 200, 300};

  // The compressor streams.
  std::unique_ptr<CompressorInterface> ctrl_stream_{nullptr};
  std::unique_ptr<CompressorInterface> diff_stream_{nullptr};
  std::unique_ptr<CompressorInterface> extra_stream_{nullptr};
};

TEST_F(PatchReaderTest, PatchReaderLegacyFormatSmoke) {
  ctrl_stream_.reset(new BZ2Compressor());
  diff_stream_.reset(new BZ2Compressor());
  extra_stream_.reset(new BZ2Compressor());

  CompressData();

  std::vector<uint8_t> patch_data;
  std::copy(kLegacyMagicHeader, kLegacyMagicHeader + 8,
            std::back_inserter(patch_data));
  ConstructPatchData(&patch_data);

  VerifyPatch(patch_data);
}

TEST_F(PatchReaderTest, PatchReaderNewFormatSmoke) {
  // Compress the data with one bz2 and two brotli compressors.
  ctrl_stream_.reset(new BZ2Compressor());
  diff_stream_.reset(new BrotliCompressor(11));
  extra_stream_.reset(new BrotliCompressor(11));

  CompressData();

  std::vector<uint8_t> patch_data;
  std::copy(kBSDF2MagicHeader, kBSDF2MagicHeader + 5,
            std::back_inserter(patch_data));
  patch_data.push_back(static_cast<uint8_t>(CompressorType::kBZ2));
  patch_data.push_back(static_cast<uint8_t>(CompressorType::kBrotli));
  patch_data.push_back(static_cast<uint8_t>(CompressorType::kBrotli));
  ConstructPatchData(&patch_data);

  VerifyPatch(patch_data);
}

TEST_F(PatchReaderTest, InvalidHeaderTest) {
  // Negative values are not allowed.
  InvalidHeaderTestHelper(-1, 0, 20, 50);
  InvalidHeaderTestHelper(30, -3, 20, 50);
  InvalidHeaderTestHelper(30, 8, -20, 50);

  // Values larger than the patch size are also not allowed for ctrl and diff,
  // or for the sum of both.
  InvalidHeaderTestHelper(30, 5, 20, 10);  // 30 > 10
  InvalidHeaderTestHelper(5, 30, 20, 10);  // 30 > 10
  InvalidHeaderTestHelper(30, 5, 20, 32);  // 30 + 5 > 32

  // Values that overflow int64 are also not allowed when used combined
  const int64_t kMax64 = std::numeric_limits<int64_t>::max();
  InvalidHeaderTestHelper(kMax64 - 5, 5, 20, 20);
  InvalidHeaderTestHelper(5, kMax64 - 5, 20, 20);

  // 2 * (kMax64 - 5) + sizeof(header) is still positive due to overflow, but
  // the patch size is too small.
  InvalidHeaderTestHelper(kMax64 - 5, kMax64 - 5, 20, 20);
}

TEST_F(PatchReaderTest, InvalidCompressionHeaderTest) {
  std::vector<uint8_t> patch_data;
  std::copy(kBSDF2MagicHeader, kBSDF2MagicHeader + 5,
            std::back_inserter(patch_data));
  // Set an invalid compression value.
  patch_data.push_back(99);
  patch_data.push_back(static_cast<uint8_t>(CompressorType::kBrotli));
  patch_data.push_back(static_cast<uint8_t>(CompressorType::kBrotli));
  ConstructPatchHeader(10, 10, 10, &patch_data);
  patch_data.resize(patch_data.size() + 30);

  BsdiffPatchReader patch_reader;
  EXPECT_FALSE(patch_reader.Init(patch_data.data(), patch_data.size()));
}

TEST_F(PatchReaderTest, InvalidControlEntryTest) {
  // Check that negative diff and extra values in a control entry are not
  // allowed.
  ctrl_stream_.reset(new BZ2Compressor());
  diff_stream_.reset(new BrotliCompressor(11));
  extra_stream_.reset(new BrotliCompressor(11));

  // Encode the header.
  uint8_t buf[24];
  EncodeInt64(-10, buf);
  EncodeInt64(0, buf + 8);
  EncodeInt64(0, buf + 16);
  ctrl_stream_->Write(buf, sizeof(buf));

  CompressData();

  std::vector<uint8_t> patch_data;
  std::copy(kBSDF2MagicHeader, kBSDF2MagicHeader + 5,
            std::back_inserter(patch_data));
  patch_data.push_back(static_cast<uint8_t>(CompressorType::kBZ2));
  patch_data.push_back(static_cast<uint8_t>(CompressorType::kBrotli));
  patch_data.push_back(static_cast<uint8_t>(CompressorType::kBrotli));
  ConstructPatchData(&patch_data);

  BsdiffPatchReader patch_reader;
  EXPECT_TRUE(patch_reader.Init(patch_data.data(), patch_data.size()));
  ControlEntry control_entry(0, 0, 0);
  EXPECT_FALSE(patch_reader.ParseControlEntry(&control_entry));
}

}  // namespace bsdiff
