// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/daemons/daemon.h>

#include <sysexits.h>
#include <time.h>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/run_loop.h>

namespace brillo {

Daemon::Daemon() : exit_code_{EX_OK} {
  message_loop_.SetAsCurrent();
}

Daemon::~Daemon() {
}

int Daemon::Run() {
  int exit_code = OnInit();
  if (exit_code != EX_OK)
    return exit_code;

  message_loop_.PostTask(
      base::Bind(&Daemon::OnEventLoopStartedTask, base::Unretained(this)));
  message_loop_.Run();

  OnShutdown(&exit_code_);

  // base::RunLoop::QuitClosure() causes the message loop to quit
  // immediately, even if pending tasks are still queued.
  // Run a secondary loop to make sure all those are processed.
  // This becomes important when working with D-Bus since dbus::Bus does
  // a bunch of clean-up tasks asynchronously when shutting down.
  while (message_loop_.RunOnce(false /* may_block */)) {}

  return exit_code_;
}

void Daemon::Quit() { QuitWithExitCode(EX_OK); }

void Daemon::QuitWithExitCode(int exit_code) {
  exit_code_ = exit_code;
  message_loop_.PostTask(FROM_HERE, QuitClosure());
}

void Daemon::RegisterHandler(
    int signal,
    const AsynchronousSignalHandlerInterface::SignalHandler& callback) {
  async_signal_handler_.RegisterHandler(signal, callback);
}

void Daemon::UnregisterHandler(int signal) {
  async_signal_handler_.UnregisterHandler(signal);
}

int Daemon::OnInit() {
  async_signal_handler_.Init();
  for (int signal : {SIGTERM, SIGINT}) {
    async_signal_handler_.RegisterHandler(
        signal, base::Bind(&Daemon::Shutdown, base::Unretained(this)));
  }
  async_signal_handler_.RegisterHandler(
      SIGHUP, base::Bind(&Daemon::Restart, base::Unretained(this)));
  return EX_OK;
}

int Daemon::OnEventLoopStarted() {
  // Do nothing.
  return EX_OK;
}

void Daemon::OnShutdown(int* /* exit_code */) {
  // Do nothing.
}

bool Daemon::OnRestart() {
  // Not handled.
  return false;  // Returning false will shut down the daemon instead.
}

bool Daemon::Shutdown(const signalfd_siginfo& /* info */) {
  Quit();
  return true;  // Unregister the signal handler.
}

bool Daemon::Restart(const signalfd_siginfo& /* info */) {
  if (OnRestart())
    return false;  // Keep listening to the signal.
  Quit();
  return true;  // Unregister the signal handler.
}

void Daemon::OnEventLoopStartedTask() {
  int exit_code = OnEventLoopStarted();
  if (exit_code != EX_OK)
    QuitWithExitCode(exit_code);
}

void UpdateLogSymlinks(const base::FilePath& latest_log_symlink,
                       const base::FilePath& previous_log_symlink,
                       const base::FilePath& log_file) {
  base::DeleteFile(previous_log_symlink, false);
  base::Move(latest_log_symlink, previous_log_symlink);
  if (!base::CreateSymbolicLink(log_file.BaseName(), latest_log_symlink)) {
    PLOG(ERROR) << "Unable to create symbolic link from "
                << latest_log_symlink.value() << " to " << log_file.value();
  }
}

std::string GetTimeAsLogString(const base::Time& time) {
  time_t utime = time.ToTimeT();
  struct tm tm;
  CHECK_EQ(localtime_r(&utime, &tm), &tm);
  char str[16];
  CHECK_EQ(strftime(str, sizeof(str), "%Y%m%d-%H%M%S", &tm), 15UL);
  return std::string(str);
}

}  // namespace brillo
