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

#include <cstring>
#include <string>

namespace bluetooth {
namespace hci {

class Address final {
 public:
  static constexpr unsigned int kLength = 6;

  uint8_t address[kLength];

  Address() = default;
  Address(const uint8_t (&addr)[6]);

  bool operator<(const Address& rhs) const {
    return (std::memcmp(address, rhs.address, sizeof(address)) < 0);
  }
  bool operator==(const Address& rhs) const {
    return (std::memcmp(address, rhs.address, sizeof(address)) == 0);
  }
  bool operator>(const Address& rhs) const {
    return (rhs < *this);
  }
  bool operator<=(const Address& rhs) const {
    return !(*this > rhs);
  }
  bool operator>=(const Address& rhs) const {
    return !(*this < rhs);
  }
  bool operator!=(const Address& rhs) const {
    return !(*this == rhs);
  }

  bool IsEmpty() const {
    return *this == kEmpty;
  }

  std::string ToString() const;

  // Converts |string| to Address and places it in |to|. If |from| does
  // not represent a Bluetooth address, |to| is not modified and this function
  // returns false. Otherwise, it returns true.
  static bool FromString(const std::string& from, Address& to);

  // Copies |from| raw Bluetooth address octets to the local object.
  // Returns the number of copied octets - should be always Address::kLength
  size_t FromOctets(const uint8_t* from);

  static bool IsValidAddress(const std::string& address);

  static const Address kEmpty;  // 00:00:00:00:00:00
  static const Address kAny;    // FF:FF:FF:FF:FF:FF
};

inline std::ostream& operator<<(std::ostream& os, const Address& a) {
  os << a.ToString();
  return os;
}

}  // namespace hci
}  // namespace bluetooth

namespace std {
template <>
struct hash<bluetooth::hci::Address> {
  std::size_t operator()(const bluetooth::hci::Address& val) const {
    static_assert(sizeof(uint64_t) >= bluetooth::hci::Address::kLength);
    uint64_t int_addr = 0;
    memcpy(reinterpret_cast<uint8_t*>(&int_addr), val.address, bluetooth::hci::Address::kLength);
    return std::hash<uint64_t>{}(int_addr);
  }
};
}  // namespace std