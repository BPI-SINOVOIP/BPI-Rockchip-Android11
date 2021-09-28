/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef LIBTEXTCLASSIFIER_UTILS_OPTIONAL_H_
#define LIBTEXTCLASSIFIER_UTILS_OPTIONAL_H_

#include "utils/base/logging.h"

namespace libtextclassifier3 {

// Holds an optional value.
template <class T>
class Optional {
 public:
  Optional() : init_(false) {}

  Optional(const Optional& other) {
    init_ = other.init_;
    if (other.init_) {
      value_ = other.value_;
    }
  }

  explicit Optional(T value) : init_(true), value_(value) {}

  Optional& operator=(Optional&& other) {
    init_ = other.init_;
    if (other.init_) {
      value_ = std::move(other);
    }
    return *this;
  }

  Optional& operator=(T&& other) {
    init_ = true;
    value_ = std::move(other);
    return *this;
  }

  constexpr bool has_value() const { return init_; }

  T const* operator->() const {
    TC3_CHECK(init_) << "Bad optional access.";
    return value_;
  }

  T const& value() const& {
    TC3_CHECK(init_) << "Bad optional access.";
    return value_;
  }

  T const& value_or(T&& default_value) {
    return (init_ ? value_ : default_value);
  }

  void set(const T& value) {
    init_ = true;
    value_ = value;
  }

 private:
  bool init_;
  T value_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_OPTIONAL_H_
