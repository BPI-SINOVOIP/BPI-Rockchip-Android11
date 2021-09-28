// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bsdiff/bsdiff_arguments.h"

#include <getopt.h>

#include <algorithm>
#include <iostream>

#include "brotli/encode.h"

using std::endl;
using std::string;

namespace {

// The name in string for the compression algorithms.
constexpr char kNoCompressionString[] = "nocompression";
constexpr char kBZ2String[] = "bz2";
constexpr char kBrotliString[] = "brotli";

// The name in string for the bsdiff format.
constexpr char kLegacyString[] = "legacy";
constexpr char kBsdf2String[] = "bsdf2";
constexpr char kBsdiff40String[] = "bsdiff40";
constexpr char kEndsleyString[] = "endsley";

const struct option OPTIONS[] = {
    {"format", required_argument, nullptr, 0},
    {"minlen", required_argument, nullptr, 0},
    {"type", required_argument, nullptr, 0},
    {"brotli_quality", required_argument, nullptr, 0},
    {nullptr, 0, nullptr, 0},
};

const uint32_t kBrotliDefaultQuality = BROTLI_MAX_QUALITY;

}  // namespace

namespace bsdiff {

std::vector<CompressorType> BsdiffArguments::compressor_types() const {
  return std::vector<CompressorType>(compressor_types_.begin(),
                                     compressor_types_.end());
}

bool BsdiffArguments::IsValid() const {
  if (compressor_types_.empty()) {
    return false;
  }

  if (IsCompressorSupported(CompressorType::kBrotli) &&
      (brotli_quality_ < BROTLI_MIN_QUALITY ||
       brotli_quality_ > BROTLI_MAX_QUALITY)) {
    return false;
  }

  if (format_ == BsdiffFormat::kLegacy) {
    return compressor_types_.size() == 1 &&
           IsCompressorSupported(CompressorType::kBZ2);
  } else if (format_ == BsdiffFormat::kBsdf2) {
    if (IsCompressorSupported(CompressorType::kNoCompression)) {
      std::cerr << "no compression is not supported in Bsdf2 format\n";
      return false;
    }
    return true;
  } else if (format_ == BsdiffFormat::kEndsley) {
    // Only one compressor is supported for this format.
    return compressor_types_.size() == 1;
  }
  return false;
}

bool BsdiffArguments::ParseCommandLine(int argc, char** argv) {
  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "", OPTIONS, &option_index)) != -1) {
    if (opt != 0) {
      return false;
    }

    string name = OPTIONS[option_index].name;
    if (name == "format") {
      if (!ParseBsdiffFormat(optarg, &format_)) {
        return false;
      }
    } else if (name == "minlen") {
      if (!ParseMinLength(optarg, &min_length_)) {
        return false;
      }
    } else if (name == "type") {
      if (!ParseCompressorTypes(optarg, &compressor_types_)) {
        return false;
      }
    } else if (name == "brotli_quality") {
      if (!ParseQuality(optarg, &brotli_quality_, BROTLI_MIN_QUALITY,
                        BROTLI_MAX_QUALITY)) {
        return false;
      }
    } else {
      std::cerr << "Unrecognized options: " << name << endl;
      return false;
    }
  }

  // If quality is uninitialized for brotli, set it to default value.
  if (format_ != BsdiffFormat::kLegacy &&
      IsCompressorSupported(CompressorType::kBrotli) && brotli_quality_ == -1) {
    brotli_quality_ = kBrotliDefaultQuality;
  } else if (!IsCompressorSupported(CompressorType::kBrotli) &&
             brotli_quality_ != -1) {
    std::cerr << "Warning: Brotli quality is only used in the brotli"
                 " compressor.\n";
  }

  return true;
}

bool BsdiffArguments::ParseCompressorTypes(const string& str,
                                           std::set<CompressorType>* types) {
  types->clear();
  // The expected types string is separated by ":", e.g. bz2:brotli
  std::vector<std::string> type_list;
  size_t base = 0;
  size_t found;
  while (true) {
    found = str.find(":", base);
    type_list.emplace_back(str, base, found - base);

    if (found == str.npos)
      break;
    base = found + 1;
  }

  for (auto& type : type_list) {
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);
    if (type == kNoCompressionString) {
      types->emplace(CompressorType::kNoCompression);
    } else if (type == kBZ2String) {
      types->emplace(CompressorType::kBZ2);
    } else if (type == kBrotliString) {
      types->emplace(CompressorType::kBrotli);
    } else {
      std::cerr << "Failed to parse compressor type in " << str << endl;
      return false;
    }
  }

  return true;
}

bool BsdiffArguments::ParseMinLength(const string& str, size_t* len) {
  errno = 0;
  char* end;
  const char* s = str.c_str();
  long result = strtol(s, &end, 10);
  if (errno != 0 || s == end || *end != '\0') {
    return false;
  }

  if (result < 0) {
    std::cerr << "Minimum length must be non-negative: " << str << endl;
    return false;
  }

  *len = result;
  return true;
}

bool BsdiffArguments::ParseBsdiffFormat(const string& str,
                                        BsdiffFormat* format) {
  string format_string = str;
  std::transform(format_string.begin(), format_string.end(),
                 format_string.begin(), ::tolower);
  if (format_string == kLegacyString || format_string == kBsdiff40String) {
    *format = BsdiffFormat::kLegacy;
    return true;
  } else if (format_string == kBsdf2String) {
    *format = BsdiffFormat::kBsdf2;
    return true;
  } else if (format_string == kEndsleyString) {
    *format = BsdiffFormat::kEndsley;
    return true;
  }
  std::cerr << "Failed to parse bsdiff format in " << str << endl;
  return false;
}

bool BsdiffArguments::ParseQuality(const string& str,
                                   int* quality,
                                   int min,
                                   int max) {
  errno = 0;
  char* end;
  const char* s = str.c_str();
  long result = strtol(s, &end, 10);
  if (errno != 0 || s == end || *end != '\0') {
    return false;
  }

  if (result < min || result > max) {
    std::cerr << "Compression quality out of range " << str << endl;
    return false;
  }

  *quality = result;
  return true;
}

bool BsdiffArguments::IsCompressorSupported(CompressorType type) const {
  return compressor_types_.find(type) != compressor_types_.end();
}

}  // namespace bsdiff
