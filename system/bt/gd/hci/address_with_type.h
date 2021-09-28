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

#include <sstream>
#include <string>
#include <utility>

#include "crypto_toolbox/crypto_toolbox.h"
#include "hci/address.h"
#include "hci/hci_packets.h"

namespace bluetooth {
namespace hci {

class AddressWithType final {
 public:
  AddressWithType(Address address, AddressType address_type) : address_(address), address_type_(address_type) {}

  explicit AddressWithType() : address_(Address::kEmpty), address_type_(AddressType::PUBLIC_DEVICE_ADDRESS) {}

  inline Address GetAddress() const {
    return address_;
  }

  inline AddressType GetAddressType() const {
    return address_type_;
  }

  /* Is this an Resolvable Private Address ? */
  inline bool IsRpa() const {
    return address_type_ == hci::AddressType::RANDOM_DEVICE_ADDRESS && ((address_.address)[0] & 0xc0) == 0x40;
  }

  /* Is this an Resolvable Private Address, that was generated from given irk ? */
  bool IsRpaThatMatchesIrk(const crypto_toolbox::Octet16& irk) const {
    if (!IsRpa()) return false;

    /* use the 3 MSB of bd address as prand */
    uint8_t prand[3];
    prand[0] = address_.address[2];
    prand[1] = address_.address[1];
    prand[2] = address_.address[0];
    /* generate X = E irk(R0, R1, R2) and R is random address 3 LSO */
    crypto_toolbox::Octet16 computed_hash = crypto_toolbox::aes_128(irk, &prand[0], 3);
    uint8_t hash[3];
    hash[0] = address_.address[5];
    hash[1] = address_.address[4];
    hash[2] = address_.address[3];
    if (memcmp(computed_hash.data(), &hash[0], 3) == 0) {
      // match
      return true;
    }
    // not a match
    return false;
  }

  bool operator<(const AddressWithType& rhs) const {
    return address_ < rhs.address_ && address_type_ < rhs.address_type_;
  }
  bool operator==(const AddressWithType& rhs) const {
    return address_ == rhs.address_ && address_type_ == rhs.address_type_;
  }
  bool operator>(const AddressWithType& rhs) const {
    return (rhs < *this);
  }
  bool operator<=(const AddressWithType& rhs) const {
    return !(*this > rhs);
  }
  bool operator>=(const AddressWithType& rhs) const {
    return !(*this < rhs);
  }
  bool operator!=(const AddressWithType& rhs) const {
    return !(*this == rhs);
  }

  std::string ToString() const {
    std::stringstream ss;
    ss << address_ << "[" << AddressTypeText(address_type_) << "]";
    return ss.str();
  }

 private:
  Address address_;
  AddressType address_type_;
};

inline std::ostream& operator<<(std::ostream& os, const AddressWithType& a) {
  os << a.ToString();
  return os;
}

}  // namespace hci
}  // namespace bluetooth

namespace std {
template <>
struct hash<bluetooth::hci::AddressWithType> {
  std::size_t operator()(const bluetooth::hci::AddressWithType& val) const {
    static_assert(sizeof(uint64_t) >= (sizeof(bluetooth::hci::Address) + sizeof(bluetooth::hci::AddressType)));
    uint64_t int_addr = 0;
    memcpy(reinterpret_cast<uint8_t*>(&int_addr), val.GetAddress().address, sizeof(bluetooth::hci::Address));
    bluetooth::hci::AddressType address_type = val.GetAddressType();
    memcpy(reinterpret_cast<uint8_t*>(&int_addr) + sizeof(bluetooth::hci::Address), &address_type,
           sizeof(address_type));
    return std::hash<uint64_t>{}(int_addr);
  }
};
}  // namespace std