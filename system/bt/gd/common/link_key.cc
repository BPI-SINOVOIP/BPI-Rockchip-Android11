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

#include "link_key.h"

namespace bluetooth {
namespace common {

const LinkKey LinkKey::kExample{
    {0x4C, 0x68, 0x38, 0x41, 0x39, 0xf5, 0x74, 0xd8, 0x36, 0xbc, 0xf3, 0x4e, 0x9d, 0xfb, 0x01, 0xbf}};

LinkKey::LinkKey(const uint8_t (&data)[16]) {
  std::copy(data, data + kLength, link_key);
}

std::string LinkKey::ToString() const {
  char buffer[33] = "";
  for (int i = 0; i < 16; i++) {
    std::snprintf(&buffer[i * 2], 3, "%02x", link_key[i]);
  }
  std::string str(buffer);
  return str;
}

bool LinkKey::FromString(const std::string& from, bluetooth::common::LinkKey& to) {
  LinkKey new_link_key;

  if (from.length() != 32) {
    return false;
  }

  char* temp = nullptr;
  for (int i = 0; i < 16; i++) {
    new_link_key.link_key[i] = strtol(from.substr(i * 2, 2).c_str(), &temp, 16);
  }

  to = new_link_key;
  return true;
}

}  // namespace common
}  // namespace bluetooth
