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

#include "prefetcher/minijail.h"

#include <android-base/logging.h>
#include <libminijail.h>

namespace iorap::prefetcher {

static const char kSeccompFilePath[] = "/system/etc/seccomp_policy/iorap.prefetcherd.policy";

bool MiniJail() {
  /* no seccomp policy for this architecture */
  if (access(kSeccompFilePath, R_OK) == -1) {
      LOG(WARNING) << "No seccomp filter defined for this architecture.";
      return true;
  }

  struct minijail* jail = minijail_new();
  if (jail == NULL) {
      LOG(WARNING) << "Failed to create minijail.";
      return false;
  }

  minijail_no_new_privs(jail);
  minijail_log_seccomp_filter_failures(jail);
  minijail_use_seccomp_filter(jail);
  minijail_parse_seccomp_filters(jail, kSeccompFilePath);
  minijail_enter(jail);
  minijail_destroy(jail);

  LOG(DEBUG) << "minijail installed.";

  return true;
}

}
