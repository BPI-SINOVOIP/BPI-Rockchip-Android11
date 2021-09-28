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

#include "utils/strings/substitute.h"

#include <algorithm>

#include "utils/base/logging.h"

namespace libtextclassifier3 {
namespace strings {

bool Substitute(const StringPiece format, const std::vector<StringPiece>& args,
                std::string* output) {
  // Determine total size needed.
  size_t size = 0;
  for (size_t i = 0; i < format.size(); i++) {
    if (format[i] == '$') {
      if (i + 1 >= format.size()) {
        TC3_LOG(ERROR) << "Invalid format string: " << format.ToString();
        return false;
      } else if (isdigit(format[i + 1])) {
        int index = format[i + 1] - '0';
        if (static_cast<size_t>(index) >= args.size()) {
          TC3_LOG(ERROR) << "Asked for " << index << ", but only "
                         << args.size() << " arguments given";
          return false;
        }
        size += args[index].size();
        ++i;  // Skip next char.
      } else if (format[i + 1] == '$') {
        ++size;
        ++i;  // Skip next char.
      } else {
        TC3_LOG(ERROR) << "Invalid format string: " << format.ToString();
        return false;
      }
    } else {
      ++size;
    }
  }

  if (size == 0) {
    output->clear();
    return true;
  }

  // Build the string.
  output->resize(size);
  char* target = &(*output)[0];
  for (size_t i = 0; i < format.size(); i++) {
    if (format[i] == '$') {
      if (isdigit(format[i + 1])) {
        const StringPiece src = args[format[i + 1] - '0'];
        target = std::copy(src.data(), src.data() + src.size(), target);
        ++i;  // Skip next char.
      } else if (format[i + 1] == '$') {
        *target++ = '$';
        ++i;  // Skip next char.
      }
    } else {
      *target++ = format[i];
    }
  }
  return true;
}

std::string Substitute(const StringPiece format,
                       const std::vector<StringPiece>& args) {
  std::string result;
  if (!Substitute(format, args, &result)) {
    return "";
  }
  return result;
}

}  // namespace strings
}  // namespace libtextclassifier3
