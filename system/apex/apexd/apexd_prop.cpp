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

#define LOG_TAG "apexd"

#include "apexd_prop.h"

#include <android-base/logging.h>
#include <android-base/properties.h>

#include "apexd_utils.h"

using android::base::GetBoolProperty;
using android::base::GetProperty;
using android::base::Result;
using android::base::WaitForProperty;

namespace android {
namespace apex {
void waitForBootStatus(Result<void> (&revert_fn)(const std::string&),
                       void (&complete_fn)()) {
  while (!GetBoolProperty("sys.boot_completed", false)) {
    // Check for change in either crashing property or sys.boot_completed
    // Wait for updatable_crashing property change for most of the time
    // (arbitrary 30s), briefly check if boot has completed successfully,
    // if not continue waiting for updatable_crashing.
    // We use this strategy so that we can quickly detect if an updatable
    // process is crashing.
    if (WaitForProperty("sys.init.updatable_crashing", "1",
                        std::chrono::seconds(30))) {
      auto name = GetProperty("sys.init.updatable_crashing_process_name", "");
      LOG(ERROR) << "Native process '" << (name.empty() ? "[unknown]" : name)
                 << "' is crashing. Attempting a revert";
      auto result = revert_fn(name);
      if (!result.ok()) {
        LOG(ERROR) << "Revert failed : " << result.error();
        break;
      } else {
        // This should never be reached, since revert_fn should've rebooted a
        // device. But if for some reason we end up here, let's reboot it
        // manually.
        LOG(ERROR) << "Active sessions were reverted, but reboot wasn't "
                      "triggered. Rebooting manually";
        Reboot();
        return;
      }
    }
  }
  // Wait for boot to complete, and then run complete_fn.
  // TODO(ioffe): this is a hack, instead we should have a binder call from
  //  system_server into apexd when boot completes.
  if (WaitForProperty("sys.boot_completed", "1", std::chrono::minutes(5))) {
    complete_fn();
    return;
  } else {
    LOG(ERROR) << "Boot never completed";
  }
}
}  // namespace apex
}  // namespace android
