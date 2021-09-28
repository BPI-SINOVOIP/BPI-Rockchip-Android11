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
#include "PipeRegistration.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace router {
namespace V1_0 {
namespace implementation {

using namespace android::automotive::computepipe;
using namespace aidl::android::automotive::computepipe::runner;
using namespace ndk;
// Methods from ::android::automotive::computepipe::registry::V1_0::IPipeRegistration follow.
ScopedAStatus PipeRegistration::registerPipeRunner(const std::string& graphName,
                                                   const std::shared_ptr<IPipeRunner>& graphRunner) {
    if (!mRegistry) {
        return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_STATE));
    }
    std::unique_ptr<PipeHandle<PipeRunner>> handle = std::make_unique<RunnerHandle>(graphRunner);
    auto err = mRegistry->RegisterPipe(std::move(handle), graphName);
    return convertToBinderStatus(err);
}

ScopedAStatus PipeRegistration::convertToBinderStatus(Error err) {
    switch (err) {
        case OK:
            return ScopedAStatus::ok();
        default:
            return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_STATE));
    }
}

const char* PipeRegistration::getIfaceName() {
    return this->descriptor;
}
}  // namespace implementation
}  // namespace V1_0
}  // namespace router
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
