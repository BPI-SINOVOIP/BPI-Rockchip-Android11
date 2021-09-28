/*
 *
 * Copyright 2017, The Android Open Source Project
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

#include <teeui/cbor.h>

namespace teeui {
namespace cbor {

namespace {

inline uint8_t getByte(const uint64_t& v, const uint8_t index) {
    return (v >> (index * 8)) & 0xff;
}

WriteState writeBytes(WriteState state, uint64_t value, uint8_t size) {
    auto pos = state.data_;
    if (!(state += size)) return state;
    switch (size) {
    case 8:
        *pos++ = getByte(value, 7);
        *pos++ = getByte(value, 6);
        *pos++ = getByte(value, 5);
        *pos++ = getByte(value, 4);
        [[fallthrough]];
    case 4:
        *pos++ = getByte(value, 3);
        *pos++ = getByte(value, 2);
        [[fallthrough]];
    case 2:
        *pos++ = getByte(value, 1);
        [[fallthrough]];
    case 1:
        *pos++ = getByte(value, 0);
        break;
    default:
        state.error_ = Error::MALFORMED;
    }
    return state;
}

}  // anonymous namespace

WriteState writeHeader(WriteState wState, Type type, const uint64_t value) {
    if (!wState) return wState;
    uint8_t& header = *wState.data_;
    if (!++wState) return wState;
    header = static_cast<uint8_t>(type) << 5;
    if (value < 24) {
        header |= static_cast<uint8_t>(value);
    } else if (value < 0x100) {
        header |= 24;
        wState = writeBytes(wState, value, 1);
    } else if (value < 0x10000) {
        header |= 25;
        wState = writeBytes(wState, value, 2);
    } else if (value < 0x100000000) {
        header |= 26;
        wState = writeBytes(wState, value, 4);
    } else {
        header |= 27;
        wState = writeBytes(wState, value, 8);
    }
    return wState;
}

static size_t byteCount(char c) {
    if ((0xc0 & c) == 0x80) {
        return 0;  // this is a multibyte payload byte
    } else if (0x80 & c) {
        /*
         * CLZ - count leading zeroes.
         * __builtin_clz promotes the argument to unsigned int.
         * We invert c to turn leading ones into leading zeroes.
         * We subtract additional leading zeroes due to the type promotion from the result.
         */
        return __builtin_clz((unsigned char)(~c)) - (sizeof(unsigned int) * 8 - 8);
    } else {
        return 1;
    }
}

bool checkUTF8Copy(const char* begin, const char* const end, uint8_t* out) {
    while (begin != end) {
        auto bc = byteCount(*begin);
        // if the string ends in the middle of a multi byte char it is invalid
        if (begin + bc > end) return false;
        switch (bc) {
        case 4:
            if (out) *out++ = *reinterpret_cast<const uint8_t*>(begin++);
            [[fallthrough]];
        case 3:
            if (out) *out++ = *reinterpret_cast<const uint8_t*>(begin++);
            [[fallthrough]];
        case 2:
            if (out) *out++ = *reinterpret_cast<const uint8_t*>(begin++);
            [[fallthrough]];
        case 1:
            if (out) *out++ = *reinterpret_cast<const uint8_t*>(begin++);
            break;
        default:
            // case 0 means we encounted a payload byte when we expected a header.
            // case > 4 is malformed.
            return false;
        }
    }
    return true;
}

}  // namespace cbor
}  // namespace teeui
