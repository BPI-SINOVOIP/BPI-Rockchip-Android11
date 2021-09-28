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

#include "DeviceImpl.h"

#include <utils/Log.h>

#include "BusDeviceProvider.h"

#undef LOG_TAG
#define LOG_TAG "AudioProxyDeviceImpl"

using namespace ::android::hardware::audio::CPP_VERSION;
using namespace ::android::hardware::audio::common::CPP_VERSION;

namespace audio_proxy {
namespace service {

DeviceImpl::DeviceImpl(BusDeviceProvider& busDeviceProvider)
    : mBusDeviceProvider(busDeviceProvider) {}

// Methods from ::android::hardware::audio::V5_0::IDevice follow.
Return<Result> DeviceImpl::initCheck() { return Result::OK; }

Return<Result> DeviceImpl::setMasterVolume(float volume) {
  // software mixer will emulate this ability
  return Result::NOT_SUPPORTED;
}

Return<void> DeviceImpl::getMasterVolume(getMasterVolume_cb _hidl_cb) {
  _hidl_cb(Result::NOT_SUPPORTED, 0.f);
  return Void();
}

Return<Result> DeviceImpl::setMicMute(bool mute) {
  return Result::NOT_SUPPORTED;
}

Return<void> DeviceImpl::getMicMute(getMicMute_cb _hidl_cb) {
  _hidl_cb(Result::NOT_SUPPORTED, false);
  return Void();
}

Return<Result> DeviceImpl::setMasterMute(bool mute) {
  return Result::NOT_SUPPORTED;
}

Return<void> DeviceImpl::getMasterMute(getMasterMute_cb _hidl_cb) {
  _hidl_cb(Result::NOT_SUPPORTED, false);
  return Void();
}

Return<void> DeviceImpl::getInputBufferSize(const AudioConfig& config,
                                            getInputBufferSize_cb _hidl_cb) {
  _hidl_cb(Result::NOT_SUPPORTED, 0);
  return Void();
}

Return<void> DeviceImpl::openOutputStream(int32_t ioHandle,
                                          const DeviceAddress& device,
                                          const AudioConfig& config,
                                          hidl_bitfield<AudioOutputFlag> flags,
                                          const SourceMetadata& sourceMetadata,
                                          openOutputStream_cb _hidl_cb) {
  sp<IBusDevice> busDevice = mBusDeviceProvider.get(device.busAddress);

  if (!busDevice) {
    ALOGE("BusDevice with address %s was not found.",
          device.busAddress.c_str());
    _hidl_cb(Result::NOT_SUPPORTED, nullptr, config);
    return Void();
  }

  return busDevice->openOutputStream(ioHandle, device, config, flags,
                                     sourceMetadata, std::move(_hidl_cb));
}

Return<void> DeviceImpl::openInputStream(int32_t ioHandle,
                                         const DeviceAddress& device,
                                         const AudioConfig& config,
                                         hidl_bitfield<AudioInputFlag> flags,
                                         const SinkMetadata& sinkMetadata,
                                         openInputStream_cb _hidl_cb) {
  _hidl_cb(Result::NOT_SUPPORTED, sp<IStreamIn>(), config);
  return Void();
}

Return<bool> DeviceImpl::supportsAudioPatches() { return true; }

Return<void> DeviceImpl::createAudioPatch(
    const hidl_vec<AudioPortConfig>& sources,
    const hidl_vec<AudioPortConfig>& sinks, createAudioPatch_cb _hidl_cb) {
  _hidl_cb(Result::OK, 0);
  return Void();
}

Return<Result> DeviceImpl::releaseAudioPatch(int32_t patch) {
  return Result::OK;
}

Return<void> DeviceImpl::getAudioPort(const AudioPort& port,
                                      getAudioPort_cb _hidl_cb) {
  _hidl_cb(Result::NOT_SUPPORTED, port);
  return Void();
}

Return<Result> DeviceImpl::setAudioPortConfig(const AudioPortConfig& config) {
  return Result::NOT_SUPPORTED;
}

Return<void> DeviceImpl::getHwAvSync(getHwAvSync_cb _hidl_cb) {
  _hidl_cb(Result::NOT_SUPPORTED, 0);
  return Void();
}

Return<Result> DeviceImpl::setScreenState(bool turnedOn) {
  return Result::NOT_SUPPORTED;
}

Return<void> DeviceImpl::getParameters(const hidl_vec<ParameterValue>& context,
                                       const hidl_vec<hidl_string>& keys,
                                       getParameters_cb _hidl_cb) {
  _hidl_cb(Result::NOT_SUPPORTED, hidl_vec<ParameterValue>());
  return Void();
}

Return<Result> DeviceImpl::setParameters(
    const hidl_vec<ParameterValue>& context,
    const hidl_vec<ParameterValue>& parameters) {
  return Result::NOT_SUPPORTED;
}

Return<void> DeviceImpl::getMicrophones(getMicrophones_cb _hidl_cb) {
  _hidl_cb(Result::NOT_SUPPORTED, hidl_vec<MicrophoneInfo>());
  return Void();
}

Return<Result> DeviceImpl::setConnectedState(const DeviceAddress& address,
                                             bool connected) {
  return Result::OK;
}

}  // namespace service
}  // namespace audio_proxy
