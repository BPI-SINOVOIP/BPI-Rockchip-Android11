/*
 *
 * Copyright 2020, The Android Open Source Project
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

#include <teeui/msg_formatting.h>

namespace teeui {
namespace msg {

void zero(volatile uint8_t* begin, const volatile uint8_t* end) {
    while (begin != end) {
        *begin++ = 0x0;
    }
}

WriteStream write(WriteStream out, const uint8_t* buffer, uint32_t size) {
    if (out.insertFieldSize(size)) {
        auto pos = out.pos();
        out += size;
        if (out) {
            std::copy(buffer, buffer + size, pos);
        }
    }
    return out;
}

std::tuple<ReadStream, ReadStream::ptr_t, uint32_t> read(ReadStream in) {
    auto size = in.extractFieldSize();
    auto pos = in.pos();
    in += size;
    return {in, pos, size};
}

}  // namespace msg
}  // namespace teeui
