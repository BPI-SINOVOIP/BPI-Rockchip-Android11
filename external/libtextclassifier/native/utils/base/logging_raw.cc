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

#include "utils/base/logging_raw.h"

#include <stdio.h>

#include <string>

#define TC3_RETURN_IF_NOT_ERROR_OR_FATAL        \
  if (severity != ERROR && severity != FATAL) { \
    return;                                     \
  }

// NOTE: this file contains two implementations: one for Android, one for all
// other cases.  We always build exactly one implementation.
#if defined(__ANDROID__)

// Compiled as part of Android.
#include <android/log.h>

namespace libtextclassifier3 {
namespace logging {

namespace {
// Converts LogSeverity to level for __android_log_write.
int GetAndroidLogLevel(LogSeverity severity) {
  switch (severity) {
    case FATAL:
      return ANDROID_LOG_FATAL;
    case ERROR:
      return ANDROID_LOG_ERROR;
    case WARNING:
      return ANDROID_LOG_WARN;
    case INFO:
      return ANDROID_LOG_INFO;
    default:
      return ANDROID_LOG_DEBUG;
  }
}
}  // namespace

void LowLevelLogging(LogSeverity severity, const std::string& tag,
                     const std::string& message) {
#if !defined(TC3_DEBUG_LOGGING)
  TC3_RETURN_IF_NOT_ERROR_OR_FATAL
#endif
  const int android_log_level = GetAndroidLogLevel(severity);
  __android_log_write(android_log_level, tag.c_str(), message.c_str());
}

}  // namespace logging
}  // namespace libtextclassifier3

#else  // if defined(__ANDROID__)

// Not on Android: implement LowLevelLogging to print to stderr (see below).
namespace libtextclassifier3 {
namespace logging {

namespace {
// Converts LogSeverity to human-readable text.
const char *LogSeverityToString(LogSeverity severity) {
  switch (severity) {
    case INFO:
      return "INFO";
    case WARNING:
      return "WARNING";
    case ERROR:
      return "ERROR";
    case FATAL:
      return "FATAL";
    default:
      return "UNKNOWN";
  }
}
}  // namespace

void LowLevelLogging(LogSeverity severity, const std::string &tag,
                     const std::string &message) {
#if !defined(TC3_DEBUG_LOGGING)
  TC3_RETURN_IF_NOT_ERROR_OR_FATAL
#endif
  fprintf(stderr, "[%s] %s : %s\n", LogSeverityToString(severity), tag.c_str(),
          message.c_str());
  fflush(stderr);
}

}  // namespace logging
}  // namespace libtextclassifier3

#endif  // if defined(__ANDROID__)
