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

#include "local_connection.h"

#include "hwsim.h"
#include "log.h"
#include "macaddress.h"
#include "netlink_message.h"

#include <net/ethernet.h>
#include <netlink/netlink.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>

#include <utils/CallStack.h>

#include <inttypes.h>
#include <stdlib.h>

static const char kHwSimFamilyName[] = "MAC80211_HWSIM";
static const int kHwSimVersion = 1;
static const unsigned int kDefaultSignalStrength = -50;
static const int kDefaultSocketBufferSize = 8 * (1 << 20);

static uint32_t getSeqNum(struct nl_msg* msg) {
    return nlmsg_hdr(msg)->nlmsg_seq;
}

static int onSent(struct nl_msg* , void*) {
    return NL_OK;
}

static int onSeqCheck(struct nl_msg* msg, void*) {
    uint32_t seq = getSeqNum(msg);
    return seq == 0 ? NL_SKIP : NL_OK;
}

LocalConnection::LocalConnection(OnFrameCallback onFrameCallback,
                                 OnAckCallback onAckCallback,
                                 OnErrorCallback onErrorCallback)
    : mOnFrameCallback(onFrameCallback)
    , mOnAckCallback(onAckCallback)
    , mOnErrorCallback(onErrorCallback) {

}

Result LocalConnection::init(std::chrono::steady_clock::time_point now) {
    Result res = mNetlinkSocket.init();
    if (!res) { return res; }
    res = mNetlinkSocket.setOnMsgInCallback(staticOnMessage, this);
    if (!res) { return res; }
    res = mNetlinkSocket.setOnMsgOutCallback(onSent, this);
    if (!res) { return res; }
    res = mNetlinkSocket.setOnSeqCheckCallback(onSeqCheck, this);
    if (!res) { return res; }
    res = mNetlinkSocket.setOnAckCallback(staticOnAck, this);
    if (!res) { return res; }
    res = mNetlinkSocket.setOnErrorCallback(staticOnError, this);
    if (!res) { return res; }
    res = mNetlinkSocket.connectGeneric();
    if (!res) { return res; }
    res = mNetlinkSocket.setBufferSizes(kDefaultSocketBufferSize,
                                        kDefaultSocketBufferSize);
    if (!res) { return res; }

    mNetlinkFamily = mNetlinkSocket.resolveNetlinkFamily(kHwSimFamilyName);
    if (mNetlinkFamily < 0) {
        return Result::error("Failed to resolve netlink family name: %s",
                             nl_geterror(mNetlinkFamily));
    }

    mPendingFrames.setCurrentTime(now);
    mSequenceNumberCookies.setCurrentTime(now);

    mLastCacheTimeUpdate = now;
    mLastCacheExpiration = now;

    return registerReceiver();
}

int LocalConnection::getFd() const {
    return mNetlinkSocket.getFd();
}

bool LocalConnection::receive() {
    return mNetlinkSocket.receive();
}

uint32_t LocalConnection::transferFrame(std::unique_ptr<Frame> frame,
                                        const MacAddress& dest) {
    NetlinkMessage msg;

    if (!msg.initGeneric(mNetlinkFamily, HWSIM_CMD_FRAME, kHwSimVersion)) {
        ALOGE("LocalConnection transferFrame failed to init msg");
        return 0;
    }

    frame->incrementAttempts();

    if (!msg.addAttribute(HWSIM_ATTR_ADDR_RECEIVER, dest.addr, ETH_ALEN) ||
        !msg.addAttribute(HWSIM_ATTR_FRAME, frame->data(), frame->size()) ||
        !msg.addAttribute(HWSIM_ATTR_RX_RATE, 1u) ||
        !msg.addAttribute(HWSIM_ATTR_SIGNAL, kDefaultSignalStrength) ||
        !msg.addAttribute(HWSIM_ATTR_FREQ, frame->channel())) {

        ALOGE("LocalConnection transferFrame failed to set attrs");
        return 0;
    }

    if (!mNetlinkSocket.send(msg)) {
        return 0;
    }

    // Store the radio destination for potential retransmissions.
    frame->setRadioDestination(dest);

    uint32_t seqNum = msg.getSeqNum();
    uint64_t cookie = frame->cookie();
    FrameId id(cookie, frame->transmitter());
    mSequenceNumberCookies[seqNum] = id;
    mPendingFrames[id] = std::move(frame);

    return seqNum;
}

uint32_t LocalConnection::cloneFrame(const Frame& frame,
                                     const MacAddress& dest) {
    auto copy = std::make_unique<Frame>(frame.data(),
                                        frame.size(),
                                        frame.transmitter(),
                                        frame.cookie(),
                                        frame.flags(),
                                        frame.channel(),
                                        frame.rates().data(),
                                        frame.rates().size());
    return transferFrame(std::move(copy), dest);
}

bool LocalConnection::ackFrame(FrameInfo& info, bool success) {
    NetlinkMessage msg;

    if (!msg.initGeneric(mNetlinkFamily,
                         HWSIM_CMD_TX_INFO_FRAME,
                         kHwSimVersion)) {
        ALOGE("LocalConnection ackFrame failed to create msg");
        return false;
    }

    uint32_t flags = info.flags();
    if (success) {
        flags |= HWSIM_TX_STAT_ACK;
    }

    const uint8_t* transmitter = info.transmitter().addr;
    const Rates& rates = info.rates();
    if (!msg.addAttribute(HWSIM_ATTR_ADDR_TRANSMITTER, transmitter, ETH_ALEN) ||
        !msg.addAttribute(HWSIM_ATTR_TX_INFO, rates.data(), rates.size()) ||
        !msg.addAttribute(HWSIM_ATTR_FLAGS, flags) ||
        !msg.addAttribute(HWSIM_ATTR_SIGNAL, kDefaultSignalStrength) ||
        !msg.addAttribute(HWSIM_ATTR_COOKIE, info.cookie())) {

        ALOGE("LocalConnection ackFrame failed to set attributes");
        return false;
    }

    if (!mNetlinkSocket.send(msg)) {
        return false;
    }
    mPendingFrames.erase(FrameId(info.cookie(), info.transmitter()));
    return true;
}

std::chrono::steady_clock::time_point LocalConnection::getTimeout() const {
    if (mRetryQueue.empty()) {
        return std::chrono::steady_clock::time_point::max();
    }
    return mRetryQueue.top().first;
}

void LocalConnection::onTimeout(std::chrono::steady_clock::time_point now) {

    if (now - mLastCacheTimeUpdate > std::chrono::seconds(1)) {
        // Only update the time once per second, there's no need for a super
        // high resolution here. We just want to make sure these caches don't
        // fill up over a long period of time.
        mPendingFrames.setCurrentTime(now);
        mSequenceNumberCookies.setCurrentTime(now);
        mLastCacheTimeUpdate = now;
    }
    if (now - mLastCacheExpiration > std::chrono::seconds(10)) {
        // Only expire entries every 10 seconds, this is an operation that has
        // some cost to it and doesn't have to happen very often.
        mPendingFrames.expireEntries();
        mSequenceNumberCookies.expireEntries();
        mLastCacheExpiration = now;
    }

    while (!mRetryQueue.empty() && now >= mRetryQueue.top().first) {
        FrameId id = mRetryQueue.top().second;
        auto frameIt = mPendingFrames.find(id);
        if (frameIt != mPendingFrames.end()) {
            // Frame is still available, retry it
            std::unique_ptr<Frame> frame = std::move(frameIt->second);
            mPendingFrames.erase(frameIt);
            MacAddress dest = frame->radioDestination();
            transferFrame(std::move(frame), dest);
        }
        mRetryQueue.pop();
    }
}

Result LocalConnection::registerReceiver() {
    NetlinkMessage msg;

    if (!msg.initGeneric(mNetlinkFamily, HWSIM_CMD_REGISTER, kHwSimVersion)) {
        return Result::error("Failed to create register receiver message");
    }

    if (!mNetlinkSocket.send(msg)) {
        return Result::error("Failed to send register receiver message");
    }
    return Result::success();
}

int LocalConnection::staticOnMessage(struct nl_msg* msg, void* context) {
    if (!context) {
        return NL_SKIP;
    }
    auto connection = static_cast<LocalConnection*>(context);
    return connection->onMessage(msg);
}

int LocalConnection::staticOnAck(struct nl_msg* msg, void* context) {
    if (!context) {
        return NL_SKIP;
    }
    auto connection = static_cast<LocalConnection*>(context);
    return connection->onAck(msg);
}

int LocalConnection::staticOnError(struct sockaddr_nl* addr,
                                   struct nlmsgerr* error,
                                   void* context) {
    if (!context) {
        return NL_SKIP;
    }
    auto connection = static_cast<LocalConnection*>(context);
    return connection->onError(addr, error);
}

int LocalConnection::onMessage(struct nl_msg* msg) {
    struct nlmsghdr* hdr = nlmsg_hdr(msg);
    auto generic = reinterpret_cast<const struct genlmsghdr*>(nlmsg_data(hdr));

    switch (generic->cmd) {
        case HWSIM_CMD_FRAME:
            return onFrame(msg);
    }
    return NL_OK;
}

int LocalConnection::onFrame(struct nl_msg* msg) {

    struct nlmsghdr* hdr = nlmsg_hdr(msg);
    std::unique_ptr<Frame> frame = parseFrame(hdr);
    if (!frame) {
        return NL_SKIP;
    }

    mOnFrameCallback(std::move(frame));
    return NL_OK;
}

std::unique_ptr<Frame> LocalConnection::parseFrame(struct nlmsghdr* hdr) {
    struct nlattr* attrs[HWSIM_ATTR_MAX + 1];
    genlmsg_parse(hdr, 0, attrs, HWSIM_ATTR_MAX, nullptr);
    if (!attrs[HWSIM_ATTR_ADDR_TRANSMITTER]) {
        ALOGE("Received cmd frame without transmitter address");
        return nullptr;
    }
    if (!attrs[HWSIM_ATTR_TX_INFO]) {
        ALOGE("Received cmd frame without tx rates");
        return nullptr;
    }
    if (!attrs[HWSIM_ATTR_FREQ]) {
        ALOGE("Recieved cmd frame without channel frequency");
        return nullptr;
    }

    uint64_t cookie = nla_get_u64(attrs[HWSIM_ATTR_COOKIE]);

    const auto& source = *reinterpret_cast<const MacAddress*>(
        nla_data(attrs[HWSIM_ATTR_ADDR_TRANSMITTER]));

    auto rates = reinterpret_cast<const hwsim_tx_rate*>(
        nla_data(attrs[HWSIM_ATTR_TX_INFO]));
    int rateLength = nla_len(attrs[HWSIM_ATTR_TX_INFO]);
    // Make sure the length is valid, must be multiple of hwsim_tx_rate
    if (rateLength <= 0 || (rateLength % sizeof(hwsim_tx_rate)) != 0) {
        ALOGE("Invalid tx rate length %d", rateLength);
    }
    size_t numRates = static_cast<size_t>(rateLength) / sizeof(hwsim_tx_rate);

    int length = nla_len(attrs[HWSIM_ATTR_FRAME]);
    auto data = reinterpret_cast<uint8_t*>(nla_data(attrs[HWSIM_ATTR_FRAME]));

    uint32_t flags = nla_get_u32(attrs[HWSIM_ATTR_FLAGS]);

    uint32_t channel = nla_get_u32(attrs[HWSIM_ATTR_FREQ]);

    return std::make_unique<Frame>(data, length, source, cookie,
                                   flags, channel, rates, numRates);
}

int LocalConnection::onAck(struct nl_msg* msg) {
    struct nlmsghdr* hdr = nlmsg_hdr(msg);
    uint32_t seqNum = hdr->nlmsg_seq;
    auto cookie = mSequenceNumberCookies.find(seqNum);
    if (cookie == mSequenceNumberCookies.end()) {
        // This is not a frame we sent. This is fairly common for libnl's
        // internal use so don't log this.
        return NL_SKIP;
    }
    auto pendingFrame = mPendingFrames.find(cookie->second);
    // We don't need to keep this around anymore, erase it.
    mSequenceNumberCookies.erase(seqNum);
    if (pendingFrame == mPendingFrames.end()) {
        // We have no cookie associated with this sequence number. This might
        // happen if the remote connection already acked the frame and removed
        // the frame info. Consider this resolved.
        return NL_SKIP;
    }

    Frame* frame = pendingFrame->second.get();
    mOnAckCallback(frame->info());
    // Make sure to erase using the cookie instead of the iterator. The callback
    // might have already erased this entry so the iterator could be invalid at
    // this point. Instead of a fancy scheme of checking this just play it safe
    // to allow the callback more freedom.
    mPendingFrames.erase(cookie->second);

    return NL_OK;
}

int LocalConnection::onError(struct sockaddr_nl*, struct nlmsgerr* error) {
    struct nlmsghdr* hdr = &error->msg;
    uint32_t seqNum = hdr->nlmsg_seq;

    auto idIt = mSequenceNumberCookies.find(seqNum);
    if (idIt == mSequenceNumberCookies.end()) {
        return NL_SKIP;
    }
    FrameId id = idIt->second;
    // No need to keep the sequence number around anymore, it's expired and is
    // no longer useful.
    mSequenceNumberCookies.erase(idIt);

    auto frameIt = mPendingFrames.find(id);
    if (frameIt == mPendingFrames.end()) {
        return NL_SKIP;
    }
    Frame* frame = frameIt->second.get();

    if (!frame->hasRemainingAttempts()) {
        // This frame has used up all its attempts, there's nothing we can do
        mOnErrorCallback(frame->info());
        mPendingFrames.erase(id);
        return NL_SKIP;
    }
    // Store the frame in the retry queue
    uint64_t timeout = frame->calcNextTimeout();
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::microseconds(timeout);
    mRetryQueue.emplace(deadline, id);

    return NL_SKIP;
}

