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

#pragma once

#include <memory>

// clang-format off
#include PATH(android/hardware/audio/FILE_VERSION/types.h)
#include PATH(android/hardware/audio/common/FILE_VERSION/types.h)
// clang-format on

#include "public/audio_proxy.h"

namespace audio_proxy {
namespace CPP_VERSION {

using ::android::hardware::hidl_bitfield;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;

using namespace ::android::hardware::audio::common::CPP_VERSION;
using namespace ::android::hardware::audio::CPP_VERSION;

// C++ friendly wrapper of audio_proxy_stream_out. It handles type conversion
// between C type and hidl type.
class AudioProxyStreamOut final {
 public:
  AudioProxyStreamOut(audio_proxy_stream_out_t* stream,
                      audio_proxy_device_t* device);
  ~AudioProxyStreamOut();

  size_t getBufferSize() const;
  uint64_t getFrameCount() const;

  hidl_vec<uint32_t> getSupportedSampleRates(AudioFormat format) const;
  uint32_t getSampleRate() const;
  Result setSampleRate(uint32_t rate);

  hidl_vec<hidl_bitfield<AudioChannelMask>> getSupportedChannelMasks(
      AudioFormat format) const;
  hidl_bitfield<AudioChannelMask> getChannelMask() const;
  Result setChannelMask(hidl_bitfield<AudioChannelMask> mask);

  hidl_vec<AudioFormat> getSupportedFormats() const;
  AudioFormat getFormat() const;
  Result setFormat(AudioFormat format);

  Result standby();

  Result setParameters(const hidl_vec<ParameterValue>& context,
                       const hidl_vec<ParameterValue>& parameters);
  hidl_vec<ParameterValue> getParameters(
      const hidl_vec<ParameterValue>& context,
      const hidl_vec<hidl_string>& keys) const;

  uint32_t getLatency() const;
  ssize_t write(const void* buffer, size_t bytes);
  Result getRenderPosition(uint32_t* dsp_frames) const;
  Result getNextWriteTimestamp(int64_t* timestamp) const;
  Result getPresentationPosition(uint64_t* frames, TimeSpec* timestamp) const;

  Result pause();
  Result resume();

  bool supportsDrain() const;
  Result drain(AudioDrain type);

  Result flush();

  Result setVolume(float left, float right);

 private:
  audio_proxy_stream_out_t* const mStream;
  audio_proxy_device_t* const mDevice;
};

}  // namespace CPP_VERSION
}  // namespace audio_proxy
