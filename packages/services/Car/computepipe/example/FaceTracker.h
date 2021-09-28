// Copyright 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef COMPUTEPIPE_EXAMPLE_FACE_TRACKER_H
#define COMPUTEPIPE_EXAMPLE_FACE_TRACKER_H

#include <aidl/android/automotive/computepipe/registry/BnClientInfo.h>
#include <aidl/android/automotive/computepipe/registry/IPipeQuery.h>
#include <aidl/android/automotive/computepipe/registry/IPipeRegistration.h>
#include <aidl/android/automotive/computepipe/runner/BnPipeStateCallback.h>
#include <aidl/android/automotive/computepipe/runner/BnPipeStream.h>
#include <aidl/android/automotive/computepipe/runner/PipeState.h>

#include <condition_variable>
#include <iostream>
#include <memory>

#include "FaceOutput.pb.h"

namespace android {
namespace automotive {
namespace computepipe {

using ::aidl::android::automotive::computepipe::registry::BnClientInfo;
using ::aidl::android::automotive::computepipe::registry::IPipeQuery;
using ::aidl::android::automotive::computepipe::runner::BnPipeStateCallback;
using ::aidl::android::automotive::computepipe::runner::BnPipeStream;
using ::aidl::android::automotive::computepipe::runner::IPipeRunner;
using ::aidl::android::automotive::computepipe::runner::IPipeStream;
using ::aidl::android::automotive::computepipe::runner::PacketDescriptor;
using ::aidl::android::automotive::computepipe::runner::PipeState;
using ::android::automotive::computepipe::example::BoundingBox;

class RemoteState {
  public:
    explicit RemoteState(std::function<void(bool, std::string)>& cb);
    PipeState GetCurrentState();
    void UpdateCurrentState(const PipeState& state);

  private:
    bool hasChanged = false;
    PipeState mState = PipeState::RESET;
    std::mutex mStateLock;
    std::condition_variable mWait;
    std::function<void(bool, std::string)> mTerminationCb;
};

class ClientInfo : public BnClientInfo {
  public:
    ndk::ScopedAStatus getClientName(std::string* _aidl_return) override {
        if (_aidl_return) {
            *_aidl_return = "FaceTrackerClient";
            return ndk::ScopedAStatus::ok();
        }
        return ndk::ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED);
    }
};

class StreamCallback : public BnPipeStream {
  public:
    StreamCallback() = default;
    ndk::ScopedAStatus deliverPacket(const PacketDescriptor& in_packet) override;

  private:
    BoundingBox mLastBox;
};

class StateCallback : public BnPipeStateCallback {
  public:
    explicit StateCallback(std::shared_ptr<RemoteState> s);
    ndk::ScopedAStatus handleState(PipeState state) override;

  private:
    std::shared_ptr<RemoteState> mStateTracker = nullptr;
};

class FaceTracker {
  public:
    FaceTracker() = default;
    ndk::ScopedAStatus init(std::function<void(bool, std::string)>&& termination);
    void start();
    void stop();

  private:
    ndk::ScopedAStatus setupConfig();
    std::shared_ptr<IPipeRunner> mPipeRunner = nullptr;
    std::shared_ptr<ClientInfo> mClientInfo = nullptr;
    std::shared_ptr<StreamCallback> mStreamCallback = nullptr;
    std::shared_ptr<StateCallback> mStateCallback = nullptr;
    std::shared_ptr<RemoteState> mRemoteState = nullptr;
};

}  // namespace computepipe
}  // namespace automotive
}  // namespace android
#endif
