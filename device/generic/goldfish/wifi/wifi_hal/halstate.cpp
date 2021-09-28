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

#include "halstate.h"

HalState::HalState() : mState(State::Constructed) {
}

bool HalState::init() {
    // Ensure that checking the state and waiting on the condition is atomic.
    std::unique_lock<std::mutex> lock(mStateMutex);

    if (mState != State::Stopped && mState != State::Constructed) {
        // We can only initialize when freshly constructed or stopped
        return false;
    }
    if (mInfo) {
        // We are in the correct state but our info object is still allocated.
        // This is a logic error somewhere and should not happen.
        return false;
    }
    auto info = std::make_unique<Info>();
    if (info->init()) {
        // Only store the info object to keep it alive if init succeeded.
        // Otherwise we're going to remain in the previous state and having an
        // uninitialized info object around will cause inconsistency.
        mInfo = std::move(info);
        mState = State::Initialized;
        return true;
    }
    // If we failed to initalize we remain in the same state.
    return false;
}

bool HalState::stop(StopHandler stopHandler) {
    {
        // Ensure atomicity in checking and setting state
        std::unique_lock<std::mutex> lock(mStateMutex);

        if (mState == State::Stopping || mState == State::Stopped) {
            // Already stopping or stopped, nothing to do
            return false;
        }
        if (mState != State::Running) {
            // Make sure there is no info object anymore. It should never exist
            // in the stopped state.
            mInfo.reset();
            // If we're neither stopping, stopped nor running then we can't stop
            // again. It seems that sometimes the client expects to be able to
            // call this when initialized or constructed so we'll set the state
            // to stopped. We have to return false to prevent the caller from
            // waiting for the callback though. Calling the callback here from
            // the same thread that called stop could cause a deadlock.
            mState = State::Stopped;
            return false;
        }
        mState = State::Stopping;
    }
    mInfo->stop(std::bind(&HalState::onStop, this, stopHandler));
    // We have now requested the stop, we'll change state in the stop handler
    // when it's called.
    return true;
}

bool HalState::eventLoop() {
    {
        // Atomically check and set state to running
        std::unique_lock<std::mutex> lock(mStateMutex);
        if (mState != State::Initialized || !mInfo) {
            return false;
        }
        mState = State::Running;
    }
    mInfo->eventLoop();
    return true;
}

Info* HalState::info() {
    return mInfo.get();
}

void HalState::onStop(StopHandler stopHandler) {
    stopHandler();
    mInfo.reset();

    std::unique_lock<std::mutex> lock(mStateMutex);
    mState = State::Stopped;
}

