/*
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

#ifndef TEEUI_GENERICOPERATION_H_
#define TEEUI_GENERICOPERATION_H_

#include <teeui/cbor.h>
#include <teeui/generic_messages.h>
#include <teeui/msg_formatting.h>
#include <teeui/utils.h>

namespace teeui {

inline bool hasOption(UIOption option, const MsgVector<UIOption>& uiOptions) {
    for (auto& o : uiOptions) {
        if (o == option) return true;
    }
    return false;
}

/**
 * The generic Confirmation Operation:
 *
 * Derived needs to implement:
 *   Derived::TimeStamp needs to be a timestamp type
 *   Derived::now() needs to return a TimeStamp value denoting the current point in time.
 *   Derived::hmac256 with the following prototype which computes the 32-byte HMAC-SHA256 over the
 *   concatenation of all provided "buffers" keyed with "key".
 *
 *     optional<Hmac> HMacImplementation::hmac256(const AuthTokenKey& key,
 *                                                std::initializer_list<ByteBufferProxy> buffers);
 *
 *   ResponseCode initHook();
 *     Gets called on PromptUserConfirmation. If initHook returns anything but ResponseCode::OK,
 *     The operation is not started and the result is returned to the HAL service.
 *   void abortHook();
 *     Gets called on Abort. Allows the implementation to perform cleanup.
 *   void finalizeHook();
 *     Gets called on FetchConfirmationResult.
 *   ResponseCode testCommandHook(TestModeCommands testCmd);
 *     Gets called on DeliverTestCommand and allows the implementation to react to test commands.
 *
 * And optionally:
 *   WriteStream vendorCommandHook(ReadStream in, WriteStream out);
 *
 * The latter allows Implementations to implement custom protocol extensions.
 *
 */
template <typename Derived, typename TimeStamp> class Operation {
    using HMacer = HMac<Derived>;

  public:
    Operation()
        : error_(ResponseCode::Ignored), formattedMessageLength_(0),
          maginifiedViewRequested_(false), invertedColorModeRequested_(false),
          languageIdLength_(0) {}

    ResponseCode init(const MsgString& promptText, const MsgVector<uint8_t>& extraData,
                      const MsgString& locale, const MsgVector<UIOption>& options) {
        // An hmacKey needs to be installed before we can commence operation.
        if (!hmacKey_) return ResponseCode::Unexpected;
        if (error_ != ResponseCode::Ignored) return ResponseCode::OperationPending;
        confirmationTokenScratchpad_ = {};

        // We need to access the prompt text multiple times. Once for formatting the CBOR message
        // and again for rendering the dialog. It is vital that the prompt does not change
        // in the meantime. As of this point the prompt text is in a shared buffer and therefore
        // susceptible to TOCTOU attacks. Note that promptText.size() resides on the stack and
        // is safe to access multiple times. So now we copy the prompt string into the
        // scratchpad promptStringBuffer_ from where we can format the CBOR message and then
        // pass it to the renderer.
        if (promptText.size() >= uint32_t(MessageSize::MAX))
            return ResponseCode::UIErrorMessageTooLong;
        auto pos = std::copy(promptText.begin(), promptText.end(), promptStringBuffer_);
        *pos = 0;  // null-terminate the prompt for the renderer.

        using namespace ::teeui::cbor;
        using CborError = ::teeui::cbor::Error;
        // Note the extra data is accessed only once for formatting the CBOR message. So it is safe
        // to read it from the shared buffer directly. Anyway we don't trust or interpret the
        // extra data in any way so all we do is take a snapshot and we don't care if it is
        // modified concurrently.
        auto state = write(WriteState(formattedMessageBuffer_),
                           map(pair(text("prompt"), text(promptStringBuffer_, promptText.size())),
                               pair(text("extra"), bytes(extraData))));
        switch (state.error_) {
        case CborError::OK:
            break;
        case CborError::OUT_OF_DATA:
            return ResponseCode::UIErrorMessageTooLong;
        case CborError::MALFORMED_UTF8:
            return ResponseCode::UIErrorMalformedUTF8Encoding;
        case CborError::MALFORMED:
        default:
            return ResponseCode::Unexpected;
        }
        formattedMessageLength_ = state.data_ - formattedMessageBuffer_;

        if (locale.size() >= kMaxLocaleSize) return ResponseCode::UIErrorMessageTooLong;
        pos = std::copy(locale.begin(), locale.end(), languageIdBuffer_);
        *pos = 0;
        languageIdLength_ = locale.size();

        invertedColorModeRequested_ = hasOption(UIOption::AccessibilityInverted, options);
        maginifiedViewRequested_ = hasOption(UIOption::AccessibilityMagnified, options);

        // on success record the start time
        startTime_ = Derived::now();
        if (!startTime_.isOk()) {
            return ResponseCode::SystemError;
        }

        auto rc = static_cast<Derived*>(this)->initHook();

        if (rc == ResponseCode::OK) {
            error_ = ResponseCode::OK;
        }
        return rc;
    }

    void setHmacKey(const AuthTokenKey& key) { hmacKey_ = key; }
    optional<AuthTokenKey> hmacKey() const { return hmacKey_; }

    void abort() {
        if (isPending()) {
            error_ = ResponseCode::Aborted;
            static_cast<Derived*>(this)->abortHook();
        }
    }

    void userCancel() {
        if (isPending()) {
            error_ = ResponseCode::Canceled;
        }
    }

    std::tuple<ResponseCode, MsgVector<uint8_t>, MsgVector<uint8_t>> fetchConfirmationResult() {
        std::tuple<ResponseCode, MsgVector<uint8_t>, MsgVector<uint8_t>> result;
        auto& [rc, message, token] = result;
        rc = error_;
        if (error_ == ResponseCode::OK) {
            message = {&formattedMessageBuffer_[0],
                       &formattedMessageBuffer_[formattedMessageLength_]};
            if (confirmationTokenScratchpad_) {
                token = {confirmationTokenScratchpad_->begin(),
                         confirmationTokenScratchpad_->end()};
            }
        }
        error_ = ResponseCode::Ignored;
        static_cast<Derived*>(this)->finalizeHook();
        return result;
    }

    bool isPending() const { return error_ != ResponseCode::Ignored; }

    const MsgString getPrompt() const {
        return {&promptStringBuffer_[0], &promptStringBuffer_[strlen(promptStringBuffer_)]};
    }

    ResponseCode deliverTestCommand(TestModeCommands testCommand) {
        constexpr const auto testKey = AuthTokenKey::fill(static_cast<uint8_t>(TestKeyBits::BYTE));
        auto rc = static_cast<Derived*>(this)->testCommandHook(testCommand);
        if (rc != ResponseCode::OK) return rc;
        switch (testCommand) {
        case TestModeCommands::OK_EVENT: {
            if (isPending()) {
                signConfirmation(testKey);
                return ResponseCode::OK;
            } else {
                return ResponseCode::Ignored;
            }
        }
        case TestModeCommands::CANCEL_EVENT: {
            bool ignored = !isPending();
            userCancel();
            return ignored ? ResponseCode::Ignored : ResponseCode::OK;
        }
        default:
            return ResponseCode::Ignored;
        }
    }

    WriteStream dispatchCommandMessage(ReadStream in_, WriteStream out) {
        auto [in, proto] = readProtocol(in_);
        if (proto == kProtoGeneric) {
            auto [in_cmd, cmd] = readCommand(in);
            switch (cmd) {
            case Command::PromptUserConfirmation:
                return command(CmdPromptUserConfirmation(), in_cmd, out);
            case Command::FetchConfirmationResult:
                return command(CmdFetchConfirmationResult(), in_cmd, out);
            case Command::DeliverTestCommand:
                return command(CmdDeliverTestCommand(), in_cmd, out);
            case Command::Abort:
                return command(CmdAbort(), in_cmd, out);
            default:
                return write(Message<ResponseCode>(), out, ResponseCode::Unimplemented);
            }
        }
        return static_cast<Derived*>(this)->extendedProtocolHook(proto, in, out);
    }

  protected:
    /*
     * The extendedProtocolHoock allows implementations to implement custom protocols on top of
     * the default commands.
     * This function is only called if "Derived" does not implement the extendedProtocolHoock
     * and writes ResponseCode::Unimplemented to the response buffer.
     */
    inline WriteStream extendedProtocolHook(Protocol proto, ReadStream, WriteStream out) {
        return write(Message<ResponseCode>(), out, ResponseCode::Unimplemented);
    }

  private:
    WriteStream command(CmdPromptUserConfirmation, ReadStream in, WriteStream out) {
        auto [in_, promt, extra, locale, options] = read(PromptUserConfirmationMsg(), in);
        if (!in_) return write(PromptUserConfirmationResponse(), out, ResponseCode::SystemError);
        auto rc = init(promt, extra, locale, options);
        return write(PromptUserConfirmationResponse(), out, rc);
    }
    WriteStream command(CmdFetchConfirmationResult, ReadStream in, WriteStream out) {
        auto [rc, message, token] = fetchConfirmationResult();
        return write(ResultMsg(), out, rc, message, token);
    }
    WriteStream command(CmdDeliverTestCommand, ReadStream in, WriteStream out) {
        auto [in_, token] = read(DeliverTestCommandMessage(), in);
        if (!in_) return write(DeliverTestCommandResponse(), out, ResponseCode::SystemError);
        auto rc = deliverTestCommand(token);
        return write(DeliverTestCommandResponse(), out, rc);
    }
    WriteStream command(CmdAbort, ReadStream in, WriteStream out) {
        abort();
        return out;
    }

    MsgVector<uint8_t> getMessage() {
        MsgVector<uint8_t> result;
        if (error_ != ResponseCode::OK) return {};
        return {&formattedMessageBuffer_[0], &formattedMessageBuffer_[formattedMessageLength_]};
    }

  protected:
    void signConfirmation(const AuthTokenKey& key) {
        if (error_ != ResponseCode::OK) return;
        confirmationTokenScratchpad_ = HMacer::hmac256(key, "confirmation token", getMessage());
        if (!confirmationTokenScratchpad_) {
            error_ = ResponseCode::Unexpected;
        }
    }

  protected:
    ResponseCode error_ = ResponseCode::Ignored;
    uint8_t formattedMessageBuffer_[uint32_t(MessageSize::MAX)];
    size_t formattedMessageLength_ = 0;
    char promptStringBuffer_[uint32_t(MessageSize::MAX)];
    optional<Hmac> confirmationTokenScratchpad_;
    TimeStamp startTime_;
    optional<AuthTokenKey> hmacKey_;
    bool maginifiedViewRequested_;
    bool invertedColorModeRequested_;
    constexpr static size_t kMaxLocaleSize = 64;
    char languageIdBuffer_[kMaxLocaleSize];
    size_t languageIdLength_;
};

}  // namespace teeui

#endif  // CONFIRMATIONUI_1_0_DEFAULT_GENERICOPERATION_H_
