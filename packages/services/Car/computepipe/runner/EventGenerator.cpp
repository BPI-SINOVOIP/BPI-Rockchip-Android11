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

#include "EventGenerator.h"

#include "RunnerComponent.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace generator {

bool DefaultEvent::isPhaseEntry() const {
    return mType == static_cast<int>(PhaseState::ENTRY);
}
bool DefaultEvent::isAborted() const {
    return mType == static_cast<int>(PhaseState::ABORTED);
}
bool DefaultEvent::isTransitionComplete() const {
    return mType == static_cast<int>(PhaseState::TRANSITION_COMPLETE);
}

Status DefaultEvent::dispatchToComponent(const std::shared_ptr<RunnerComponentInterface>& iface) {
    switch (mPhase) {
        case RESET:
            return iface->handleResetPhase(*this);
        case RUN:
            return iface->handleExecutionPhase(*this);
        case STOP_WITH_FLUSH:
            return iface->handleStopWithFlushPhase(*this);
        case STOP_IMMEDIATE:
            return iface->handleStopImmediatePhase(*this);
    }
    return Status::ILLEGAL_STATE;
}

DefaultEvent DefaultEvent::generateEntryEvent(Phase p) {
    return DefaultEvent(PhaseState::ENTRY, p);
}
DefaultEvent DefaultEvent::generateAbortEvent(Phase p) {
    return DefaultEvent(PhaseState::ABORTED, p);
}
DefaultEvent DefaultEvent::generateTransitionCompleteEvent(Phase p) {
    return DefaultEvent(PhaseState::TRANSITION_COMPLETE, p);
}

}  // namespace generator
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
