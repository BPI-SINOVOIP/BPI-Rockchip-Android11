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

#ifndef COMPUTEPIPE_RUNNER_ENGINE_DEFAULTENGINE_H_
#define COMPUTEPIPE_RUNNER_ENGINE_DEFAULTENGINE_H_

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "ConfigBuilder.h"
#include "DebugDisplayManager.h"
#include "InputManager.h"
#include "Options.pb.h"
#include "RunnerEngine.h"
#include "StreamManager.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace engine {

class InputCallback;

/**
 * EngineCommand represents client requests or error events.
 * Each command is queued, and processed by the engine thread.
 */
struct EngineCommand {
  public:
    enum Type {
        BROADCAST_CONFIG = 0,
        BROADCAST_START_RUN,
        BROADCAST_INITIATE_STOP,
        POLL_COMPLETE,
        RESET_CONFIG,
        RELEASE_DEBUGGER,
        READ_PROFILING,
    };
    std::string source;
    Type cmdType;
    explicit EngineCommand(std::string s, Type t) : source(s), cmdType(t) {
    }
};

/**
 * Component Error represents the type of error reported by a component.
 */
struct ComponentError {
  public:
    bool isFatal;
    std::string source;
    std::string message;
    std::string currentPhase;
    explicit ComponentError(std::string s, std::string m, std::string p, bool fatal = false)
        : isFatal(fatal), source(s), message(m), currentPhase(p) {
    }
};

/**
 * Default Engine implementation.
 * Takes ownership of externally instantiated graph & client interface
 * instances. Brings the runner online. Manages components.
 * Responds to client events.
 */
class DefaultEngine : public RunnerEngine {
  public:
    static constexpr char kDisplayStreamId[] = "display_stream:";
    static constexpr char kNoInputManager[] = "no_input_manager";
    static constexpr char kResetPhase[] = "Reset";
    static constexpr char kConfigPhase[] = "Config";
    static constexpr char kRunPhase[] = "Running";
    static constexpr char kStopPhase[] = "Stopping";
    /**
     * Methods from Runner Engine to override
     */
    Status setArgs(std::string engine_args) override;
    void setClientInterface(std::unique_ptr<client_interface::ClientInterface>&& client) override;
    void setPrebuiltGraph(std::unique_ptr<graph::PrebuiltGraph>&& graph) override;
    Status activate() override;
    /**
     * Methods from ClientEngineInterface to override
     */
    Status processClientConfigUpdate(const proto::ConfigurationCommand& command) override;
    Status processClientCommand(const proto::ControlCommand& command) override;
    Status freePacket(int bufferId, int streamId) override;
    /**
     * Methods from PrebuiltEngineInterface to override
     */
    void DispatchPixelData(int streamId, int64_t timestamp, const InputFrame& frame) override;

    void DispatchSerializedData(int streamId, int64_t timestamp, std::string&& output) override;

    void DispatchGraphTerminationMessage(Status s, std::string&& msg) override;

  private:
    // TODO: b/147704051 Add thread analyzer annotations
    /**
     * BroadCast Client config to all components. If all components handle the
     * notification correctly, then broadcast transition complete.
     * Successful return from this function implies runner has transitioned to
     * configuration done.
     * @Lock held mEngineLock
     */
    Status broadcastClientConfig();
    /**
     * Abort an ongoing attempt to apply client configs.
     * @Lock held mEngineLock
     */
    void abortClientConfig(const ClientConfig& config, bool resetGraph = false);
    /**
     * BroadCast start to all components. The order of entry into run phase
     * notification delivery is downstream components to upstream components.
     * If all components handle the entry notification correctly then broadcast
     * transition complete notification again from down stream to upstream.
     * Successful return from this function implies runner has transitioned to
     * running.
     * @Lock held mEngineLock
     */
    Status broadcastStartRun();
    /**
     * BroadCast abort of started run to given components. This gets called if during
     * broadcastStartRun(), one of the components failed to set itself up for the run. In that case
     * the components that had successfully acknowledged broacastStartRun(),
     * need to be told to abort. We transition back to config phase at the end
     * of this call.
     * @Lock held mEngineLock
     */
    void broadcastAbortRun(const std::vector<int>& streamIds, const std::vector<int>& inputIds,
                           bool graph = false);

    /**
     * Broadcast stop with flush to all components. The stop with flush phase
     * entry notification is sent from in the order of upstream to downstream.
     * A successful return can leave the runner in stopping phase.
     * We transition to stop completely, once all inflight traffic has been drained at a later
     * point, identified by stream managers.
     * @Lock held mEngineLock
     */
    Status broadcastStopWithFlush();
    /**
     * Broadcast transtion to stop complete. This is a confirmation to all
     * components that stop has finished. At the end of this we transition back
     * to config phase.
     * @Lock held mEngineLock
     */
    Status broadcastStopComplete();
    /**
     * Broadcast halt to all components. All inflight traffic is dropped.
     * Successful return from this function implies all components have
     * exited run phase and are back in config phase.
     * @Lock held mEngineLock
     */
    void broadcastHalt();
    /**
     * Broadcast reset to all components. All components drop client
     * specific configuration and transition to reset state. For RAII
     * components, they are freed at this point. ALso resets the mConfigBuilder
     * to its original state. Successful return puts the runner in reset phase.
     * @Lock held mEngineLock
     */
    void broadcastReset();
    /**
     * Populate stream managers for a given client config. For each client
     * selected output config, we generate stream managers. During reset phase
     * we clear out any previously constructed stream managers. This should be
     * invoked only in response to applyConfigs() issued by client.
     * @Lock held mEngineLock
     */
    Status populateStreamManagers(const ClientConfig& config);
    /**
     * Populate input managers for a given client config. For each client
     * selected output config, we generate input managers. During reset phase
     * we clear out any previously constructed input managers. This should be
     * invoked only in response to applyConfigs() issued by client.
     * @Lock held mEngineLock
     */
    Status populateInputManagers(const ClientConfig& config);
    /**
     * Helper method to forward packet to client interface for transmission
     */
    Status forwardOutputDataToClient(int streamId, std::shared_ptr<MemHandle>& handle);
    /**
     * Helper to handle error notification from components, in the errorQueue.
     * In case the source of the error is client interface, it will
     * broadcastReset().
     * This called in the mEngineThread when processing an entry from the
     * errorQueue,
     * @Lock acquires mEngineLock.
     */
    void processComponentError(std::string source);
    /**
     * Method run by the engine thread to process commands.
     * Uses condition variable. Acquires lock mEngineLock to access
     * command queues.
     */
    void processCommands();
    /**
     * Method run by external components to queue commands to the engine.
     * Must be called with mEngineLock held. Wakes up the looper.
     */
    void queueCommand(std::string source, EngineCommand::Type type);
    /**
     * Method called by component reporting error.
     * This will acquire mEngineLock and queue the error.
     */
    void queueError(std::string source, std::string msg, bool fatal);
    /**
     * client interface handle
     */
    std::unique_ptr<client_interface::ClientInterface> mClient = nullptr;
    /**
     * builder to build up client config incrementally.
     */
    ConfigBuilder mConfigBuilder;
    /**
     * Stream management members
     */
    std::map<int, std::unique_ptr<stream_manager::StreamManager>> mStreamManagers;
    stream_manager::StreamManagerFactory mStreamFactory;
    /**
     * Input manager members
     */
    std::map<int, std::unique_ptr<input_manager::InputManager>> mInputManagers;
    input_manager::InputManagerFactory mInputFactory;
    /**
     * stream to dump to display for debug purposes
     */
    int32_t mDisplayStream = ClientConfig::kInvalidId;
    /**
     * Debug display manager.
     */
    std::unique_ptr<debug_display_manager::DebugDisplayManager> mDebugDisplayManager = nullptr;
    /**
     * graph descriptor
     */
    proto::Options mGraphDescriptor;
    std::unique_ptr<graph::PrebuiltGraph> mGraph;
    /**
     * stop signal source
     */
    bool mStopFromClient = true;
    /**
     * Phase management members
     */
    std::string mCurrentPhase = kResetPhase;
    std::mutex mEngineLock;
    /**
     * Used to track the first error occurrence for a given phase.
     */
    std::unique_ptr<ComponentError> mCurrentPhaseError = nullptr;

    /**
     * Queue for client commands
     */
    std::queue<EngineCommand> mCommandQueue;
    /**
     * Queue for error notifications
     */
    std::queue<ComponentError> mErrorQueue;
    /**
     * Engine looper
     */
    std::unique_ptr<std::thread> mEngineThread;
    /**
     * Condition variable for looper
     */
    std::condition_variable mWakeLooper;
    /**
     * ignore input manager allocation
     */
    bool mIgnoreInputManager = false;
};

/**
 * Handles callbacks from individual stream managers as specified in the
 * StreamEngineInterface.
 */
class StreamCallback : public stream_manager::StreamEngineInterface {
  public:
    explicit StreamCallback(
        const std::function<void()>&& eos, const std::function<void(std::string)>&& errorCb,
        const std::function<Status(const std::shared_ptr<MemHandle>&)>&& packetHandler);
    void notifyEndOfStream() override;
    void notifyError(std::string msg) override;
    Status dispatchPacket(const std::shared_ptr<MemHandle>& outData) override;
    ~StreamCallback() = default;

  private:
    std::function<void(std::string)> mErrorHandler;
    std::function<void()> mEndOfStreamHandler;
    std::function<Status(const std::shared_ptr<MemHandle>&)> mPacketHandler;
};

/**
 * Handles callbacks from input managers. Forwards frames to the graph.
 * Only used if graph implementation is local
 */
class InputCallback : public input_manager::InputEngineInterface {
  public:
    explicit InputCallback(int id, const std::function<void(int)>&& cb,
                           const std::function<Status(int, int64_t, const InputFrame&)>&& packetCb);
    Status dispatchInputFrame(int streamId, int64_t timestamp, const InputFrame& frame) override;
    void notifyInputError() override;
    ~InputCallback() = default;

  private:
    std::function<void(int)> mErrorCallback;
    std::function<Status(int, int64_t, const InputFrame&)> mPacketHandler;
    int mInputId;
};

}  // namespace engine
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_RUNNER_ENGINE_DEFAULTENGINE_H_
