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

#include "packet/fragmenting_inserter.h"

#include "os/log.h"

namespace bluetooth {
namespace packet {

FragmentingInserter::FragmentingInserter(size_t mtu,
                                         std::back_insert_iterator<std::vector<std::unique_ptr<RawBuilder>>> iterator)
    : BitInserter(to_construct_bit_inserter_), mtu_(mtu), curr_packet_(std::make_unique<RawBuilder>(mtu)),
      iterator_(iterator) {}

void FragmentingInserter::insert_bits(uint8_t byte, size_t num_bits) {
  ASSERT(curr_packet_ != nullptr);
  size_t total_bits = num_bits + num_saved_bits_;
  uint16_t new_value = static_cast<uint8_t>(saved_bits_) | (static_cast<uint16_t>(byte) << num_saved_bits_);
  if (total_bits >= 8) {
    uint8_t new_byte = static_cast<uint8_t>(new_value);
    on_byte(new_byte);
    curr_packet_->AddOctets1(new_byte);
    if (curr_packet_->size() >= mtu_) {
      iterator_ = std::move(curr_packet_);
      curr_packet_ = std::make_unique<RawBuilder>(mtu_);
    }
    total_bits -= 8;
    new_value = new_value >> 8;
  }
  num_saved_bits_ = total_bits;
  uint8_t mask = static_cast<uint8_t>(0xff) >> (8 - num_saved_bits_);
  saved_bits_ = static_cast<uint8_t>(new_value) & mask;
}

void FragmentingInserter::finalize() {
  if (curr_packet_->size() != 0) {
    iterator_ = std::move(curr_packet_);
  }
  curr_packet_.reset();
}

}  // namespace packet
}  // namespace bluetooth
