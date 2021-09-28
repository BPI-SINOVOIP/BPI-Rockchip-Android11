/*
 * Copyright 2018 The Android Open Source Project
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

namespace test_vendor_lib {
namespace sco {

// SCO data packets are specified in the Bluetooth Core Specification Version
// 4.2, Volume 2, Part E, Section 5.4.3
enum class PacketStatusFlagsType : uint8_t {
  CORRECTLY_RECEIVED = 0,
  POSSIBLY_INCOMPLETE = 1,
  NO_DATA = 2,
  PARTIALLY_LOST = 3
};
}  // namespace sco
}  // namespace test_vendor_lib
