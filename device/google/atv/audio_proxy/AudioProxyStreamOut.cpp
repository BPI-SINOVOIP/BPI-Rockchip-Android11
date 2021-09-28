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

#include "AudioProxyStreamOut.h"

#include "HidlTypeUtil.h"

#define CHECK_OPT_API(func)                           \
  do {                                                \
    if (!mStream->func) return Result::NOT_SUPPORTED; \
  } while (0)

namespace audio_proxy {
namespace CPP_VERSION {
namespace {

template <typename T>
size_t getArraySize(const T* const arr, T terminator) {
  if (!arr) {
    return 0;
  }

  const T* ptr = arr;
  while (*ptr != terminator) {
    ptr++;
  }

  return ptr - arr;
}

template <typename T, typename H>
hidl_vec<H> convertToHidlVec(const T* const arr, T terminator) {
  const size_t size = getArraySize(arr, terminator);
  hidl_vec<H> vec(size);

  for (size_t i = 0; i < size; i++) {
    vec[i] = H(arr[i]);
  }

  return vec;
}

std::vector<audio_proxy_key_val_t> buildKeyValVec(
    const hidl_vec<ParameterValue>& parameters) {
  std::vector<audio_proxy_key_val_t> kvVec(parameters.size() + 1);

  for (size_t i = 0; i < parameters.size(); i++) {
    kvVec[i] = {parameters[i].key.c_str(), parameters[i].value.c_str()};
  }

  // Terminator.
  kvVec.back() = {};
  return kvVec;
}

std::vector<const char*> buildKeyVec(const hidl_vec<hidl_string>& keys) {
  std::vector<const char*> keyVec(keys.size() + 1);
  for (size_t i = 0; i < keys.size(); i++) {
    keyVec[i] = keys[i].c_str();
  }

  // Terminator.
  keyVec.back() = nullptr;
  return keyVec;
}

void onParametersAvailable(void* obj, const audio_proxy_key_val_t* params) {
  std::vector<ParameterValue>* results =
      static_cast<std::vector<ParameterValue>*>(obj);
  while (params->key != nullptr) {
    ParameterValue result;
    result.key = params->key;
    result.value = params->val;
    results->push_back(result);
    params++;
  }
}
}  // namespace

AudioProxyStreamOut::AudioProxyStreamOut(audio_proxy_stream_out_t* stream,
                                         audio_proxy_device_t* device)
    : mStream(stream), mDevice(device) {}

AudioProxyStreamOut::~AudioProxyStreamOut() {
  mDevice->close_output_stream(mDevice, mStream);
}

uint64_t AudioProxyStreamOut::getFrameCount() const {
  return mStream->get_frame_count(mStream);
}

uint32_t AudioProxyStreamOut::getSampleRate() const {
  return mStream->get_sample_rate(mStream);
}

Result AudioProxyStreamOut::setSampleRate(uint32_t rate) {
  CHECK_OPT_API(set_sample_rate);
  return toResult(mStream->set_sample_rate(mStream, rate));
}

hidl_vec<uint32_t> AudioProxyStreamOut::getSupportedSampleRates(
    AudioFormat format) const {
  return convertToHidlVec<uint32_t, uint32_t>(
      mStream->get_supported_sample_rates(
          mStream, static_cast<audio_proxy_format_t>(format)),
      0);
}

size_t AudioProxyStreamOut::getBufferSize() const {
  return mStream->get_buffer_size(mStream);
}

hidl_bitfield<AudioChannelMask> AudioProxyStreamOut::getChannelMask() const {
  return hidl_bitfield<AudioChannelMask>(mStream->get_channel_mask(mStream));
}

Result AudioProxyStreamOut::setChannelMask(
    hidl_bitfield<AudioChannelMask> mask) {
  CHECK_OPT_API(set_channel_mask);
  return toResult(mStream->set_channel_mask(
      mStream, static_cast<audio_proxy_channel_mask_t>(mask)));
}

hidl_vec<hidl_bitfield<AudioChannelMask>>
AudioProxyStreamOut::getSupportedChannelMasks(AudioFormat format) const {
  const audio_proxy_channel_mask_t* channelMasks =
      mStream->get_supported_channel_masks(
          mStream, static_cast<audio_proxy_format_t>(format));

  return convertToHidlVec<audio_proxy_channel_mask_t,
                          hidl_bitfield<AudioChannelMask>>(
      channelMasks, AUDIO_PROXY_CHANNEL_INVALID);
}

AudioFormat AudioProxyStreamOut::getFormat() const {
  return AudioFormat(mStream->get_format(mStream));
}

hidl_vec<AudioFormat> AudioProxyStreamOut::getSupportedFormats() const {
  return convertToHidlVec<audio_proxy_format_t, AudioFormat>(
      mStream->get_supported_formats(mStream), AUDIO_PROXY_FORMAT_INVALID);
}

Result AudioProxyStreamOut::setFormat(AudioFormat format) {
  CHECK_OPT_API(set_format);
  return toResult(
      mStream->set_format(mStream, static_cast<audio_proxy_format_t>(format)));
}

Result AudioProxyStreamOut::standby() {
  return toResult(mStream->standby(mStream));
}

Result AudioProxyStreamOut::setParameters(
    const hidl_vec<ParameterValue>& context,
    const hidl_vec<ParameterValue>& parameters) {
  std::vector<audio_proxy_key_val_t> contextKvVec = buildKeyValVec(context);
  std::vector<audio_proxy_key_val_t> parameterKvVec =
      buildKeyValVec(parameters);
  return toResult(mStream->set_parameters(mStream, contextKvVec.data(),
                                          parameterKvVec.data()));
}

hidl_vec<ParameterValue> AudioProxyStreamOut::getParameters(
    const hidl_vec<ParameterValue>& context,
    const hidl_vec<hidl_string>& keys) const {
  std::vector<audio_proxy_key_val_t> contextKvVec = buildKeyValVec(context);
  std::vector<const char*> keyVec = buildKeyVec(keys);

  std::vector<ParameterValue> results;
  results.reserve(keys.size());

  mStream->get_parameters(mStream, contextKvVec.data(), keyVec.data(),
                          onParametersAvailable, &results);

  return hidl_vec<ParameterValue>(results);
}

ssize_t AudioProxyStreamOut::write(const void* buffer, size_t bytes) {
  return mStream->write(mStream, buffer, bytes);
}

uint32_t AudioProxyStreamOut::getLatency() const {
  return mStream->get_latency(mStream);
}

Result AudioProxyStreamOut::getRenderPosition(uint32_t* dsp_frames) const {
  CHECK_OPT_API(get_render_position);
  return toResult(mStream->get_render_position(mStream, dsp_frames));
}

Result AudioProxyStreamOut::getNextWriteTimestamp(int64_t* timestamp) const {
  CHECK_OPT_API(get_next_write_timestamp);
  return toResult(mStream->get_next_write_timestamp(mStream, timestamp));
}

Result AudioProxyStreamOut::getPresentationPosition(uint64_t* frames,
                                                    TimeSpec* timestamp) const {
  struct timespec ts;
  int ret = mStream->get_presentation_position(mStream, frames, &ts);
  if (ret != 0) {
    return toResult(ret);
  }

  timestamp->tvSec = ts.tv_sec;
  timestamp->tvNSec = ts.tv_nsec;
  return Result::OK;
}

Result AudioProxyStreamOut::pause() {
  return toResult(mStream->pause(mStream));
}

Result AudioProxyStreamOut::resume() {
  return toResult(mStream->resume(mStream));
}

bool AudioProxyStreamOut::supportsDrain() const {
  return mStream->drain != nullptr;
}

Result AudioProxyStreamOut::drain(AudioDrain type) {
  return toResult(
      mStream->drain(mStream, static_cast<audio_proxy_drain_type_t>(type)));
}

Result AudioProxyStreamOut::flush() {
  return toResult(mStream->flush(mStream));
}

Result AudioProxyStreamOut::setVolume(float left, float right) {
  CHECK_OPT_API(set_volume);
  return toResult(mStream->set_volume(mStream, left, right));
}

}  // namespace CPP_VERSION
}  // namespace audio_proxy
