/*
 * Copyright 2019 The Android Open Source Project
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
#pragma once

#include <cstdint>

namespace bluetooth {
namespace neighbor {

static constexpr double kTimeTickMs = 0.625;

using ScanInterval = uint16_t;  // Range 0x0012 to 0x1000; only even values valid  11.25 to 2560ms
using ScanWindow = uint16_t;    // Range 0x0011 to 0x1000;  10.625ms to 2560ms

inline double ScanIntervalTimeMs(ScanInterval interval) {
  return kTimeTickMs * interval;
}

inline double ScanWindowTimeMs(ScanWindow window) {
  return kTimeTickMs * window;
}

using ScanParameters = struct {
  ScanInterval interval;
  ScanWindow window;
};

}  // namespace neighbor
}  // namespace bluetooth
