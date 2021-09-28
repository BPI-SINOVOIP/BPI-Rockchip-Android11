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

#pragma once

#include <secure_input/evdev.h>
#include <secure_input/secure_input_proto.h>
#include <stdint.h>
#include <teeui/utils.h>

#include <functional>
#include <memory>
#include <tuple>

namespace secure_input {

class SecureInput {
  public:
    using KeyEvent = DTupKeyEvent;

    using HsBeginCb = std::function<std::tuple<teeui::ResponseCode, Nonce>()>;
    using HsFinalizeCb = std::function<teeui::ResponseCode(const Signature&, const Nonce&)>;
    using DeliverEventCb =
        std::function<std::tuple<teeui::ResponseCode, secure_input::InputResponse>(
            KeyEvent, const Signature&)>;
    using InputResultCb = std::function<void(teeui::ResponseCode)>;

    virtual ~SecureInput() {}

    virtual void handleEvent(const EventDev& evdev) = 0;
    virtual operator bool() const = 0;
    virtual void start() = 0;
};

std::shared_ptr<SecureInput> createSecureInput(SecureInput::HsBeginCb hsBeginCb,
                                               SecureInput::HsFinalizeCb hsFinalizeCb,
                                               SecureInput::DeliverEventCb deliverEventCb,
                                               SecureInput::InputResultCb inputResultCb);

}  // namespace secure_input
