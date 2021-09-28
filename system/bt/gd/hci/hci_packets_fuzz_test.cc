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
#include "hci/hci_packets.h"

#include <memory>

#include "os/log.h"
#include "packet/bit_inserter.h"
#include "packet/raw_builder.h"

using bluetooth::packet::BitInserter;
using bluetooth::packet::RawBuilder;
using std::vector;

namespace bluetooth {
namespace hci {

std::vector<void (*)(const uint8_t*, size_t)> hci_packet_fuzz_tests;

DEFINE_AND_REGISTER_ResetReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ResetCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadBufferSizeReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadBufferSizeCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_HostBufferSizeReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_HostBufferSizeCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadLocalVersionInformationReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadLocalVersionInformationCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadBdAddrReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadBdAddrCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadLocalSupportedCommandsReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadLocalSupportedCommandsCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_WriteSimplePairingModeReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_WriteSimplePairingModeCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_WriteLeHostSupportReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_WriteLeHostSupportCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadLocalExtendedFeaturesReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadLocalExtendedFeaturesCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_WriteSecureConnectionsHostSupportReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_WriteSecureConnectionsHostSupportCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_LeReadWhiteListSizeReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_LeReadWhiteListSizeCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_LeReadBufferSizeReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_LeReadBufferSizeCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_WriteCurrentIacLapReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_WriteCurrentIacLapCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_WriteInquiryScanActivityReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_WriteInquiryScanActivityCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadInquiryScanActivityReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadInquiryScanActivityCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadCurrentIacLapReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadCurrentIacLapCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadNumberOfSupportedIacReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadNumberOfSupportedIacCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadPageTimeoutReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ReadPageTimeoutCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_WritePageTimeoutReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_WritePageTimeoutCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_InquiryReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_InquiryStatusReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_InquiryCancelReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_InquiryCancelCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_PeriodicInquiryModeReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_PeriodicInquiryModeCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ExitPeriodicInquiryModeReflectionFuzzTest(hci_packet_fuzz_tests);

DEFINE_AND_REGISTER_ExitPeriodicInquiryModeCompleteReflectionFuzzTest(hci_packet_fuzz_tests);

}  // namespace hci
}  // namespace bluetooth

void RunHciPacketFuzzTest(const uint8_t* data, size_t size) {
  if (data == nullptr) return;
  for (auto test_function : bluetooth::hci::hci_packet_fuzz_tests) {
    test_function(data, size);
  }
}