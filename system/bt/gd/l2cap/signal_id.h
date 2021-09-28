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

namespace bluetooth {
namespace l2cap {

struct SignalId {
 public:
  constexpr SignalId(uint8_t value) : value_(value) {}
  constexpr SignalId() : value_(1) {}
  ~SignalId() = default;

  uint8_t Value() const {
    return value_;
  }

  bool IsValid() const {
    return value_ != 0;
  }

  friend bool operator==(const SignalId& lhs, const SignalId& rhs);
  friend bool operator!=(const SignalId& lhs, const SignalId& rhs);

  struct SignalId& operator++();    // Prefix increment operator.
  struct SignalId operator++(int);  // Postfix increment operator.
  struct SignalId& operator--();    // Prefix decrement operator.
  struct SignalId operator--(int);  // Postfix decrement operator.

 private:
  uint8_t value_;
};

constexpr SignalId kInvalidSignalId{0};
constexpr SignalId kInitialSignalId{1};

inline bool operator==(const SignalId& lhs, const SignalId& rhs) {
  return lhs.value_ == rhs.value_;
}

inline bool operator!=(const SignalId& lhs, const SignalId& rhs) {
  return !(lhs == rhs);
}

inline struct SignalId& SignalId::operator++() {
  value_++;
  if (value_ == 0) value_++;
  return *this;
}

inline struct SignalId SignalId::operator++(int) {
  struct SignalId tmp = *this;
  ++*this;
  return tmp;
}

inline struct SignalId& SignalId::operator--() {
  value_--;
  if (value_ == 0) value_ = 0xff;
  return *this;
}

inline struct SignalId SignalId::operator--(int) {
  struct SignalId tmp = *this;
  --*this;
  return tmp;
}

}  // namespace l2cap
}  // namespace bluetooth
