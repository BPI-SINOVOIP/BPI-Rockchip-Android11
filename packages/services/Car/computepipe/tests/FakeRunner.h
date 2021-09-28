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

#ifndef ANDROID_AUTOMOTIVE_COMPUTEPIPE_TESTS
#define ANDROID_AUTOMOTIVE_COMPUTEPIPE_TESTS

#include <aidl/android/automotive/computepipe/runner/BnPipeRunner.h>
#include <aidl/android/automotive/computepipe/runner/IPipeRunner.h>

#include <memory>

namespace android {
namespace automotive {
namespace computepipe {
namespace tests {

// TODO: Wrap under version flag

// This is a fake runner class whose various methods can be mocked in order
// to test the Runner logic.

class FakeRunner : public aidl::android::automotive::computepipe::runner::BnPipeRunner {
  public:
    ::ndk::ScopedAStatus init(
        const std::shared_ptr<::aidl::android::automotive::computepipe::runner::IPipeStateCallback>&
            in_statecb) override;
    ::ndk::ScopedAStatus getPipeDescriptor(
        ::aidl::android::automotive::computepipe::runner::PipeDescriptor* _aidl_return) override;
    ::ndk::ScopedAStatus setPipeInputSource(int32_t in_configId) override;
    ::ndk::ScopedAStatus setPipeOffloadOptions(int32_t in_configId) override;
    ::ndk::ScopedAStatus setPipeTermination(int32_t in_configId) override;
    ::ndk::ScopedAStatus setPipeOutputConfig(
        int32_t in_configId, int32_t in_maxInFlightCount,
        const std::shared_ptr<::aidl::android::automotive::computepipe::runner::IPipeStream>&
            in_handler) override;
    ::ndk::ScopedAStatus applyPipeConfigs() override;
    ::ndk::ScopedAStatus resetPipeConfigs() override;
    ::ndk::ScopedAStatus startPipe() override;
    ::ndk::ScopedAStatus stopPipe() override;
    ::ndk::ScopedAStatus doneWithPacket(int32_t in_bufferId, int32_t in_streamId) override;
    ::ndk::ScopedAStatus getPipeDebugger(
        std::shared_ptr<::aidl::android::automotive::computepipe::runner::IPipeDebugger>*
            _aidl_return) override;
    ::ndk::ScopedAStatus releaseRunner() override;
    ~FakeRunner() {
        mOutputCallbacks.clear();
    }

  private:
    aidl::android::automotive::computepipe::runner::PipeDescriptor mDesc;
    std::vector<std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeStream>>
        mOutputCallbacks;
    std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeStateCallback> mStateCallback;
};

}  // namespace tests
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
#endif
