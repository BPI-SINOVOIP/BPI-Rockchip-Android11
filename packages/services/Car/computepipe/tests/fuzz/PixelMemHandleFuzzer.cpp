/*
 * Copyright 2020 The Android Open Source Project
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vndk/hardware_buffer.h>

#include "InputFrame.h"
#include "PixelFormatUtils.h"
#include "PixelStreamManager.h"
#include "gmock/gmock-matchers.h"
#include "Fuzz.pb.h"
#include "src/libfuzzer/libfuzzer_macro.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace stream_manager {
namespace {

InputFrame convertToInputFrame(const fuzz::proto::Frame& frame) {
    uint32_t height = frame.height();
    uint32_t width = frame.width();
    uint32_t stride = frame.stride();
    PixelFormat pixelFormat = static_cast<PixelFormat>(frame.format());
    const uint8_t* data = reinterpret_cast<const uint8_t*>(frame.buffer().c_str());
    return InputFrame(height, width, pixelFormat, stride, data);
}

int setFrameDataTest(const fuzz::proto::PixelMemHandleFuzzerInput& input) {
    int bufferId = 10;
    int streamId = 1;
    uint64_t timestamp = 100;

    PixelMemHandle memHandle(bufferId, streamId, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN);
    InputFrame oldInputFrame = convertToInputFrame(input.frames().Get(0));
    memHandle.setFrameData(timestamp, oldInputFrame);

    // overwrite frame data with different format and dimensions
    InputFrame newInputFrame = convertToInputFrame(input.frames().Get(1));
    memHandle.setFrameData(timestamp, newInputFrame);

    AHardwareBuffer_Desc desc;
    AHardwareBuffer* buffer = memHandle.getHardwareBuffer();
    AHardwareBuffer_describe(buffer, &desc);
    return 0;
}

bool isValid(const fuzz::proto::PixelMemHandleFuzzerInput& input) {
    for (auto& frame : input.frames()) {
        uint64_t height = frame.height();
        uint64_t width = frame.width();
        uint64_t stride = frame.stride();
        uint64_t size = frame.buffer().size();
        if (stride > width * height) {
            return false;
        }

        if (height * width != size) {
            return false;
        }

        if (height * width == 0) {
            return false;
        }
    }

    return true;
}

// generate guided and mutated frame data for fuzzing
DEFINE_PROTO_FUZZER(const fuzz::proto::PixelMemHandleFuzzerInput& input) {
    static PostProcessorRegistration reg = {
            [](const fuzz::proto::PixelMemHandleFuzzerInput* input, unsigned int seed) {
                if (input->frames().size() < 2) {
                    return;
                }

                if (!isValid(*input)) {
                    return;
                }

                setFrameDataTest(*input);
            }
    };
}

}  // namespace
}  // namespace stream_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
