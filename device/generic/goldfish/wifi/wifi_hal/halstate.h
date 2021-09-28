/*
 * Copyright (C) 2017 The Android Open Source Project
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

#pragma once

#include "info.h"

#include <memory>
#include <mutex>

// A class that keeps track of the current state of the WiFi HAL. This is
// accomplished by having the HAL call this state tracker which in turn will
// call the appropriate functionality.
//
// This is needed because the HAL must have some form of global state to prevent
// situations where the HAL is initialized while stopping. This class allows
// the Info class to stay relatively simple and this class can focus on just the
// state behavior.
class HalState {
public:
    using StopHandler = std::function<void ()>;

    HalState();

    bool init();
    bool stop(StopHandler stopHandler);
    bool eventLoop();

    Info* info();

private:
    enum class State {
        Constructed,
        Initialized,
        Running,
        Stopping,
        Stopped,
    };

    void onStop(StopHandler stopHandler);

    std::unique_ptr<Info> mInfo;
    std::mutex mStateMutex;
    State mState;
};

