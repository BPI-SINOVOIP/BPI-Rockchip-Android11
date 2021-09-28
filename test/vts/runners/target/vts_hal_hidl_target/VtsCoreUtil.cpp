/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <log/log.h>
#include <iostream>

#include "VtsCoreUtil.h"

namespace testing {

// Runs "cmd" and attempts to find the specified feature in its
// output.
bool checkSubstringInCommandOutput(const char* cmd, const char* feature) {
  bool hasFeature = false;
  // This is one of the best stable native interface. Calling AIDL directly
  // would be problematic if the binder interface changes.
  FILE* p = popen(cmd, "re");
  if (p) {
    char* line = NULL;
    size_t len = 0;
    __android_log_print(ANDROID_LOG_FATAL, LOG_TAG,
                        "checkSubstringInCommandOutput check with cmd: %s",
                        cmd);
    while (getline(&line, &len, p) > 0) {
      // TODO: b/148904287, check if we should match the whole line
      if (strstr(line, feature)) {
        hasFeature = true;
        break;
      }
    }
    pclose(p);
  } else {
    __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, "popen failed: %d", errno);
    _exit(EXIT_FAILURE);
  }
  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Feature %s: %ssupported",
                      feature, hasFeature ? "" : "not ");
  return hasFeature;
}

// Runs "pm list features" and attempts to find the specified feature in its
// output.
bool deviceSupportsFeature(const char* feature) {
  return checkSubstringInCommandOutput("/system/bin/pm list features", feature);
}

}  // namespace testing
