// Copyright (C) 2019 The Android Open Source Project
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

#include <string>

#include "AidlClient.h"

#include <aidl/android/automotive/computepipe/registry/IPipeRegistration.h>
#include <android/binder_manager.h>
#include <android-base/logging.h>

#include <thread>

#include "types/GraphState.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace client_interface {
namespace aidl_client {

using ::aidl::android::automotive::computepipe::registry::IPipeRegistration;
using ::ndk::ScopedAStatus;

namespace {
constexpr char kRegistryInterfaceName[] = "router";
constexpr int kMaxRouterConnectionAttempts = 10;
constexpr int kRouterConnectionAttemptIntervalSeconds = 2;

void deathNotifier(void* cookie) {
    CHECK(cookie);
    AidlClient* runnerIface = static_cast<AidlClient*>(cookie);
    runnerIface->routerDied();
}

}  // namespace

Status AidlClient::dispatchPacketToClient(int32_t streamId,
                                          const std::shared_ptr<MemHandle> packet) {
    if (!mPipeRunner) {
        return Status::ILLEGAL_STATE;
    }
    return mPipeRunner->dispatchPacketToClient(streamId, packet);
}

Status AidlClient::activate() {
    if (mPipeRunner) {
        return Status::ILLEGAL_STATE;
    }

    mPipeRunner = ndk::SharedRefBase::make<AidlClientImpl>(mGraphOptions, mRunnerEngine);
    mPipeDebugger = ndk::SharedRefBase::make<DebuggerImpl>(mGraphOptions, mRunnerEngine);
    mPipeRunner->setPipeDebugger(mPipeDebugger);

    std::thread t(&AidlClient::tryRegisterPipeRunner, this);
    t.detach();
    return Status::SUCCESS;
}

Status AidlClient::deliverGraphDebugInfo(const std::string& debugData) {
    if (mPipeDebugger) {
        return mPipeDebugger->deliverGraphDebugInfo(debugData);
    }
    return Status::SUCCESS;
}

void AidlClient::routerDied() {
    std::thread t(&AidlClient::tryRegisterPipeRunner, this);
    t.detach();
}

void AidlClient::tryRegisterPipeRunner() {
    if (!mPipeRunner) {
        LOG(ERROR) << "Init must be called before attempting to connect to router.";
        return;
    }

    const std::string instanceName =
        std::string() + IPipeRegistration::descriptor + "/" + kRegistryInterfaceName;

    for (int i =0; i < kMaxRouterConnectionAttempts; i++) {
        if (i != 0) {
            sleep(kRouterConnectionAttemptIntervalSeconds);
        }

        ndk::SpAIBinder binder(AServiceManager_getService(instanceName.c_str()));
        if (binder.get() == nullptr) {
            LOG(ERROR) << "Failed to connect to router service";
            continue;
        }

        // Connected to router registry, register the runner and dealth callback.
        std::shared_ptr<IPipeRegistration> registryService = IPipeRegistration::fromBinder(binder);
        ndk::ScopedAStatus status =
            registryService->registerPipeRunner(mGraphOptions.graph_name().c_str(), mPipeRunner);

        if (!status.isOk()) {
            LOG(ERROR) << "Failed to register runner instance at router registy.";
            continue;
        }

        AIBinder_DeathRecipient* recipient = AIBinder_DeathRecipient_new(&deathNotifier);
        AIBinder_linkToDeath(registryService->asBinder().get(), recipient, this);
        LOG(ERROR) << "Runner was registered at router registry.";
        return;
    }

    LOG(ERROR) << "Max connection attempts reached, router connection attempts failed.";
    return;
}

Status AidlClient::handleResetPhase(const RunnerEvent& e) {
    if (!mPipeRunner) {
        return Status::ILLEGAL_STATE;
    }
    if (e.isTransitionComplete()) {
        mPipeRunner->stateUpdateNotification(GraphState::RESET);
    }

    if (mPipeDebugger) {
        mPipeDebugger->handleResetPhase(e);
    }
    return SUCCESS;
}

Status AidlClient::handleConfigPhase(const ClientConfig& e) {
    if (!mPipeRunner) {
        return Status::ILLEGAL_STATE;
    }
    if (e.isTransitionComplete()) {
        mPipeRunner->stateUpdateNotification(GraphState::CONFIG_DONE);
    } else if (e.isAborted()) {
        mPipeRunner->stateUpdateNotification(GraphState::ERR_HALT);
    }
    if (mPipeDebugger) {
        mPipeDebugger->handleConfigPhase(e);
    }

    return SUCCESS;
}

Status AidlClient::handleExecutionPhase(const RunnerEvent& e) {
    if (!mPipeRunner) {
        return Status::ILLEGAL_STATE;
    }
    if (e.isTransitionComplete()) {
        mPipeRunner->stateUpdateNotification(GraphState::RUNNING);
    } else if (e.isAborted()) {
        mPipeRunner->stateUpdateNotification(GraphState::ERR_HALT);
    }
    if (mPipeDebugger) {
        mPipeDebugger->handleExecutionPhase(e);
    }
    return SUCCESS;
}

Status AidlClient::handleStopWithFlushPhase(const RunnerEvent& e) {
    if (!mPipeRunner) {
        return Status::ILLEGAL_STATE;
    }
    if (e.isTransitionComplete()) {
        mPipeRunner->stateUpdateNotification(GraphState::DONE);
    }
    if (mPipeDebugger) {
        mPipeDebugger->handleStopWithFlushPhase(e);
    }
    return SUCCESS;
}

Status AidlClient::handleStopImmediatePhase(const RunnerEvent& e) {
    if (!mPipeRunner) {
        return Status::ILLEGAL_STATE;
    }
    if (e.isTransitionComplete()) {
        mPipeRunner->stateUpdateNotification(GraphState::ERR_HALT);
    }
    if (mPipeDebugger) {
        mPipeDebugger->handleStopImmediatePhase(e);
    }
    return SUCCESS;
}

}  // namespace aidl_client
}  // namespace client_interface
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
