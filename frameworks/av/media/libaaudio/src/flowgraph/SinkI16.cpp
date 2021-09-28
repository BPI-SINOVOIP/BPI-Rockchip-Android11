/*
 * Copyright 2018 The Android Open Source Project
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

#include <algorithm>
#include <unistd.h>

#ifdef __ANDROID__
#include <audio_utils/primitives.h>
#endif

#include "AudioProcessorBase.h"
#include "SinkI16.h"

using namespace flowgraph;

SinkI16::SinkI16(int32_t channelCount)
        : AudioSink(channelCount) {}

int32_t SinkI16::read(void *data, int32_t numFrames) {
    int16_t *shortData = (int16_t *) data;
    const int32_t channelCount = input.getSamplesPerFrame();

    int32_t framesLeft = numFrames;
    while (framesLeft > 0) {
        // Run the graph and pull data through the input port.
        int32_t framesRead = pull(framesLeft);
        if (framesRead <= 0) {
            break;
        }
        const float *signal = input.getBlock();
        int32_t numSamples = framesRead * channelCount;
#ifdef __ANDROID__
        memcpy_to_i16_from_float(shortData, signal, numSamples);
        shortData += numSamples;
        signal += numSamples;
#else
        for (int i = 0; i < numSamples; i++) {
            int32_t n = (int32_t) (*signal++ * 32768.0f);
            *shortData++ = std::min(INT16_MAX, std::max(INT16_MIN, n)); // clip
        }
#endif
        framesLeft -= framesRead;
    }
    return numFrames - framesLeft;
}
