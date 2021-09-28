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

#define LOG_TAG "audio_proxy_client"

#include "AudioProxyDevice.h"

#include <utils/Log.h>

#include "AudioProxyStreamOut.h"
#include "HidlTypeUtil.h"

#define CHECK_API(func)                 \
  do {                                  \
    if (!stream->func) {                \
      ALOGD("Undefined API %s", #func); \
      return false;                     \
    }                                   \
  } while (0)

namespace audio_proxy {
namespace CPP_VERSION {
namespace {
bool isValidStreamOut(const audio_proxy_stream_out_t* stream) {
  CHECK_API(get_buffer_size);
  CHECK_API(get_frame_count);
  CHECK_API(get_supported_sample_rates);
  CHECK_API(get_sample_rate);
  CHECK_API(get_supported_channel_masks);
  CHECK_API(get_channel_mask);
  CHECK_API(get_supported_formats);
  CHECK_API(get_format);
  CHECK_API(get_latency);
  CHECK_API(standby);
  CHECK_API(pause);
  CHECK_API(resume);
  CHECK_API(flush);
  CHECK_API(write);
  CHECK_API(get_presentation_position);
  CHECK_API(set_parameters);
  CHECK_API(get_parameters);

  return true;
}
}  // namespace

AudioProxyDevice::AudioProxyDevice(audio_proxy_device_t* device)
    : mDevice(device) {}

AudioProxyDevice::~AudioProxyDevice() = default;

const char* AudioProxyDevice::getAddress() {
  return mDevice->get_address(mDevice);
}

Result AudioProxyDevice::openOutputStream(
    hidl_bitfield<AudioOutputFlag> flags, const AudioConfig& hidlConfig,
    std::unique_ptr<AudioProxyStreamOut>* streamOut,
    AudioConfig* hidlConfigOut) {
  audio_proxy_config_t config = toAudioProxyConfig(hidlConfig);

  audio_proxy_stream_out_t* stream = nullptr;
  int ret = mDevice->open_output_stream(
      mDevice, static_cast<audio_proxy_output_flags_t>(flags), &config,
      &stream);

  if (stream) {
    if (!isValidStreamOut(stream)) {
      mDevice->close_output_stream(mDevice, stream);
      return Result::NOT_SUPPORTED;
    }

    *streamOut = std::make_unique<AudioProxyStreamOut>(stream, mDevice);
  }

  // Pass the config out even if open_output_stream returns error, as the
  // suggested config, so that audio service can re-open the stream with
  // suggested config.
  *hidlConfigOut = toHidlAudioConfig(config);
  return toResult(ret);
}

}  // namespace CPP_VERSION
}  // namespace audio_proxy
