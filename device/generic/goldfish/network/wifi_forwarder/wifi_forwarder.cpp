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

#include "wifi_forwarder.h"

#include "frame.h"
#include "log.h"

static constexpr bool kDebugTraffic = false;
static constexpr bool kDebugBeaconTraffic = false;

// How many seconds to keep aliases alive for. Since this is used to keep track
// of randomized MAC addresses they will expire after a while. Set the value
// pretty high to ensure that we don't accidentally lose entries just because
// there's not a lot of frames going through.
static constexpr auto kAliasesCacheTimeout = std::chrono::hours(8);

WifiForwarder::WifiForwarder()
    : mAliases(kAliasesCacheTimeout)
    , mLocalConnection([this](std::unique_ptr<Frame> frame) {
                            forwardFrame(std::move(frame), RadioType::Local);
                       },
                       [this](FrameInfo& info) { onAck(info, true); },
                       [this](FrameInfo& info) { onAck(info, false); })
    , mRemoteConnection([this](std::unique_ptr<Frame> frame) {
                            forwardFrame(std::move(frame), RadioType::Remote);
                        },
                        [this](FrameInfo& info) { onAck(info, true); },
                        [this](FrameInfo& info) { onAck(info, false); }) {
}

Result WifiForwarder::init() {
    auto now = Pollable::Clock::now();
    Result res = mRemoteConnection.init();
    if (!res) {
        // It's OK if this fails, the emulator might not have been started with
        // this feature enabled. If it's not enabled we'll try again later. If
        // this does not succeed we don't need to perform the rest of the
        // initialization either. Just let WiFi work the same as normal.
        ALOGE("RemoteConnection failed to initialize: %s", res.c_str());
        mInitDeadline = now + std::chrono::minutes(1);
        return Result::success();
    }

    mAliases.setCurrentTime(now);
    res = mLocalConnection.init(now);
    if (!res) {
        return res;
    }
    mDeadline = now + std::chrono::seconds(1);
    return Result::success();
}

void WifiForwarder::getPollData(std::vector<pollfd>* fds) const {
    int fd = mLocalConnection.getFd();
    if (fd >= 0) {
        struct pollfd pfd = { fd, POLLIN, 0 };
        fds->push_back(pfd);
    }
    fd = mRemoteConnection.getFd();
    if (fd >= 0) {
        struct pollfd pfd = { fd, POLLIN, 0 };
        fds->push_back(pfd);
    }
}

Pollable::Timestamp WifiForwarder::getTimeout() const {
    if (mRemoteConnection.getFd() == -1) {
        return mInitDeadline;
    }
    return std::min(mLocalConnection.getTimeout(), mDeadline);
}

bool WifiForwarder::onReadAvailable(int fd, int* /*status*/) {
    if (fd == mRemoteConnection.getFd()) {
        mRemoteConnection.receive();
    } else if (fd == mLocalConnection.getFd()) {
        mLocalConnection.receive();
    }
    return true;
}

bool WifiForwarder::onClose(int /*fd*/, int* /*status*/) {
    ALOGE("WifiForwarder socket closed unexpectedly");
    return false;
}

bool WifiForwarder::onTimeout(Timestamp now, int* /*status*/) {
    bool success = true;
    if (now >= mInitDeadline) {
        Result res = init();
        success = res.isSuccess();
        if (mRemoteConnection.getFd() == -1) {
            // Remote connection not set up by init, try again later
            mInitDeadline = now + std::chrono::minutes(1);
        }
    }
    mLocalConnection.onTimeout(now);
    if (now >= mDeadline) {
        mDeadline += std::chrono::seconds(1);
        mAliases.setCurrentTime(now);
        mAliases.expireEntries();
    }
    return success;
}

const char* WifiForwarder::radioTypeToStr(RadioType type) const {
    switch (type) {
        case RadioType::Unknown:
            return "Unknown";
        case RadioType::Local:
            return "Local";
        case RadioType::Remote:
            return "Remote";
    }
}

void WifiForwarder::onAck(FrameInfo& info, bool success) {
    RadioType type = mRadios[info.transmitter()];
    if (type == RadioType::Remote) {
        if (kDebugTraffic) {
            ALOGE("] ACK -] " PRIMAC " [ %" PRIu64 " ] success: %s",
                  MACARG(info.transmitter()), info.cookie(),
                  success ? "true" : "false");
        }
        if (!mRemoteConnection.ackFrame(info, success)) {
            ALOGE("WifiForwarder failed to ack remote frame");
        }
    } else if (type == RadioType::Local) {
        if (kDebugTraffic) {
            ALOGE("> ACK -> " PRIMAC " [ %" PRIu64 " ] success: %s",
                  MACARG(info.transmitter()), info.cookie(),
                  success ? "true" : "false");
        }
        if (!mLocalConnection.ackFrame(info, success)) {
            ALOGE("WifiForwarder failed to ack local frame");
        }
    } else {
        ALOGE("Unknown transmitter in ack: " PRIMAC,
              MACARG(info.transmitter()));
    }
}

void WifiForwarder::forwardFrame(std::unique_ptr<Frame> frame,
                                 RadioType sourceType) {
    if (kDebugTraffic) {
        if (!frame->isBeacon() || kDebugBeaconTraffic) {
            bool isRemote = sourceType == RadioType::Remote;
            ALOGE("%c " PRIMAC " -%c " PRIMAC " %s",
                  isRemote ? '[' : '<', isRemote ? ']' : '>',
                  MACARG(frame->source()),
                  MACARG(frame->destination()),
                  frame->str().c_str());
        }
    }

    const MacAddress& source = frame->source();
    const MacAddress& transmitter = frame->transmitter();
    const MacAddress& destination = frame->destination();
    auto& currentType = mRadios[transmitter];
    if (currentType != RadioType::Unknown && currentType != sourceType) {
        ALOGE("Replacing type for MAC " PRIMAC " of type %s with type %s, "
              "this might indicate duplicate MACs on different emulators",
              MACARG(source), radioTypeToStr(currentType),
              radioTypeToStr(sourceType));
    }
    currentType = sourceType;
    mAliases[source] = transmitter;

    bool isMulticast = destination.isMulticast();
    bool sendOnRemote = isMulticast;
    for (const auto& radio : mRadios) {
        const MacAddress& radioAddress = radio.first;
        RadioType radioType = radio.second;
        if (radioAddress == transmitter) {
            // Don't send back to the transmitter
            continue;
        }
        if (sourceType == RadioType::Remote && radioType == RadioType::Remote) {
            // Don't forward frames back to the remote, the remote will have
            // taken care of this.
            continue;
        }
        bool forward = false;
        if (isMulticast || destination == radioAddress) {
            // The frame is either multicast or directly intended for this
            // radio. Forward it.
            forward = true;
        } else {
            auto alias = mAliases.find(destination);
            if (alias != mAliases.end() && alias->second == radioAddress) {
                // The frame is destined for an address that is a known alias
                // for this radio.
                forward = true;
            }
        }
        uint32_t seq = 0;
        if (forward) {
            switch (radioType) {
                case RadioType::Unknown:
                    ALOGE("Attempted to forward frame to unknown radio type");
                    break;
                case RadioType::Local:
                    if (kDebugTraffic) {
                        if (!frame->isBeacon() || kDebugBeaconTraffic) {
                            ALOGE("> " PRIMAC " -> " PRIMAC " %s",
                                  MACARG(frame->source()),
                                  MACARG(frame->destination()),
                                  frame->str().c_str());
                        }
                    }
                    if (isMulticast) {
                        // Clone the frame, it might be reused
                        seq = mLocalConnection.cloneFrame(*frame, radioAddress);
                    } else {
                        // This frame has a specific destination, move it
                        seq = mLocalConnection.transferFrame(std::move(frame),
                                                             radioAddress);
                        // Return so that we don't accidentally reuse the frame.
                        // This should be safe now because it's a unicast frame
                        // so it should not be sent to multiple radios.
                        return;
                    }
                    break;
                case RadioType::Remote:
                    sendOnRemote = true;
                    break;
            }
        }
    }
    if (sendOnRemote && sourceType != RadioType::Remote) {
        if (kDebugTraffic) {
            if (!frame->isBeacon() || kDebugBeaconTraffic) {
                ALOGE("] " PRIMAC " -] " PRIMAC " %s",
                      MACARG(frame->source()),
                      MACARG(frame->destination()),
                      frame->str().c_str());
            }
        }
        // This is either a multicast message or destined for a radio known to
        // be a remote. No need to send multiple times to a remote, the remote
        // will handle that on its own.
        mRemoteConnection.sendFrame(std::move(frame));
    }
}
