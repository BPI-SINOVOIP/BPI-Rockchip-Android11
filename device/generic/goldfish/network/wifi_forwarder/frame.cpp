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

#include "frame.h"

#include "hwsim.h"
#include "ieee80211.h"

#include <asm/byteorder.h>

#include <sstream>

static constexpr uint64_t kSlotTime = 9;

static const AccessCategory kPriorityToAc[8] = {
    AccessCategory::BestEffort,
    AccessCategory::Background,
    AccessCategory::Background,
    AccessCategory::BestEffort,
    AccessCategory::Video,
    AccessCategory::Video,
    AccessCategory::Voice,
    AccessCategory::Voice,
};

static uint32_t calcContentionWindowMin(AccessCategory ac) {
    switch (ac) {
        case AccessCategory::Voice:
            return 3;
        case AccessCategory::Video:
            return 7;
        case AccessCategory::BestEffort:
            return 15;
        case AccessCategory::Background:
            return 15;
    }
}

static uint32_t calcContentionWindowMax(AccessCategory ac) {
    switch (ac) {
        case AccessCategory::Voice:
            return 7;
        case AccessCategory::Video:
            return 15;
        case AccessCategory::BestEffort:
            return 1023;
        case AccessCategory::Background:
            return 1023;
    }
}

FrameType frameTypeFromByte(uint8_t byte) {
    if (byte == static_cast<uint8_t>(FrameType::Ack)) {
        return FrameType::Ack;
    } else if (byte == static_cast<uint8_t>(FrameType::Data)) {
        return FrameType::Data;
    }
    return FrameType::Unknown;
}

FrameInfo::FrameInfo(const MacAddress& transmitter,
                     uint64_t cookie,
                     uint32_t flags,
                     uint32_t channel,
                     const hwsim_tx_rate* rates,
                     size_t numRates)
    : mTransmitter(transmitter)
    , mCookie(cookie)
    , mFlags(flags)
    , mChannel(channel) {
    size_t i = 0;
    for (; i < numRates; ++i) {
        mTxRates[i].count = 0;
        mTxRates[i].idx = rates[i].idx;
    }
    for (; i < mTxRates.size(); ++i) {
        mTxRates[i].count = 0;
        mTxRates[i].idx = -1;
    }
}

bool FrameInfo::shouldAck() const {
    return !!(mFlags & HWSIM_TX_CTL_REQ_TX_STATUS);
}

Frame::Frame(const uint8_t* data, size_t size) : mData(data, data + size) {
}

Frame::Frame(const uint8_t* data, size_t size, const MacAddress& transmitter,
             uint64_t cookie, uint32_t flags, uint32_t channel,
             const hwsim_tx_rate* rates, size_t numRates)
    : mData(data, data + size)
    , mInfo(transmitter, cookie, flags, channel, rates, numRates)
    , mContentionWindow(calcContentionWindowMin(getAc()))
    , mContentionWindowMax(calcContentionWindowMax(getAc())) {
    memcpy(mInitialTxRates.data(), rates, numRates * sizeof(*rates));
}

bool Frame::incrementAttempts() {
    Rates& rates = mInfo.rates();
    for (size_t i = 0; i < rates.size(); ++i) {
        if (mInitialTxRates[i].idx == -1) {
            // We've run out of attempts
            break;
        }
        if (rates[i].count < mInitialTxRates[i].count) {
            ++rates[i].count;
            return true;
        }
    }
    return false;
}

bool Frame::hasRemainingAttempts() {
    Rates& rates = mInfo.rates();
    for (size_t i = 0; i < rates.size(); ++i) {
        if (mInitialTxRates[i].idx == -1) {
            break;
        }
        if (rates[i].count < mInitialTxRates[i].count) {
            return true;
        }
    }
    return false;
}

const MacAddress& Frame::source() const {
    auto hdr = reinterpret_cast<const struct ieee80211_hdr*>(mData.data());
    return *reinterpret_cast<const MacAddress*>(hdr->addr2);
}

const MacAddress& Frame::destination() const {
    auto hdr = reinterpret_cast<const struct ieee80211_hdr*>(mData.data());
    return *reinterpret_cast<const MacAddress*>(hdr->addr1);
}

std::string Frame::str() const {
    uint8_t type = (mData[0] >> 2) & 0x3;
    uint8_t subType = (mData[0] >> 4) & 0x0F;

    std::stringstream ss;
    ss << "[ Ck: " << cookie() << " Ch: " << channel() << " ] ";
    switch (type) {
        case 0:
            // Management
            ss << "Management (";
            switch (subType) {
                case 0:
                    ss << "Association Request";
                    break;
                case 1:
                    ss << "Association Response";
                    break;
                case 2:
                    ss << "Reassociation Request";
                    break;
                case 3:
                    ss << "Reassociation Response";
                    break;
                case 4:
                    ss << "Probe Request";
                    break;
                case 5:
                    ss << "Probe Response";
                    break;
                case 6:
                    ss << "Timing Advertisement";
                    break;
                case 8:
                    ss << "Beacon";
                    break;
                case 9:
                    ss << "ATIM";
                    break;
                case 10:
                    ss << "Disassociation";
                    break;
                case 11:
                    ss << "Authentication";
                    break;
                case 12:
                    ss << "Deauthentication";
                    break;
                case 13:
                    ss << "Action";
                    break;
                case 14:
                    ss << "Action No Ack";
                    break;
                default:
                    ss << subType;
                    break;
            }
            ss << ')';
            break;
        case 1:
            // Control
            ss << "Control (";
            switch (subType) {
                case 4:
                    ss << "Beamforming Report Poll";
                    break;
                case 5:
                    ss << "VHT NDP Announcement";
                    break;
                case 6:
                    ss << "Control Frame Extension";
                    break;
                case 7:
                    ss << "Control Wrapper";
                    break;
                case 8:
                    ss << "Block Ack Request";
                    break;
                case 9:
                    ss << "Block Ack";
                    break;
                case 10:
                    ss << "PS-Poll";
                    break;
                case 11:
                    ss << "RTS";
                    break;
                case 12:
                    ss << "CTS";
                    break;
                case 13:
                    ss << "Ack";
                    break;
                case 14:
                    ss << "CF-End";
                    break;
                case 15:
                    ss << "CF-End+CF-Ack";
                    break;
                default:
                    ss << subType;
                    break;
            }
            ss << ')';
            break;
        case 2:
            // Data
            ss << "Data (";
            switch (subType) {
                case 0:
                    ss << "Data";
                    break;
                case 1:
                    ss << "Data+CF-Ack";
                    break;
                case 2:
                    ss << "Data+CF-Poll";
                    break;
                case 3:
                    ss << "Data+CF-Ack+CF-Poll";
                    break;
                case 4:
                    ss << "Null";
                    break;
                case 5:
                    ss << "CF-Ack";
                    break;
                case 6:
                    ss << "CF-Poll";
                    break;
                case 7:
                    ss << "CF-Ack+CF-Poll";
                    break;
                case 8:
                    ss << "QoS Data";
                    break;
                case 9:
                    ss << "QoS Data+CF-Ack";
                    break;
                case 10:
                    ss << "QoS Data+CF-Poll";
                    break;
                case 11:
                    ss << "QoS Data+CF-Ack+CF-Poll";
                    break;
                case 12:
                    ss << "QoS Null";
                    break;
                case 14:
                    ss << "QoS CF-Poll";
                    break;
                case 15:
                    ss << "QoS CF-Poll+CF-Ack";
                    break;
                default:
                    ss << subType;
                    break;
            }
            ss << ')';
            break;
        case 3:
            // Extension
            ss << "Extension (";
            switch (subType) {
                case 0:
                    ss << "DMG Beacon";
                    break;
                default:
                    ss << subType;
                    break;
            }
            ss << ')';
            break;
        default:
            ss << type << " (" << subType << ')';
            break;
    }
    return ss.str();
}

bool Frame::isBeacon() const {
    uint8_t totalType = mData[0] & 0xFC;
    return totalType == 0x80;
}

bool Frame::isData() const {
    uint8_t totalType = mData[0] & 0x0C;
    return totalType == 0x08;
}

bool Frame::isDataQoS() const {
    uint8_t totalType = mData[0] & 0xFC;
    return totalType == 0x88;
}

uint16_t Frame::getQoSControl() const {
    // Some frames have 4 address fields instead of 3 which pushes QoS control
    // forward 6 bytes.
    bool uses4Addresses = !!(mData[1] & 0x03);
    const uint8_t* addr = &mData[uses4Addresses ? 30 : 24];

    return __le16_to_cpu(*reinterpret_cast<const uint16_t*>(addr));
}

uint64_t Frame::calcNextTimeout() {
    mNextTimeout = (mContentionWindow * kSlotTime) / 2;
    mContentionWindow = std::min((mContentionWindow * 2) + 1,
                                 mContentionWindowMax);
    return mNextTimeout;
}

AccessCategory Frame::getAc() const {
    if (!isData()) {
        return AccessCategory::Voice;
    }
    if (!isDataQoS()) {
        return AccessCategory::BestEffort;
    }
    uint16_t priority = getQoSControl() & 0x07;
    return kPriorityToAc[priority];
}
