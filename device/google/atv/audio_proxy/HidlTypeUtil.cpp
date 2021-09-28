// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "HidlTypeUtil.h"

#include <error.h>

using ::android::hardware::hidl_bitfield;

namespace audio_proxy {
namespace CPP_VERSION {

Result toResult(int res) {
  switch (res) {
    case 0:
      return Result::OK;
    case EINVAL:
      return Result::INVALID_ARGUMENTS;
    case ENOSYS:
      return Result::NOT_SUPPORTED;
    default:
      return Result::INVALID_STATE;
  }
}

AudioConfig toHidlAudioConfig(const audio_proxy_config_t& config) {
  AudioConfig hidlConfig;
  hidlConfig.sampleRateHz = config.sample_rate;
  hidlConfig.channelMask = hidl_bitfield<AudioChannelMask>(config.channel_mask);
  hidlConfig.format = AudioFormat(config.format);
  hidlConfig.frameCount = config.frame_count;
  return hidlConfig;
}

audio_proxy_config_t toAudioProxyConfig(const AudioConfig& hidlConfig) {
  audio_proxy_config_t config = {};
  config.sample_rate = hidlConfig.sampleRateHz;
  config.channel_mask =
      static_cast<audio_proxy_channel_mask_t>(hidlConfig.channelMask);
  config.format = static_cast<audio_proxy_format_t>(hidlConfig.format);
  config.frame_count = hidlConfig.frameCount;
  return config;
}

}  // namespace CPP_VERSION
}  // namespace audio_proxy