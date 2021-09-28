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

#ifndef IORAP_SRC_INODE2FILENAME_OUT_OF_PROCESS_INDOE_RESOLVER_H_
#define IORAP_SRC_INODE2FILENAME_OUT_OF_PROCESS_INDOE_RESOLVER_H_

#include "common/expected.h"
#include "inode2filename/inode_resolver.h"

namespace iorap::inode2filename {

// Create an InodeResolver that fork+exec+pipes into the binary 'iorap.inode2filename'
// and transmits the results back via an IPC mechanism.
//
// This is instantiated by InodeResolver::Create + ProcessMode::kOutOfProcessIpc
class OutOfProcessInodeResolver : public InodeResolver {
 public:
  virtual rxcpp::observable<InodeResult>
      FindFilenamesFromInodes(std::vector<Inode> inodes) const override;

  virtual rxcpp::observable<InodeResult>
      EmitAll() const override;

  OutOfProcessInodeResolver(InodeResolverDependencies dependencies);
  ~OutOfProcessInodeResolver();
 private:
  struct Impl;
  Impl* impl_;
};

// Reads one line data from the stream.
// Each line is in the format of "<4 bytes line length><state> <inode info> <file path>"
// The <4 bytes line length> is the length rest data of "<state> <inode info> <file path>".
// The return string is "<state> <inode info> <file path>".
// For example: for "<size>K 253:9:6 ./test", the return value is
// "K 253:9:6 ./test". The <size> is encoded in the first 4 bytes.
// Note: there is no newline in the end of each line and the line shouldn't be
// empty unless there is some error.
std::string ReadOneLine(FILE* stream, bool* eof);
}

#endif  // IORAP_SRC_INODE2FILENAME_OUT_OF_PROCESS_INDOE_RESOLVER_H_
