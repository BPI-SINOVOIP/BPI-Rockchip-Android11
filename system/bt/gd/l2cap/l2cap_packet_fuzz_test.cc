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

#define PACKET_FUZZ_TESTING
#include "l2cap/l2cap_packets.h"

#include <memory>

#include "os/log.h"
#include "packet/bit_inserter.h"
#include "packet/raw_builder.h"

using bluetooth::packet::BitInserter;
using bluetooth::packet::RawBuilder;
using std::vector;

namespace bluetooth {
namespace l2cap {

std::vector<void (*)(const uint8_t*, size_t)> l2cap_packet_fuzz_tests;

DEFINE_AND_REGISTER_ExtendedInformationStartFrameReflectionFuzzTest(l2cap_packet_fuzz_tests);

DEFINE_AND_REGISTER_StandardInformationFrameWithFcsReflectionFuzzTest(l2cap_packet_fuzz_tests);

DEFINE_AND_REGISTER_StandardSupervisoryFrameWithFcsReflectionFuzzTest(l2cap_packet_fuzz_tests);

DEFINE_AND_REGISTER_GroupFrameReflectionFuzzTest(l2cap_packet_fuzz_tests);

DEFINE_AND_REGISTER_ConfigurationRequestReflectionFuzzTest(l2cap_packet_fuzz_tests);

}  // namespace l2cap
}  // namespace bluetooth

void RunL2capPacketFuzzTest(const uint8_t* data, size_t size) {
  if (data == nullptr) return;
  for (auto test_function : bluetooth::l2cap::l2cap_packet_fuzz_tests) {
    test_function(data, size);
  }
}