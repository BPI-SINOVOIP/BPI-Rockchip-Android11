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

#ifndef ANDROID_AUTOMOTIVE_COMPUTEPIPE_ROUTER_V1_0_PIPERUNNER
#define ANDROID_AUTOMOTIVE_COMPUTEPIPE_ROUTER_V1_0_PIPERUNNER
#include <aidl/android/automotive/computepipe/runner/IPipeRunner.h>

#include <functional>
#include <memory>
#include <mutex>

#include "PipeHandle.h"
#include "RemoteState.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace router {
namespace V1_0 {
namespace implementation {

/**
 * Wrapper for IPC handle
 */
struct PipeRunner {
    explicit PipeRunner(
        const std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeRunner>&
            graphRunner);
    std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeRunner> runner;
};

/**
 * Runner Handle to be stored with registry.
 *
 * This is used to represent a runner at the time of registration as well as for
 * query purposes
 */
class RunnerHandle : public android::automotive::computepipe::router::PipeHandle<PipeRunner> {
  public:
    explicit RunnerHandle(
        const std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeRunner>& r);
    /**
     * override registry pipehandle methods
     */
    bool isAlive() override;
    bool startPipeMonitor() override;
    PipeHandle<PipeRunner>* clone() const override;
    ~RunnerHandle();

  private:
    std::shared_ptr<RemoteState> mState;
    ndk::ScopedAIBinder_DeathRecipient mDeathMonitor;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace router
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
#endif
