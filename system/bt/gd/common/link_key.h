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

#include <string>

namespace bluetooth {
namespace common {

class LinkKey final {
 public:
  LinkKey() = default;
  LinkKey(const uint8_t (&data)[16]);

  static constexpr unsigned int kLength = 16;
  uint8_t link_key[kLength];

  std::string ToString() const;
  static bool FromString(const std::string& from, LinkKey& to);

  static const LinkKey kExample;
};

}  // namespace common
}  // namespace bluetooth