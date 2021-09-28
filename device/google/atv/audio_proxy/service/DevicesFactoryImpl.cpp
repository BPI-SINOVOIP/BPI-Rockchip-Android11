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

#include "DevicesFactoryImpl.h"

#include <utils/Log.h>

#undef LOG_TAG
#define LOG_TAG "AudioProxyDevicesManagerImpl"

using namespace ::android::hardware::audio::CPP_VERSION;

namespace audio_proxy {
namespace service {

DevicesFactoryImpl::DevicesFactoryImpl(BusDeviceProvider& busDeviceProvider)
    : mBusDeviceProvider(busDeviceProvider) {}

// Methods from ::android::hardware::audio::V5_0::IDevicesFactory follow.
Return<void> DevicesFactoryImpl::openDevice(const hidl_string& device,
                                            openDevice_cb _hidl_cb) {
  ALOGE("openDevice");
  if (device == "audio_proxy") {
    ALOGE("Audio Device was opened: %s", device.c_str());
    _hidl_cb(Result::OK, new DeviceImpl(mBusDeviceProvider));
  } else {
    _hidl_cb(Result::NOT_SUPPORTED, nullptr);
  }

  return Void();
}

Return<void> DevicesFactoryImpl::openPrimaryDevice(
    openPrimaryDevice_cb _hidl_cb) {
  // The AudioProxy HAL does not support a primary device.
  _hidl_cb(Result::NOT_SUPPORTED, nullptr);
  return Void();
}

}  // namespace service
}  // namespace audio_proxy
