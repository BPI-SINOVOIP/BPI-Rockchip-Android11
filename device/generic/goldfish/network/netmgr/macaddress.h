/*
 * Copyright (C) 2017 The Android Open Source Project
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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <functional>

#include <linux/if_ether.h>
#include <stdint.h>
#include <string.h>

struct MacAddress {
    uint8_t addr[ETH_ALEN];
    bool isBroadcast() const {
        return memcmp(addr, "\xFF\xFF\xFF\xFF\xFF\xFF", ETH_ALEN) == 0;
    }
} __attribute__((__packed__));

template<class T>
inline void hash_combine(size_t& seed, const T& value) {
    std::hash<T> hasher;
    seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std {
template<> struct hash<MacAddress> {
    size_t operator()(const MacAddress& addr) const {
        size_t seed = 0;
        // Treat the first 4 bytes as an uint32_t to save some computation
        hash_combine(seed, *reinterpret_cast<const uint32_t*>(addr.addr));
        // And the remaining 2 bytes as an uint16_t
        hash_combine(seed, *reinterpret_cast<const uint16_t*>(addr.addr + 4));
        return seed;
    }
};
}

