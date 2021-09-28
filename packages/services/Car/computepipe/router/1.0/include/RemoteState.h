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

#ifndef ANDROID_AUTOMOTIVE_COMPUTEPIPE_ROUTER_V1_0_REMOTESTATE
#define ANDROID_AUTOMOTIVE_COMPUTEPIPE_ROUTER_V1_0_REMOTESTATE

#include <utils/RefBase.h>

#include <memory>
#include <mutex>

namespace android {
namespace automotive {
namespace computepipe {
namespace router {
namespace V1_0 {
namespace implementation {

/**
 * Wrapper for the Runner State machine
 */
class RemoteState {
  public:
    RemoteState() = default;
    void markDead();
    bool isAlive();

  private:
    std::mutex mStateLock;
    bool mAlive = true;
};

/**
 * Monitor for tracking remote state
 */
class RemoteMonitor : public RefBase {
  public:
    explicit RemoteMonitor(const std::weak_ptr<RemoteState>& s) : mState(s) {
    }
    static void BinderDiedCallback(void* cookie);
    void binderDied();

  private:
    std::weak_ptr<RemoteState> mState;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace router
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
#endif
