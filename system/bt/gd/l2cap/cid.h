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

using Cid = uint16_t;

constexpr Cid kInvalidCid = 0;
constexpr Cid kFirstFixedChannel = 1;
constexpr Cid kLastFixedChannel = 63;
constexpr Cid kFirstDynamicChannel = kLastFixedChannel + 1;
constexpr Cid kLastDynamicChannel = (uint16_t)(0xffff);

constexpr Cid kClassicSignallingCid = 1;
constexpr Cid kConnectionlessCid = 2;
constexpr Cid kLeAttributeCid = 4;
constexpr Cid kLeSignallingCid = 5;
constexpr Cid kSmpCid = 6;
constexpr Cid kSmpBrCid = 7;

constexpr Cid kClassicPairingTriggerCid = kLastFixedChannel - 1;

}  // namespace l2cap
}  // namespace bluetooth
