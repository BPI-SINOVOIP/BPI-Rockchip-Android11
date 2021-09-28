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

#include "packet/byte_observer.h"

namespace bluetooth {
namespace packet {

ByteObserver::ByteObserver(const std::function<void(uint8_t)>& on_byte, const std::function<uint64_t()>& get_value)
    : on_byte_(on_byte), get_value_(get_value) {}

void ByteObserver::OnByte(uint8_t byte) {
  on_byte_(byte);
}

uint64_t ByteObserver::GetValue() {
  return get_value_();
}

}  // namespace packet
}  // namespace bluetooth
