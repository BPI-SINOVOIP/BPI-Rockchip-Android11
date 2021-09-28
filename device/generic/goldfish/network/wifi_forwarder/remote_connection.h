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

#include "frame.h"
#include "pollable.h"
#include "result.h"

#include <bitset>
#include <functional>
#include <memory>
#include <vector>

struct MacAddress;

class RemoteConnection {
public:
    using OnFrameCallback = std::function<void (std::unique_ptr<Frame>)>;
    using OnAckCallback = std::function<void (FrameInfo&)>;
    using OnErrorCallback = OnAckCallback;

    RemoteConnection(OnFrameCallback onFrameCallback,
                     OnAckCallback onAckCallback,
                     OnErrorCallback onErrorCallback);
    ~RemoteConnection();

    Result init();

    int getFd() const { return mPipeFd; }
    Pollable::Timestamp getTimeout() const;
    void receive();
    bool sendFrame(std::unique_ptr<Frame> frame);
    bool ackFrame(FrameInfo& info, bool success);

private:
    RemoteConnection(const RemoteConnection&) = delete;
    RemoteConnection& operator=(const RemoteConnection&) = delete;

    OnFrameCallback mOnFrameCallback;
    OnAckCallback mOnAckCallback;
    OnErrorCallback mOnErrorCallback;

    Pollable::Timestamp mDeadline = Pollable::Timestamp::max();
    std::vector<unsigned char> mBuffer;
    int mPipeFd = -1;
    std::bitset<256> mReportedVersionMismatches;
};

