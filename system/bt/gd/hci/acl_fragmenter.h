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

#include "packet/base_packet_builder.h"
#include "packet/raw_builder.h"

namespace bluetooth {
namespace hci {

class AclFragmenter {
 public:
  AclFragmenter(size_t mtu, std::unique_ptr<packet::BasePacketBuilder> input);
  virtual ~AclFragmenter() = default;

  std::vector<std::unique_ptr<packet::RawBuilder>> GetFragments();

 private:
  size_t mtu_;
  std::unique_ptr<packet::BasePacketBuilder> packet_;
};

}  // namespace hci
}  // namespace bluetooth
