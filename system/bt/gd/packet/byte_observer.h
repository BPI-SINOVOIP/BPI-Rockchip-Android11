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
#include <functional>

namespace bluetooth {
namespace packet {

class ByteObserver {
 public:
  ByteObserver(const std::function<void(uint8_t)>& on_byte_, const std::function<uint64_t()>& get_value_);

  void OnByte(uint8_t byte);

  uint64_t GetValue();

 private:
  std::function<void(uint8_t)> on_byte_;
  std::function<uint64_t()> get_value_;
};

}  // namespace packet
}  // namespace bluetooth
