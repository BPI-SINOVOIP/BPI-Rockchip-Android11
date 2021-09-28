// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BSDIFF_BSDIFF_ARGUMENTS_H_
#define _BSDIFF_BSDIFF_ARGUMENTS_H_

#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "bsdiff/constants.h"
#include "bsdiff/patch_writer_interface.h"

namespace bsdiff {

// Class to store the patch writer options about format, type and
// brotli_quality.
class BsdiffArguments {
 public:
  BsdiffArguments() : format_(BsdiffFormat::kLegacy), brotli_quality_(-1) {
    compressor_types_.emplace(CompressorType::kBZ2);
  }

  BsdiffArguments(BsdiffFormat format,
                  std::set<CompressorType> types,
                  int brotli_quality)
      : format_(format),
        compressor_types_(std::move(types)),
        brotli_quality_(brotli_quality) {}

  // Check if the compressor type is compatible with the bsdiff format.
  bool IsValid() const;

  // Getter functions.
  BsdiffFormat format() const { return format_; }

  int min_length() const { return min_length_; }

  std::vector<CompressorType> compressor_types() const;

  int brotli_quality() const { return brotli_quality_; }

  // Parse the command line arguments of the main function and set all the
  // fields accordingly.
  bool ParseCommandLine(int argc, char** argv);

  // Parse the compression type from string.
  static bool ParseCompressorTypes(const std::string& str,
                                   std::set<CompressorType>* types);

  // Parse the minimum length parameter from string.
  static bool ParseMinLength(const std::string& str, size_t* len);

  // Parse the bsdiff format from string.
  static bool ParseBsdiffFormat(const std::string& str, BsdiffFormat* format);

  // Parse the compression quality (for brotli) from string; also check if the
  // value is within the valid range.
  static bool ParseQuality(const std::string& str,
                           int* quality,
                           int min,
                           int max);

 private:
  bool IsCompressorSupported(CompressorType type) const;

  // Current format supported are the legacy "BSDIFF40" or "BSDF2".
  BsdiffFormat format_;

  // The algorithms to compress the patch, e.g. bz2, brotli.
  std::set<CompressorType> compressor_types_;

  // The quality of brotli compressor.
  int brotli_quality_;

  size_t min_length_{0};
};

}  // namespace bsdiff

#endif  // _BSDIFF_BSDIFF_ARGUMENTS_H_
