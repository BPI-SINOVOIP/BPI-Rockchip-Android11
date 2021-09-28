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

#include "io_thread.h"
#include "debug.h"

namespace android {
namespace hardware {
namespace audio {
namespace V6_0 {
namespace implementation {

bool IOThread::notify(const uint32_t mask) {
    EventFlag *evf = getEventFlag();
    if (evf) {
        if (NO_ERROR == evf->wake(mask)) {
            return true;
        } else {
            return FAILURE(false);
        }
    } else {
        return FAILURE(false);
    }
}

bool IOThread::standby() {
    return notify(STAND_BY_REQUEST);
}

void IOThread::requestExit() {
    notify(EXIT_REQUEST);
}

}  // namespace implementation
}  // namespace V6_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
