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

#include "utils/base/status.h"

namespace libtextclassifier3 {

const Status& Status::OK = *new Status(StatusCode::OK, "");
const Status& Status::UNKNOWN = *new Status(StatusCode::UNKNOWN, "");

Status::Status() : code_(StatusCode::OK) {}
Status::Status(StatusCode error, const std::string& message)
    : code_(error), message_(message) {}

logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const Status& status) {
  stream << status.error_code();
  return stream;
}

}  // namespace libtextclassifier3
