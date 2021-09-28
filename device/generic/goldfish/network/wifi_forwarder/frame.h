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

#include "macaddress.h"

#include <stdint.h>

#include <array>
#include <vector>

#define IEEE80211_TX_MAX_RATES 4

struct hwsim_tx_rate {
    signed char idx;
    unsigned char count;
} __attribute__((__packed__));

enum class FrameType : uint8_t {
    Unknown,
    Ack,
    Data,
};

enum class AccessCategory {
    Voice,
    Video,
    BestEffort,
    Background,
};

FrameType frameTypeFromByte(uint8_t byte);

using Rates = std::array<hwsim_tx_rate, IEEE80211_TX_MAX_RATES>;

class FrameInfo {
public:
    FrameInfo() = default;
    FrameInfo(const FrameInfo&) = default;
    FrameInfo(FrameInfo&&) = default;
    FrameInfo(const MacAddress& transmitter,
              uint64_t cookie,
              uint32_t flags,
              uint32_t channel,
              const hwsim_tx_rate* rates,
              size_t numRates);

    FrameInfo& operator=(const FrameInfo&) = default;
    FrameInfo& operator=(FrameInfo&&) = default;

    const Rates& rates() const { return mTxRates; }
    Rates& rates() { return mTxRates; }

    const MacAddress& transmitter() const { return mTransmitter; }
    uint64_t cookie() const { return mCookie; }
    uint32_t flags() const { return mFlags; }
    uint32_t channel() const { return mChannel; }

    bool shouldAck() const;

private:
    Rates mTxRates;
    MacAddress mTransmitter;
    uint64_t mCookie = 0;
    uint32_t mFlags = 0;
    uint32_t mChannel = 0;
};

class Frame {
public:
    Frame() = default;
    Frame(const uint8_t* data, size_t size);
    Frame(const uint8_t* data,
          size_t size,
          const MacAddress& transmitter,
          uint64_t cookie,
          uint32_t flags,
          uint32_t channel,
          const hwsim_tx_rate* rates,
          size_t numRates);
    Frame(Frame&& other) = default;

    Frame& operator=(Frame&& other) = default;

    size_t size() const { return mData.size(); }
    const uint8_t* data() const { return mData.data(); }
    uint8_t* data() { return mData.data(); }
    uint64_t cookie() const { return mInfo.cookie(); }
    uint32_t flags() const { return mInfo.flags(); }
    uint32_t channel() const { return mInfo.channel(); }

    Rates& rates() { return mInfo.rates(); }
    const Rates& rates() const { return mInfo.rates(); }

    const Rates& initialRates() const { return mInitialTxRates; }

    // Increment the number of attempts made in the tx rates
    bool incrementAttempts();
    bool hasRemainingAttempts();

    // The transmitter as indicated by hwsim
    const MacAddress& transmitter() const { return mInfo.transmitter(); }
    // The source as indicated by the 802.11 frame
    const MacAddress& source() const;
    // The destination as indicated by the 802.11 frame
    const MacAddress& destination() const;

    std::string str() const;

    const FrameInfo& info() const { return mInfo; }
    FrameInfo& info() { return mInfo; }

    bool isBeacon() const;
    bool isData() const;
    bool isDataQoS() const;

    uint16_t getQoSControl() const;

    bool shouldAck() const { return mInfo.shouldAck(); }

    uint64_t calcNextTimeout();

    void setRadioDestination(const MacAddress& destination) {
        mRadioDestination = destination;
    }
    const MacAddress& radioDestination() const { return mRadioDestination; }
private:
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;

    AccessCategory getAc() const;

    std::vector<uint8_t> mData;
    FrameInfo mInfo;
    MacAddress mRadioDestination;
    Rates mInitialTxRates;
    uint64_t mNextTimeout = 0;
    // The contention winow determines how much time to back off on each retry.
    // The contention window initial value and max value are determined by the
    // access category of the frame.
    uint32_t mContentionWindow = 0;
    uint32_t mContentionWindowMax = 0;
};

