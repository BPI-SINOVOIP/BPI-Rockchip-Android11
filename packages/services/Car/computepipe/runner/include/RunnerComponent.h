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

#ifndef COMPUTEPIPE_RUNNER_INCLUDE_RUNNERCOMPONENT_H_
#define COMPUTEPIPE_RUNNER_INCLUDE_RUNNERCOMPONENT_H_
#include <map>
#include <memory>
#include <string>

#include "types/Status.h"
#include "ProfilingType.pb.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {

class RunnerComponentInterface;

/**
 * Represents the state of the config phase a particular client config is in
 */
enum PhaseState {
    ENTRY = 0,
    TRANSITION_COMPLETE,
    ABORTED,
};

/**
 * RunnerEvent represents an event corresponding to a runner phase
 * Along with start, abort or transition complete query methods.
 */
class RunnerEvent {
  public:
    /* Is this a notification to enter the phase */
    virtual bool isPhaseEntry() const;
    /* Is this a notification that all components have transitioned to the phase */
    virtual bool isTransitionComplete() const;
    /* Is this a notification to abort the transition to the started phase */
    virtual bool isAborted() const;
    /* Dispatch event to component */
    virtual Status dispatchToComponent(const std::shared_ptr<RunnerComponentInterface>& iface) = 0;
    /* Destructor */
    virtual ~RunnerEvent() = default;
};

/**
 * Configuration that gets emitted once client has completely specified config
 * options
 */
class ClientConfig : public RunnerEvent {
  public:
    static const int kInvalidId = -1;

    /**
     * Override relevant methods from RunnerEvent
     */
    bool isPhaseEntry() const override {
        return mState == ENTRY;
    }
    bool isTransitionComplete() const override {
        return mState == TRANSITION_COMPLETE;
    }
    bool isAborted() const override {
        return mState == ABORTED;
    }

    Status dispatchToComponent(const std::shared_ptr<RunnerComponentInterface>& iface) override;
    /**
     * Accessor methods
     */
    Status getInputConfigId(int* outId) const;
    Status getOffloadId(int* outId) const;
    Status getTerminationId(int* outId) const;
    Status getOptionalConfigs(std::string& outOptional) const;
    Status getOutputStreamConfigs(std::map<int, int>& outputConfig) const;
    Status getProfilingType(proto::ProfilingType* profilingType) const;
    std::string getSerializedClientConfig() const;

    /**
     * Constructors
     */
    ClientConfig& operator=(ClientConfig&& r) {
        mInputConfigId = r.mInputConfigId;
        mTerminationId = r.mTerminationId;
        mOffloadId = r.mOffloadId;
        mOptionalConfigs = std::move(r.mOptionalConfigs);
        mOutputConfigs = std::move(r.mOutputConfigs);
        return *this;
    }
    ClientConfig(ClientConfig&& c) {
        *this = std::move(c);
    }
    ClientConfig(int inputConfigId, int offload, int termination, std::map<int, int>& outputConfigs,
                 proto::ProfilingType profilingType, std::string opt = "")
        : mInputConfigId(inputConfigId),
          mOutputConfigs(outputConfigs),
          mTerminationId(termination),
          mOffloadId(offload),
          mProfilingType(profilingType),
          mOptionalConfigs(opt) {
    }

    void setPhaseState(PhaseState state) {
        mState = state;
    }

  private:
    /**
     * input streamd id from the graph descriptor options
     */
    int mInputConfigId = kInvalidId;
    /**
     * Options for different output streams
     */
    std::map<int, int> mOutputConfigs;
    /**
     * Termination Option
     */
    int mTerminationId = kInvalidId;
    /**
     * offload option
     */
    int mOffloadId = kInvalidId;

    proto::ProfilingType mProfilingType = proto::ProfilingType::DISABLED;
    /**
     * serialized optional config
     */
    std::string mOptionalConfigs = "";
    /**
     * The state of the client config corresponding
     * to entry, transition complete or aborted
     */
    PhaseState mState = ENTRY;
};

/**
 * A component of the Runner Engine implements this interface to receive
 * RunnerEvents.
 * A SUCCESS return value indicates the component has handled the particular
 * event. A failure return value will result in a subsequent abort call
 * that should be ignored by the component that reported failure.
 */
class RunnerComponentInterface {
  public:
    /* handle a ConfigPhase related event notification from Runner Engine */
    virtual Status handleConfigPhase(const ClientConfig& e);
    /* handle execution phase notification from Runner Engine */
    virtual Status handleExecutionPhase(const RunnerEvent& e);
    /* handle a stop with flushing semantics phase notification from the engine */
    virtual Status handleStopWithFlushPhase(const RunnerEvent& e);
    /* handle an immediate stop phase notification from the engine */
    virtual Status handleStopImmediatePhase(const RunnerEvent& e);
    /* handle an engine notification to return to reset state */
    virtual Status handleResetPhase(const RunnerEvent& e);
    virtual ~RunnerComponentInterface() = default;
};

}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
#endif  // COMPUTEPIPE_RUNNER_INCLUDE_RUNNERCOMPONENT_H_
