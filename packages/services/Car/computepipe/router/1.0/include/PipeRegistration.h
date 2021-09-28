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

#ifndef ANDROID_AUTOMOTIVE_COMPUTEPIPE_ROUTER_V1_0_PIPEREGISTRATION
#define ANDROID_AUTOMOTIVE_COMPUTEPIPE_ROUTER_V1_0_PIPEREGISTRATION
#include <aidl/android/automotive/computepipe/registry/BnPipeRegistration.h>

#include <memory>
#include <utility>

#include "PipeRunner.h"
#include "Registry.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace router {
namespace V1_0 {
namespace implementation {

class PipeRegistration
    : public aidl::android::automotive::computepipe::registry::BnPipeRegistration {
  public:
    // Method from ::android::automotive::computepipe::registry::V1_0::IPipeRegistration
    ndk::ScopedAStatus registerPipeRunner(
        const std::string& graphName,
        const std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeRunner>&
            graphRunner) override;

    explicit PipeRegistration(std::shared_ptr<PipeRegistry<PipeRunner>> r) : mRegistry(r) {
    }
    const char* getIfaceName();

  private:
    // Convert internal registry error codes to PipeStatus
    ndk::ScopedAStatus convertToBinderStatus(Error err);
    std::shared_ptr<PipeRegistry<PipeRunner>> mRegistry;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace router
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
#endif
