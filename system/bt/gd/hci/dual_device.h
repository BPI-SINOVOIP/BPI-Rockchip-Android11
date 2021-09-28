/******************************************************************************
 *
 *  Copyright 2019 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#pragma once

#include "hci/classic_device.h"
#include "hci/device.h"
#include "hci/le_device.h"

namespace bluetooth::hci {

/**
 * A device representing a DUAL device.
 *
 * <p>This can be a DUAL only.
 */
class DualDevice : public Device {
 public:
  std::shared_ptr<Device> GetClassicDevice() {
    return classic_device_;
  }

  std::shared_ptr<Device> GetLeDevice() {
    return le_device_;
  }

 protected:
  friend class DeviceDatabase;
  DualDevice(Address address, std::shared_ptr<ClassicDevice> classic_device, std::shared_ptr<LeDevice> le_device)
      : Device(address, DUAL), classic_device_(std::move(classic_device)), le_device_(std::move(le_device)) {
    classic_device_->SetDeviceType(DUAL);
    le_device_->SetDeviceType(DUAL);
  }

  void SetAddress(Address address) override {
    Device::SetAddress(address);
    GetClassicDevice()->SetAddress(address);
    GetLeDevice()->SetAddress(address);
  }

 private:
  std::shared_ptr<ClassicDevice> classic_device_;
  std::shared_ptr<LeDevice> le_device_;
};

}  // namespace bluetooth::hci
