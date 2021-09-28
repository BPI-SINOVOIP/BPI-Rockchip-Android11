// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef IORAP_COMMON_TRACE_H_
#define IORAP_COMMON_TRACE_H_

#include <cutils/trace.h>

#include <cstdio>
#include <sstream>

namespace iorap {

// TODO: refactor into utils/Trace.h

class ScopedFormatTrace {
 public:
  template <typename ... Args>
  ScopedFormatTrace(uint64_t tag, const char* fmt, Args&&... args) : tag_{tag} {
    char buffer[1024];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    snprintf(buffer, sizeof(buffer), fmt, args...);
#pragma GCC diagnostic pop
    atrace_begin(tag, buffer);
  }

  ~ScopedFormatTrace() {
    atrace_end(tag_);
  }
 private:
  uint64_t tag_;
};

}  // namespace iorap

#endif  // IORAP_COMMON_TRACE_H_
