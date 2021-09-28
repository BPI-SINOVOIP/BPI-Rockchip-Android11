/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef IORAP_SRC_COMMON_LOGGERS
#define IORAP_SRC_COMMON_LOGGERS

#include <android-base/logging.h>

namespace iorap {
namespace common {

// Log to both Stderr and Logd for convenience when running this from the command line.
class StderrAndLogdLogger {
 public:
  explicit StderrAndLogdLogger(android::base::LogId default_log_id = android::base::MAIN)
#ifdef __ANDROID__
      : logd_(default_log_id)
#endif
  {
  }

  void operator()(::android::base::LogId id,
                  ::android::base::LogSeverity sev,
                  const char* tag,
                  const char* file,
                  unsigned int line,
                  const char* message) {
#ifdef __ANDROID__
    logd_(id, sev, tag, file, line, message);
#endif
    StderrLogger(id, sev, tag, file, line, message);
  }

 private:
#ifdef __ANDROID__
  ::android::base::LogdLogger logd_;
#endif
};

}  // namespace iorap
}  // namespace common

#endif
