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
 * TODO(optedoblivion): Build out AddressType getter/setter
 */
// enum AddressType {};

/**
 * A device representing a LE device.
 *
 * <p>This can be a LE only or a piece of a DUAL MODE device.
 *
 * <p>LE specific public address logic goes here.
 */
class LeDevice : public Device {
 public:
  void SetPublicAddress(Address public_address) {
    public_address_ = public_address;
  }

  Address GetPublicAddress() {
    return public_address_;
  }

  void SetIrk(uint8_t irk) {
    irk_ = irk;
    // TODO(optedoblivion): Set derived Address
  }

  uint8_t GetIrk() {
    return irk_;
  }

 protected:
  friend class DeviceDatabase;
  // TODO(optedoblivion): How to set public address.  Do I set it when no IRK is known?
  // Right now my thought is to do this:
  // 1. Construct LeDevice with address of all 0s
  // 2. IF NO IRK AND NO PRIVATE ADDRESS: (i.e. nothing in disk cache)
  //  a. Hopefully pairing will happen
  //  b. Pending successful pairing get the IRK and Private Address
  //  c. Set Both to device.
  //  (d). If available set IRK to the controller (later iteration)
  // [#3 should indicate we have a bug]
  // 3. IF YES IRK AND NO PRIVATE ADDRESS: (Partial Disk Cache Information)
  //  a. Set IRK
  //  b. Generate Private Address
  //  c. Set Private Address to device
  //  (d). If available set IRK to the controller (later iteration)
  // 4. IF YES IRK AND YES PRIVATE ADDRESS: (i.e. Disk cache hit)
  //  a. Construct with private address
  //  b. Set IRK
  //  (c). If available set IRK to the controller (later iteration)
  // 5. IF NO IRK AND YES PRIVATE ADDRESS (we have a bug)
  //  a1. -Construct with private address-
  //  b. -Indicate we need to repair or query for IRK?-
  //
  //  or
  //
  //  a2. Don't use class
  explicit LeDevice(Address address) : Device(address, DeviceType::LE), public_address_(), irk_(0) {}

 private:
  Address public_address_;
  uint8_t irk_;
};

}  // namespace bluetooth::hci
