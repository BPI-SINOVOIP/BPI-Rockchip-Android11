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

#ifndef IORAP_SRC_INODE2FILENAME_INODE_RESULT_H_
#define IORAP_SRC_INODE2FILENAME_INODE_RESULT_H_

#include "common/expected.h"
#include "inode2filename/inode.h"
#include "inode2filename/inode_result.h"
#include "inode2filename/system_call.h"

#include <rxcpp/rx.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace iorap::inode2filename {

// Tuple of (Inode -> (Filename|Errno))
struct InodeResult {
  // We set this error when all root directories have been searched and
  // yet we still could not find a corresponding filename for the inode under search.
  static constexpr int kCouldNotFindFilename = ENOKEY;

  // An initial inode->filename mapping was found, but subsequent verification for it failed.
  static constexpr int kVerificationFailed = EKEYEXPIRED;

  // There is always an inode, but sometimes we may fail to resolve the filename.
  Inode inode;
  // Value: Contains the filename (with a root directory as a prefix).
  // Error: Contains the errno, usually one of the above, otherwise some system error.
  iorap::expected<std::string /*filename*/, int /*errno*/> data;

  static InodeResult makeSuccess(Inode inode, std::string filename) {
    return InodeResult{inode, std::move(filename)};
  }

  static InodeResult makeFailure(Inode inode, int err_no) {
    return InodeResult{inode, iorap::unexpected{err_no}};
  }

  constexpr explicit operator bool() const {
    return data.has_value();
  }

  constexpr bool operator==(const InodeResult& other) const {
    if (inode == other.inode) {
      if (data && other.data) {
        return *data == *other.data;
      } else if (!data && !other.data) {
        return data.error() == other.data.error();
      }
      // TODO: operator== for expected
    }
    return false;
  }

  constexpr bool operator!=(const InodeResult& other) const {
    return !(*this == other);
  }

  // Returns a human-readable error message, or 'nullopt' if there was no error.
  std::optional<std::string_view> ErrorMessage() const;
};

std::ostream& operator<<(std::ostream& os, const InodeResult& result);

}  // namespace iorap::inode2filename

#endif  // IORAP_SRC_INODE2FILENAME_INODE_RESULT_H_