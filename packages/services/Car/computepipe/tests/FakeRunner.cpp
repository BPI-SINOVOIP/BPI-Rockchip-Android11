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

#include "FakeRunner.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace tests {

using namespace aidl::android::automotive::computepipe::runner;

// Methods from ::android::automotive::computepipe::runner::V1_0::IFakeRunnerV1_0 follow.

::ndk::ScopedAStatus FakeRunner::init(
    const std::shared_ptr<
        ::aidl::android::automotive::computepipe::runner::IPipeStateCallback>& /* in_statecb */) {
    return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FakeRunner::getPipeDescriptor(
    ::aidl::android::automotive::computepipe::runner::PipeDescriptor* desc) {
    *desc = mDesc;
    return ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus FakeRunner::setPipeInputSource(int32_t /*in_configId*/) {
    ::ndk::ScopedAStatus _aidl_status;
    _aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));
    return _aidl_status;
}
::ndk::ScopedAStatus FakeRunner::setPipeOffloadOptions(int32_t /*in_configId*/) {
    ::ndk::ScopedAStatus _aidl_status;
    _aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));
    return _aidl_status;
}
::ndk::ScopedAStatus FakeRunner::setPipeTermination(int32_t /*in_configId*/) {
    ::ndk::ScopedAStatus _aidl_status;
    _aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));
    return _aidl_status;
}

::ndk::ScopedAStatus FakeRunner::setPipeOutputConfig(
    int32_t /*in_configId*/, int32_t /*in_maxInFlightCount*/,
    const std::shared_ptr<
        ::aidl::android::automotive::computepipe::runner::IPipeStream>& /*in_handler*/) {
    ::ndk::ScopedAStatus _aidl_status;
    _aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));
    return _aidl_status;
}
::ndk::ScopedAStatus FakeRunner::applyPipeConfigs() {
    ::ndk::ScopedAStatus _aidl_status;
    _aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));
    return _aidl_status;
}

::ndk::ScopedAStatus FakeRunner::resetPipeConfigs() {
    ::ndk::ScopedAStatus _aidl_status;
    _aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));
    return _aidl_status;
}

::ndk::ScopedAStatus FakeRunner::startPipe() {
    ::ndk::ScopedAStatus _aidl_status;
    _aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));
    return _aidl_status;
}
::ndk::ScopedAStatus FakeRunner::stopPipe() {
    ::ndk::ScopedAStatus _aidl_status;
    _aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));
    return _aidl_status;
}
::ndk::ScopedAStatus FakeRunner::doneWithPacket(int32_t /*in_bufferId*/, int32_t /*in_streamId*/) {
    ::ndk::ScopedAStatus _aidl_status;
    _aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));
    return _aidl_status;
}
::ndk::ScopedAStatus FakeRunner::getPipeDebugger(
    std::shared_ptr<::aidl::android::automotive::computepipe::runner::IPipeDebugger>* /*_aidl_return*/) {
    ::ndk::ScopedAStatus _aidl_status;
    _aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));
    return _aidl_status;
}
::ndk::ScopedAStatus FakeRunner::releaseRunner() {
    ::ndk::ScopedAStatus _aidl_status;
    _aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));
    return _aidl_status;
}
}  // namespace tests
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
