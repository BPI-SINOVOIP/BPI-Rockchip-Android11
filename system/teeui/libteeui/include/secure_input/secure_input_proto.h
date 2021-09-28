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

#include <stdint.h>
#include <teeui/utils.h>

#include <memory>
#include <tuple>

#include <teeui/common_message_types.h>
#include <teeui/generic_messages.h>

namespace secure_input {

enum class DTupKeyEvent : uint32_t { RESERVED = 0, VOL_DOWN = 114, VOL_UP = 115, PWR = 116 };

enum class InputResponse : uint32_t {
    OK,
    PENDING_MORE,
    TIMED_OUT,
};

enum class SecureInputCommand : uint32_t {
    Invalid,
    InputHandshake,
    FinalizeInputSession,
    DeliverInputEvent,
};

static constexpr const ::teeui::Protocol kSecureInputProto = 1;

#define DECLARE_SECURE_INPUT_COMMAND(cmd)                                                          \
    using Cmd##cmd = ::teeui::Cmd<kSecureInputProto, SecureInputCommand, SecureInputCommand::cmd>

DECLARE_SECURE_INPUT_COMMAND(InputHandshake);
DECLARE_SECURE_INPUT_COMMAND(FinalizeInputSession);
DECLARE_SECURE_INPUT_COMMAND(DeliverInputEvent);

constexpr size_t kNonceBytes = 32;
constexpr size_t kSignatureBytes = 32;
constexpr char kConfirmationUIHandshakeLabel[] = "DTup input handshake";
constexpr char kConfirmationUIEventLabel[] = "DTup input event";
using Nonce = teeui::Array<uint8_t, kNonceBytes>;
using Signature = teeui::Array<uint8_t, kSignatureBytes>;

constexpr const uint64_t kUserPreInputGracePeriodMillis = 750;
constexpr const uint64_t kUserDoupleClickTimeoutMillis = 350;

using InputHandshake = teeui::Message<CmdInputHandshake>;
using InputHandshakeResponse = teeui::Message<teeui::ResponseCode, Nonce>;

/*
 * This command delivers the nonce Nci and the HMAC signature over
 * kConfirmationUIHandshakeLabel||Nco||Nci to the TA.
 * Note the terminating 0 of the label does NOT go into the signature.
 * Command::Vendor, TrustyCommand::FinalizeInputSession, Nci, <signature>.
 */
using FinalizeInputSessionHandshake = teeui::Message<CmdFinalizeInputSession, Nonce, Signature>;
using FinalizeInputSessionHandshakeResponse = teeui::Message<teeui::ResponseCode>;

/*
 * This command delivers an input event to the TA.
 * Command::Vendor, TrustyCommand::DeliverInputEvent, <key event>,
 * signature over kConfirmationUIEventLabel||<key event>||Nci.
 * Note the terminating 0 of the label does NOT go into the signature.
 */
using DeliverInputEvent = teeui::Message<CmdDeliverInputEvent, DTupKeyEvent, Nonce>;
using DeliverInputEventResponse = teeui::Message<teeui::ResponseCode, InputResponse>;

// Input Event
inline std::tuple<teeui::ReadStream, DTupKeyEvent> read(teeui::Message<DTupKeyEvent>,
                                                        teeui::ReadStream in) {
    return teeui::readSimpleType<DTupKeyEvent>(in);
}
inline teeui::WriteStream write(teeui::WriteStream out, const DTupKeyEvent& v) {
    return write(out, teeui::bytesCast(v));
}

// InputResponse
inline std::tuple<teeui::ReadStream, InputResponse> read(teeui::Message<InputResponse>,
                                                         teeui::ReadStream in) {
    return teeui::readSimpleType<InputResponse>(in);
}
inline teeui::WriteStream write(teeui::WriteStream out, const InputResponse& v) {
    return write(out, teeui::bytesCast(v));
}

}  // namespace secure_input
