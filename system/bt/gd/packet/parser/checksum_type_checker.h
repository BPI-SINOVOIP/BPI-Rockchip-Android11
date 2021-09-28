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

#include <optional>

namespace bluetooth {
namespace packet {
namespace parser {

// Checks for Initialize(), AddByte(), and GetChecksum().
// T and TRET are the checksum class Type and the checksum return type
// C and CRET are the substituted types for T and TRET
template <typename T, typename TRET>
class ChecksumTypeChecker {
 public:
  template <class C, void (C::*)()>
  struct InitializeChecker {};

  template <class C, void (C::*)(uint8_t byte)>
  struct AddByteChecker {};

  template <class C, typename CRET, CRET (C::*)() const>
  struct GetChecksumChecker {};

  // If all the methods are defined, this one matches
  template <class C, typename CRET>
  static int Test(InitializeChecker<C, &C::Initialize>*, AddByteChecker<C, &C::AddByte>*,
                  GetChecksumChecker<C, CRET, &C::GetChecksum>*);

  // This one matches everything else
  template <class C, typename CRET>
  static char Test(...);

  // This checks which template was matched
  static constexpr bool value = (sizeof(Test<T, TRET>(0, 0, 0)) == sizeof(int));
};
}  // namespace parser
}  // namespace packet
}  // namespace bluetooth
