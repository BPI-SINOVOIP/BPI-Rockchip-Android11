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

#include "BusDeviceImpl.h"

#include <utils/Log.h>

#include "AudioProxyDevice.h"
#include "AudioProxyStreamOut.h"
#include "StreamOutImpl.h"

using ::android::hardware::Void;

namespace audio_proxy {
namespace CPP_VERSION {

BusDeviceImpl::BusDeviceImpl(AudioProxyDevice* device) : mDevice(device) {}
BusDeviceImpl::~BusDeviceImpl() = default;

Return<void> BusDeviceImpl::openOutputStream(
    int32_t ioHandle, const DeviceAddress& device, const AudioConfig& config,
    hidl_bitfield<AudioOutputFlag> flags, const SourceMetadata& sourceMetadata,
    openOutputStream_cb _hidl_cb) {
  std::unique_ptr<AudioProxyStreamOut> stream;
  AudioConfig suggestedConfig;

  Result res =
      mDevice->openOutputStream(flags, config, &stream, &suggestedConfig);
  ALOGE_IF(res != Result::OK, "Open output stream error.");

  // Still pass `suggestedConfig` back when `openOutputStream` returns error,
  // so that audio service can re-open the stream with a new config.
  _hidl_cb(res, stream ? new StreamOutImpl(std::move(stream)) : nullptr,
           suggestedConfig);
  return Void();
}

}  // namespace CPP_VERSION
}  // namespace audio_proxy
