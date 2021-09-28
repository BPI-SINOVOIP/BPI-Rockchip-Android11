/*
 * Copyright (C) 2019 The Android Open Source Project
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
#ifndef CAR_LIB_EVS_SUPPORT_FRAME_H
#define CAR_LIB_EVS_SUPPORT_FRAME_H

namespace android {
namespace automotive {
namespace evs {
namespace support {

struct Frame {
    unsigned width;
    unsigned height;
    unsigned stride;
    uint8_t* data;
};

}  // namespace support
}  // namespace evs
}  // namespace automotive
}  // namespace android

#endif  // CAR_LIB_EVS_SUPPORT_FRAME_H
