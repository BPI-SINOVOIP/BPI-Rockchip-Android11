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

#include "common/cmd_utils.h"
#include "common/debug.h"
#include "compiler/compiler.h"
#include "inode2filename/inode_resolver.h"

#include <android-base/parseint.h>
#include <android-base/logging.h>

#include <iostream>
#include <limits>
#include <optional>
#include <string>

#if defined(IORAP_COMPILER_MAIN)

namespace iorap::compiler {

void Usage(char** argv) {
  std::cerr << "Usage: " << argv[0] << " [--output-proto=output.pb] input1.pb [input2.pb ...]" << std::endl;
  std::cerr << "" << std::endl;
  std::cerr << "  Request a compilation of multiple inputs (format: PerfettoTraceProto)." << std::endl;
  std::cerr << "  The result is a TraceFile, representing a merged compiled trace with inodes resolved." << std::endl;
  std::cerr << "" << std::endl;
  std::cerr << "  Optional flags:" << std::endl;
  std::cerr << "    --help,-h                  Print this Usage." << std::endl;
  std::cerr << "    --blacklist-filter,-bf     Specify regex acting as a blacklist filter." << std::endl;
  std::cerr << "                               Filepaths matching this regex are removed from the output file." << std::endl;
  std::cerr << "    --output-text,-ot          Output ascii text instead of protobuf (default off)." << std::endl;
  std::cerr << "    --output-proto $,-op $     TraceFile tracebuffer output file (default stdout)." << std::endl;
  std::cerr << "    --inode-textcache $,-it $  Resolve inode->filename from textcache (disables diskscan)." << std::endl;
  std::cerr << "    --verbose,-v               Set verbosity (default off)." << std::endl;
  std::cerr << "    --wait,-w                  Wait for key stroke before continuing (default off)." << std::endl;
  std::cerr << "    --timestamp_limit_ns,-tl   Set the limit timestamp in nanoseconds for the compiled trace. "
                                              "The order and size of the timestamp should match that of "
                                              "the input trace files. If not specified at all, All of"
                                              "the timestamps are set to max."<< std::endl;
  exit(1);
}

int Main(int argc, char** argv) {
  android::base::InitLogging(argv);
  android::base::SetLogger(android::base::StderrLogger);

  bool wait_for_keystroke = false;
  bool enable_verbose = false;

  std::optional<std::string> arg_blacklist_filter;
  std::string arg_output_proto;
  bool arg_output_text = false;
  std::optional<std::string> arg_inode_textcache;

  std::vector<uint64_t> timestamp_limit_ns;

  if (argc == 1) {
    // Need at least 1 input file to do anything.
    Usage(argv);
  }

  std::vector<std::string> arg_input_filenames;

  for (int arg = 1; arg < argc; ++arg) {
    std::string argstr = argv[arg];
    bool has_arg_next = (arg+1)<argc;
    std::string arg_next = has_arg_next ? argv[arg+1] : "";

    if (argstr == "--help" || argstr == "-h") {
      Usage(argv);
    } else if (argstr == "--output-proto" || argstr == "-op") {
      if (!has_arg_next) {
        std::cerr << "Missing --output-proto <value>" << std::endl;
        return 1;
      }
      arg_output_proto = arg_next;
      ++arg;
    } else if (argstr == "--output-text" || argstr == "-ot") {
      arg_output_text = true;
    } else if (argstr == "--inode-textcache" || argstr == "-it") {
      if (!has_arg_next) {
        std::cerr << "Missing --inode-textcache <value>" << std::endl;
        return 1;
      }
      arg_inode_textcache = arg_next;
      ++arg;
    } else if (argstr == "--blacklist-filter" || argstr == "-bf") {
      if (!has_arg_next) {
        std::cerr << "Missing --blacklist-filter <value>" << std::endl;
        return 1;
      }
      arg_blacklist_filter = arg_next;
      ++arg;
    }
    else if (argstr == "--verbose" || argstr == "-v") {
      enable_verbose = true;
    } else if (argstr == "--wait" || argstr == "-w") {
      wait_for_keystroke = true;
    } else if (argstr == "--timestamp_limit_ns" || argstr == "-tl") {
      if (!has_arg_next) {
        std::cerr << "Missing --timestamp_limit_ns <value>" << std::endl;
        return 1;
      }
      uint64_t timestamp;
      if (!::android::base::ParseUint<uint64_t>(arg_next, &timestamp)) {
        std::cerr << "Invalid --timestamp-limit-ns "<< arg_next << std::endl;
        return 1;
      }
      timestamp_limit_ns.push_back(timestamp);
      ++arg;
    } else {
      arg_input_filenames.push_back(argstr);
    }
  }

  if (!timestamp_limit_ns.empty() &&
      timestamp_limit_ns.size() != arg_input_filenames.size()) {
    std::cerr << "The size of timestamp limits doesn't match the size of input files."
              << std::endl;
    return 1;
  }

  if (enable_verbose) {
    android::base::SetMinimumLogSeverity(android::base::VERBOSE);

    LOG(VERBOSE) << "Verbose check";
    LOG(VERBOSE) << "Debug check: " << ::iorap::kIsDebugBuild;
  } else {
    android::base::SetMinimumLogSeverity(android::base::DEBUG);
  }

  // Useful to attach a debugger...
  // 1) $> iorap.cmd.compiler -w <args>
  // 2) $> gdbclient <pid>
  if (wait_for_keystroke) {
    LOG(INFO) << "Self pid: " << getpid();
    LOG(INFO) << "Press any key to continue...";
    std::cin >> wait_for_keystroke;
  }

  auto system_call = std::make_unique<SystemCallImpl>();

  using namespace inode2filename;

  InodeResolverDependencies ir_dependencies;
  // Passed from command-line.
  if (arg_inode_textcache) {
    ir_dependencies.data_source = DataSourceKind::kTextCache;
    ir_dependencies.text_cache_filename = arg_inode_textcache;
    ir_dependencies.verify = VerifyKind::kNone;  // required for determinism.
  } else {
    ir_dependencies.data_source = DataSourceKind::kDiskScan;
    LOG(WARNING) << "--inode-textcache unspecified. "
                 << "Inodes will be resolved by scanning the disk, which makes compilation "
                 << "non-deterministic.";
  }
  // TODO: add command line.
  ir_dependencies.root_directories.push_back("/system");
  ir_dependencies.root_directories.push_back("/apex");
  ir_dependencies.root_directories.push_back("/data");
  ir_dependencies.root_directories.push_back("/vendor");
  ir_dependencies.root_directories.push_back("/product");
  ir_dependencies.root_directories.push_back("/metadata");
  // Hardcoded.
  if (iorap::common::GetBoolEnvOrProperty("iorap.inode2filename.out_of_process", true)) {
    ir_dependencies.process_mode = ProcessMode::kOutOfProcessIpc;
  } else {
    ir_dependencies.process_mode = ProcessMode::kInProcessDirect;
  }
  ir_dependencies.system_call = /*borrowed*/system_call.get();

  int return_code = 0;
  std::vector<CompilationInput> perfetto_traces =
      MakeCompilationInputs(arg_input_filenames, timestamp_limit_ns);
  return_code =
      !PerformCompilation(std::move(perfetto_traces),
                          std::move(arg_output_proto),
                          !arg_output_text,
                          arg_blacklist_filter,
                          std::move(ir_dependencies));

  // Uncomment this if we want to leave the process around to inspect it from adb shell.
  // sleep(100000);

  // 0 -> successfully wrote the proto out to file.
  // 1 -> failed along the way (#on_error and also see the error logs).
  return return_code;
}

}  // namespace iorap::compiler

int main(int argc, char** argv) {
  return ::iorap::compiler::Main(argc, argv);
}

#endif  // IORAP_COMPILER_MAIN
