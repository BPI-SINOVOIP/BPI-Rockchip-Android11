/**
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

#include "RemoteState.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace router {
namespace V1_0 {
namespace implementation {

void RemoteState::markDead() {
    std::lock_guard<std::mutex> lock(mStateLock);
    mAlive = false;
}

bool RemoteState::isAlive() {
    std::lock_guard<std::mutex> lock(mStateLock);
    return mAlive;
}

void RemoteMonitor::binderDied() {
    auto state = mState.lock();
    if (state != nullptr) {
        state->markDead();
    }
}

void RemoteMonitor::BinderDiedCallback(void* cookie) {
    auto monitor = static_cast<RemoteMonitor*>(cookie);
    monitor->binderDied();
}
}  // namespace implementation
}  // namespace V1_0
}  // namespace router
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
