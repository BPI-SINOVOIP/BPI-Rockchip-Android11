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

#include "common/debug.h"
#include "prefetcher/read_ahead.h"
#include "prefetcher/task_id.h"

#include <android-base/parseint.h>
#include <android-base/logging.h>

#include <iostream>
#include <optional>
#include <string_view>
#include <string>
#include <vector>

#include <signal.h>
#include <unistd.h>


namespace iorap::prefetcher {

static void UsageClient(char** argv) {
  std::cerr << "UsageClient: " << argv[0] << " <path-to-compiled-trace.pb> [... pathN]" << std::endl;
  std::cerr << "" << std::endl;
  std::cerr << "  Run the readahead daemon which can prefetch files given a command." << std::endl;
  std::cerr << "" << std::endl;
  std::cerr << "  Optional flags:" << std::endl;
  std::cerr << "    --help,-h                  Print this UsageClient." << std::endl;
  std::cerr << "    --verbose,-v               Set verbosity (default off)." << std::endl;
  std::cerr << "    --task-duration-ms,-tdm    Set task duration (default: 0ms)." << std::endl;
  std::cerr << "    --use-sockets,-us          Use AF_UNIX sockets (default: off)" << std::endl;
  std::cerr << "    --wait,-w                  Wait for key stroke before continuing (default off)." << std::endl;
  exit(1);
}

int MainClient(int argc, char** argv) {
  android::base::InitLogging(argv);
  android::base::SetLogger(android::base::StderrLogger);

  bool wait_for_keystroke = false;
  bool enable_verbose = false;

  bool command_format_text = false; // false = binary.

  unsigned int arg_task_duration_ms = 10000;
  std::vector<std::string> arg_input_filenames;
  bool arg_use_sockets = false;

  LOG(VERBOSE) << "argparse: argc=" << argc;

  for (int arg = 1; arg < argc; ++arg) {
    std::string argstr = argv[arg];
    bool has_arg_next = (arg+1)<argc;
    std::string arg_next = has_arg_next ? argv[arg+1] : "";

    LOG(VERBOSE) << "argparse: argv[" << arg << "]=" << argstr;

    if (argstr == "--help" || argstr == "-h") {
      UsageClient(argv);
    } else if (argstr == "--use-sockets" || argstr == "-us") {
      arg_use_sockets = true;
    } else if (argstr == "--verbose" || argstr == "-v") {
      enable_verbose = true;
    } else if (argstr == "--wait" || argstr == "-w") {
      wait_for_keystroke = true;
    } else if (argstr == "--task-duration-ms" || argstr == "-tdm") {
      if (!has_arg_next) {
        LOG(ERROR) << "--task-duration-ms: requires uint parameter";
        UsageClient(argv);
      } else if (!::android::base::ParseUint(arg_next, &arg_task_duration_ms)) {
        LOG(ERROR) << "--task-duration-ms: requires non-negative parameter";
        UsageClient(argv);
      }
    } else {
      arg_input_filenames.push_back(argstr);
    }
  }

  if (enable_verbose) {
    android::base::SetMinimumLogSeverity(android::base::VERBOSE);

    LOG(VERBOSE) << "Verbose check";
    LOG(VERBOSE) << "Debug check: " << ::iorap::kIsDebugBuild;
  } else {
    android::base::SetMinimumLogSeverity(android::base::DEBUG);
  }

  LOG(VERBOSE) << "argparse: argc=" << argc;

  for (int arg = 1; arg < argc; ++arg) {
    std::string argstr = argv[arg];

    LOG(VERBOSE) << "argparse: argv[" << arg << "]=" << argstr;
  }

  // Useful to attach a debugger...
  // 1) $> iorap.cmd.readahead -w <args>
  // 2) $> gdbclient <pid>
  if (wait_for_keystroke) {
    LOG(INFO) << "Self pid: " << getpid();

    raise(SIGSTOP);
    // LOG(INFO) << "Press any key to continue...";
    // std::cin >> wait_for_keystroke;
  }

  // auto system_call = std::make_unique<SystemCallImpl>();
  // TODO: mock readahead calls?
  //
  // Uncomment this if we want to leave the process around to inspect it from adb shell.
  // sleep(100000);

  int return_code = 0;

  LOG(VERBOSE) << "Hello world";

  ReadAhead read_ahead{arg_use_sockets};  // Don't count the time it takes to fork+exec.

  size_t task_id_counter = 0;
  for (const std::string& compiled_trace_path : arg_input_filenames) {
    TaskId task_id{task_id_counter++, compiled_trace_path};

    LOG(DEBUG) << "main: ReadAhead BeginTask: "
                 << "task_duration_ms=" << arg_task_duration_ms << ","
                 << task_id;

    read_ahead.BeginTask(task_id);
    usleep(arg_task_duration_ms*1000);

    LOG(DEBUG) << "main: ReadAhead FinishTask: " << task_id;

    read_ahead.FinishTask(task_id);
  }
  LOG(VERBOSE) << "main: Terminating";

  // 0 -> successfully executed all commands.
  // 1 -> failed along the way (#on_error and also see the error logs).
  return return_code;
}

}  // namespace iorap::prefetcher

#if defined(IORAP_PREFETCHER_MAIN_CLIENT)
int main(int argc, char** argv) {
  return ::iorap::prefetcher::MainClient(argc, argv);
}

#endif  // IORAP_PREFETCHER_MAIN_CLIENT
