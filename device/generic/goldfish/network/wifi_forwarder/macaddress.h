/*
 * Copyright 2019, The Android Open Source Project
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

#include "hash.h"

#include <linux/if_ether.h>
#include <stdint.h>
#include <string.h>

// Format macros for printf, e.g. printf("MAC address: " PRIMAC, MACARG(mac))
#define PRIMAC "%02x:%02x:%02x:%02x:%02x:%02x"
#define MACARG(x) (x)[0], (x)[1], (x)[2], (x)[3],(x)[4], (x)[5]

struct MacAddress {
    MacAddress() {
        memset(addr, 0, sizeof(addr));
    }
    MacAddress(uint8_t b1, uint8_t b2, uint8_t b3,
               uint8_t b4, uint8_t b5, uint8_t b6) {
        addr[0] = b1; addr[1] = b2; addr[2] = b3;
        addr[3] = b4; addr[4] = b5; addr[5] = b6;
    }
    uint8_t addr[ETH_ALEN];
    bool isBroadcast() const {
        return memcmp(addr, "\xFF\xFF\xFF\xFF\xFF\xFF", ETH_ALEN) == 0;
    }
    bool isMulticast() const {
        return addr[0] & 0x01;
    }
    bool empty() const {
        return memcmp(addr, "\x00\x00\x00\x00\x00\x00", ETH_ALEN) == 0;
    }
    uint8_t operator[](size_t index) const {
        return addr[index];
    }
} __attribute__((__packed__));

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

inline bool operator==(const MacAddress& left, const MacAddress& right) {
    return memcmp(left.addr, right.addr, ETH_ALEN) == 0;
}

inline bool operator!=(const MacAddress& left, const MacAddress& right) {
    return memcmp(left.addr, right.addr, ETH_ALEN) != 0;
}

