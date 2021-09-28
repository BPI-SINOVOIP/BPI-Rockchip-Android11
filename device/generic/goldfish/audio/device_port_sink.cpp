/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <log/log.h>
#include <utils/Timers.h>
#include "device_port_sink.h"
#include "talsa.h"
#include "util.h"
#include "debug.h"

namespace android {
namespace hardware {
namespace audio {
namespace V6_0 {
namespace implementation {

namespace {

struct TinyalsaSink : public DevicePortSink {
    TinyalsaSink(unsigned pcmCard, unsigned pcmDevice,
                 const AudioConfig &cfg, uint64_t &frames)
            : mFrames(frames)
            , mPcm(talsa::pcmOpen(pcmCard, pcmDevice,
                                  util::countChannels(cfg.channelMask),
                                  cfg.sampleRateHz,
                                  cfg.frameCount,
                                  true /* isOut */)) {}

    Result getPresentationPosition(uint64_t &frames, TimeSpec &ts) override {
        frames = mFrames;
        ts = util::nsecs2TimeSpec(systemTime(SYSTEM_TIME_MONOTONIC));
        return Result::OK;
    }

    int write(const void *data, size_t nBytes) override {
        const int res = ::pcm_write(mPcm.get(), data, nBytes);
        if (res < 0) {
            return FAILURE(res);
        } else if (res == 0) {
            mFrames += ::pcm_bytes_to_frames(mPcm.get(), nBytes);
            return nBytes;
        } else {
            mFrames += ::pcm_bytes_to_frames(mPcm.get(), res);
            return res;
        }
    }

    static std::unique_ptr<TinyalsaSink> create(unsigned pcmCard,
                                                unsigned pcmDevice,
                                                const AudioConfig &cfg,
                                                uint64_t &frames) {
        auto src = std::make_unique<TinyalsaSink>(pcmCard, pcmDevice, cfg, frames);
        if (src->mPcm) {
            return src;
        } else {
            return FAILURE(nullptr);
        }
    }

private:
    uint64_t &mFrames;
    talsa::PcmPtr mPcm;
};

struct NullSink : public DevicePortSink {
    NullSink(const AudioConfig &cfg, uint64_t &frames)
            : mFrames(frames)
            , mSampleRateHz(cfg.sampleRateHz)
            , mNChannels(util::countChannels(cfg.channelMask))
            , mTimestamp(systemTime(SYSTEM_TIME_MONOTONIC)) {}

    Result getPresentationPosition(uint64_t &frames, TimeSpec &ts) override {
        simulatePresentationPosition();
        frames = mFrames;
        ts = util::nsecs2TimeSpec(mTimestamp);
        return Result::OK;
    }

    int write(const void *, size_t nBytes) override {
        simulatePresentationPosition();
        mAvailableFrames += nBytes / mNChannels / sizeof(int16_t);
        return nBytes;
    }

    void simulatePresentationPosition() {
        const nsecs_t nowNs = systemTime(SYSTEM_TIME_MONOTONIC);
        const nsecs_t deltaNs = nowNs - mTimestamp;
        const uint64_t deltaFrames = uint64_t(mSampleRateHz) * ns2ms(deltaNs) / 1000;
        const uint64_t f = std::min(deltaFrames, mAvailableFrames);

        mFrames += f;
        mAvailableFrames -= f;
        if (mAvailableFrames) {
            mTimestamp += us2ns(f * 1000000 / mSampleRateHz);
        } else {
            mTimestamp = nowNs;
        }
    }

    static std::unique_ptr<NullSink> create(const AudioConfig &cfg,
                                            uint64_t &frames) {
        return std::make_unique<NullSink>(cfg, frames);
    }

private:
    uint64_t &mFrames;
    const unsigned mSampleRateHz;
    const unsigned mNChannels;
    uint64_t mAvailableFrames = 0;
    nsecs_t mTimestamp;
};

}  // namespace

std::unique_ptr<DevicePortSink>
DevicePortSink::create(const DeviceAddress &address,
                       const AudioConfig &cfg,
                       const hidl_bitfield<AudioOutputFlag> &flags,
                       uint64_t &frames) {
    (void)flags;

    if (cfg.format != AudioFormat::PCM_16_BIT) {
        ALOGE("%s:%d Only PCM_16_BIT is supported", __func__, __LINE__);
        return FAILURE(nullptr);
    }

    switch (address.device) {
    case AudioDevice::OUT_SPEAKER:
        return TinyalsaSink::create(talsa::kPcmCard, talsa::kPcmDevice,
                                    cfg, frames);

    case AudioDevice::OUT_TELEPHONY_TX:
        return NullSink::create(cfg, frames);

    default:
        ALOGE("%s:%d unsupported device: %x", __func__, __LINE__, address.device);
        return FAILURE(nullptr);
    }
}

}  // namespace implementation
}  // namespace V6_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
