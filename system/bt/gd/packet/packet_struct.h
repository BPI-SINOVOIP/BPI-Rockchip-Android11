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

#include <cstdint>
#include <forward_list>
#include <iterator>
#include <memory>
#include <vector>

#include "os/log.h"
#include "packet/base_struct.h"
#include "packet/bit_inserter.h"
#include "packet/endian_inserter.h"

namespace bluetooth {
namespace packet {

// Abstract base class that is subclassed to build specifc structs.
// The template parameter little_endian controls the generation of insert().
template <bool little_endian>
class PacketStruct : public BaseStruct, protected EndianInserter<little_endian> {
 public:
  PacketStruct() = default;
  virtual ~PacketStruct() = default;
};

}  // namespace packet
}  // namespace bluetooth
