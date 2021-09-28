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

#include "packet/bit_inserter.h"

#include "os/log.h"

namespace bluetooth {
namespace packet {

BitInserter::BitInserter(std::vector<uint8_t>& vector) : ByteInserter(vector) {}

BitInserter::~BitInserter() {
  ASSERT(num_saved_bits_ == 0);
}

void BitInserter::insert_bits(uint8_t byte, size_t num_bits) {
  size_t total_bits = num_bits + num_saved_bits_;
  uint16_t new_value = static_cast<uint8_t>(saved_bits_) | (static_cast<uint16_t>(byte) << num_saved_bits_);
  if (total_bits >= 8) {
    ByteInserter::insert_byte(static_cast<uint8_t>(new_value));
    total_bits -= 8;
    new_value = new_value >> 8;
  }
  num_saved_bits_ = total_bits;
  uint8_t mask = static_cast<uint8_t>(0xff) >> (8 - num_saved_bits_);
  saved_bits_ = static_cast<uint8_t>(new_value) & mask;
}

void BitInserter::insert_byte(uint8_t byte) {
  insert_bits(byte, 8);
}

}  // namespace packet
}  // namespace bluetooth
