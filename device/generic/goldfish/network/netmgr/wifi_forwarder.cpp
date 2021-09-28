/*
 * Copyright 2018, The Android Open Source Project
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

#include "log.h"

#include <inttypes.h>
#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <linux/kernel.h>
#include <qemu_pipe_bp.h>
#include <string.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <pcap/pcap.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static const char kQemuPipeName[] = "qemud:wififorward";

// The largest packet size to capture with pcap on the monitor interface
static const int kPcapSnapLength = 65536;

static const size_t kForwardBufferIncrement = 32768;
static const size_t kForwardBufferMaxSize = 1 << 20;

static const uint32_t kWifiForwardMagic = 0xD5C4B3A2;

struct WifiForwardHeader {
    WifiForwardHeader(uint32_t dataLength, uint32_t radioLength)
        : magic(__cpu_to_le32(kWifiForwardMagic))
        , fullLength(__cpu_to_le32(dataLength + sizeof(WifiForwardHeader)))
        , radioLength(__cpu_to_le32(radioLength)) { }

    uint32_t magic;
    uint32_t fullLength;
    uint32_t radioLength;
} __attribute__((__packed__));

struct RadioTapHeader {
    uint8_t it_version;
    uint8_t it_pad;
    uint16_t it_len;
    uint32_t it_present;
} __attribute__((__packed__));

enum class FrameType {
    Management,
    Control,
    Data,
    Extension
};

enum class ManagementType {
    AssociationRequest,
    AssociationResponse,
    ReassociationRequest,
    ReassociationResponse,
    ProbeRequest,
    ProbeResponse,
    TimingAdvertisement,
    Beacon,
    Atim,
    Disassociation,
    Authentication,
    Deauthentication,
    Action,
    ActionNoAck,
};

enum class ControlType {
    BeamFormingReportPoll,
    VhtNdpAnnouncement,
    ControlFrameExtension,
    ControlWrapper,
    BlockAckReq,
    BlockAck,
    PsPoll,
    Rts,
    Cts,
    Ack,
    CfEnd,
    CfEndCfAck
};

// Since the IEEE 802.11 header can vary in size depending on content we have
// to establish a minimum size that we need to be able to inspect and forward
// the frame. Every frame need to contain at least frame_control, duration_id,
// and addr1.
static const uint32_t kMinimumIeee80211Size = sizeof(uint16_t) +
                                              sizeof(uint16_t) +
                                              sizeof(MacAddress);

WifiForwarder::WifiForwarder(const char* monitorInterfaceName)
    : mInterfaceName(monitorInterfaceName),
      mDeadline(Pollable::Timestamp::max()),
      mMonitorPcap(nullptr),
      mPipeFd(-1) {
}

WifiForwarder::~WifiForwarder() {
    cleanup();
}

Result WifiForwarder::init() {
    if (mMonitorPcap || mPipeFd != -1) {
        return Result::error("WifiForwarder already initialized");
    }

    mPipeFd = qemu_pipe_open_ns(NULL, kQemuPipeName, O_RDWR);
    if (mPipeFd == -1) {
        // It's OK if this fails, the emulator might not have been started with
        // this feature enabled. If it's not enabled we'll try again later, in
        // the meantime there is no point in opening the monitor socket either.
        LOGE("WifiForwarder unable to open QEMU pipe: %s", strerror(errno));
        mDeadline = Pollable::Clock::now() + std::chrono::minutes(1);
        return Result::success();
    }

    char errorMsg[PCAP_ERRBUF_SIZE];
    memset(errorMsg, 0, sizeof(errorMsg));
    mMonitorPcap = pcap_create(mInterfaceName.c_str(), errorMsg);
    if (mMonitorPcap == nullptr) {
        return Result::error("WifiForwarder cannot create pcap handle: %s",
                             errorMsg);
    }
    int result = pcap_set_snaplen(mMonitorPcap, kPcapSnapLength);
    if (result != 0) {
        return Result::error("WifiForwader cannot set pcap snap length: %s",
                             pcap_statustostr(result));
    }

    result = pcap_set_promisc(mMonitorPcap, 1);
    if (result != 0) {
        return Result::error("WifiForwader cannot set pcap promisc mode: %s",
                             pcap_statustostr(result));
    }

    result = pcap_set_immediate_mode(mMonitorPcap, 1);
    if (result != 0) {
        return Result::error("WifiForwader cannot set pcap immediate mode: %s",
                             pcap_statustostr(result));
    }

    result = pcap_activate(mMonitorPcap);
    if (result > 0) {
        // A warning, log it but keep going
        LOGW("WifiForwader received warnings when activating pcap: %s",
             pcap_statustostr(result));
    } else if (result < 0) {
        // An error, return
        return Result::error("WifiForwader unable to activate pcap: %s",
                             pcap_statustostr(result));
    }

    int datalinkType = pcap_datalink(mMonitorPcap);
    if (datalinkType != DLT_IEEE802_11_RADIO) {
        // Unexpected data link encapsulation, we don't support this
        return Result::error("WifiForwarder detected incompatible data link "
                             "encapsulation: %d", datalinkType);
    }
    // All done
    return Result::success();
}


void WifiForwarder::getPollData(std::vector<pollfd>* fds) const {
    if (mPipeFd == -1) {
        return;
    }
    int pcapFd = pcap_get_selectable_fd(mMonitorPcap);
    if (pcapFd != -1) {
        fds->push_back(pollfd{pcapFd, POLLIN, 0});
    } else {
        LOGE("WifiForwarder unable to get pcap fd");
    }
    if (mPipeFd != -1) {
        fds->push_back(pollfd{mPipeFd, POLLIN, 0});
    }
}

Pollable::Timestamp WifiForwarder::getTimeout() const {
    // If there is no pipe return the deadline, we're going to retry, otherwise
    // use an infinite timeout.
    return mPipeFd == -1 ? mDeadline : Pollable::Timestamp::max();
}

bool WifiForwarder::onReadAvailable(int fd, int* /*status*/) {
    if (fd == mPipeFd) {
        injectFromPipe();
    } else {
        forwardFromPcap();
    }
    return true;
}

void WifiForwarder::forwardFromPcap() {
    struct pcap_pkthdr* header = nullptr;
    const u_char* data = nullptr;
    int result = pcap_next_ex(mMonitorPcap, &header, &data);
    if (result == 0) {
        // Timeout, nothing to do
        return;
    } else if (result < 0) {
        LOGE("WifiForwarder failed to read from pcap: %s",
             pcap_geterr(mMonitorPcap));
        return;
    }
    if (header->caplen < header->len) {
        LOGE("WifiForwarder received packet exceeding capture length: %u < %u",
             header->caplen, header->len);
        return;
    }

    if (mPipeFd == -1) {
        LOGE("WifiForwarder unable to forward data, pipe not open");
        return;
    }

    if (header->caplen < sizeof(RadioTapHeader)) {
        // This packet is too small to be a valid radiotap packet, drop it
        LOGE("WifiForwarder captured packet that is too small: %u",
             header->caplen);
        return;
    }

    auto radiotap = reinterpret_cast<const RadioTapHeader*>(data);
    uint32_t radioLen = __le16_to_cpu(radiotap->it_len);
    if (header->caplen < radioLen + kMinimumIeee80211Size) {
        // This packet is too small to contain a valid IEEE 802.11 frame
        LOGE("WifiForwarder captured packet that is too small: %u < %u",
             header->caplen, radioLen + kMinimumIeee80211Size);
        return;
    }

    WifiForwardHeader forwardHeader(header->caplen, radioLen);

    if (qemu_pipe_write_fully(mPipeFd, &forwardHeader, sizeof(forwardHeader))) {
        LOGE("WifiForwarder failed to write to pipe: %s", strerror(errno));
        return;
    }

    if (qemu_pipe_write_fully(mPipeFd, data, header->caplen)) {
        LOGE("WifiForwarder failed to write to pipe: %s", strerror(errno));
        return;
    }
}

void WifiForwarder::injectFromPipe() {
    size_t start = mMonitorBuffer.size();
    size_t newSize = start + kForwardBufferIncrement;
    if (newSize > kForwardBufferMaxSize) {
        // We've exceeded the maximum allowed size, drop everything we have so
        // far and start over. This is most likely caused by some delay in
        // injection or the injection failing in which case keeping old data
        // around isn't going to be very useful.
        LOGE("WifiForwarder ran out of buffer space");
        newSize = kForwardBufferIncrement;
        start = 0;
    }
    mMonitorBuffer.resize(newSize);

    while (true) {
        int result = ::read(mPipeFd,
                            mMonitorBuffer.data() + start,
                            mMonitorBuffer.size() - start);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOGE("WifiForwarder failed to read to forward buffer: %s",
                 strerror(errno));
            // Return the buffer to its previous size
            mMonitorBuffer.resize(start);
            return;
        } else if (result == 0) {
            // Nothing received, nothing to write
            // Return the buffer to its previous size
            mMonitorBuffer.resize(start);
            LOGE("WifiForwarder did not receive anything to inject");
            return;
        }
        // Adjust the buffer size to match everything we recieved
        mMonitorBuffer.resize(start + static_cast<size_t>(result));
        break;
    }

    while (mMonitorBuffer.size() >=
           sizeof(WifiForwardHeader) + sizeof(RadioTapHeader)) {
        auto fwd = reinterpret_cast<WifiForwardHeader*>(mMonitorBuffer.data());
        if (__le32_to_cpu(fwd->magic) != kWifiForwardMagic) {
            // We are not properly aligned, this can happen for the first read
            // if the client or server happens to send something that's in the
            // middle of a stream. Attempt to find the next packet boundary.
            LOGE("WifiForwarder found incorrect magic, finding next magic");
            uint32_t le32magic = __cpu_to_le32(kWifiForwardMagic);
            auto next = reinterpret_cast<unsigned char*>(
                    ::memmem(mMonitorBuffer.data(), mMonitorBuffer.size(),
                             &le32magic, sizeof(le32magic)));
            if (next) {
                // We've found a possible candidate, erase everything before
                size_t length = next - mMonitorBuffer.data();
                mMonitorBuffer.erase(mMonitorBuffer.begin(),
                                     mMonitorBuffer.begin() + length);
                continue;
            } else {
                // There is no possible candidate, drop everything except the
                // last three bytes. The last three bytes could possibly be the
                // start of the next magic without actually triggering the
                // search above.
                if (mMonitorBuffer.size() > 3) {
                    mMonitorBuffer.erase(mMonitorBuffer.begin(),
                                         mMonitorBuffer.end() - 3);
                }
                // In this case there is nothing left to parse so just return
                // right away.
                return;
            }
        }
        // The length according to the wifi forward header
        const size_t fullLength = __le32_to_cpu(fwd->fullLength);
        const size_t payloadLength = fullLength - sizeof(WifiForwardHeader);
        const size_t radioLength = __le32_to_cpu(fwd->radioLength);
        // Get the radio tap header, right after the wifi forward header
        unsigned char* radioTapLocation = mMonitorBuffer.data() + sizeof(*fwd);
        auto hdr = reinterpret_cast<RadioTapHeader*>(radioTapLocation);
        const size_t radioHdrLength = __le16_to_cpu(hdr->it_len);

        if (radioLength != radioHdrLength) {
            LOGE("WifiForwarder radiotap (%u), forwarder (%u) length mismatch",
                 (unsigned)(radioHdrLength), (unsigned)radioLength);
            // The wifi forward header radio length does not match up with the
            // radiotap header length. Either this was not an actual packet
            // boundary or the packet is malformed. Remove a single byte from
            // the buffer to trigger a new magic marker search.
            mMonitorBuffer.erase(mMonitorBuffer.begin(),
                                 mMonitorBuffer.begin() + 1);
            continue;
        }
        // At this point we have verified that the magic marker is present and
        // that the length in the wifi forward header matches the radiotap
        // header length. We're now reasonably sure this is actually a valid
        // packet that we can process.

        if (fullLength > mMonitorBuffer.size()) {
            // We have not received enough data yet, wait for more to arrive.
            return;
        }

        if (hdr->it_version != 0) {
            // Unknown header version, skip this packet because we don't know
            // how to handle it.
            LOGE("WifiForwarder encountered unknown radiotap version %u",
                 static_cast<unsigned>(hdr->it_version));
            mMonitorBuffer.erase(mMonitorBuffer.begin(),
                                 mMonitorBuffer.begin() + fullLength);
            continue;
        }

        if (mMonitorPcap) {
            // A sufficient amount of data has arrived, forward it.
            int result = pcap_inject(mMonitorPcap, hdr, payloadLength);
            if (result < 0) {
                LOGE("WifiForwarder failed to inject %" PRIu64 " bytes: %s",
                     static_cast<uint64_t>(payloadLength),
                     pcap_geterr(mMonitorPcap));
            } else if (static_cast<size_t>(result) < payloadLength) {
                LOGE("WifiForwarder only injected %d out of %" PRIu64 " bytes",
                     result, static_cast<uint64_t>(payloadLength));
            }
        } else {
            LOGE("WifiForwarder could not forward to monitor, pcap not set up");
        }
        mMonitorBuffer.erase(mMonitorBuffer.begin(),
                             mMonitorBuffer.begin() + fullLength);
    }

}

void WifiForwarder::cleanup() {
    if (mMonitorPcap) {
        pcap_close(mMonitorPcap);
        mMonitorPcap = nullptr;
    }
    if (mPipeFd != -1) {
        ::close(mPipeFd);
        mPipeFd = -1;
    }
}

bool WifiForwarder::onClose(int /*fd*/, int* status) {
    // Don't care which fd, just start all over again for simplicity
    cleanup();
    Result res = init();
    if (!res) {
        *status = 1;
        return false;
    }
    return true;
}

bool WifiForwarder::onTimeout(int* status) {
    if (mPipeFd == -1 && mMonitorPcap == nullptr) {
        Result res = init();
        if (!res) {
            *status = 1;
            return false;
        }
    }
    return true;
}

