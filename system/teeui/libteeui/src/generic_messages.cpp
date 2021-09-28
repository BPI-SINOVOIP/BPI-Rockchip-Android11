/*
 *
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

#include <teeui/generic_messages.h>

namespace teeui {

std::tuple<ReadStream, uint32_t> readU32(ReadStream in) {
    volatile const uint32_t* pos = reinterpret_cast<volatile const uint32_t*>(in.pos());
    in += sizeof(uint32_t);
    if (!in) return {in, 0};
    return {in, *pos};
}

std::tuple<ReadStream, Command> readCommand(ReadStream in) {
    return readCmd<Command>(in);
}

Command peakCommand(ReadStream in) {
    auto [_, cmd] = readCommand(in);
    return cmd;
}

std::tuple<ReadStream, Protocol> readProtocol(ReadStream in) {
    return readCmd<Protocol, kProtoInvalid>(in);
}

Protocol peakProtocol(ReadStream in) {
    auto [_, proto] = readProtocol(in);
    return proto;
}

}  // namespace teeui
