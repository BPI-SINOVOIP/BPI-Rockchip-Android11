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

#include "remote_connection.h"

#include "hwsim.h"
#include "frame.h"
#include "log.h"

#include <errno.h>
#include <linux/kernel.h>
#include <netinet/in.h>
#include <qemu_pipe_bp.h>
#include <sys/uio.h>
#include <unistd.h>

static const char kQemuPipeName[] = "qemud:wififorward";
static const size_t kReceiveBufferIncrement = 32768;
static const size_t kReceiveBufferMaxSize = 1 << 20;

static const uint8_t kWifiForwardVersion = 0x01;
static const uint32_t kWifiForwardMagic = 0xD6C4B3A2;

// This matches with the kernel constant IEEE80211_TX_MAX_RATES in
// include/net/mac80211.h in the kernel tree.
static const uint32_t kMaxNumRates = 4;

struct WifiForwardHeader {
    WifiForwardHeader(FrameType type, const MacAddress& transmitter,
                      uint32_t fullLength, uint64_t cookie,
                      uint32_t flags, uint32_t channel, uint32_t numRates,
                      const hwsim_tx_rate* txRates)
        : magic(__cpu_to_le32(kWifiForwardMagic))
        , version(kWifiForwardVersion)
        , type(static_cast<uint16_t>(type))
        , transmitter(transmitter)
        , dataOffset(sizeof(WifiForwardHeader))
        , fullLength(__cpu_to_le32(fullLength))
        , cookie(__cpu_to_le64(cookie))
        , flags(__cpu_to_le32(flags))
        , channel(__cpu_to_le32(channel))
        , numRates(__cpu_to_le32(numRates)) {
            memcpy(rates, txRates, std::min(numRates, kMaxNumRates));
            if (numRates < kMaxNumRates) {
                memset(&rates[numRates],
                       0,
                       sizeof(rates[0]) * (kMaxNumRates - numRates));
            }
        }

    uint32_t magic;
    uint8_t version;
    uint8_t type;
    MacAddress transmitter;
    uint16_t dataOffset;
    uint32_t fullLength;
    uint64_t cookie;
    uint32_t flags;
    uint32_t channel;
    uint32_t numRates;
    hwsim_tx_rate rates[kMaxNumRates];
} __attribute__((__packed__));

RemoteConnection::RemoteConnection(OnFrameCallback onFrameCallback,
                                   OnAckCallback onAckCallback,
                                   OnErrorCallback onErrorCallback)
    : mOnFrameCallback(onFrameCallback)
    , mOnAckCallback(onAckCallback)
    , mOnErrorCallback(onErrorCallback) {

}

RemoteConnection::~RemoteConnection() {
    if (mPipeFd != -1) {
        ::close(mPipeFd);
        mPipeFd = -1;
    }
}

Result RemoteConnection::init() {
    if (mPipeFd != -1) {
        return Result::error("RemoteConnetion already initialized");
    }

    mPipeFd = qemu_pipe_open_ns(NULL, kQemuPipeName, O_RDWR);
    if (mPipeFd == -1) {
        return Result::error("RemoteConnection failed to open pipe");
    }
    return Result::success();
}

Pollable::Timestamp RemoteConnection::getTimeout() const {
    // If there is no pipe return the deadline, we're going to retry, otherwise
    // use an infinite timeout.
    return mPipeFd == -1 ? mDeadline : Pollable::Timestamp::max();
}

void RemoteConnection::receive() {
    size_t start = mBuffer.size();
    size_t newSize = start + kReceiveBufferIncrement;
    if (newSize > kReceiveBufferMaxSize) {
        // We've exceeded the maximum allowed size, drop everything we have so
        // far and start over. This is most likely caused by some delay in
        // injection or the injection failing in which case keeping old data
        // around isn't going to be very useful.
        ALOGE("RemoteConnection ran out of buffer space");
        newSize = kReceiveBufferIncrement;
        start = 0;
    }
    mBuffer.resize(newSize);

    while (true) {
        int result = ::read(mPipeFd,
                            mBuffer.data() + start,
                            mBuffer.size() - start);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            ALOGE("RemoteConnection failed to read to forward buffer: %s",
                 strerror(errno));
            // Return the buffer to its previous size
            mBuffer.resize(start);
            return;
        } else if (result == 0) {
            // Nothing received, nothing to write
            // Return the buffer to its previous size
            mBuffer.resize(start);
            ALOGE("RemoteConnection did not receive anything to inject");
            return;
        }
        // Adjust the buffer size to match everything we recieved
        mBuffer.resize(start + static_cast<size_t>(result));
        break;
    }

    while (mBuffer.size() >= sizeof(WifiForwardHeader)) {
        auto fwd = reinterpret_cast<WifiForwardHeader*>(mBuffer.data());
        if (__le32_to_cpu(fwd->magic) != kWifiForwardMagic) {
            // We are not properly aligned, this can happen for the first read
            // if the client or server happens to send something that's in the
            // middle of a stream. Attempt to find the next packet boundary.
            ALOGE("RemoteConnection found incorrect magic, finding next magic");
            uint32_t le32magic = __cpu_to_le32(kWifiForwardMagic);
            auto next = reinterpret_cast<unsigned char*>(
                    ::memmem(mBuffer.data(), mBuffer.size(),
                             &le32magic, sizeof(le32magic)));
            if (next) {
                // We've found a possible candidate, erase everything before
                size_t length = next - mBuffer.data();
                mBuffer.erase(mBuffer.begin(), mBuffer.begin() + length);
                continue;
            } else {
                // There is no possible candidate, drop everything except the
                // last three bytes. The last three bytes could possibly be the
                // start of the next magic without actually triggering the
                // search above.
                if (mBuffer.size() > 3) {
                    mBuffer.erase(mBuffer.begin(), mBuffer.end() - 3);
                }
                // In this case there is nothing left to parse so just return
                // right away.
                return;
            }
        }
        // Check if we support this version
        if (fwd->version != kWifiForwardVersion) {
            // Unsupported version
            if (!mReportedVersionMismatches.test(fwd->version)) {
                // Only report these once per version or this might become
                // very spammy.
                ALOGE("RemoteConnection encountered unknown version %u",
                     fwd->version);
                mReportedVersionMismatches.set(fwd->version);
            }
            // Drop the magic from the buffer and attempt to find the next magic
            mBuffer.erase(mBuffer.begin(), mBuffer.begin() + 4);
            continue;
        }
        // The length according to the wifi forward header
        const uint32_t fullLength = __le32_to_cpu(fwd->fullLength);
        const uint16_t offset = __le16_to_cpu(fwd->dataOffset);
        if (offset < sizeof(WifiForwardHeader) || offset > fullLength) {
            // The frame offset is not large enough to go past the header
            // or it's outside of the bounds of the length of the frame.
            ALOGE("Invalid data offset in header %u, full length is %u",
                 offset, fullLength);
            // Erase the magic and try again
            mBuffer.erase(mBuffer.begin(), mBuffer.begin() + 4);
            continue;
        }
        const size_t frameLength = fullLength - offset;

        if (fullLength > mBuffer.size()) {
            // We have not received enough data yet, wait for more to arrive.
            return;
        }

        FrameType type = frameTypeFromByte(fwd->type);
        if (frameLength == 0 && type != FrameType::Ack) {
            ALOGE("Received empty frame for non-ack frame");
            return;
        }
        unsigned char* frameData = mBuffer.data() + offset;
        if (type == FrameType::Ack) {
            FrameInfo info(fwd->transmitter,
                           __le64_to_cpu(fwd->cookie),
                           __le32_to_cpu(fwd->flags),
                           __le32_to_cpu(fwd->channel),
                           fwd->rates,
                           __le32_to_cpu(fwd->numRates));

            if (info.flags() & HWSIM_TX_STAT_ACK) {
                mOnAckCallback(info);
            } else {
                mOnErrorCallback(info);
            }
        } else if (type == FrameType::Data) {
            auto frame = std::make_unique<Frame>(frameData,
                                                 frameLength,
                                                 fwd->transmitter,
                                                 __le64_to_cpu(fwd->cookie),
                                                 __le32_to_cpu(fwd->flags),
                                                 __le32_to_cpu(fwd->channel),
                                                 fwd->rates,
                                                 __le32_to_cpu(fwd->numRates));
            mOnFrameCallback(std::move(frame));
        } else {
            ALOGE("Received unknown message type %u from remote",
                 static_cast<uint8_t>(fwd->type));
        }

        mBuffer.erase(mBuffer.begin(), mBuffer.begin() + fullLength);
    }
}

bool RemoteConnection::sendFrame(std::unique_ptr<Frame> frame) {
    if (mPipeFd == -1) {
        ALOGE("RemoteConnection unable to forward data, pipe not open");
        return false;
    }

    WifiForwardHeader header(FrameType::Data,
                             frame->transmitter(),
                             frame->size() + sizeof(WifiForwardHeader),
                             frame->cookie(),
                             frame->flags(),
                             frame->channel(),
                             frame->rates().size(),
                             frame->rates().data());
#if 1
    constexpr size_t count = 2;
    struct iovec iov[count];
    iov[0].iov_base = &header;
    iov[0].iov_len = sizeof(header);
    iov[1].iov_base = frame->data();
    iov[1].iov_len = frame->size();

    size_t totalSize = iov[0].iov_len + iov[1].iov_len;

    size_t current = 0;
    for (;;) {
        ssize_t written = ::writev(mPipeFd, iov + current, count - current);
        if (written < 0) {
            ALOGE("RemoteConnection failed to write to pipe: %s",
                  strerror(errno));
            return false;
        }
        if (static_cast<size_t>(written) == totalSize) {
            // Optimize for most common case, everything was written
            break;
        }
        totalSize -= written;
        // Determine how much is left to write after this
        while (current < count && written >= iov[current].iov_len) {
            written -= iov[current++].iov_len;
        }
        if (current == count) {
            break;
        }
        iov[current].iov_base =
            reinterpret_cast<char*>(iov[current].iov_base) + written;
        iov[current].iov_len -= written;
    }
#else
    if (qemu_pipe_write_fully(mPipeFd, &header, sizeof(header))) {
        ALOGE("RemoteConnection failed to write to pipe: %s", strerror(errno));
        return false;
    }

    if (qemu_pipe_write_fully(mPipeFd, frame->data(), frame->size())) {
        ALOGE("RemoteConnection failed to write to pipe: %s", strerror(errno));
        return false;
    }
#endif
    return true;
}

bool RemoteConnection::ackFrame(FrameInfo& info, bool success) {
    uint32_t flags = info.flags();
    if (success) {
        flags |= HWSIM_TX_STAT_ACK;
    }
    WifiForwardHeader header(FrameType::Ack,
                             info.transmitter(),
                             sizeof(WifiForwardHeader),
                             info.cookie(),
                             flags,
                             info.channel(),
                             info.rates().size(),
                             info.rates().data());

    if (qemu_pipe_write_fully(mPipeFd, &header, sizeof(header))) {
        ALOGE("RemoteConnection failed to write to pipe: %s", strerror(errno));
        return false;
    }
    return true;
}
