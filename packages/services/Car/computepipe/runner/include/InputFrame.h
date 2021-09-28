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

#ifndef COMPUTEPIPE_RUNNER_INPUT_FRAME
#define COMPUTEPIPE_RUNNER_INPUT_FRAME

#include <cstdint>
#include <functional>

#include "types/Status.h"
namespace android {
namespace automotive {
namespace computepipe {
namespace runner {

typedef std::function<void(uint8_t[])> FrameDeleter;
/**
 * Information about the input frame
 */
struct FrameInfo {
    uint32_t height;  // In pixels
    uint32_t width;   // In pixels
    PixelFormat format;
    uint32_t stride;  // In bytes
    int cameraId;
};

/**
 * Wrapper around the pixel data of the input frame
 */
struct InputFrame {
  public:
    /**
     * Take info about frame data. InputFrame does not take ownership of the data.
     */
    explicit InputFrame(uint32_t height, uint32_t width, PixelFormat format, uint32_t stride,
                        const uint8_t* ptr) {
        mInfo.height = height;
        mInfo.width = width;
        mInfo.format = format;
        mInfo.stride = stride;
        mDataPtr = ptr;
    }

    /**
     * This is an unsafe method, that a consumer should use to copy the
     * underlying frame data
     */
    const uint8_t* getFramePtr() const {
        return mDataPtr;
    }
    FrameInfo getFrameInfo() const {
        return mInfo;
    }
    /**
     * Delete evil constructors
     */
    InputFrame() = delete;
    InputFrame(const InputFrame&) = delete;
    InputFrame& operator=(const InputFrame& f) = delete;
    InputFrame(InputFrame&& f) = delete;
    InputFrame& operator=(InputFrame&& f) = delete;

  private:
    FrameInfo mInfo;
    FrameDeleter mDeleter;
    const uint8_t* mDataPtr;
};

}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif
