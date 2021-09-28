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
#include <cutils/bitops.h>
#include <system/audio.h>
#include "util.h"
#include "debug.h"

namespace android {
namespace hardware {
namespace audio {
namespace V6_0 {
namespace implementation {
namespace util {

namespace {

const std::array<uint32_t, 8> kSupportedRatesHz = {
    8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000
};

const std::array<hidl_bitfield<AudioChannelMask>, 4> kSupportedInChannelMask = {
    AudioChannelMask::IN_LEFT | 0,
    AudioChannelMask::IN_RIGHT | 0,
    AudioChannelMask::IN_FRONT | 0,
    AudioChannelMask::IN_STEREO | 0,
};

const std::array<hidl_bitfield<AudioChannelMask>, 4> kSupportedOutChannelMask = {
    AudioChannelMask::OUT_FRONT_LEFT | 0,
    AudioChannelMask::OUT_FRONT_RIGHT | 0,
    AudioChannelMask::OUT_FRONT_CENTER | 0,
    AudioChannelMask::OUT_STEREO | 0,
};

const std::array<AudioFormat, 1> kSupportedAudioFormats = {
    AudioFormat::PCM_16_BIT,
};

bool checkSampleRateHz(uint32_t value, uint32_t &suggest) {
    for (const uint32_t supported : kSupportedRatesHz) {
        if (value <= supported) {
            suggest = supported;
            return (value == supported);
        }
    }

    suggest = kSupportedRatesHz.back();
    return FAILURE(false);
}

size_t align(size_t v, size_t a) {
    return (v + a - 1) / a * a;
}

size_t getBufferSizeFrames(size_t duration_ms, uint32_t sample_rate) {
    // AudioFlinger requires the buffer to be aligned by 16 frames
    return align(sample_rate * duration_ms / 1000, 16);
}

}  // namespace

MicrophoneInfo getMicrophoneInfo() {
    MicrophoneInfo mic;

    mic.deviceId = "mic_goldfish";
    mic.group = 0;
    mic.indexInTheGroup = 0;
    mic.sensitivity = AUDIO_MICROPHONE_SENSITIVITY_UNKNOWN;
    mic.maxSpl = AUDIO_MICROPHONE_SENSITIVITY_UNKNOWN;
    mic.minSpl = AUDIO_MICROPHONE_SENSITIVITY_UNKNOWN;
    mic.directionality = AudioMicrophoneDirectionality::UNKNOWN;
    mic.position.x = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;
    mic.position.y = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;
    mic.position.z = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;
    mic.orientation.x = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;
    mic.orientation.y = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;
    mic.orientation.z = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;

    return mic;
}

size_t countChannels(hidl_bitfield<AudioChannelMask> mask) {
    return popcount(mask);
}

size_t getBytesPerSample(AudioFormat format) {
    return audio_bytes_per_sample(static_cast<audio_format_t>(format));
}

bool checkAudioConfig(bool isOut,
                      size_t duration_ms,
                      const AudioConfig &cfg,
                      AudioConfig &suggested) {
    bool valid = checkSampleRateHz(cfg.sampleRateHz, suggested.sampleRateHz);

    if (isOut) {
        if (std::find(kSupportedOutChannelMask.begin(),
                      kSupportedOutChannelMask.end(),
                      cfg.channelMask) == kSupportedOutChannelMask.end()) {
            suggested.channelMask = AudioChannelMask::OUT_STEREO | 0;
            valid = FAILURE(false);
        } else {
            suggested.channelMask = cfg.channelMask;
        }
    } else {
        if (std::find(kSupportedInChannelMask.begin(),
                      kSupportedInChannelMask.end(),
                      cfg.channelMask) == kSupportedInChannelMask.end()) {
            suggested.channelMask = AudioChannelMask::IN_STEREO | 0;
            valid = FAILURE(false);
        } else {
            suggested.channelMask = cfg.channelMask;
        }
    }

    if (std::find(kSupportedAudioFormats.begin(),
                  kSupportedAudioFormats.end(),
                  cfg.format) == kSupportedAudioFormats.end()) {
        suggested.format = AudioFormat::PCM_16_BIT;
        valid = FAILURE(false);
    } else {
        suggested.format = cfg.format;
    }

    suggested.offloadInfo = cfg.offloadInfo;    // don't care

    suggested.frameCount = (cfg.frameCount == 0)
        ? getBufferSizeFrames(duration_ms, suggested.sampleRateHz)
        : cfg.frameCount;

    return valid;
}

TimeSpec nsecs2TimeSpec(nsecs_t ns) {
    TimeSpec ts;
    ts.tvSec = ns2s(ns);
    ts.tvNSec = ns - s2ns(ts.tvSec);
    return ts;
}

}  // namespace util
}  // namespace implementation
}  // namespace V6_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
