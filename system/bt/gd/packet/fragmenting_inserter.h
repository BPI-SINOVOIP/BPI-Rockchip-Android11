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

#include "packet/bit_inserter.h"
#include "packet/raw_builder.h"

namespace bluetooth {
namespace packet {

class FragmentingInserter : public BitInserter {
 public:
  FragmentingInserter(size_t mtu, std::back_insert_iterator<std::vector<std::unique_ptr<RawBuilder>>> iterator);

  void insert_bits(uint8_t byte, size_t num_bits) override;

  void finalize();

 protected:
  std::vector<uint8_t> to_construct_bit_inserter_;
  size_t mtu_;
  std::unique_ptr<RawBuilder> curr_packet_;
  std::back_insert_iterator<std::vector<std::unique_ptr<RawBuilder>>> iterator_;
};

}  // namespace packet
}  // namespace bluetooth
