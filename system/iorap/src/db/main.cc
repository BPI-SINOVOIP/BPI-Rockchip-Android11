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
#include "db/app_component_name.h"
#include "db/models.h"

#include <android-base/parseint.h>
#include <android-base/logging.h>

#include <iostream>
#include <optional>
#include <string_view>
#include <string>
#include <vector>

#include <sqlite3.h>

#include <signal.h>

namespace iorap::db {

void Usage(char** argv) {
  std::cerr << "Usage: " << argv[0] << " <path-to-sqlite.db>" << std::endl;
  std::cerr << "" << std::endl;
  std::cerr << "  Interface with the iorap sqlite database and issue commands." << std::endl;
  std::cerr << "" << std::endl;
  std::cerr << "  Optional flags:" << std::endl;
  std::cerr << "    --help,-h                  Print this Usage." << std::endl;
  std::cerr << "    --register-raw-trace,-rrt  Register raw trace file path." << std::endl;
  std::cerr << "    --register-compiled-trace,-rct  Register compiled trace file path." << std::endl;
  std::cerr << "    --insert-component,-ic     Add component if it doesn't exist." << std::endl;
  std::cerr << "    --initialize,-i            Initialize new database." << std::endl;
  std::cerr << "    --rescan,-rs               Update all from canonical directories." << std::endl;
  std::cerr << "    --prune,-pr                Remove any stale file paths." << std::endl;
  std::cerr << "    --verbose,-v               Set verbosity (default off)." << std::endl;
  std::cerr << "    --wait,-w                  Wait for key stroke before continuing (default off)." << std::endl;
  exit(1);
}

void error_log_sqlite_callback(void *pArg, int iErrCode, const char *zMsg) {
  LOG(ERROR) << "SQLite error (" << iErrCode << "): " << zMsg;
}

const constexpr int64_t kNoVersion = -1;

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

  std::vector<std::pair<std::string,std::string>> arg_register_raw_trace;
  std::vector<std::pair<std::string,std::string>> arg_register_compiled_trace;

  std::vector<std::string> arg_insert_component;

  bool arg_initialize = false;
  bool arg_rescan = false;
  bool arg_prune = false;

  LOG(VERBOSE) << "argparse: argc=" << argc;

  for (int arg = 1; arg < argc; ++arg) {
    std::string argstr = argv[arg];
    bool has_arg_next = (arg+1)<argc;
    std::string arg_next = has_arg_next ? argv[arg+1] : "";

    bool has_arg_next_next = (arg+2)<argc;
    std::string arg_next_next = has_arg_next_next ? argv[arg+2] : "";

    LOG(VERBOSE) << "argparse: argv[" << arg << "]=" << argstr;

    if (argstr == "--help" || argstr == "-h") {
      Usage(argv);
    } else if (argstr == "--register-raw-trace" || argstr == "-rrt") {
      if (!has_arg_next_next) {
        LOG(ERROR) << "--register-raw-trace <component/name> <filepath>";
        Usage(argv);
      }
      arg_register_raw_trace.push_back({arg_next, arg_next_next});
    } else if (argstr == "--register-compiled-trace" || argstr == "-rct") {
      if (!has_arg_next_next) {
        LOG(ERROR) << "--register-compiled-trace <component/name> <filepath>";
        Usage(argv);
      }
      arg_register_compiled_trace.push_back({arg_next, arg_next_next});
    } else if (argstr == "--insert-component" || argstr == "-ic") {
      if (!has_arg_next) {
        LOG(ERROR) << "--insert-component <component/name>";
        Usage(argv);
      }
      arg_insert_component.push_back(arg_next);
    } else if (argstr == "--initialize" || argstr == "-i") {
      arg_initialize = true;
    } else if (argstr == "--rescan" || argstr == "-rs") {
      arg_rescan = true;
    } else if (argstr == "--prune" || argstr == "-pr") {
      arg_prune = true;
    } else if (argstr == "--verbose" || argstr == "-v") {
      enable_verbose = true;
    } else if (argstr == "--wait" || argstr == "-w") {
      wait_for_keystroke = true;
    } else {
      arg_input_filenames.push_back(argstr);
    }
  }

  if (arg_input_filenames.empty()) {
    LOG(ERROR) << "Missing positional filename to a sqlite database.";
    Usage(argv);
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


  do {
    SchemaModel schema_model = SchemaModel::GetOrCreate(arg_input_filenames[0]);
    DbHandle db = schema_model.db();
    if (arg_initialize) {
      // Drop tables and restart from scratch. All rows are effectively dropped.
      schema_model.Reinitialize();
    }

    for (const auto& component_and_file_name : arg_register_raw_trace) {
      AppComponentName component_name = AppComponentName::FromString(component_and_file_name.first);
      const std::string& file_path = component_and_file_name.second;

      LOG(VERBOSE) << "--register-raw-trace " << component_name << ", file_path: " << file_path;

      std::optional<ActivityModel> activity =
          ActivityModel::SelectOrInsert(db,
                                        component_name.package,
                                        kNoVersion,
                                        component_name.activity_name);
      DCHECK(activity.has_value());
      LOG(DEBUG) << "Component selected/inserted: " << *activity;
    }

    for (const std::string& component : arg_insert_component) {
      AppComponentName component_name = AppComponentName::FromString(component);

      LOG(VERBOSE) << "raw component: " << component;
      LOG(VERBOSE) << "package: " << component_name.package;
      LOG(VERBOSE) << "activity name: " << component_name.activity_name;

      LOG(VERBOSE) << "--insert-component " << component_name;

      std::optional<ActivityModel> activity =
          ActivityModel::SelectOrInsert(db,
                                        component_name.package,
                                        kNoVersion,
                                        component_name.activity_name);

      DCHECK(activity.has_value());
      LOG(DEBUG) << "Component selected/inserted: " << *activity;
    }
  } while (false);

  LOG(VERBOSE) << "main: Terminating";

  // 0 -> successfully executed all commands.
  // 1 -> failed along the way (#on_error and also see the error logs).
  return return_code;
}

}  // namespace iorap::db

#if defined(IORAP_DB_MAIN)
int main(int argc, char** argv) {
  return ::iorap::db::Main(argc, argv);
}
#endif  // IORAP_DB_MAIN
