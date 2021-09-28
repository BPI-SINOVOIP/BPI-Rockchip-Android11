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

#include "AudioProxyDevicesManagerImpl.h"

#include <utils/Log.h>

using ::android::OK;
using ::android::status_t;

namespace audio_proxy {
namespace service {

AudioProxyDevicesManagerImpl::AudioProxyDevicesManagerImpl() = default;
AudioProxyDevicesManagerImpl::~AudioProxyDevicesManagerImpl() = default;

Return<bool> AudioProxyDevicesManagerImpl::registerDevice(
    const hidl_string& address, const sp<IBusDevice>& device) {
  if (address.empty() || !device) {
    return false;
  }

  if (!mBusDeviceProvider.add(address, device)) {
    ALOGE("Failed to register bus device with addr %s", address.c_str());
    return false;
  }

  if (!ensureDevicesFactory()) {
    ALOGE("Failed to register audio devices factory.");
    mBusDeviceProvider.removeAll();
    return false;
  }

  return true;
}

bool AudioProxyDevicesManagerImpl::ensureDevicesFactory() {
  sp<DevicesFactoryImpl> factory = mDevicesFactory.promote();
  if (factory) {
    return true;
  }

  factory = new DevicesFactoryImpl(mBusDeviceProvider);
  status_t status = factory->registerAsService("audio_proxy");
  if (status != OK) {
    return false;
  }

  mDevicesFactory = factory;
  return true;
}

}  // namespace service
}  // namespace audio_proxy
