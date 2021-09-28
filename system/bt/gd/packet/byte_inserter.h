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

#include "packet/byte_observer.h"

namespace bluetooth {
namespace packet {

class ByteInserter : public std::back_insert_iterator<std::vector<uint8_t>> {
 public:
  ByteInserter(std::vector<uint8_t>& vector);
  virtual ~ByteInserter();

  virtual void insert_byte(uint8_t byte);

  void RegisterObserver(const ByteObserver& observer);

  ByteObserver UnregisterObserver();

 protected:
  void on_byte(uint8_t);

 private:
  std::vector<ByteObserver> registered_observers_;
};

}  // namespace packet
}  // namespace bluetooth
