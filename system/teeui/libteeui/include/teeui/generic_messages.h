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

#ifndef TEEUI_GENERICMESSAGES_H_
#define TEEUI_GENERICMESSAGES_H_

#include <limits>
#include <tuple>

#include <stdint.h>

#include <teeui/common_message_types.h>
#include <teeui/msg_formatting.h>

namespace teeui {

enum class Command : uint32_t {
    Invalid,
    PromptUserConfirmation,
    FetchConfirmationResult,
    DeliverTestCommand,
    Abort,
};

using Protocol = uint32_t;

template <Protocol Proto, typename CmdT, CmdT cmd> struct Cmd {};

static constexpr const Protocol kProtoGeneric = 0;
static constexpr const Protocol kProtoInvalid = std::numeric_limits<Protocol>::max();

#define DECLARE_GENERIC_COMMAND(cmd) using Cmd##cmd = Cmd<kProtoGeneric, Command, Command::cmd>

DECLARE_GENERIC_COMMAND(PromptUserConfirmation);
DECLARE_GENERIC_COMMAND(FetchConfirmationResult);
DECLARE_GENERIC_COMMAND(DeliverTestCommand);
DECLARE_GENERIC_COMMAND(Abort);

using PromptUserConfirmationMsg = Message<CmdPromptUserConfirmation, MsgString, MsgVector<uint8_t>,
                                          MsgString, MsgVector<UIOption>>;
using PromptUserConfirmationResponse = Message<ResponseCode>;
using DeliverTestCommandMessage = Message<CmdDeliverTestCommand, TestModeCommands>;
using DeliverTestCommandResponse = Message<ResponseCode>;
using AbortMsg = Message<CmdAbort>;
using ResultMsg = Message<ResponseCode, MsgVector<uint8_t>, MsgVector<uint8_t>>;
using FetchConfirmationResult = Message<CmdFetchConfirmationResult>;

template <uint32_t proto, typename CmdT, CmdT cmd>
inline WriteStream write(WriteStream out, Cmd<proto, CmdT, cmd>) {
    volatile Protocol* protoptr = reinterpret_cast<volatile Protocol*>(out.pos());
    out += sizeof(Protocol);
    if (out) *protoptr = proto;

    volatile CmdT* cmdptr = reinterpret_cast<volatile CmdT*>(out.pos());
    out += sizeof(CmdT);
    if (out) *cmdptr = cmd;

    return out;
}

template <typename CmdSpec, typename... Tail>
WriteStream write(Message<CmdSpec, Tail...>, WriteStream out, const Tail&... tail) {
    out = write(out, CmdSpec());
    return write(Message<Tail...>(), out, tail...);
}

template <Protocol proto, typename CmdT, CmdT cmd, typename... Fields>
std::tuple<ReadStream, Fields...> read(Message<Cmd<proto, CmdT, cmd>, Fields...>, ReadStream in) {
    return read(Message<Fields...>(), in);
}

template <Protocol proto, typename CmdT, CmdT cmd, typename... T>
struct msg2tuple<Message<Cmd<proto, CmdT, cmd>, T...>> {
    using type = std::tuple<T...>;
};

std::tuple<ReadStream, uint32_t> readU32(ReadStream in);

template <typename CmdT, CmdT Invalid = CmdT::Invalid>
std::tuple<ReadStream, CmdT> readCmd(ReadStream in) {
    static_assert(sizeof(CmdT) == sizeof(uint32_t),
                  "Can only be used with types the size of uint32_t");
    auto [stream, value] = readU32(in);
    if (stream) return {stream, CmdT(value)};
    return {stream, Invalid};
}

template <typename CmdT, CmdT Invalid = CmdT::Invalid> CmdT peakCmd(ReadStream in) {
    auto [_, cmd] = readCmd<CmdT>(in);
    return cmd;
}

std::tuple<ReadStream, Command> readCommand(ReadStream in);
Command peakCommand(ReadStream in);
std::tuple<ReadStream, Protocol> readProtocol(ReadStream in);
Protocol peakProtocol(ReadStream in);

}  // namespace teeui

#endif  // TEEUI_GENERICMESSAGES_H_
