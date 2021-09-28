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

#ifndef PREFETCHER_READAHEAD_H_
#define PREFETCHER_READAHEAD_H_

#include <memory>
#include <optional>

namespace android {
class Printer;
}  // namespace android

namespace iorap {
namespace prefetcher {

struct TaskId;
struct ReadAheadFileEntry;

// Manage I/O readahead for a task.
class ReadAhead {
  struct Impl;
 public:
  // Process a task *now*. Currently will block until all readaheads have been
  // issued for all entries in that task.
  //
  // Any memory mapped or file descriptors opened as a side effect must be
  // cleaned up with #FinishTask.
  void BeginTask(const TaskId& id);
  // Complete a task, releasing any memory/file descriptors associated with it.
  void FinishTask(const TaskId& id);

  static void Dump(/*borrow*/::android::Printer& printer);

  // Calculate the sum of file_lengths. Returns nullopt if the file path does not
  // point to a valid compiled TraceFile.
  static std::optional<size_t> PrefetchSizeInBytes(const std::string& file_path);

  ReadAhead(bool use_sockets);

  ReadAhead();
  ~ReadAhead();
 private:
  void BeginTaskForSockets(const TaskId& id, int32_t trace_cookie);
  std::unique_ptr<Impl> impl_;
};

}  // namespace prefetcher
}  // namespace iorap

#endif

