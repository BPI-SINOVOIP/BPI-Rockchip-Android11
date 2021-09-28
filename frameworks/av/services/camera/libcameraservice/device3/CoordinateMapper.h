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

#ifndef ANDROID_SERVERS_COORDINATEMAPPER_H
#define ANDROID_SERVERS_COORDINATEMAPPER_H

#include <array>

namespace android {

namespace camera3 {

class CoordinateMapper {
    // Right now only stores metadata tags containing 2D coordinates
    // to be corrected.
protected:
    // Metadata key lists to correct

    // Both capture request and result
    static const std::array<uint32_t, 3> kMeteringRegionsToCorrect;

    // Both capture request and result
    static const std::array<uint32_t, 1> kRectsToCorrect;

    // Only for capture results; don't clamp
    static const std::array<uint32_t, 2> kResultPointsToCorrectNoClamp;
}; // class CoordinateMapper

} // namespace camera3

} // namespace android

#endif
