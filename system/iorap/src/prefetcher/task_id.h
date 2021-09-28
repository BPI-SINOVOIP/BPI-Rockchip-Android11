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

#ifndef PREFETCHER_TASK_ID_H_
#define PREFETCHER_TASK_ID_H_

#include <ostream>
#include <string>

namespace iorap {
namespace prefetcher {

struct TaskId {
  size_t id;  // Unique monotonically increasing ID.
  std::string path;  // File path to the trace file.

  friend std::ostream& operator<<(std::ostream& os, const TaskId& task_id) {
    os << "TaskId { id: " << task_id.id << ", path: " << task_id.path << "}";
    return os;
  }

};

}  // namespace prefetcher
}  // namespace iorap

#endif

