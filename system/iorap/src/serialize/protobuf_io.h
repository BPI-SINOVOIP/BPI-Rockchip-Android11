// Copyright (C) 2017 The Android Open Source Project
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

#ifndef SERIALIZE_PROTOBUF_IO_H_
#define SERIALIZE_PROTOBUF_IO_H_

#include "common/expected.h"
#include "serialize/arena_ptr.h"
#include "system/iorap/src/serialize/TraceFile.pb.h"

#include <string>
#include <string_view>

namespace iorap {
namespace serialize {

// XX: either the namespace should be called pb|proto[buf]
// or we should hide the protobuf-ness from the names and call this class "IO" , "Reader", etc?
// although an obvious name might be the "OpenFactory" or "ProtobufFacade" that reads too much
// like a bad joke.

// Helpers to read a TraceFile protobuf from a file [descriptor].
class ProtobufIO {
 public:
  // XX: proto::TraceFile seems annoying, maybe just serialize::TraceFile ?

  // Open the protobuf associated at the filepath. Returns null on failure.
  static ArenaPtr<proto::TraceFile> Open(std::string file_path);
  // Open the protobuf from the file descriptor. Returns null on failure.
  static ArenaPtr<proto::TraceFile> Open(int fd, const char* file_path = "<unknown>");

  // Save the protobuf by overwriting the file at file_path.
  // The file state is indeterminate at failure.
  // Returns # of bytes written out on success, otherwise the errno value.
  static iorap::expected<size_t /*bytes written*/, int /*errno*/> WriteFully(
      const ::google::protobuf::MessageLite& message,
      std::string_view file_path);
  // Save the protobuf by truncating the file already open at 'fd'.
  // The file state is indeterminate at failure.
  // Returns # of bytes written out on success, otherwise the errno value.
  static iorap::expected<size_t /*bytes written*/, int /*errno*/> WriteFully(
      const ::google::protobuf::MessageLite& message,
      int fd,
      std::string_view file_path = "<unknown>");

  ProtobufIO() = delete;
};

}  // namespace serialize
}  // namespace iorap

#endif

