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

#include "BusDeviceProvider.h"
#include "DevicesFactoryImpl.h"

// clang-format off
#include PATH(device/google/atv/audio_proxy/FILE_VERSION/IAudioProxyDevicesManager.h)
// clang-format on

using ::android::sp;
using ::android::wp;
using ::android::hardware::hidl_string;
using ::android::hardware::Return;
using ::device::google::atv::audio_proxy::CPP_VERSION::IAudioProxyDevicesManager;
using ::device::google::atv::audio_proxy::CPP_VERSION::IBusDevice;

namespace audio_proxy {
namespace service {

class AudioProxyDevicesManagerImpl : public IAudioProxyDevicesManager {
 public:
  AudioProxyDevicesManagerImpl();
  ~AudioProxyDevicesManagerImpl() override;

  Return<bool> registerDevice(const hidl_string& address,
                              const sp<IBusDevice>& device) override;

 private:
  bool ensureDevicesFactory();

  BusDeviceProvider mBusDeviceProvider;
  wp<DevicesFactoryImpl> mDevicesFactory;
};

}  // namespace service
}  // namespace audio_proxy
