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

#ifndef PREFETCHER_DAEMON_H_
#define PREFETCHER_DAEMON_H_

#include "prefetcher/session_manager.h"

#include <memory>
#include <optional>
#include <ostream>

namespace iorap {
namespace prefetcher {

struct PrefetcherForkParameters {
  int input_fd;
  int output_fd;
  bool use_sockets;  // use the socket path instead of simpler read/write path.
  bool format_text;  // true=>text, false=>binary
};

inline std::ostream& operator<<(std::ostream& os, const PrefetcherForkParameters& p) {
  os << "PrefetcherForkParameters{";
  os << "input_fd=" << p.input_fd << ",";
  os << "output_fd=" << p.output_fd << ",";
  os << "format_text=" << p.format_text << ",";
  os << "use_sockets=" << p.use_sockets << ",";
  os << "}";
  return os;
}


#ifndef READ_AHEAD_KIND
enum class ReadAheadKind : uint32_t {
  kFadvise = 0,
  kMmapLocked = 1,
  kMlock = 2,
};
#define READ_AHEAD_KIND 1
#endif

std::ostream& operator<<(std::ostream& os, ReadAheadKind k);

enum class CommandChoice : uint32_t {
  kRegisterFilePath,   // kRegisterFilePath <sid:uint32> <id:uint32> <path:c-string>
  kUnregisterFilePath, // kUnregisterFilePath <sid:uint32> <id:uint32>
  kReadAhead,          // kReadAhead <sid:uint32> <id:uint32> <kind:uint32_t> <length:uint64> <offset:uint64>
  kExit,               // kExit
  kCreateSession,      // kCreateSession <sid:uint32> <description:c-string>
  kDestroySession,     // kDestroySession <sid:uint32>
  kDumpSession,        // kDumpSession <sid:uint32>
  kDumpEverything,     // kDumpEverything
  kCreateFdSession,    // kCreateFdSession $CMSG{<fd:int>} <sid:uint32> <description:c-string>
};

struct Command {
  CommandChoice choice;
  uint32_t session_id;
  uint32_t id;  // file_path_id
  std::optional<std::string> file_path;  // required for choice=kRegisterFilePath.
  // also serves as the description for choice=kCreateSession

  // choice=kReadAhead
  ReadAheadKind read_ahead_kind;
  uint64_t length;
  uint64_t offset;

  std::optional<int> fd;  // only valid in kCreateFdSession.

  // Deserialize from a char buffer.
  // This can only fail if buf_size is too small.
  static std::optional<Command> Read(char* buf, size_t buf_size, /*out*/size_t* consumed_bytes);
  // Serialize to a char buffer.
  // This can only fail if the buf_size is too small.
  bool Write(char* buf, size_t buf_size, /*out*/size_t* produced_bytes) const;

  bool RequiresFd() const {
    return choice == CommandChoice::kCreateFdSession;
  }
};

std::ostream& operator<<(std::ostream& os, const Command& command);

class PrefetcherDaemon {
 public:
  PrefetcherDaemon();
  ~PrefetcherDaemon();

  // Asynchronously launch a new fork.
  //
  // The destructor will waitpid automatically on the child process.
  bool StartViaFork(PrefetcherForkParameters params);

  // Launch a new fork , returning the pipes as input/output fds.
  std::optional<PrefetcherForkParameters> StartPipesViaFork();

  // Launch a new fork , returning the socket pair as input/output fds.
  std::optional<PrefetcherForkParameters> StartSocketViaFork();

  // Execute the main code in-process.
  //
  // Intended as the execve target.
  bool Main(PrefetcherForkParameters params);

  // Send a command via IPC.
  // The caller must be the parent process after using StartViaFork.
  bool SendCommand(const Command& command);

 private:
  class Impl;
  std::unique_ptr<PrefetcherDaemon::Impl> impl_;
};

}  // namespace prefetcher
}  // namespace iorap

#endif

