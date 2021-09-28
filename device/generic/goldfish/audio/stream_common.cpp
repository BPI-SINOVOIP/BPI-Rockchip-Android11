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
#include "stream_common.h"
#include "util.h"
#include "debug.h"

namespace android {
namespace hardware {
namespace audio {
namespace V6_0 {
namespace implementation {

using ::android::hardware::Void;

StreamCommon::StreamCommon(int32_t ioHandle,
                           const DeviceAddress& device,
                           const AudioConfig& config,
                           hidl_bitfield<AudioInputFlag> flags)
        : m_ioHandle(ioHandle)
        , m_device(device)
        , m_config(config)
        , m_flags(flags)
{}

uint64_t StreamCommon::getFrameSize() const {
    return util::countChannels(m_config.channelMask)
           * util::getBytesPerSample(m_config.format);
}

uint64_t StreamCommon::getFrameCount() const {
    return m_config.frameCount;
}

uint64_t StreamCommon::getBufferSize() const {
    return getFrameSize() * getFrameCount();
}

uint32_t StreamCommon::getSampleRate() const {
    return m_config.sampleRateHz;
}

void StreamCommon::getSupportedSampleRates(AudioFormat format,
                                           IStream::getSupportedSampleRates_cb _hidl_cb) const {
    if (m_config.format == format) {
        _hidl_cb(Result::OK, {m_config.sampleRateHz});
    } else {
        _hidl_cb(Result::OK, {});
    }
}

Result StreamCommon::setSampleRate(uint32_t sampleRateHz) const {
    (void)sampleRateHz;
    return FAILURE(Result::NOT_SUPPORTED);
}

hidl_bitfield<AudioChannelMask> StreamCommon::getChannelMask() const {
    return m_config.channelMask;
}

void StreamCommon::getSupportedChannelMasks(AudioFormat format,
                                            IStream::getSupportedChannelMasks_cb _hidl_cb) const {
    if (m_config.format == format) {
        _hidl_cb(Result::OK, {m_config.channelMask});
    } else {
        _hidl_cb(Result::OK, {});
    }
}

Result StreamCommon::setChannelMask(hidl_bitfield<AudioChannelMask> mask) const {
    (void)mask;
    return FAILURE(Result::NOT_SUPPORTED);
}

AudioFormat StreamCommon::getFormat() const {
    return m_config.format;
}

void StreamCommon::getSupportedFormats(IStream::getSupportedFormats_cb _hidl_cb) const {
    _hidl_cb(Result::OK, {m_config.format});
}

Result StreamCommon::setFormat(AudioFormat format) const {
    (void)format;
    return FAILURE(Result::NOT_SUPPORTED);
}

void StreamCommon::getAudioProperties(IStream::getAudioProperties_cb _hidl_cb) const {
    _hidl_cb(m_config.sampleRateHz, m_config.channelMask, m_config.format);
}

void StreamCommon::getDevices(IStream::getDevices_cb _hidl_cb) const {
    _hidl_cb(Result::OK, {m_device});
}

Result StreamCommon::setDevices(const hidl_vec<DeviceAddress>& devices) const {
    (void)devices;
    return FAILURE(Result::NOT_SUPPORTED);
}

}  // namespace implementation
}  // namespace V6_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
