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

#ifndef COMPUTEPIPE_RUNNER_EVENT_GENERATOR_H
#define COMPUTEPIPE_RUNNER_EVENT_GENERATOR_H

#include "RunnerComponent.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace generator {

/**
 * Generate default events for different phases
 */
class DefaultEvent : public RunnerEvent {
  public:
    enum Phase {
        RESET = 0,
        RUN,
        STOP_WITH_FLUSH,
        STOP_IMMEDIATE,
    };
    /**
     * Override runner event methods
     */
    bool isPhaseEntry() const override;
    bool isAborted() const override;
    bool isTransitionComplete() const override;
    Status dispatchToComponent(const std::shared_ptr<RunnerComponentInterface>& iface) override;

    /**
     * generator methods
     */
    static DefaultEvent generateEntryEvent(Phase p);
    static DefaultEvent generateAbortEvent(Phase p);
    static DefaultEvent generateTransitionCompleteEvent(Phase p);

  private:
    explicit DefaultEvent(int type, Phase p) : mType(type), mPhase(p){};
    int mType;
    Phase mPhase;
};

}  // namespace generator
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
#endif
