/*
 * Copyright 2019 The Android Open Source Project
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

#pragma once

#include <memory>

#include "packet/base_packet_builder.h"

namespace bluetooth {
namespace hal {

inline std::vector<uint8_t> SerializePacket(std::unique_ptr<packet::BasePacketBuilder> packet) {
  std::vector<uint8_t> packet_bytes;
  packet_bytes.reserve(packet->size());
  packet::BitInserter it(packet_bytes);
  packet->Serialize(it);
  return packet_bytes;
}

}  // namespace hal
}  // namespace bluetooth
