/*
 * Copyright 2019 The Android Open Source Project
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

#include <iostream>

#include <wakelock/wakelock.h>

static constexpr const char *gWakeLockName = "block_suspend";

static void usage() {
    std::cout << "Usage: block_suspend\n"
              << "Prevent device from suspending indefinitely. "
              << "Process must be killed to unblock suspend.\n";
}

int main(int argc, char ** /* argv */) {
    if (argc > 1) {
        usage();
        return 0;
    }

    android::wakelock::WakeLock wl{gWakeLockName};  // RAII object
    while (true) {};
    std::abort();  // should never reach here
    return 0;
}
