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

#pragma once

#include <algorithm>
#include <iostream>
#include <regex>
#include <string>

#include "logging.h"

namespace util {

inline std::string GetTypeForSize(int size) {
  if (size > 64) {
    ERROR() << __func__ << ": Cannot use a type larger than 64 bits. (" << size << ")\n";
  }

  if (size <= 8) return "uint8_t";

  if (size <= 16) return "uint16_t";

  if (size <= 32) return "uint32_t";

  return "uint64_t";
}

inline int RoundSizeUp(int size) {
  if (size > 64) {
    ERROR() << __func__ << ": Cannot use a type larger than 64 bits. (" << size << ")\n";
  }

  if (size <= 8) return 8;
  if (size <= 16) return 16;
  if (size <= 32) return 32;
  return 64;
}

// Returns the max value that can be contained unsigned in a number of bits.
inline uint64_t GetMaxValueForBits(int bits) {
  if (bits > 64) {
    ERROR() << __func__ << ": Cannot use a type larger than 64 bits. (" << bits << ")\n";
  }

  // Set all the bits to 1, then shift off extras.
  return ~(static_cast<uint64_t>(0)) >> (64 - bits);
}

inline std::string CamelCaseToUnderScore(std::string value) {
  if (value[0] < 'A' || value[0] > 'Z') {
    ERROR() << value << " doesn't look like CamelCase";
  }

  // Use static to avoid compiling the regex more than once.
  static const std::regex camel_case_regex("[A-Z][a-z0-9]*");

  // Add an underscore to the end of each pattern match.
  value = std::regex_replace(value, camel_case_regex, "$&_");

  // Remove the last underscore at the end of the string.
  value.pop_back();

  // Convert all characters to lowercase.
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });

  return value;
}

inline std::string UnderscoreToCamelCase(std::string value) {
  if (value[0] < 'a' || value[0] > 'z') {
    ERROR() << value << " invalid identifier";
  }

  std::ostringstream camel_case;

  bool capitalize = true;
  for (unsigned char c : value) {
    if (c == '_') {
      capitalize = true;
    } else {
      if (capitalize) {
        c = std::toupper(c);
        capitalize = false;
      }
      camel_case << c;
    }
  }

  return camel_case.str();
}

inline bool IsEnumCase(std::string value) {
  if (value[0] < 'A' || value[0] > 'Z') {
    return false;
  }

  // Use static to avoid compiling the regex more than once.
  static const std::regex enum_regex("[A-Z][A-Z0-9_]*");

  return std::regex_match(value, enum_regex);
}

inline std::string StringJoin(const std::string& delimiter, const std::vector<std::string>& vec) {
  std::stringstream ss;
  for (size_t i = 0; i < vec.size(); i++) {
    ss << vec[i];
    if (i != (vec.size() - 1)) {
      ss << delimiter;
    }
  }
  return ss.str();
}

inline std::string StringFindAndReplaceAll(std::string text, const std::string& old, const std::string& replacement) {
  auto pos = text.find(old);
  while (pos != std::string::npos) {
    text.replace(pos, old.size(), replacement);
    pos = text.find(old, pos + replacement.size());
  }
  return text;
}

}  // namespace util
