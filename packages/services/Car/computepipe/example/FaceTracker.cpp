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

#include "FaceTracker.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <utils/Log.h>

#include <memory>
#include <mutex>
#include <thread>

namespace android {
namespace automotive {
namespace computepipe {

using ::android::automotive::computepipe::example::FaceOutput;
namespace {
constexpr char kReigstryInterface[] = "router";
constexpr char kGraphName[] = "Face Tracker Graph";
}  // namespace

/**
 * RemoteState monitor
 */
PipeState RemoteState::GetCurrentState() {
    std::unique_lock<std::mutex> lock(mStateLock);
    mWait.wait(lock, [this]() { return hasChanged; });
    hasChanged = false;
    return mState;
}

void RemoteState::UpdateCurrentState(const PipeState& state) {
    std::lock_guard<std::mutex> lock(mStateLock);
    mState = state;
    if (mState == PipeState::ERR_HALT) {
        mTerminationCb(true, "Received error from runner");
    } else if (mState == PipeState::DONE) {
        mTerminationCb(false, "");
    } else {
        hasChanged = true;
        mWait.notify_all();
    }
}

RemoteState::RemoteState(std::function<void(bool, std::string)>& cb) : mTerminationCb(cb) {
}

/**
 * StateCallback methods
 */
StateCallback::StateCallback(std::shared_ptr<RemoteState> s) : mStateTracker(s) {
}

ndk::ScopedAStatus StateCallback::handleState(PipeState state) {
    mStateTracker->UpdateCurrentState(state);
    return ndk::ScopedAStatus::ok();
}

/**
 * FaceTracker methods
 */
ndk::ScopedAStatus FaceTracker::init(std::function<void(bool, std::string)>&& cb) {
    auto termination = cb;
    mRemoteState = std::make_shared<RemoteState>(termination);
    std::string instanceName = std::string() + IPipeQuery::descriptor + "/" + kReigstryInterface;

    ndk::SpAIBinder binder(AServiceManager_getService(instanceName.c_str()));
    CHECK(binder.get());

    std::shared_ptr<IPipeQuery> queryService = IPipeQuery::fromBinder(binder);
    mClientInfo = ndk::SharedRefBase::make<ClientInfo>();
    ndk::ScopedAStatus status = queryService->getPipeRunner(kGraphName, mClientInfo, &mPipeRunner);
    if (!status.isOk()) {
        LOG(ERROR) << "Failed to get handle to runner";
        return status;
    }
    mStreamCallback = ndk::SharedRefBase::make<StreamCallback>();
    mStateCallback = ndk::SharedRefBase::make<StateCallback>(mRemoteState);
    return setupConfig();
}

ndk::ScopedAStatus FaceTracker::setupConfig() {
    ndk::ScopedAStatus status = mPipeRunner->init(mStateCallback);
    if (!status.isOk()) {
        LOG(ERROR) << "Failed to init runner";
        return status;
    }
    status = mPipeRunner->setPipeInputSource(0);
    if (!status.isOk()) {
        LOG(ERROR) << "Failed to set pipe input config";
        return status;
    }
    status = mPipeRunner->setPipeOutputConfig(0, 10, mStreamCallback);
    if (!status.isOk()) {
        LOG(ERROR) << "Failed to set pipe output config";
        return status;
    }
    status = mPipeRunner->applyPipeConfigs();
    if (!status.isOk()) {
        LOG(ERROR) << "Failed to set apply configs";
        return status;
    }
    std::thread t(&FaceTracker::start, this);
    t.detach();
    return ndk::ScopedAStatus::ok();
}

void FaceTracker::start() {
    PipeState state = mRemoteState->GetCurrentState();
    CHECK(state == PipeState::CONFIG_DONE);
    ndk::ScopedAStatus status = mPipeRunner->startPipe();
    CHECK(status.isOk());
    state = mRemoteState->GetCurrentState();
    CHECK(state == PipeState::RUNNING);
}

void FaceTracker::stop() {
    ndk::ScopedAStatus status = mPipeRunner->startPipe();
    CHECK(status.isOk());
}

/**
 * Stream Callback implementation
 */

ndk::ScopedAStatus StreamCallback::deliverPacket(const PacketDescriptor& in_packet) {
    std::string output(in_packet.data.begin(), in_packet.data.end());

    FaceOutput faceData;
    faceData.ParseFromString(output);

    BoundingBox currentBox = faceData.box();

    if (!faceData.has_box()) {
        mLastBox = BoundingBox();
        return ndk::ScopedAStatus::ok();
    }

    if (!mLastBox.has_top_x()) {
        mLastBox = currentBox;
        return ndk::ScopedAStatus::ok();
    }

    if (currentBox.top_x() > mLastBox.top_x() + 1) {
        LOG(ERROR) << "Face moving left";
    } else if (currentBox.top_x() + 1 < mLastBox.top_x()) {
        LOG(ERROR) << "Face moving right";
    }
    mLastBox = currentBox;
    return ndk::ScopedAStatus::ok();
}

}  // namespace computepipe
}  // namespace automotive
}  // namespace android
