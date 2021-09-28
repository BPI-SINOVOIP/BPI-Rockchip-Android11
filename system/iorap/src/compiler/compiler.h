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

#ifndef IORAP_SRC_COMPILER_COMPILER_H_
#define IORAP_SRC_COMPILER_COMPILER_H_

#include "inode2filename/inode_resolver.h"

#include <optional>
#include <string>
#include <vector>

namespace iorap::compiler {
struct  CompilationInput {
  /* The name of the perfetto trace. */
  std::string filename;
  /*
   * The timestamp limit of the trace.
   * It's used to truncate the trace file.
   */
  uint64_t timestamp_limit_ns;
};

// Compile one or more perfetto TracePacket protobufs that are stored on the filesystem
// by the filenames given with `input_file_names` and timestamp limit given with
// timestamp_limit_ns.
//
// For each input file name and timestamp limit, ignore any events from the input that
// exceed the associated timestamp limit.
//
// If blacklist_filter is non-null, ignore any file entries whose file path matches the
// regular expression in blacklist_filter.
//
// The result is stored into the file path specified by `output_file_name`.
//
// This is a blocking function which returns only when compilation finishes. The return value
// determines success/failure.
//
// Operation is transactional -- that is if there is a failure, `output_file_name` is untouched.
bool PerformCompilation(std::vector<iorap::compiler::CompilationInput> perfetto_traces,
                        std::string output_file_name,
                        bool output_proto,
                        std::optional<std::string> blacklist_filter,
                        inode2filename::InodeResolverDependencies dependencies);

// The size and order of timestamp_limit_ns should match that of
// input_file_names, if not empty.
// If timestamp_limit_ns is empty, will use the max uint64_t.
std::vector<CompilationInput> MakeCompilationInputs(
    std::vector<std::string> input_file_names,
    std::vector<uint64_t> timestamp_limit_ns);
}

#endif  // IORAP_SRC_COMPILER_COMPILER_H_
