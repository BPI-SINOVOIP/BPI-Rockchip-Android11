/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <chrono>
#include "util.h"

namespace goldfish {
namespace util {

int64_t nowNanos() {
    using namespace std::chrono;
    return time_point_cast<nanoseconds>(system_clock::now()).time_since_epoch().count();
}

ahg20::ElapsedRealtime makeElapsedRealtime(long long timestampNs) {
    ahg20::ElapsedRealtime ts = {
        .flags = ahg20::ElapsedRealtimeFlags::HAS_TIMESTAMP_NS |
                 ahg20::ElapsedRealtimeFlags::HAS_TIME_UNCERTAINTY_NS,
        .timestampNs = static_cast<uint64_t>(timestampNs),
        .timeUncertaintyNs = 1000000
    };

    return ts;
}

}  // namespace util
}  // namespace goldfish
