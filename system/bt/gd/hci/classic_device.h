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

#include "hci/device.h"

namespace bluetooth::hci {

/**
 * A device representing a CLASSIC device.
 *
 * <p>This can be a CLASSIC only or a piece of a DUAL MODE device.
 */
class ClassicDevice : public Device {
 protected:
  friend class DeviceDatabase;
  explicit ClassicDevice(Address address) : Device(address, DeviceType::CLASSIC) {}
};

}  // namespace bluetooth::hci
