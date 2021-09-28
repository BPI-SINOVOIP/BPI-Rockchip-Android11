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

#include "utils/strings/append.h"

#include <stdarg.h>

#include <cstring>
#include <string>
#include <vector>

namespace libtextclassifier3 {
namespace strings {

void SStringAppendV(std::string *strp, int bufsize, const char *fmt,
                    va_list arglist) {
  int capacity = bufsize;
  if (capacity <= 0) {
    va_list backup;
    va_copy(backup, arglist);
    capacity = vsnprintf(nullptr, 0, fmt, backup);
    va_end(arglist);
  }

  size_t start = strp->size();
  strp->resize(strp->size() + capacity + 1);

  int written = vsnprintf(&(*strp)[start], capacity + 1, fmt, arglist);
  va_end(arglist);
  strp->resize(start + std::min(capacity, written));
}

void SStringAppendF(std::string *strp,
                                int bufsize,
                                const char *fmt, ...) {
  va_list arglist;
  va_start(arglist, fmt);
  SStringAppendV(strp, bufsize, fmt, arglist);
}

std::string StringPrintf(const char* fmt, ...) {
  std::string s;
  va_list arglist;
  va_start(arglist, fmt);
  SStringAppendV(&s, 0, fmt, arglist);
  return s;
}

std::string JoinStrings(const char *delim,
                        const std::vector<std::string> &vec) {
  int delim_len = strlen(delim);

  // Calc size.
  int out_len = 0;
  for (size_t i = 0; i < vec.size(); i++) {
    out_len += vec[i].size() + delim_len;
  }

  // Write out.
  std::string ret;
  ret.reserve(out_len);
  for (size_t i = 0; i < vec.size(); i++) {
    ret.append(vec[i]);
    ret.append(delim, delim_len);
  }

  // Strip last delimiter.
  if (!ret.empty()) {
    // Must be at least delim_len.
    ret.resize(ret.size() - delim_len);
  }
  return ret;
}

}  // namespace strings
}  // namespace libtextclassifier3
