// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bsdiff/patch_writer.h"

#include <string.h>
#include <limits>

#include "bsdiff/brotli_compressor.h"
#include "bsdiff/bz2_compressor.h"
#include "bsdiff/constants.h"
#include "bsdiff/control_entry.h"
#include "bsdiff/logging.h"

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

BsdiffPatchWriter::BsdiffPatchWriter(const std::string& patch_filename)
    : patch_filename_(patch_filename),
      format_(BsdiffFormat::kLegacy),
      brotli_quality_(-1) {
  types_.emplace_back(CompressorType::kBZ2);
}


BsdiffPatchWriter::BsdiffPatchWriter(const std::string& patch_filename,
                                     const std::vector<CompressorType>& types,
                                     int brotli_quality)
    : patch_filename_(patch_filename),
      format_(BsdiffFormat::kBsdf2),
      types_(types),
      brotli_quality_(brotli_quality) {}

bool BsdiffPatchWriter::InitializeCompressorList(
    std::vector<std::unique_ptr<bsdiff::CompressorInterface>>*
        compressor_list) {
  if (types_.empty()) {
    LOG(ERROR) << "Patch writer expects at least one compressor.";
    return false;
  }

  for (const auto& type : types_) {
    switch (type) {
      case CompressorType::kBZ2:
        compressor_list->emplace_back(new BZ2Compressor());
        break;
      case CompressorType::kBrotli:
        compressor_list->emplace_back(new BrotliCompressor(brotli_quality_));
        break;
      case CompressorType::kNoCompression:
        LOG(ERROR) << "Unsupported compression type " << static_cast<int>(type);
        return false;
    }
  }

  for (const auto& compressor : *compressor_list) {
    if (!compressor) {
      return false;
    }
  }

  return true;
}

bool BsdiffPatchWriter::Init(size_t /* new_size */) {
  if (!InitializeCompressorList(&ctrl_stream_list_)) {
    LOG(ERROR) << "Failed to initialize control stream compressors.";
    return false;
  }

  if (!InitializeCompressorList(&diff_stream_list_)) {
    LOG(ERROR) << "Failed to initialize diff stream compressors.";
    return false;
  }

  if (!InitializeCompressorList(&extra_stream_list_)) {
    LOG(ERROR) << "Failed to initialize extra stream compressors.";
    return false;
  }

  fp_ = fopen(patch_filename_.c_str(), "w");
  if (!fp_) {
    LOG(ERROR) << "Opening " << patch_filename_;
    return false;
  }
  return true;
}

bool BsdiffPatchWriter::WriteDiffStream(const uint8_t* data, size_t size) {
  for (const auto& compressor : diff_stream_list_) {
    if (!compressor->Write(data, size)) {
      return false;
    }
  }

  return true;
}

bool BsdiffPatchWriter::WriteExtraStream(const uint8_t* data, size_t size) {
  for (const auto& compressor : extra_stream_list_) {
    if (!compressor->Write(data, size)) {
      return false;
    }
  }

  return true;
}

bool BsdiffPatchWriter::AddControlEntry(const ControlEntry& entry) {
  // Generate the 24 byte control entry.
  uint8_t buf[24];
  EncodeInt64(entry.diff_size, buf);
  EncodeInt64(entry.extra_size, buf + 8);
  EncodeInt64(entry.offset_increment, buf + 16);

  for (const auto& compressor : ctrl_stream_list_) {
    if (!compressor->Write(buf, sizeof(buf))) {
      return false;
    }
  }

  written_output_ += entry.diff_size + entry.extra_size;
  return true;
}

bool BsdiffPatchWriter::SelectSmallestResult(
    const std::vector<std::unique_ptr<CompressorInterface>>& compressor_list,
    CompressorInterface** smallest_compressor) {
  size_t min_size = std::numeric_limits<size_t>::max();
  for (const auto& compressor : compressor_list) {
    if (!compressor->Finish()) {
      LOG(ERROR) << "Failed to finalize compressed streams.";
      return false;
    }

    // Update |smallest_compressor| if the current compressor produces a
    // smaller result.
    if (compressor->GetCompressedData().size() < min_size) {
      min_size = compressor->GetCompressedData().size();
      *smallest_compressor = compressor.get();
    }
  }

  return true;
}

bool BsdiffPatchWriter::Close() {
  if (!fp_) {
    LOG(ERROR) << "File not open.";
    return false;
  }

  CompressorInterface* ctrl_stream = nullptr;
  if (!SelectSmallestResult(ctrl_stream_list_, &ctrl_stream) || !ctrl_stream) {
    return false;
  }

  CompressorInterface* diff_stream = nullptr;
  if (!SelectSmallestResult(diff_stream_list_, &diff_stream) || !diff_stream) {
    return false;
  }

  CompressorInterface* extra_stream = nullptr;
  if (!SelectSmallestResult(extra_stream_list_, &extra_stream) ||
      !extra_stream) {
    return false;
  }

  auto ctrl_data = ctrl_stream->GetCompressedData();
  auto diff_data = diff_stream->GetCompressedData();
  auto extra_data = extra_stream->GetCompressedData();

  uint8_t types[3] = {static_cast<uint8_t>(ctrl_stream->Type()),
                      static_cast<uint8_t>(diff_stream->Type()),
                      static_cast<uint8_t>(extra_stream->Type())};

  if (!WriteHeader(types, ctrl_data.size(), diff_data.size()))
    return false;

  if (fwrite(ctrl_data.data(), 1, ctrl_data.size(), fp_) != ctrl_data.size()) {
    LOG(ERROR) << "Writing ctrl_data.";
    return false;
  }
  if (fwrite(diff_data.data(), 1, diff_data.size(), fp_) != diff_data.size()) {
    LOG(ERROR) << "Writing diff_data.";
    return false;
  }
  if (fwrite(extra_data.data(), 1, extra_data.size(), fp_) !=
      extra_data.size()) {
    LOG(ERROR) << "Writing extra_data.";
    return false;
  }
  if (fclose(fp_) != 0) {
    LOG(ERROR) << "Closing the patch file.";
    return false;
  }
  fp_ = nullptr;
  return true;
}

bool BsdiffPatchWriter::WriteHeader(uint8_t types[3],
                                    uint64_t ctrl_size,
                                    uint64_t diff_size) {
  /* Header format is
   * 0 8 magic header
   * 8 8 length of compressed ctrl block
   * 16  8 length of compressed diff block
   * 24  8 length of new file
   *
   * File format is
   * 0 32  Header
   * 32  ??  compressed ctrl block
   * ??  ??  compressed diff block
   * ??  ??  compressed extra block
   */
  uint8_t header[32];
  if (format_ == BsdiffFormat::kLegacy) {
    // The magic header is "BSDIFF40" for legacy format.
    memcpy(header, kLegacyMagicHeader, 8);
  } else if (format_ == BsdiffFormat::kBsdf2) {
    // The magic header for BSDF2 format:
    // 0 5 BSDF2
    // 5 1 compressed type for control stream
    // 6 1 compressed type for diff stream
    // 7 1 compressed type for extra stream
    memcpy(header, kBSDF2MagicHeader, 5);
    memcpy(header + 5, types, 3);
  } else {
    LOG(ERROR) << "Unsupported bsdiff format.";
    return false;
  }

  EncodeInt64(ctrl_size, header + 8);
  EncodeInt64(diff_size, header + 16);
  EncodeInt64(written_output_, header + 24);
  if (fwrite(header, sizeof(header), 1, fp_) != 1) {
    LOG(ERROR) << "writing to the patch file";
    return false;
  }
  return true;
}

}  // namespace bsdiff
