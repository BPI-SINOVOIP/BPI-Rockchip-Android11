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

#pragma once
#include <array>
#include <android/hardware/audio/common/6.0/types.h>
#include <android/hardware/audio/6.0/types.h>
#include <utils/Timers.h>

namespace android {
namespace hardware {
namespace audio {
namespace V6_0 {
namespace implementation {
namespace util {

using ::android::hardware::hidl_bitfield;
using ::android::hardware::audio::common::V6_0::AudioFormat;
using ::android::hardware::audio::common::V6_0::AudioChannelMask;
using ::android::hardware::audio::common::V6_0::AudioConfig;
using ::android::hardware::audio::V6_0::MicrophoneInfo;

MicrophoneInfo getMicrophoneInfo();

size_t countChannels(hidl_bitfield<AudioChannelMask>);
size_t getBytesPerSample(AudioFormat);

bool checkAudioConfig(bool isOut,
                      size_t duration_ms,
                      const AudioConfig &cfg,
                      AudioConfig &suggested);

TimeSpec nsecs2TimeSpec(nsecs_t);

}  // namespace util
}  // namespace implementation
}  // namespace V6_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
