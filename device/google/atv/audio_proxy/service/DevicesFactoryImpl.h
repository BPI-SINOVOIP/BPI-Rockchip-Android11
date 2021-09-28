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

// clang-format off
#include PATH(android/hardware/audio/FILE_VERSION/IDevicesFactory.h)
// clang-format on
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

#include "DeviceImpl.h"

namespace audio_proxy {
namespace service {

using ::android::sp;
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::audio::V5_0::IDevicesFactory;

class BusDeviceProvider;

class DevicesFactoryImpl : public IDevicesFactory {
 public:
  explicit DevicesFactoryImpl(BusDeviceProvider& busDeviceProvider);

  // Methods from ::android::hardware::audio::V5_0::IDevicesFactory follow.
  Return<void> openDevice(const hidl_string& device,
                          openDevice_cb _hidl_cb) override;
  Return<void> openPrimaryDevice(openPrimaryDevice_cb _hidl_cb) override;

 private:
  BusDeviceProvider& mBusDeviceProvider;
};

}  // namespace service
}  // namespace audio_proxy
