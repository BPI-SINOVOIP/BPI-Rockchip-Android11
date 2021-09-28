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

#include <array>

#include "packet/bit_inserter.h"
#include "packet/iterator.h"

namespace bluetooth {
namespace packet {
// Checks a custom type has all the necessary functions with the correct signatures.
template <typename T, bool packet_little_endian>
class CustomTypeChecker {
 public:
  template <class C, void (C::*)(BitInserter&) const>
  struct SerializeChecker {};

  template <class C, size_t (C::*)() const>
  struct SizeChecker {};

  template <class C, bool little_endian, std::optional<Iterator<little_endian>> (*)(C* vec, Iterator<little_endian> it)>
  struct ParseChecker {};

  template <class C, bool little_endian>
  static int Test(SerializeChecker<C, &C::Serialize>*, SizeChecker<C, &C::size>*,
                  ParseChecker<C, little_endian, &C::Parse>*);

  template <class C, bool little_endian>
  static char Test(...);

  static constexpr bool value = (sizeof(Test<T, packet_little_endian>(0, 0, 0)) == sizeof(int));
};
}  // namespace packet
}  // namespace bluetooth
