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

#include <stdint.h>
#include <string>

namespace bluetooth {
namespace packet {
namespace parser {
namespace test {

class SixBytes final {
 public:
  static constexpr unsigned int kLength = 6;

  uint8_t six_bytes[kLength];

  SixBytes() = default;
  SixBytes(const uint8_t (&addr)[6]);

  bool operator<(const SixBytes& rhs) const {
    return (std::memcmp(six_bytes, rhs.six_bytes, sizeof(six_bytes)) < 0);
  }
  bool operator==(const SixBytes& rhs) const {
    return (std::memcmp(six_bytes, rhs.six_bytes, sizeof(six_bytes)) == 0);
  }
  bool operator>(const SixBytes& rhs) const {
    return (rhs < *this);
  }
  bool operator<=(const SixBytes& rhs) const {
    return !(*this > rhs);
  }
  bool operator>=(const SixBytes& rhs) const {
    return !(*this < rhs);
  }
  bool operator!=(const SixBytes& rhs) const {
    return !(*this == rhs);
  }
};

}  // namespace test
}  // namespace parser
}  // namespace packet
}  // namespace bluetooth
