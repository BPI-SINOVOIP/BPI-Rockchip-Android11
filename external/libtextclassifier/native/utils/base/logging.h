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

#ifndef LIBTEXTCLASSIFIER_UTILS_BASE_LOGGING_H_
#define LIBTEXTCLASSIFIER_UTILS_BASE_LOGGING_H_

#include <cassert>
#include <string>

#include "utils/base/integral_types.h"
#include "utils/base/logging_levels.h"
#include "utils/base/port.h"


namespace libtextclassifier3 {
namespace logging {

// A tiny code footprint string stream for assembling log messages.
struct LoggingStringStream {
  LoggingStringStream() {}
  LoggingStringStream& stream() { return *this; }
  // Needed for invocation in TC3_CHECK macro.
  explicit operator bool() const { return true; }

  std::string message;
};

template <typename T>
inline LoggingStringStream& operator<<(LoggingStringStream& stream,
                                       const T& entry) {
  stream.message.append(std::to_string(entry));
  return stream;
}

template <typename T>
inline LoggingStringStream& operator<<(LoggingStringStream& stream,
                                       T* const entry) {
  stream.message.append(std::to_string(reinterpret_cast<const uint64>(entry)));
  return stream;
}

inline LoggingStringStream& operator<<(LoggingStringStream& stream,
                                       const char* message) {
  stream.message.append(message);
  return stream;
}

inline LoggingStringStream& operator<<(LoggingStringStream& stream,
                                       const std::string& message) {
  stream.message.append(message);
  return stream;
}

inline LoggingStringStream& operator<<(LoggingStringStream& stream,
                                       const std::string_view message) {
  stream.message.append(message);
  return stream;
}

template <typename T1, typename T2>
inline LoggingStringStream& operator<<(LoggingStringStream& stream,
                                       const std::pair<T1, T2>& entry) {
  stream << "(" << entry.first << ", " << entry.second << ")";
  return stream;
}

// The class that does all the work behind our TC3_LOG(severity) macros.  Each
// TC3_LOG(severity) << obj1 << obj2 << ...; logging statement creates a
// LogMessage temporary object containing a stringstream.  Each operator<< adds
// info to that stringstream and the LogMessage destructor performs the actual
// logging.  The reason this works is that in C++, "all temporary objects are
// destroyed as the last step in evaluating the full-expression that (lexically)
// contains the point where they were created."  For more info, see
// http://en.cppreference.com/w/cpp/language/lifetime.  Hence, the destructor is
// invoked after the last << from that logging statement.
class LogMessage {
 public:
  LogMessage(LogSeverity severity, const char* file_name,
             int line_number) TC3_ATTRIBUTE_NOINLINE;

  ~LogMessage() TC3_ATTRIBUTE_NOINLINE;

  // Returns the stream associated with the logger object.
  LoggingStringStream& stream() { return stream_; }

 private:
  const LogSeverity severity_;

  // Stream that "prints" all info into a string (not to a file).  We construct
  // here the entire logging message and next print it in one operation.
  LoggingStringStream stream_;
};

// Pseudo-stream that "eats" the tokens <<-pumped into it, without printing
// anything.
class NullStream {
 public:
  NullStream() {}
  NullStream& stream() { return *this; }
};
template <typename T>
inline NullStream& operator<<(NullStream& str, const T&) {
  return str;
}

}  // namespace logging
}  // namespace libtextclassifier3

#define TC3_LOG(severity)                                          \
  ::libtextclassifier3::logging::LogMessage(                       \
      ::libtextclassifier3::logging::severity, __FILE__, __LINE__) \
      .stream()

// If condition x is true, does nothing.  Otherwise, crashes the program (like
// LOG(FATAL)) with an informative message.  Can be continued with extra
// messages, via <<, like any logging macro, e.g.,
//
// TC3_CHECK(my_cond) << "I think we hit a problem";
#define TC3_CHECK(x)                                                           \
  (x) || TC3_LOG(FATAL) << __FILE__ << ":" << __LINE__ << ": check failed: \"" \
                        << #x << "\" "

#define TC3_CHECK_EQ(x, y) TC3_CHECK((x) == (y))
#define TC3_CHECK_LT(x, y) TC3_CHECK((x) < (y))
#define TC3_CHECK_GT(x, y) TC3_CHECK((x) > (y))
#define TC3_CHECK_LE(x, y) TC3_CHECK((x) <= (y))
#define TC3_CHECK_GE(x, y) TC3_CHECK((x) >= (y))
#define TC3_CHECK_NE(x, y) TC3_CHECK((x) != (y))

#define TC3_NULLSTREAM ::libtextclassifier3::logging::NullStream().stream()

// Debug checks: a TC3_DCHECK<suffix> macro should behave like TC3_CHECK<suffix>
// in debug mode an don't check / don't print anything in non-debug mode.
#if defined(NDEBUG) && !defined(TC3_DEBUG_LOGGING) && !defined(TC3_DEBUG_CHECKS)

#define TC3_DCHECK(x) TC3_NULLSTREAM
#define TC3_DCHECK_EQ(x, y) TC3_NULLSTREAM
#define TC3_DCHECK_LT(x, y) TC3_NULLSTREAM
#define TC3_DCHECK_GT(x, y) TC3_NULLSTREAM
#define TC3_DCHECK_LE(x, y) TC3_NULLSTREAM
#define TC3_DCHECK_GE(x, y) TC3_NULLSTREAM
#define TC3_DCHECK_NE(x, y) TC3_NULLSTREAM

#else  // NDEBUG

// In debug mode, each TC3_DCHECK<suffix> is equivalent to TC3_CHECK<suffix>,
// i.e., a real check that crashes when the condition is not true.
#define TC3_DCHECK(x) TC3_CHECK(x)
#define TC3_DCHECK_EQ(x, y) TC3_CHECK_EQ(x, y)
#define TC3_DCHECK_LT(x, y) TC3_CHECK_LT(x, y)
#define TC3_DCHECK_GT(x, y) TC3_CHECK_GT(x, y)
#define TC3_DCHECK_LE(x, y) TC3_CHECK_LE(x, y)
#define TC3_DCHECK_GE(x, y) TC3_CHECK_GE(x, y)
#define TC3_DCHECK_NE(x, y) TC3_CHECK_NE(x, y)

#endif  // NDEBUG

#ifdef TC3_ENABLE_VLOG
#define TC3_VLOG(severity)                                     \
  ::libtextclassifier3::logging::LogMessage(                   \
      ::libtextclassifier3::logging::INFO, __FILE__, __LINE__) \
      .stream()
#else
#define TC3_VLOG(severity) TC3_NULLSTREAM
#endif

#endif  // LIBTEXTCLASSIFIER_UTILS_BASE_LOGGING_H_
