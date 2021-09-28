/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include "profile-extras.h"

extern "C" {

static sighandler_t chained_signal_handler = SIG_ERR;

int __llvm_profile_write_file(void);

static void llvm_signal_handler(__unused int signum) {
  __llvm_profile_write_file();

  if (chained_signal_handler != SIG_ERR &&
      chained_signal_handler != SIG_IGN &&
      chained_signal_handler != SIG_DFL) {
    (chained_signal_handler)(signum);
  }
}

__attribute__((weak)) int init_profile_extras_once = 0;

// Initialize libprofile-extras:
// - Install a signal handler that triggers __llvm_profile_write_file on <COVERAGE_FLUSH_SIGNAL>.
//
// We want this initializer to run during load time.
//
// Just marking init_profile_extras() with __attribute__((constructor)) isn't
// enough since the linker drops it from its output since no other symbol from
// this static library is referenced.
//
// We force the linker to include init_profile_extras() by passing
// '-uinit_profile_extras' to the linker (in build/soong).
__attribute__((constructor)) int init_profile_extras(void) {
  if (init_profile_extras_once)
    return 0;
  init_profile_extras_once = 1;

  if (chained_signal_handler != SIG_ERR) {
    return -1;
  }
  sighandler_t ret1 = signal(COVERAGE_FLUSH_SIGNAL, llvm_signal_handler);
  if (ret1 == SIG_ERR) {
    return -1;
  }
  chained_signal_handler = ret1;

  return 0;
}
}
