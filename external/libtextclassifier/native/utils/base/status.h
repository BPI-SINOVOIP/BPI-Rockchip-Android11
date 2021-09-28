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

#ifndef LIBTEXTCLASSIFIER_UTILS_BASE_STATUS_H_
#define LIBTEXTCLASSIFIER_UTILS_BASE_STATUS_H_

#include <string>

#include "utils/base/logging.h"

namespace libtextclassifier3 {

enum class StatusCode {
  // Not an error; returned on success
  OK = 0,

  // All of the following StatusCodes represent errors.
  CANCELLED = 1,
  UNKNOWN = 2,
  INVALID_ARGUMENT = 3,
  DEADLINE_EXCEEDED = 4,
  NOT_FOUND = 5,
  ALREADY_EXISTS = 6,
  PERMISSION_DENIED = 7,
  RESOURCE_EXHAUSTED = 8,
  FAILED_PRECONDITION = 9,
  ABORTED = 10,
  OUT_OF_RANGE = 11,
  UNIMPLEMENTED = 12,
  INTERNAL = 13,
  UNAVAILABLE = 14,
  DATA_LOSS = 15,
  UNAUTHENTICATED = 16
};

// A Status is a combination of an error code and a string message (for non-OK
// error codes).
class Status {
 public:
  // Creates an OK status
  Status();

  // Make a Status from the specified error and message.
  Status(StatusCode error, const std::string& error_message);

  // Some pre-defined Status objects
  static const Status& OK;
  static const Status& UNKNOWN;

  // Accessors
  bool ok() const { return code_ == StatusCode::OK; }
  int error_code() const { return static_cast<int>(code_); }

  StatusCode CanonicalCode() const { return code_; }

  const std::string& error_message() const { return message_; }

  // Noop function provided to allow callers to suppress compiler warnings about
  // ignored return values.
  void IgnoreError() const {}

  bool operator==(const Status& x) const;
  bool operator!=(const Status& x) const;

  std::string ToString() const;

 private:
  StatusCode code_;
  std::string message_;
};

logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const Status& status);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_BASE_STATUS_H_
