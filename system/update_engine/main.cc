//
// Copyright (C) 2012 The Android Open Source Project
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
//

#include <sys/stat.h>
#include <sys/types.h>
#include <xz.h>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <brillo/flag_helper.h>

#include "update_engine/common/terminator.h"
#include "update_engine/common/utils.h"
#include "update_engine/daemon.h"
#include "update_engine/logging.h"

using std::string;

int main(int argc, char** argv) {
  DEFINE_bool(logtofile, false, "Write logs to a file in log_dir.");
  DEFINE_bool(logtostderr,
              false,
              "Write logs to stderr instead of to a file in log_dir.");
  DEFINE_bool(foreground, false, "Don't daemon()ize; run in foreground.");

  chromeos_update_engine::Terminator::Init();
  brillo::FlagHelper::Init(argc, argv, "A/B Update Engine");

  // We have two logging flags "--logtostderr" and "--logtofile"; and the logic
  // to choose the logging destination is:
  // 1. --logtostderr --logtofile -> logs to both
  // 2. --logtostderr             -> logs to system debug
  // 3. --logtofile or no flags   -> logs to file
  bool log_to_system = FLAGS_logtostderr;
  bool log_to_file = FLAGS_logtofile || !FLAGS_logtostderr;
  chromeos_update_engine::SetupLogging(log_to_system, log_to_file);
  if (!FLAGS_foreground)
    PLOG_IF(FATAL, daemon(0, 0) == 1) << "daemon() failed";

  LOG(INFO) << "A/B Update Engine starting";

  // xz-embedded requires to initialize its CRC-32 table once on startup.
  xz_crc32_init();

  // Ensure that all written files have safe permissions.
  // This is a mask, so we _block_ all permissions for the group owner and other
  // users but allow all permissions for the user owner. We allow execution
  // for the owner so we can create directories.
  // Done _after_ log file creation.
  umask(S_IRWXG | S_IRWXO);

  chromeos_update_engine::UpdateEngineDaemon update_engine_daemon;
  int exit_code = update_engine_daemon.Run();

  chromeos_update_engine::Subprocess::Get().FlushBufferedLogsAtExit();

  LOG(INFO) << "A/B Update Engine terminating with exit code " << exit_code;
  return exit_code;
}
