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

#include "cache.h"
#include "frame.h"
#include "local_connection.h"
#include "macaddress.h"
#include "pollable.h"
#include "remote_connection.h"
#include "result.h"

#include <unordered_map>

class WifiForwarder : public Pollable {
public:
    WifiForwarder();
    virtual ~WifiForwarder() = default;
    Result init();

    // Pollable interface
    void getPollData(std::vector<pollfd>* fds) const override;
    Timestamp getTimeout() const override;
    bool onReadAvailable(int fd, int* status) override;
    bool onClose(int fd, int* status) override;
    bool onTimeout(Timestamp now, int* status) override;
private:
    enum class RadioType {
        Unknown,
        Local,
        Remote
    };

    const char* radioTypeToStr(RadioType type) const;

    void onAck(FrameInfo& info, bool success);

    void forwardFrame(std::unique_ptr<Frame> frame, RadioType sourceType);

    std::unordered_map<MacAddress, RadioType> mRadios;
    Cache<MacAddress, MacAddress> mAliases;
    LocalConnection mLocalConnection;
    RemoteConnection mRemoteConnection;
    Pollable::Timestamp mInitDeadline = Pollable::Timestamp::max();
    Pollable::Timestamp mDeadline = Pollable::Timestamp::max();
};

