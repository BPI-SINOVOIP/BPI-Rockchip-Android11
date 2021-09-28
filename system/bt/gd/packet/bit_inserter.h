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
#include <iterator>
#include <memory>
#include <vector>

#include "packet/byte_inserter.h"

namespace bluetooth {
namespace packet {

class BitInserter : public ByteInserter {
 public:
  BitInserter(std::vector<uint8_t>& vector);
  ~BitInserter() override;

  virtual void insert_bits(uint8_t byte, size_t num_bits);

  void insert_byte(uint8_t byte) override;

 protected:
  size_t num_saved_bits_{0};
  uint8_t saved_bits_{0};
};

}  // namespace packet
}  // namespace bluetooth
