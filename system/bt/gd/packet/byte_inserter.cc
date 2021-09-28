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

#include "packet/byte_inserter.h"

#include "os/log.h"

namespace bluetooth {
namespace packet {

ByteInserter::ByteInserter(std::vector<uint8_t>& vector) : std::back_insert_iterator<std::vector<uint8_t>>(vector) {}

ByteInserter::~ByteInserter() {
  ASSERT(registered_observers_.empty());
}

void ByteInserter::RegisterObserver(const ByteObserver& observer) {
  registered_observers_.push_back(observer);
}

ByteObserver ByteInserter::UnregisterObserver() {
  ByteObserver observer = registered_observers_.back();
  registered_observers_.pop_back();
  return observer;
}

void ByteInserter::on_byte(uint8_t byte) {
  for (auto& observer : registered_observers_) {
    observer.OnByte(byte);
  }
}

void ByteInserter::insert_byte(uint8_t byte) {
  on_byte(byte);
  std::back_insert_iterator<std::vector<uint8_t>>::operator=(byte);
}

}  // namespace packet
}  // namespace bluetooth
