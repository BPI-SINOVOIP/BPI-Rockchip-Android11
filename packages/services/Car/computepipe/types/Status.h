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

#ifndef COMPUTEPIPE_TYPES_STATUS_H_
#define COMPUTEPIPE_TYPES_STATUS_H_

namespace android {
namespace automotive {
namespace computepipe {

enum Status {
    SUCCESS = 0,
    INTERNAL_ERROR,
    INVALID_ARGUMENT,
    ILLEGAL_STATE,
    NO_MEMORY,
    FATAL_ERROR,
    STATUS_MAX,
};

enum PixelFormat {
    RGB = 0,
    RGBA,
    GRAY,
    PIXELFORMAT_MAX,
};

}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_TYPES_STATUS_H_
