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

#include "PixelFormatUtils.h"

#include <android-base/logging.h>

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {

int numBytesPerPixel(AHardwareBuffer_Format format) {
    switch (format) {
        case AHardwareBuffer_Format::AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
            return 4;
        case AHardwareBuffer_Format::AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM:
            return 3;
        case AHardwareBuffer_Format::AHARDWAREBUFFER_FORMAT_S8_UINT:
            return 1;
        default:
            CHECK(false) << "Unrecognized pixel format seen";
    }
    return 0;
}

AHardwareBuffer_Format PixelFormatToHardwareBufferFormat(PixelFormat pixelFormat) {
    switch (pixelFormat) {
        case PixelFormat::RGBA:
            return AHardwareBuffer_Format::AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
        case PixelFormat::RGB:
            return AHardwareBuffer_Format::AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;
        case PixelFormat::GRAY:
            return AHardwareBuffer_Format::AHARDWAREBUFFER_FORMAT_S8_UINT;  // TODO: Check if this works
        default:
            CHECK(false) << "Unrecognized pixel format seen";
    }
    return AHardwareBuffer_Format::AHARDWAREBUFFER_FORMAT_BLOB;
}

}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
