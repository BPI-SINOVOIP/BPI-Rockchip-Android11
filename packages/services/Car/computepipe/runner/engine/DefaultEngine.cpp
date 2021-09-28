// Copyright (C) 2020 The Android Open Source Project
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

#include "DefaultEngine.h"

#include <android-base/logging.h>

#include <algorithm>
#include <cassert>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "ClientInterface.h"
#include "EventGenerator.h"
#include "EvsDisplayManager.h"
#include "InputFrame.h"
#include "PrebuiltGraph.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace engine {

using android::automotive::computepipe::graph::PrebuiltGraph;
using android::automotive::computepipe::runner::client_interface::ClientInterface;
using android::automotive::computepipe::runner::generator::DefaultEvent;
using android::automotive::computepipe::runner::input_manager::InputEngineInterface;
using android::automotive::computepipe::runner::stream_manager::StreamEngineInterface;
using android::automotive::computepipe::runner::stream_manager::StreamManager;

namespace {

int getStreamIdFromSource(std::string source) {
    auto pos = source.find(":");
    return std::stoi(source.substr(pos + 1));
}
}  // namespace

void DefaultEngine::setClientInterface(std::unique_ptr<ClientInterface>&& client) {
    mClient = std::move(client);
}

void DefaultEngine::setPrebuiltGraph(std::unique_ptr<PrebuiltGraph>&& graph) {
    mGraph = std::move(graph);
    mGraphDescriptor = mGraph->GetSupportedGraphConfigs();
    if (mGraph->GetGraphType() == graph::PrebuiltGraphType::REMOTE ||
        mGraphDescriptor.input_configs_size() == 0) {
        mIgnoreInputManager = true;
    }
}

Status DefaultEngine::setArgs(std::string engine_args) {
    auto pos = engine_args.find(kNoInputManager);
    if (pos != std::string::npos) {
        mIgnoreInputManager = true;
    }
    pos = engine_args.find(kDisplayStreamId);
    if (pos == std::string::npos) {
        return Status::SUCCESS;
    }
    mDisplayStream = std::stoi(engine_args.substr(pos + strlen(kDisplayStreamId)));
    mConfigBuilder.setDebugDisplayStream(mDisplayStream);
    mDebugDisplayManager = std::make_unique<debug_display_manager::EvsDisplayManager>();
    mDebugDisplayManager->setArgs(engine_args);
    return Status::SUCCESS;
}

Status DefaultEngine::activate() {
    mConfigBuilder.reset();
    mEngineThread = std::make_unique<std::thread>(&DefaultEngine::processCommands, this);
    return mClient->activate();
}

Status DefaultEngine::processClientConfigUpdate(const proto::ConfigurationCommand& command) {
    // TODO check current phase
    std::lock_guard<std::mutex> lock(mEngineLock);
    if (mCurrentPhase != kResetPhase) {
        return Status::ILLEGAL_STATE;
    }
    if (command.has_set_input_source()) {
        mConfigBuilder =
            mConfigBuilder.updateInputConfigOption(command.set_input_source().source_id());
    } else if (command.has_set_termination_option()) {
        mConfigBuilder = mConfigBuilder.updateTerminationOption(
            command.set_termination_option().termination_option_id());
    } else if (command.has_set_output_stream()) {
        mConfigBuilder = mConfigBuilder.updateOutputStreamOption(
            command.set_output_stream().stream_id(),
            command.set_output_stream().max_inflight_packets_count());
    } else if (command.has_set_offload_offload()) {
        mConfigBuilder =
            mConfigBuilder.updateOffloadOption(command.set_offload_offload().offload_option_id());
    } else if (command.has_set_profile_options()) {
        mConfigBuilder =
            mConfigBuilder.updateProfilingType(command.set_profile_options().profile_type());
    } else {
        return SUCCESS;
    }
    return Status::SUCCESS;
}

Status DefaultEngine::processClientCommand(const proto::ControlCommand& command) {
    // TODO check current phase
    std::lock_guard<std::mutex> lock(mEngineLock);

    if (command.has_apply_configs()) {
        if (mCurrentPhase != kResetPhase) {
            return Status::ILLEGAL_STATE;
        }
        queueCommand("ClientInterface", EngineCommand::Type::BROADCAST_CONFIG);
        return Status::SUCCESS;
    }
    if (command.has_start_graph()) {
        if (mCurrentPhase != kConfigPhase) {
            return Status::ILLEGAL_STATE;
        }
        queueCommand("ClientInterface", EngineCommand::Type::BROADCAST_START_RUN);
        return Status::SUCCESS;
    }
    if (command.has_stop_graph()) {
        if (mCurrentPhase != kRunPhase) {
            return Status::ILLEGAL_STATE;
        }
        mStopFromClient = true;
        queueCommand("ClientInterface", EngineCommand::Type::BROADCAST_INITIATE_STOP);
        return Status::SUCCESS;
    }
    if (command.has_death_notification()) {
        if (mCurrentPhase == kResetPhase) {
            /**
             * The runner is already in reset state, no need to broadcast client death
             * to components
             */
            LOG(INFO) << "client death notification with no configuration";
            return Status::SUCCESS;
        }
        mCurrentPhaseError = std::make_unique<ComponentError>("ClientInterface", "Client death",
                                                              mCurrentPhase, false);
        mWakeLooper.notify_all();
        return Status::SUCCESS;
    }
    if (command.has_reset_configs()) {
        if (mCurrentPhase != kConfigPhase) {
            return Status::ILLEGAL_STATE;
        }
        queueCommand("ClientInterface", EngineCommand::Type::RESET_CONFIG);
        return Status::SUCCESS;
    }
    if (command.has_start_pipe_profile()) {
        if (mCurrentPhase != kRunPhase) {
            return Status::ILLEGAL_STATE;
        }
        if (mGraph) {
            return mGraph->StartGraphProfiling();
        }
        return Status::SUCCESS;
    }
    if (command.has_stop_pipe_profile()) {
        if (mCurrentPhase != kRunPhase) {
            return Status::SUCCESS;
        }
        if (mGraph) {
            return mGraph->StopGraphProfiling();
        }
        return Status::SUCCESS;
    }
    if (command.has_release_debugger()) {
        if (mCurrentPhase != kConfigPhase && mCurrentPhase != kResetPhase) {
            return Status::ILLEGAL_STATE;
        }
        queueCommand("ClientInterface", EngineCommand::Type::RELEASE_DEBUGGER);
    }
    if (command.has_read_debug_data()) {
        queueCommand("ClientInterface", EngineCommand::Type::READ_PROFILING);
        return Status::SUCCESS;
    }
    return Status::SUCCESS;
}

Status DefaultEngine::freePacket(int bufferId, int streamId) {
    if (mStreamManagers.find(streamId) == mStreamManagers.end()) {
        LOG(ERROR)
            << "Unable to find the stream manager corresponding to the id for freeing the packet.";
        return Status::INVALID_ARGUMENT;
    }
    return mStreamManagers[streamId]->freePacket(bufferId);
}

/**
 * Methods from PrebuiltEngineInterface
 */
void DefaultEngine::DispatchPixelData(int streamId, int64_t timestamp, const InputFrame& frame) {
    LOG(DEBUG) << "Engine::Received data for pixel stream  " << streamId << " with timestamp "
              << timestamp;
    if (mStreamManagers.find(streamId) == mStreamManagers.end()) {
        LOG(ERROR) << "Engine::Received bad stream id from prebuilt graph";
        return;
    }
    mStreamManagers[streamId]->queuePacket(frame, timestamp);
}

void DefaultEngine::DispatchSerializedData(int streamId, int64_t timestamp, std::string&& output) {
    LOG(DEBUG) << "Engine::Received data for stream  " << streamId << " with timestamp "
            << timestamp;
    if (mStreamManagers.find(streamId) == mStreamManagers.end()) {
        LOG(ERROR) << "Engine::Received bad stream id from prebuilt graph";
        return;
    }
    std::string data(output);
    mStreamManagers[streamId]->queuePacket(data.c_str(), data.size(), timestamp);
}

void DefaultEngine::DispatchGraphTerminationMessage(Status s, std::string&& msg) {
    std::lock_guard<std::mutex> lock(mEngineLock);
    if (s == SUCCESS) {
        if (mCurrentPhase == kRunPhase) {
            queueCommand("PrebuiltGraph", EngineCommand::Type::BROADCAST_INITIATE_STOP);
        } else {
            LOG(WARNING) << "Graph termination when not in run phase";
        }
    } else {
        std::string error = msg;
        queueError("PrebuiltGraph", error, false);
    }
}

Status DefaultEngine::broadcastClientConfig() {
    ClientConfig config = mConfigBuilder.emitClientOptions();

    LOG(INFO) << "Engine::create stream manager";
    Status ret = populateStreamManagers(config);
    if (ret != Status::SUCCESS) {
        return ret;
    }

    if (mGraph) {
        ret = populateInputManagers(config);
        if (ret != Status::SUCCESS) {
            abortClientConfig(config);
            return ret;
        }

        LOG(INFO) << "Engine::send client config entry to graph";
        config.setPhaseState(PhaseState::ENTRY);
        ret = mGraph->handleConfigPhase(config);
        if (ret != Status::SUCCESS) {
            abortClientConfig(config);
            return ret;
        }
        LOG(INFO) << "Engine::send client config transition complete to graph";
        config.setPhaseState(PhaseState::TRANSITION_COMPLETE);
        ret = mGraph->handleConfigPhase(config);
        if (ret != Status::SUCCESS) {
            abortClientConfig(config);
            return ret;
        }
    }
    LOG(INFO) << "Engine::Graph configured";
    // TODO add handling for remote graph
    if (mDebugDisplayManager) {
        mDebugDisplayManager->setFreePacketCallback(std::bind(
                &DefaultEngine::freePacket, this, std::placeholders::_1, mDisplayStream));

        ret = mDebugDisplayManager->handleConfigPhase(config);
        if (ret != Status::SUCCESS) {
            config.setPhaseState(PhaseState::ABORTED);
            abortClientConfig(config, true);
            return ret;
        }
    }

    ret = mClient->handleConfigPhase(config);
    if (ret != Status::SUCCESS) {
        config.setPhaseState(PhaseState::ABORTED);
        abortClientConfig(config, true);
        return ret;
    }

    mCurrentPhase = kConfigPhase;
    return Status::SUCCESS;
}

void DefaultEngine::abortClientConfig(const ClientConfig& config, bool resetGraph) {
    mStreamManagers.clear();
    mInputManagers.clear();
    if (resetGraph && mGraph) {
        (void)mGraph->handleConfigPhase(config);
    }
    (void)mClient->handleConfigPhase(config);
    // TODO add handling for remote graph
}

Status DefaultEngine::broadcastStartRun() {
    DefaultEvent runEvent = DefaultEvent::generateEntryEvent(DefaultEvent::RUN);

    std::vector<int> successfulStreams;
    std::vector<int> successfulInputs;
    for (auto& it : mStreamManagers) {
        if (it.second->handleExecutionPhase(runEvent) != Status::SUCCESS) {
            LOG(ERROR) << "Engine::failure to enter run phase for stream " << it.first;
            broadcastAbortRun(successfulStreams, successfulInputs);
            return Status::INTERNAL_ERROR;
        }
        successfulStreams.push_back(it.first);
    }
    // TODO: send to remote
    if (mDebugDisplayManager) {
        (void)mDebugDisplayManager->handleExecutionPhase(runEvent);
    }

    Status ret;
    if (mGraph) {
        LOG(INFO) << "Engine::sending start run to prebuilt";
        ret = mGraph->handleExecutionPhase(runEvent);
        if (ret != Status::SUCCESS) {
            broadcastAbortRun(successfulStreams, successfulInputs);
        }
        for (auto& it : mInputManagers) {
            if (it.second->handleExecutionPhase(runEvent) != Status::SUCCESS) {
                LOG(ERROR) << "Engine::failure to enter run phase for input manager " << it.first;
                broadcastAbortRun(successfulStreams, successfulInputs, true);
                return Status::INTERNAL_ERROR;
            }
            successfulInputs.push_back(it.first);
        }
    }

    runEvent = DefaultEvent::generateTransitionCompleteEvent(DefaultEvent::RUN);
    LOG(INFO) << "Engine::sending run transition complete to client";
    ret = mClient->handleExecutionPhase(runEvent);
    if (ret != Status::SUCCESS) {
        LOG(ERROR) << "Engine::client failure to acknowledge transition to run complete ";
        broadcastAbortRun(successfulStreams, successfulInputs, true);
        return ret;
    }
    for (auto& it : mStreamManagers) {
        (void)it.second->handleExecutionPhase(runEvent);
    }
    // TODO: send to remote
    if (mDebugDisplayManager) {
        (void)mDebugDisplayManager->handleExecutionPhase(runEvent);
    }

    if (mGraph) {
        LOG(INFO) << "Engine::sending run transition complete to prebuilt";
        (void)mGraph->handleExecutionPhase(runEvent);
        for (auto& it : mInputManagers) {
            (void)it.second->handleExecutionPhase(runEvent);
        }
    }

    LOG(INFO) << "Engine::Running";
    mCurrentPhase = kRunPhase;
    return Status::SUCCESS;
}

void DefaultEngine::broadcastAbortRun(const std::vector<int>& streamIds,
                                      const std::vector<int>& inputIds, bool abortGraph) {
    DefaultEvent runEvent = DefaultEvent::generateAbortEvent(DefaultEvent::RUN);
    if (mDebugDisplayManager) {
        (void)mDebugDisplayManager->handleExecutionPhase(runEvent);
    }
    std::for_each(streamIds.begin(), streamIds.end(), [this, runEvent](int id) {
        (void)this->mStreamManagers[id]->handleExecutionPhase(runEvent);
    });
    std::for_each(inputIds.begin(), inputIds.end(), [this, runEvent](int id) {
        (void)this->mInputManagers[id]->handleExecutionPhase(runEvent);
    });
    if (abortGraph) {
        if (mGraph) {
            (void)mGraph->handleExecutionPhase(runEvent);
        }
    }
    (void)mClient->handleExecutionPhase(runEvent);
}

Status DefaultEngine::broadcastStopWithFlush() {
    DefaultEvent runEvent = DefaultEvent::generateEntryEvent(DefaultEvent::STOP_WITH_FLUSH);
    if (mDebugDisplayManager) {
        (void)mDebugDisplayManager->handleStopWithFlushPhase(runEvent);
    }

    if (mGraph) {
        for (auto& it : mInputManagers) {
            (void)it.second->handleStopWithFlushPhase(runEvent);
        }
        if (mStopFromClient) {
            (void)mGraph->handleStopWithFlushPhase(runEvent);
        }
    }
    // TODO: send to remote.
    for (auto& it : mStreamManagers) {
        (void)it.second->handleStopWithFlushPhase(runEvent);
    }
    if (!mStopFromClient) {
        (void)mClient->handleStopWithFlushPhase(runEvent);
    }
    mCurrentPhase = kStopPhase;
    return Status::SUCCESS;
}

Status DefaultEngine::broadcastStopComplete() {
    DefaultEvent runEvent =
        DefaultEvent::generateTransitionCompleteEvent(DefaultEvent::STOP_WITH_FLUSH);
    if (mGraph) {
        for (auto& it : mInputManagers) {
            (void)it.second->handleStopWithFlushPhase(runEvent);
        }
        (void)mGraph->handleStopWithFlushPhase(runEvent);
    }
    if (mDebugDisplayManager) {
        (void)mDebugDisplayManager->handleStopWithFlushPhase(runEvent);
    }
    // TODO: send to remote.
    for (auto& it : mStreamManagers) {
        (void)it.second->handleStopWithFlushPhase(runEvent);
    }
    (void)mClient->handleStopWithFlushPhase(runEvent);
    mCurrentPhase = kConfigPhase;
    return Status::SUCCESS;
}

void DefaultEngine::broadcastHalt() {
    DefaultEvent stopEvent = DefaultEvent::generateEntryEvent(DefaultEvent::STOP_IMMEDIATE);

    if (mGraph) {
        for (auto& it : mInputManagers) {
            (void)it.second->handleStopImmediatePhase(stopEvent);
        }

        if ((mCurrentPhaseError->source.find("PrebuiltGraph") == std::string::npos)) {
            (void)mGraph->handleStopImmediatePhase(stopEvent);
        }
    }
    if (mDebugDisplayManager) {
        (void)mDebugDisplayManager->handleStopImmediatePhase(stopEvent);
    }
    // TODO: send to remote if client was source.
    for (auto& it : mStreamManagers) {
        (void)it.second->handleStopImmediatePhase(stopEvent);
    }
    if (mCurrentPhaseError->source.find("ClientInterface") == std::string::npos) {
        (void)mClient->handleStopImmediatePhase(stopEvent);
    }

    stopEvent = DefaultEvent::generateTransitionCompleteEvent(DefaultEvent::STOP_IMMEDIATE);
    if (mGraph) {
        for (auto& it : mInputManagers) {
            (void)it.second->handleStopImmediatePhase(stopEvent);
        }
        // TODO: send to graph or remote if client was source.

        if ((mCurrentPhaseError->source.find("PrebuiltGraph") == std::string::npos) && mGraph) {
            (void)mGraph->handleStopImmediatePhase(stopEvent);
        }
    }
    if (mDebugDisplayManager) {
        (void)mDebugDisplayManager->handleStopImmediatePhase(stopEvent);
    }
    for (auto& it : mStreamManagers) {
        (void)it.second->handleStopImmediatePhase(stopEvent);
    }
    if (mCurrentPhaseError->source.find("ClientInterface") == std::string::npos) {
        (void)mClient->handleStopImmediatePhase(stopEvent);
    }
    mCurrentPhase = kConfigPhase;
}

void DefaultEngine::broadcastReset() {
    mStreamManagers.clear();
    mInputManagers.clear();
    DefaultEvent resetEvent = DefaultEvent::generateEntryEvent(DefaultEvent::RESET);
    (void)mClient->handleResetPhase(resetEvent);
    if (mGraph) {
        (void)mGraph->handleResetPhase(resetEvent);
    }
    resetEvent = DefaultEvent::generateTransitionCompleteEvent(DefaultEvent::RESET);
    (void)mClient->handleResetPhase(resetEvent);
    if (mGraph) {
        (void)mGraph->handleResetPhase(resetEvent);
    }
    if (mDebugDisplayManager) {
        (void)mDebugDisplayManager->handleResetPhase(resetEvent);
    }
    // TODO: send to remote runner
    mConfigBuilder.reset();
    mCurrentPhase = kResetPhase;
    mStopFromClient = false;
}

Status DefaultEngine::populateStreamManagers(const ClientConfig& config) {
    std::map<int, int> outputConfigs;
    if (config.getOutputStreamConfigs(outputConfigs) != Status::SUCCESS) {
        return Status::ILLEGAL_STATE;
    }
    for (auto& configIt : outputConfigs) {
        int streamId = configIt.first;
        int maxInFlightPackets = configIt.second;
        proto::OutputConfig outputDescriptor;
        // find the output descriptor for requested stream id
        bool foundDesc = false;
        for (auto& optionIt : mGraphDescriptor.output_configs()) {
            if (optionIt.stream_id() == streamId) {
                outputDescriptor = optionIt;
                foundDesc = true;
                break;
            }
        }
        if (!foundDesc) {
            LOG(ERROR) << "no matching output config for requested id " << streamId;
            return Status::INVALID_ARGUMENT;
        }
        std::function<Status(std::shared_ptr<MemHandle>)> packetCb =
            [this, streamId](std::shared_ptr<MemHandle> handle) -> Status {
            return this->forwardOutputDataToClient(streamId, handle);
        };

        std::function<void(std::string)> errorCb = [this, streamId](std::string m) {
            std::string source = "StreamManager:" + std::to_string(streamId) + " : " + m;
            this->queueError(source, m, false);
        };

        std::function<void()> eos = [this, streamId]() {
            std::string source = "StreamManager:" + std::to_string(streamId);
            std::lock_guard<std::mutex> lock(this->mEngineLock);
            this->queueCommand(source, EngineCommand::Type::POLL_COMPLETE);
        };

        std::shared_ptr<StreamEngineInterface> engine = std::make_shared<StreamCallback>(
            std::move(eos), std::move(errorCb), std::move(packetCb));
        mStreamManagers.emplace(configIt.first, mStreamFactory.getStreamManager(
                                                    outputDescriptor, engine, maxInFlightPackets));
        if (mStreamManagers[streamId] == nullptr) {
            LOG(ERROR) << "unable to create stream manager for stream " << streamId;
            return Status::INTERNAL_ERROR;
        }
    }
    return Status::SUCCESS;
}

Status DefaultEngine::forwardOutputDataToClient(int streamId,
                                                std::shared_ptr<MemHandle>& dataHandle) {
    if (streamId != mDisplayStream) {
        return mClient->dispatchPacketToClient(streamId, dataHandle);
    }

    auto displayMgrPacket = dataHandle;
    if (mConfigBuilder.clientConfigEnablesDisplayStream()) {
        if (mStreamManagers.find(streamId) == mStreamManagers.end()) {
            displayMgrPacket = nullptr;
        } else {
            displayMgrPacket = mStreamManagers[streamId]->clonePacket(dataHandle);
        }
        Status status = mClient->dispatchPacketToClient(streamId, dataHandle);
        if (status != Status::SUCCESS) {
            return status;
        }
    }
    CHECK(mDebugDisplayManager);
    return mDebugDisplayManager->displayFrame(dataHandle);
}

Status DefaultEngine::populateInputManagers(const ClientConfig& config) {
    if (mIgnoreInputManager) {
        return Status::SUCCESS;
    }

    proto::InputConfig inputDescriptor;
    int selectedId;

    if (config.getInputConfigId(&selectedId) != Status::SUCCESS) {
        return Status::INVALID_ARGUMENT;
    }

    for (auto& inputIt : mGraphDescriptor.input_configs()) {
        if (selectedId == inputIt.config_id()) {
            inputDescriptor = inputIt;
            std::shared_ptr<InputCallback> cb = std::make_shared<InputCallback>(
                selectedId,
                [this](int id) {
                    std::string source = "InputManager:" + std::to_string(id);
                    this->queueError(source, "", false);
                },
                [this](int streamId, int64_t timestamp, const InputFrame& frame) {
                    return this->mGraph->SetInputStreamPixelData(streamId, timestamp, frame);
                });
            mInputManagers.emplace(selectedId,
                                   mInputFactory.createInputManager(inputDescriptor, cb));
            if (mInputManagers[selectedId] == nullptr) {
                LOG(ERROR) << "unable to create input manager for stream " << selectedId;
                // TODO: Add print
                return Status::INTERNAL_ERROR;
            }
            return Status::SUCCESS;
        }
    }
    return Status::INVALID_ARGUMENT;
}

/**
 * Engine Command Queue and Error Queue handling
 */
void DefaultEngine::processCommands() {
    std::unique_lock<std::mutex> lock(mEngineLock);
    while (1) {
        LOG(INFO) << "Engine::Waiting on commands ";
        mWakeLooper.wait(lock, [this] {
            if (this->mCommandQueue.empty() && !mCurrentPhaseError) {
                return false;
            } else {
                return true;
            }
        });
        if (mCurrentPhaseError) {
            mErrorQueue.push(*mCurrentPhaseError);

            processComponentError(mCurrentPhaseError->source);
            mCurrentPhaseError = nullptr;
            std::queue<EngineCommand> empty;
            std::swap(mCommandQueue, empty);
            continue;
        }
        EngineCommand ec = mCommandQueue.front();
        mCommandQueue.pop();
        switch (ec.cmdType) {
            case EngineCommand::Type::BROADCAST_CONFIG:
                LOG(INFO) << "Engine::Received broadcast config request";
                (void)broadcastClientConfig();
                break;
            case EngineCommand::Type::BROADCAST_START_RUN:
                LOG(INFO) << "Engine::Received broadcast run request";
                (void)broadcastStartRun();
                break;
            case EngineCommand::Type::BROADCAST_INITIATE_STOP:
                if (ec.source.find("ClientInterface") != std::string::npos) {
                    mStopFromClient = true;
                }
                LOG(INFO) << "Engine::Received broadcast stop with flush request";
                broadcastStopWithFlush();
                break;
            case EngineCommand::Type::POLL_COMPLETE:
                LOG(INFO) << "Engine::Received Poll stream managers for completion request";
                {
                    int id = getStreamIdFromSource(ec.source);
                    bool all_done = true;
                    for (auto& it : mStreamManagers) {
                        if (it.first == id) {
                            continue;
                        }
                        if (it.second->getState() != StreamManager::State::STOPPED) {
                            all_done = false;
                        }
                    }
                    if (all_done) {
                        broadcastStopComplete();
                    }
                }
                break;
            case EngineCommand::Type::RESET_CONFIG:
                (void)broadcastReset();
                break;
            case EngineCommand::Type::RELEASE_DEBUGGER:
                {
                    // broadcastReset() resets the previous copy, so save a copy of the old config.
                    ConfigBuilder previousConfig = mConfigBuilder;
                    (void)broadcastReset();
                    mConfigBuilder =
                            previousConfig.updateProfilingType(proto::ProfilingType::DISABLED);
                    (void)broadcastClientConfig();
                }
                break;
            case EngineCommand::Type::READ_PROFILING:
                std::string debugData;
                if (mGraph && (mCurrentPhase == kConfigPhase || mCurrentPhase == kRunPhase
                                || mCurrentPhase == kStopPhase)) {
                    debugData = mGraph->GetDebugInfo();
                }
                if (mClient) {
                    Status status = mClient->deliverGraphDebugInfo(debugData);
                    if (status != Status::SUCCESS) {
                        LOG(ERROR) << "Failed to deliver graph debug info to client.";
                    }
                }
                break;
        }
    }
}

void DefaultEngine::processComponentError(std::string source) {
    if (mCurrentPhase == kRunPhase || mCurrentPhase == kStopPhase) {
        (void)broadcastHalt();
    }
    if (source.find("ClientInterface") != std::string::npos) {
        (void)broadcastReset();
    }
}

void DefaultEngine::queueCommand(std::string source, EngineCommand::Type type) {
    mCommandQueue.push(EngineCommand(source, type));
    mWakeLooper.notify_all();
}

void DefaultEngine::queueError(std::string source, std::string msg, bool fatal) {
    std::lock_guard<std::mutex> lock(mEngineLock);
    // current phase already has an error report
    if (!mCurrentPhaseError) {
        mCurrentPhaseError = std::make_unique<ComponentError>(source, msg, mCurrentPhase, fatal);
        mWakeLooper.notify_all();
    }
}

/**
 * InputCallback implementation
 */
InputCallback::InputCallback(
    int id, const std::function<void(int)>&& cb,
    const std::function<Status(int, int64_t timestamp, const InputFrame&)>&& packetCb)
    : mErrorCallback(cb), mPacketHandler(packetCb), mInputId(id) {
}

Status InputCallback::dispatchInputFrame(int streamId, int64_t timestamp, const InputFrame& frame) {
    return mPacketHandler(streamId, timestamp, frame);
}

void InputCallback::notifyInputError() {
    mErrorCallback(mInputId);
}

/**
 * StreamCallback implementation
 */
StreamCallback::StreamCallback(
    const std::function<void()>&& eos, const std::function<void(std::string)>&& errorCb,
    const std::function<Status(const std::shared_ptr<MemHandle>&)>&& packetHandler)
    : mErrorHandler(errorCb), mEndOfStreamHandler(eos), mPacketHandler(packetHandler) {
}

void StreamCallback::notifyError(std::string msg) {
    mErrorHandler(msg);
}

void StreamCallback::notifyEndOfStream() {
    mEndOfStreamHandler();
}

Status StreamCallback::dispatchPacket(const std::shared_ptr<MemHandle>& packet) {
    return mPacketHandler(packet);
}

}  // namespace engine
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
