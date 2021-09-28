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
#include "frame_id.h"
#include "macaddress.h"
#include "netlink_socket.h"
#include "result.h"

#include <chrono>
#include <functional>
#include <memory>
#include <queue>

struct MacAddress;

class LocalConnection {
public:
    using OnFrameCallback = std::function<void (std::unique_ptr<Frame>)>;
    using OnAckCallback = std::function<void (FrameInfo&)>;
    using OnErrorCallback = OnAckCallback;

    LocalConnection(OnFrameCallback onFrameCallback,
                    OnAckCallback onAckCallback,
                    OnErrorCallback onErrorCallback);
    Result init(std::chrono::steady_clock::time_point now);

    int getFd() const;
    bool receive();
    uint32_t cloneFrame(const Frame& frame, const MacAddress& destination);
    uint32_t transferFrame(std::unique_ptr<Frame> frame,
                           const MacAddress& destination);
    bool ackFrame(FrameInfo& info, bool success);

    std::chrono::steady_clock::time_point getTimeout() const;
    void onTimeout(std::chrono::steady_clock::time_point now);

private:
    using Timestamp = std::chrono::steady_clock::time_point;

    Result registerReceiver();
    static int staticOnMessage(struct nl_msg* msg, void* context);
    static int staticOnAck(struct nl_msg* msg, void* context);
    static int staticOnError(struct sockaddr_nl* addr,
                             struct nlmsgerr* error,
                             void* context);
    int onMessage(struct nl_msg* msg);
    int onFrame(struct nl_msg* msg);
    std::unique_ptr<Frame> parseFrame(struct nlmsghdr* hdr);

    int onAck(struct nl_msg* msg);
    int onError(struct sockaddr_nl* addr, struct nlmsgerr* error);
    bool sendFrameOnNetlink(const Frame& frame, const MacAddress& dest);

    OnFrameCallback mOnFrameCallback;
    OnAckCallback mOnAckCallback;
    OnErrorCallback mOnErrorCallback;

    NetlinkSocket mNetlinkSocket;
    int mNetlinkFamily = -1;

    // [cookie,transmitter] -> frame.
    Cache<FrameId, std::unique_ptr<Frame>> mPendingFrames;
    // sequence number -> [cookie,transmitter]
    Cache<uint32_t, FrameId> mSequenceNumberCookies;

    Timestamp mLastCacheTimeUpdate;
    Timestamp mLastCacheExpiration;

    // A queue (using an ordered map) with the next timeout mapping to the
    // cookie and transmitter of the frame to retry. This way we can easily
    // determine when the next deadline is by looking at the first entry and we
    // can quickly determine which frames to retry by starting at the beginning.
    std::priority_queue<std::pair<Timestamp, FrameId>> mRetryQueue;
};

