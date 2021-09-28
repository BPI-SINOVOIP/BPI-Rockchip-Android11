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

#ifndef ANDROID_AUTOMOTIVE_COMPUTEPIPE_ROUTER_V1_0_PIPECLIENT
#define ANDROID_AUTOMOTIVE_COMPUTEPIPE_ROUTER_V1_0_PIPECLIENT

#include <aidl/android/automotive/computepipe/registry/IClientInfo.h>
#include <android/binder_auto_utils.h>

#include <functional>
#include <memory>
#include <mutex>

#include "ClientHandle.h"
#include "RemoteState.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace router {
namespace V1_0 {
namespace implementation {

/**
 * PipeClient: Encapsulated the IPC interface to the client.
 *
 * Allows for querrying the client state
 */
class PipeClient : public ClientHandle {
  public:
    explicit PipeClient(
        const std::shared_ptr<aidl::android::automotive::computepipe::registry::IClientInfo>& info);
    bool startClientMonitor() override;
    std::string getClientName() override;
    bool isAlive() override;
    ~PipeClient();

  private:
    ::ndk::ScopedAIBinder_DeathRecipient mDeathMonitor;
    std::shared_ptr<RemoteState> mState;
    const std::shared_ptr<aidl::android::automotive::computepipe::registry::IClientInfo> mClientInfo;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace router
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
#endif
