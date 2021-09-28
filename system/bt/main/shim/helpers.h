/*
 * Copyright 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hci/address_with_type.h"

namespace bluetooth {

hci::AddressWithType ToAddressWithType(const RawAddress& legacy_address,
                                       tBLE_ADDR_TYPE legacy_type) {
  // Address and RawAddress are binary equivalent;
  hci::Address address(legacy_address.address);

  hci::AddressType type;
  if (legacy_type == BLE_ADDR_PUBLIC)
    type = hci::AddressType::PUBLIC_DEVICE_ADDRESS;
  else if (legacy_type == BLE_ADDR_RANDOM)
    type = hci::AddressType::RANDOM_DEVICE_ADDRESS;
  else if (legacy_type == BLE_ADDR_PUBLIC_ID)
    type = hci::AddressType::PUBLIC_IDENTITY_ADDRESS;
  else if (legacy_type == BLE_ADDR_RANDOM_ID)
    type = hci::AddressType::RANDOM_IDENTITY_ADDRESS;
  else {
    LOG_ALWAYS_FATAL("Bad address type");
    return hci::AddressWithType{address,
                                hci::AddressType::PUBLIC_DEVICE_ADDRESS};
  }

  return hci::AddressWithType{address, type};
}
}  // namespace bluetooth