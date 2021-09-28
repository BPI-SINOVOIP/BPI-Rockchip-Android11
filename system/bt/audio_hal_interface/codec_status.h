/*
 * Copyright 2019 The Android Open Source Project
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

#include <vector>

#include <android/hardware/bluetooth/audio/2.0/types.h>

#include "a2dp_codec_api.h"

namespace bluetooth {
namespace audio {
namespace codec {

using ::android::hardware::bluetooth::audio::V2_0::BitsPerSample;
using ::android::hardware::bluetooth::audio::V2_0::ChannelMode;
using ::android::hardware::bluetooth::audio::V2_0::CodecConfiguration;
using ::android::hardware::bluetooth::audio::V2_0::SampleRate;

extern const CodecConfiguration kInvalidCodecConfiguration;

SampleRate A2dpCodecToHalSampleRate(
    const btav_a2dp_codec_config_t& a2dp_codec_config);
BitsPerSample A2dpCodecToHalBitsPerSample(
    const btav_a2dp_codec_config_t& a2dp_codec_config);
ChannelMode A2dpCodecToHalChannelMode(
    const btav_a2dp_codec_config_t& a2dp_codec_config);

bool A2dpSbcToHalConfig(CodecConfiguration* codec_config,
                        A2dpCodecConfig* a2dp_config);
bool A2dpAacToHalConfig(CodecConfiguration* codec_config,
                        A2dpCodecConfig* a2dp_config);
bool A2dpAptxToHalConfig(CodecConfiguration* codec_config,
                         A2dpCodecConfig* a2dp_config);
bool A2dpLdacToHalConfig(CodecConfiguration* codec_config,
                         A2dpCodecConfig* a2dp_config);

bool UpdateOffloadingCapabilities(
    const std::vector<btav_a2dp_codec_config_t>& framework_preference);
// Check whether this codec is supported by the audio HAL and is allowed to use
// by prefernece of framework / Bluetooth SoC / runtime property.
bool IsCodecOffloadingEnabled(const CodecConfiguration& codec_config);

}  // namespace codec
}  // namespace audio
}  // namespace bluetooth
