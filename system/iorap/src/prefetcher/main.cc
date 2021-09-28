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
#include "common/loggers.h"
#include "prefetcher/prefetcher_daemon.h"

#include <android-base/parseint.h>
#include <android-base/logging.h>

#include <iostream>
#include <optional>
#include <string_view>
#include <string>
#include <vector>

#include <signal.h>

#if defined(IORAP_PREFETCHER_MAIN)

namespace iorap::prefetcher {

void Usage(char** argv) {
  std::cerr << "Usage: " << argv[0] << " [--input-fd=#] [--output-fd=#]" << std::endl;
  std::cerr << "" << std::endl;
  std::cerr << "  Run the readahead daemon which can prefetch files given a command." << std::endl;
  std::cerr << "" << std::endl;
  std::cerr << "  Optional flags:" << std::endl;
  std::cerr << "    --help,-h                  Print this Usage." << std::endl;
  std::cerr << "    --input-fd,-if             Input FD (default stdin)." << std::endl;
  std::cerr << "    --output-fd,-of            Output FD (default stdout)." << std::endl;
  std::cerr << "    --use-sockets,-us          Use AF_UNIX sockets (default off)." << std::endl;
  std::cerr << "    --command-format=[text|binary],-cf   (default text)." << std::endl;
  std::cerr << "    --verbose,-v               Set verbosity (default off)." << std::endl;
  std::cerr << "    --wait,-w                  Wait for key stroke before continuing (default off)." << std::endl;
  exit(1);
}

int Main(int argc, char** argv) {
  // Go to system logcat + stderr when running from command line.
  android::base::InitLogging(argv, iorap::common::StderrAndLogdLogger{android::base::SYSTEM});

  bool wait_for_keystroke = false;
  bool enable_verbose = false;

  bool command_format_text = false; // false = binary.

  int arg_input_fd = -1;
  int arg_output_fd = -1;

  std::vector<std::string> arg_input_filenames;
  bool arg_use_sockets = false;

  LOG(VERBOSE) << "argparse: argc=" << argc;

  for (int arg = 1; arg < argc; ++arg) {
    std::string argstr = argv[arg];
    bool has_arg_next = (arg+1)<argc;
    std::string arg_next = has_arg_next ? argv[arg+1] : "";

    LOG(VERBOSE) << "argparse: argv[" << arg << "]=" << argstr;

    if (argstr == "--help" || argstr == "-h") {
      Usage(argv);
    } else if (argstr == "--input-fd" || argstr == "-if") {
      if (!has_arg_next) {
        LOG(ERROR) << "--input-fd=<numeric-value>";
        Usage(argv);
      }
      if (!::android::base::ParseInt(arg_next, /*out*/&arg_input_fd)) {
        LOG(ERROR) << "--input-fd value must be numeric";
        Usage(argv);
      }
    } else if (argstr == "--output-fd" || argstr == "-of") {
      if (!has_arg_next) {
        LOG(ERROR) << "--output-fd=<numeric-value>";
        Usage(argv);
      }
      if (!::android::base::ParseInt(arg_next, /*out*/&arg_output_fd)) {
        LOG(ERROR) << "--output-fd value must be numeric";
        Usage(argv);
      }
    } else if (argstr == "--command-format=" || argstr == "-cf") {
      if (!has_arg_next) {
        LOG(ERROR) << "--command-format=text|binary";
        Usage(argv);
      }
      if (arg_next == "text") {
        command_format_text = true;
      } else if (arg_next == "binary") {
        command_format_text = false;
      } else {
        LOG(ERROR) << "--command-format must be one of {text,binary}";
        Usage(argv);
      }
    } else if (argstr == "--use-sockets" || argstr == "-us") {
      arg_use_sockets = true;
    } else if (argstr == "--verbose" || argstr == "-v") {
      enable_verbose = true;
    } else if (argstr == "--wait" || argstr == "-w") {
      wait_for_keystroke = true;
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

  if (arg_input_fd == -1) {
    arg_input_fd = STDIN_FILENO;
  }
  if (arg_output_fd == -1) {
    arg_output_fd = STDOUT_FILENO;
  }

  PrefetcherForkParameters params{};
  params.input_fd = arg_input_fd;
  params.output_fd = arg_output_fd;
  params.format_text = command_format_text;
  params.use_sockets = arg_use_sockets;

  LOG(VERBOSE) << "main: Starting PrefetcherDaemon: "
               << "input_fd=" << params.input_fd
               << ",output_fd=" << params.output_fd;
  {
    PrefetcherDaemon daemon;
    // Blocks until receiving an exit command.
    daemon.Main(std::move(params));
  }
  LOG(VERBOSE) << "main: Terminating";

  // 0 -> successfully executed all commands.
  // 1 -> failed along the way (#on_error and also see the error logs).
  return return_code;
}

}  // namespace iorap::prefetcher

int main(int argc, char** argv) {
  return ::iorap::prefetcher::Main(argc, argv);
}

#endif  // IORAP_PREFETCHER_MAIN
