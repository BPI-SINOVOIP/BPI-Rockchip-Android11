/*
 * Copyright 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "macaddress.h"

#include <string.h>

struct FrameId {
    FrameId() {}
    FrameId(uint64_t cookie, const MacAddress& transmitter)
        : cookie(cookie), transmitter(transmitter) {
    }
    FrameId(const FrameId&) = default;
    FrameId(FrameId&&) = default;

    FrameId& operator=(const FrameId&) = default;
    FrameId& operator=(FrameId&&) = default;

    uint64_t cookie;
    MacAddress transmitter;
};

namespace std {
template<> struct hash<FrameId> {
    size_t operator()(const FrameId& id) const {
        size_t seed = 0;
        hash_combine(seed, id.cookie);
        hash_combine(seed, id.transmitter);
        return seed;
    }
};
}

inline bool operator==(const FrameId& left, const FrameId& right) {
    return left.cookie == right.cookie && left.transmitter == right.transmitter;
}

inline bool operator<(const FrameId& left, const FrameId& right) {
    if (left.cookie < right.cookie) {
        return true;
    }
    if (left.cookie > right.cookie) {
        return false;
    }
    return memcmp(left.transmitter.addr,
                  right.transmitter.addr,
                  sizeof(left.transmitter.addr)) < 0;
}
