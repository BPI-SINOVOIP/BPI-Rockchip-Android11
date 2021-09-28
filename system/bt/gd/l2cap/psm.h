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
namespace l2cap {

using Psm = uint16_t;
constexpr Psm kDefaultPsm = 0;  // Invalid Psm as a default value

constexpr bool IsPsmValid(Psm psm) {
  // See Core spec 5.1 Vol 3 Part A 4.2 for definition
  return (psm & 0x0101u) == 0x0001u;
}

}  // namespace l2cap
}  // namespace bluetooth
