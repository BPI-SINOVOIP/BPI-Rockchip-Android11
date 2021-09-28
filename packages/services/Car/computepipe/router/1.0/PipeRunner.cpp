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

#include "PipeRunner.h"

#include <android/binder_ibinder.h>

#include <mutex>

namespace android {
namespace automotive {
namespace computepipe {
namespace router {
namespace V1_0 {
namespace implementation {

using namespace aidl::android::automotive::computepipe::runner;
using namespace ndk;

PipeRunner::PipeRunner(const std::shared_ptr<IPipeRunner>& graphRunner) : runner(graphRunner) {
}

RunnerHandle::RunnerHandle(const std::shared_ptr<IPipeRunner>& r)
    : PipeHandle(std::make_unique<PipeRunner>(r)),
      mDeathMonitor(AIBinder_DeathRecipient_new(RemoteMonitor::BinderDiedCallback)) {
}

bool RunnerHandle::startPipeMonitor() {
    mState = std::make_shared<RemoteState>();
    auto monitor = new RemoteMonitor(mState);
    auto status = ScopedAStatus::fromStatus(
        AIBinder_linkToDeath(mInterface->runner->asBinder().get(), mDeathMonitor.get(), monitor));
    return status.isOk();
}

bool RunnerHandle::isAlive() {
    return mState->isAlive();
}

PipeHandle<PipeRunner>* RunnerHandle::clone() const {
    return new RunnerHandle(mInterface->runner);
}

RunnerHandle::~RunnerHandle() {
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace router
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
