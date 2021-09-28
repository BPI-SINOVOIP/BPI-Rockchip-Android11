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

#ifndef COMPUTEPIPE_RUNNER_PIXEL_FORMAT_UTILS
#define COMPUTEPIPE_RUNNER_PIXEL_FORMAT_UTILS

#include <vndk/hardware_buffer.h>

#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {

// Returns number of bytes per pixel for given format.
int numBytesPerPixel(AHardwareBuffer_Format format);

AHardwareBuffer_Format PixelFormatToHardwareBufferFormat(PixelFormat pixelFormat);

}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_RUNNER_PIXEL_FORMAT_UTILS
