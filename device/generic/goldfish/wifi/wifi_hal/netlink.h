/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <stdint.h>
#include <unordered_map>

class NetlinkMessage;

class Netlink {
public:
    using ReplyHandler = std::function<void (const NetlinkMessage&)>;
    using StopHandler = std::function<void ()>;
    Netlink();
    ~Netlink();

    bool init();
    void stop(StopHandler stopHandler);

    bool eventLoop();

    uint32_t getSequenceNumber();

    bool sendMessage(const NetlinkMessage& message, ReplyHandler handler);
private:
    Netlink(const Netlink&) = delete;
    Netlink& operator=(const Netlink&) = delete;

    bool readNetlinkMessage(int fd);
    bool readControlMessage();

    void notifyHandler(const char* data, size_t size);

    std::atomic<uint32_t> mNextSequenceNumber;
    int mSocket;
    int mControlPipe[2];
    // Map sequence number to reply handler
    std::unordered_map<uint32_t, ReplyHandler> mHandlers;
    std::mutex mHandlersMutex;
    std::mutex mStopHandlerMutex;
    StopHandler mStopHandler;
};

