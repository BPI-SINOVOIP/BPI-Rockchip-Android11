/*
 * Copyright 2015 The Android Open Source Project
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

#define LOG_TAG "RampLinear"
//#define LOG_NDEBUG 0
#include <utils/Log.h>

#include <algorithm>
#include <unistd.h>
#include "AudioProcessorBase.h"
#include "RampLinear.h"

using namespace flowgraph;

RampLinear::RampLinear(int32_t channelCount)
        : input(*this, channelCount)
        , output(*this, channelCount) {
    mTarget.store(1.0f);
}

void RampLinear::setLengthInFrames(int32_t frames) {
    mLengthInFrames = frames;
}

void RampLinear::setTarget(float target) {
    mTarget.store(target);
}

float RampLinear::interpolateCurrent() {
    return mLevelTo - (mRemaining * mScaler);
}

int32_t RampLinear::onProcess(int64_t framePosition, int32_t numFrames) {
    int32_t framesToProcess = input.pullData(framePosition, numFrames);
    const float *inputBuffer = input.getBlock();
    float *outputBuffer = output.getBlock();
    int32_t channelCount = output.getSamplesPerFrame();

    float target = getTarget();
    if (target != mLevelTo) {
        // Start new ramp. Continue from previous level.
        mLevelFrom = interpolateCurrent();
        mLevelTo = target;
        mRemaining = mLengthInFrames;
        ALOGV("%s() mLevelFrom = %f, mLevelTo = %f, mRemaining = %d, mScaler = %f",
              __func__, mLevelFrom, mLevelTo, mRemaining, mScaler);
        mScaler = (mLevelTo - mLevelFrom) / mLengthInFrames; // for interpolation
    }

    int32_t framesLeft = framesToProcess;

    if (mRemaining > 0) { // Ramping? This doesn't happen very often.
        int32_t framesToRamp = std::min(framesLeft, mRemaining);
        framesLeft -= framesToRamp;
        while (framesToRamp > 0) {
            float currentLevel = interpolateCurrent();
            for (int ch = 0; ch < channelCount; ch++) {
                *outputBuffer++ = *inputBuffer++ * currentLevel;
            }
            mRemaining--;
            framesToRamp--;
        }
    }

    // Process any frames after the ramp.
    int32_t samplesLeft = framesLeft * channelCount;
    for (int i = 0; i < samplesLeft; i++) {
        *outputBuffer++ = *inputBuffer++ * mLevelTo;
    }

    return framesToProcess;
}
