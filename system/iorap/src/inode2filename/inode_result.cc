// Copyright (C) 2019 The Android Open Source Project
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

#include "inode2filename/inode_result.h"

#include <string.h>

namespace iorap::inode2filename {

std::optional<std::string_view> InodeResult::ErrorMessage() const {
  if (data) {
    return std::nullopt;
  }

  const int err_no = data.error();
  return std::string_view{
    [=]() -> const char * {
      switch (err_no) {
        case InodeResult::kCouldNotFindFilename:
          return "Could not find filename";
        case InodeResult::kVerificationFailed:
          return "Verification failed";
        default:
          return strerror(err_no);
      }
    }()
  };
}

std::ostream& operator<<(std::ostream& os, const InodeResult& result) {
  os << "InodeResult{";
  if (result) {
      os << "OK,";
  } else {
      os << "ERR,";
  }

  os << result.inode << ",";

  if (result) {
    os << "\"" << result.data.value() << "\"";
  } else {
    os << result.data.error();
    os << " (" << *result.ErrorMessage() << ")";
  }

  os << "}";
  return os;
}

}  // namespace iorap::inode2filename