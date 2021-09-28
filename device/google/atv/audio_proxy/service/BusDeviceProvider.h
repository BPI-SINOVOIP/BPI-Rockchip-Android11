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

#include <mutex>
#include <vector>

// clang-format off
#include PATH(device/google/atv/audio_proxy/FILE_VERSION/IBusDevice.h)
// clang-format on

namespace audio_proxy {
namespace service {

using ::android::sp;
using ::android::hardware::hidl_death_recipient;
using ::android::hardware::hidl_string;
using ::device::google::atv::audio_proxy::CPP_VERSION::IBusDevice;

class BusDeviceProvider {
 public:
  BusDeviceProvider();
  ~BusDeviceProvider();

  // Register bus device under `address`. The device is unregistered when its
  // process exits.
  bool add(const hidl_string& address, sp<IBusDevice> device);

  // Get IBusDevice with `address`. Caller should only keep the strong pointer
  // shortly, a.k.a caller should release the strong pointer as soon as its
  // function scope exits.
  sp<IBusDevice> get(const hidl_string& address);

  // Remove all the added devices.
  void removeAll();

 private:
  class DeathRecipient;
  struct BusDeviceHolder {
    sp<IBusDevice> device;
    hidl_string address;
    uint64_t token;
  };

  friend class DeathRecipient;

  // Called by DeathRecipient to remove device when the host process exits.
  void removeByToken(uint64_t token);

  sp<hidl_death_recipient> mDeathRecipient;

  std::mutex mDevicesLock;

  // Use a vector since we don't have too much registered devices.
  std::vector<BusDeviceHolder> mBusDevices;
  uint64_t mNextToken = 0;
};

}  // namespace service
}  // namespace audio_proxy