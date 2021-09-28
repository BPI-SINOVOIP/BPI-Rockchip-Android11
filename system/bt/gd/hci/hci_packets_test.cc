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

#define PACKET_TESTING
#include "hci/hci_packets.h"

#include <gtest/gtest.h>
#include <forward_list>
#include <memory>

#include "os/log.h"
#include "packet/bit_inserter.h"
#include "packet/raw_builder.h"

using bluetooth::packet::BitInserter;
using bluetooth::packet::RawBuilder;
using std::vector;

namespace bluetooth {
namespace hci {

std::vector<uint8_t> reset = {0x03, 0x0c, 0x00};
DEFINE_AND_INSTANTIATE_ResetReflectionTest(reset);

std::vector<uint8_t> reset_complete = {0x0e, 0x04, 0x01, 0x03, 0x0c, 0x00};
DEFINE_AND_INSTANTIATE_ResetCompleteReflectionTest(reset_complete);

std::vector<uint8_t> read_buffer_size = {0x05, 0x10, 0x00};
DEFINE_AND_INSTANTIATE_ReadBufferSizeReflectionTest(read_buffer_size);

std::vector<uint8_t> read_buffer_size_complete = {0x0e, 0x0b, 0x01, 0x05, 0x10, 0x00, 0x00,
                                                  0x04, 0x3c, 0x07, 0x00, 0x08, 0x00};
DEFINE_AND_INSTANTIATE_ReadBufferSizeCompleteReflectionTest(read_buffer_size_complete);

std::vector<uint8_t> host_buffer_size = {0x33, 0x0c, 0x07, 0x9b, 0x06, 0xff, 0x14, 0x00, 0x0a, 0x00};
DEFINE_AND_INSTANTIATE_HostBufferSizeReflectionTest(host_buffer_size);

std::vector<uint8_t> host_buffer_size_complete = {0x0e, 0x04, 0x01, 0x33, 0x0c, 0x00};
DEFINE_AND_INSTANTIATE_HostBufferSizeCompleteReflectionTest(host_buffer_size_complete);

std::vector<uint8_t> read_local_version_information = {0x01, 0x10, 0x00};
DEFINE_AND_INSTANTIATE_ReadLocalVersionInformationReflectionTest(read_local_version_information);

std::vector<uint8_t> read_local_version_information_complete = {0x0e, 0x0c, 0x01, 0x01, 0x10, 0x00, 0x09,
                                                                0x00, 0x00, 0x09, 0x1d, 0x00, 0xbe, 0x02};
DEFINE_AND_INSTANTIATE_ReadLocalVersionInformationCompleteReflectionTest(read_local_version_information_complete);

std::vector<uint8_t> read_bd_addr = {0x09, 0x10, 0x00};
DEFINE_AND_INSTANTIATE_ReadBdAddrReflectionTest(read_bd_addr);

std::vector<uint8_t> read_bd_addr_complete = {0x0e, 0x0a, 0x01, 0x09, 0x10, 0x00, 0x14, 0x8e, 0x61, 0x5f, 0x36, 0x88};
DEFINE_AND_INSTANTIATE_ReadBdAddrCompleteReflectionTest(read_bd_addr_complete);

std::vector<uint8_t> read_local_supported_commands = {0x02, 0x10, 0x00};
DEFINE_AND_INSTANTIATE_ReadLocalSupportedCommandsReflectionTest(read_local_supported_commands);

std::vector<uint8_t> read_local_supported_commands_complete = {
    0x0e, 0x44, 0x01, 0x02, 0x10, 0x00, /* Supported commands start here (total 64 bytes) */
    0xff, 0xff, 0xff, 0x03, 0xce, 0xff, 0xef, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xf2, 0x0f, 0xe8, 0xfe,
    0x3f, 0xf7, 0x83, 0xff, 0x1c, 0x00, 0x00, 0x00, 0x61, 0xff, 0xff, 0xff, 0x7f, 0xbe, 0x20, 0xf5,
    0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
DEFINE_AND_INSTANTIATE_ReadLocalSupportedCommandsCompleteReflectionTest(read_local_supported_commands_complete);

std::vector<uint8_t> read_local_extended_features_0 = {0x04, 0x10, 0x01, 0x00};

std::vector<uint8_t> read_local_extended_features_complete_0 = {0x0e, 0x0e, 0x01, 0x04, 0x10, 0x00, 0x00, 0x02,
                                                                0xff, 0xfe, 0x8f, 0xfe, 0xd8, 0x3f, 0x5b, 0x87};

std::vector<uint8_t> write_simple_paring_mode = {0x56, 0x0c, 0x01, 0x01};
DEFINE_AND_INSTANTIATE_WriteSimplePairingModeReflectionTest(write_simple_paring_mode);

std::vector<uint8_t> write_simple_paring_mode_complete = {0x0e, 0x04, 0x01, 0x56, 0x0c, 0x00};
DEFINE_AND_INSTANTIATE_WriteSimplePairingModeCompleteReflectionTest(write_simple_paring_mode_complete);

std::vector<uint8_t> write_le_host_supported = {0x6d, 0x0c, 0x02, 0x01, 0x01};
DEFINE_AND_INSTANTIATE_WriteLeHostSupportReflectionTest(write_le_host_supported);

std::vector<uint8_t> write_le_host_supported_complete = {0x0e, 0x04, 0x01, 0x6d, 0x0c, 0x00};
DEFINE_AND_INSTANTIATE_WriteLeHostSupportCompleteReflectionTest(write_le_host_supported_complete);

std::vector<uint8_t> read_local_extended_features_1 = {0x04, 0x10, 0x01, 0x01};

std::vector<uint8_t> read_local_extended_features_complete_1 = {0x0e, 0x0e, 0x01, 0x04, 0x10, 0x00, 0x01, 0x02,
                                                                0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

std::vector<uint8_t> read_local_extended_features_2 = {0x04, 0x10, 0x01, 0x02};
DEFINE_AND_INSTANTIATE_ReadLocalExtendedFeaturesReflectionTest(read_local_extended_features_0,
                                                               read_local_extended_features_1,
                                                               read_local_extended_features_2);

std::vector<uint8_t> read_local_extended_features_complete_2 = {0x0e, 0x0e, 0x01, 0x04, 0x10, 0x00, 0x02, 0x02,
                                                                0x45, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
DEFINE_AND_INSTANTIATE_ReadLocalExtendedFeaturesCompleteReflectionTest(read_local_extended_features_complete_0,
                                                                       read_local_extended_features_complete_1,
                                                                       read_local_extended_features_complete_2);

std::vector<uint8_t> write_secure_connections_host_support = {0x7a, 0x0c, 0x01, 0x01};
DEFINE_AND_INSTANTIATE_WriteSecureConnectionsHostSupportReflectionTest(write_secure_connections_host_support);

std::vector<uint8_t> write_secure_connections_host_support_complete = {0x0e, 0x04, 0x01, 0x7a, 0x0c, 0x00};
DEFINE_AND_INSTANTIATE_WriteSecureConnectionsHostSupportCompleteReflectionTest(
    write_secure_connections_host_support_complete);

std::vector<uint8_t> le_read_white_list_size = {0x0f, 0x20, 0x00};
DEFINE_AND_INSTANTIATE_LeReadWhiteListSizeReflectionTest(le_read_white_list_size);

std::vector<uint8_t> le_read_white_list_size_complete = {0x0e, 0x05, 0x01, 0x0f, 0x20, 0x00, 0x80};
DEFINE_AND_INSTANTIATE_LeReadWhiteListSizeCompleteReflectionTest(le_read_white_list_size_complete);

std::vector<uint8_t> le_read_buffer_size = {0x02, 0x20, 0x00};
DEFINE_AND_INSTANTIATE_LeReadBufferSizeReflectionTest(le_read_buffer_size);

std::vector<uint8_t> le_read_buffer_size_complete = {0x0e, 0x07, 0x01, 0x02, 0x20, 0x00, 0xfb, 0x00, 0x10};
DEFINE_AND_INSTANTIATE_LeReadBufferSizeCompleteReflectionTest(le_read_buffer_size_complete);

std::vector<uint8_t> write_current_iac_laps = {0x3a, 0x0c, 0x07, 0x02, 0x11, 0x8b, 0x9e, 0x22, 0x8b, 0x9e};
DEFINE_AND_INSTANTIATE_WriteCurrentIacLapReflectionTest(write_current_iac_laps);

std::vector<uint8_t> write_current_iac_laps_complete = {0x0e, 0x04, 0x01, 0x3a, 0x0c, 0x00};
DEFINE_AND_INSTANTIATE_WriteCurrentIacLapCompleteReflectionTest(write_current_iac_laps_complete);

std::vector<uint8_t> write_inquiry_scan_activity = {0x1e, 0x0c, 0x04, 0x00, 0x08, 0x12, 0x00};
DEFINE_AND_INSTANTIATE_WriteInquiryScanActivityReflectionTest(write_inquiry_scan_activity);

std::vector<uint8_t> write_inquiry_scan_activity_complete = {0x0e, 0x04, 0x01, 0x1e, 0x0c, 0x00};
DEFINE_AND_INSTANTIATE_WriteInquiryScanActivityCompleteReflectionTest(write_inquiry_scan_activity_complete);

std::vector<uint8_t> read_inquiry_scan_activity = {0x1d, 0x0c, 0x00};
DEFINE_AND_INSTANTIATE_ReadInquiryScanActivityReflectionTest(read_inquiry_scan_activity);

std::vector<uint8_t> read_inquiry_scan_activity_complete = {0x0e, 0x08, 0x01, 0x1d, 0x0c, 0x00, 0xaa, 0xbb, 0xcc, 0xdd};
DEFINE_AND_INSTANTIATE_ReadInquiryScanActivityCompleteReflectionTest(read_inquiry_scan_activity_complete);

std::vector<uint8_t> read_current_iac_lap = {0x39, 0x0c, 0x00};
DEFINE_AND_INSTANTIATE_ReadCurrentIacLapReflectionTest(read_current_iac_lap);

std::vector<uint8_t> read_current_iac_lap_complete = {0x0e, 0x0b, 0x01, 0x39, 0x0c, 0x00, 0x02,
                                                      0x11, 0x8b, 0x9e, 0x22, 0x8b, 0x9e};
DEFINE_AND_INSTANTIATE_ReadCurrentIacLapCompleteReflectionTest(read_current_iac_lap_complete);

std::vector<uint8_t> read_number_of_supported_iac = {0x38, 0x0c, 0x00};
DEFINE_AND_INSTANTIATE_ReadNumberOfSupportedIacReflectionTest(read_number_of_supported_iac);

std::vector<uint8_t> read_number_of_supported_iac_complete = {0x0e, 0x05, 0x01, 0x38, 0x0c, 0x00, 0x99};
DEFINE_AND_INSTANTIATE_ReadNumberOfSupportedIacCompleteReflectionTest(read_number_of_supported_iac_complete);

std::vector<uint8_t> read_page_timeout = {0x17, 0x0c, 0x00};
DEFINE_AND_INSTANTIATE_ReadPageTimeoutReflectionTest(read_page_timeout);

std::vector<uint8_t> read_page_timeout_complete = {0x0e, 0x06, 0x01, 0x17, 0x0c, 0x00, 0x11, 0x22};
DEFINE_AND_INSTANTIATE_ReadPageTimeoutCompleteReflectionTest(read_page_timeout_complete);

std::vector<uint8_t> write_page_timeout = {0x18, 0x0c, 0x02, 0x00, 0x20};
DEFINE_AND_INSTANTIATE_WritePageTimeoutReflectionTest(write_page_timeout);

std::vector<uint8_t> write_page_timeout_complete = {0x0e, 0x04, 0x01, 0x18, 0x0c, 0x00};
DEFINE_AND_INSTANTIATE_WritePageTimeoutCompleteReflectionTest(write_page_timeout_complete);

std::vector<uint8_t> inquiry = {0x01, 0x04, 0x05, 0x33, 0x8b, 0x9e, 0xaa, 0xbb};
DEFINE_AND_INSTANTIATE_InquiryReflectionTest(inquiry);

std::vector<uint8_t> inquiry_status = {0x0f, 0x04, 0x00, 0x01, 0x01, 0x04};
DEFINE_AND_INSTANTIATE_InquiryStatusReflectionTest(inquiry_status);

std::vector<uint8_t> inquiry_cancel = {0x02, 0x04, 0x00};
DEFINE_AND_INSTANTIATE_InquiryCancelReflectionTest(inquiry_cancel);

std::vector<uint8_t> inquiry_cancel_complete = {0x0e, 0x04, 0x01, 0x02, 0x04, 0x00};
DEFINE_AND_INSTANTIATE_InquiryCancelCompleteReflectionTest(inquiry_cancel_complete);

std::vector<uint8_t> periodic_inquiry_mode = {0x03, 0x04, 0x09, 0x12, 0x34, 0x56, 0x78, 0x11, 0x8b, 0x9e, 0x9a, 0xbc};
DEFINE_AND_INSTANTIATE_PeriodicInquiryModeReflectionTest(periodic_inquiry_mode);

std::vector<uint8_t> periodic_inquiry_mode_complete = {0x0e, 0x04, 0x01, 0x03, 0x04, 0x00};
DEFINE_AND_INSTANTIATE_PeriodicInquiryModeCompleteReflectionTest(periodic_inquiry_mode_complete);

std::vector<uint8_t> exit_periodic_inquiry_mode = {0x04, 0x04, 0x00};
DEFINE_AND_INSTANTIATE_ExitPeriodicInquiryModeReflectionTest(exit_periodic_inquiry_mode);

std::vector<uint8_t> exit_periodic_inquiry_mode_complete = {0x0e, 0x04, 0x01, 0x04, 0x04, 0x00};
DEFINE_AND_INSTANTIATE_ExitPeriodicInquiryModeCompleteReflectionTest(exit_periodic_inquiry_mode_complete);

std::vector<uint8_t> pixel_3_xl_write_extended_inquiry_response{
    0x52, 0x0c, 0xf1, 0x01, 0x0b, 0x09, 0x50, 0x69, 0x78, 0x65, 0x6c, 0x20, 0x33, 0x20, 0x58, 0x4c, 0x19, 0x03, 0x05,
    0x11, 0x0a, 0x11, 0x0c, 0x11, 0x0e, 0x11, 0x12, 0x11, 0x15, 0x11, 0x16, 0x11, 0x1f, 0x11, 0x2d, 0x11, 0x2f, 0x11,
    0x00, 0x12, 0x32, 0x11, 0x01, 0x05, 0x81, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

std::vector<uint8_t> pixel_3_xl_write_extended_inquiry_response_no_uuids{
    0x52, 0x0c, 0xf1, 0x01, 0x0b, 0x09, 0x50, 0x69, 0x78, 0x65, 0x6c, 0x20, 0x33, 0x20, 0x58, 0x4c, 0x01, 0x03, 0x01,
    0x05, 0x81, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

std::vector<uint8_t> pixel_3_xl_write_extended_inquiry_response_no_uuids_just_eir{
    pixel_3_xl_write_extended_inquiry_response_no_uuids.begin() + 4,  // skip command, size, and fec_required
    pixel_3_xl_write_extended_inquiry_response_no_uuids.end()};

TEST(HciPacketsTest, testWriteExtendedInquiryResponse) {
  std::shared_ptr<std::vector<uint8_t>> view_bytes =
      std::make_shared<std::vector<uint8_t>>(pixel_3_xl_write_extended_inquiry_response);

  PacketView<kLittleEndian> packet_bytes_view(view_bytes);
  auto view = WriteExtendedInquiryResponseView::Create(CommandPacketView::Create(packet_bytes_view));
  ASSERT_TRUE(view.IsValid());
  auto gap_data = view.GetExtendedInquiryResponse();
  ASSERT_GE(gap_data.size(), 4);
  ASSERT_EQ(gap_data[0].data_type_, GapDataType::COMPLETE_LOCAL_NAME);
  ASSERT_EQ(gap_data[0].data_.size(), 10);
  ASSERT_EQ(gap_data[1].data_type_, GapDataType::COMPLETE_LIST_16_BIT_UUIDS);
  ASSERT_EQ(gap_data[1].data_.size(), 24);
  ASSERT_EQ(gap_data[2].data_type_, GapDataType::COMPLETE_LIST_32_BIT_UUIDS);
  ASSERT_EQ(gap_data[2].data_.size(), 0);
  ASSERT_EQ(gap_data[3].data_type_, GapDataType::COMPLETE_LIST_128_BIT_UUIDS);
  ASSERT_EQ(gap_data[3].data_.size(), 128);

  std::vector<GapData> no_padding{gap_data.begin(), gap_data.begin() + 4};
  auto builder = WriteExtendedInquiryResponseBuilder::Create(view.GetFecRequired(), no_padding);

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  builder->Serialize(it);

  EXPECT_EQ(packet_bytes->size(), view_bytes->size());
  for (size_t i = 0; i < view_bytes->size(); i++) {
    ASSERT_EQ(packet_bytes->at(i), view_bytes->at(i));
  }
}

//  TODO: Revisit reflection tests for EIR
// DEFINE_AND_INSTANTIATE_WriteExtendedInquiryResponseReflectionTest(pixel_3_xl_write_extended_inquiry_response,
// pixel_3_xl_write_extended_inquiry_response_no_uuids);

std::vector<uint8_t> le_set_scan_parameters{
    0x0b, 0x20, 0x07, 0x01, 0x12, 0x00, 0x12, 0x00, 0x01, 0x00,
};
TEST(HciPacketsTest, testLeSetScanParameters) {
  PacketView<kLittleEndian> packet_bytes_view(std::make_shared<std::vector<uint8_t>>(le_set_scan_parameters));
  auto view =
      LeSetScanParametersView::Create(LeScanningCommandView::Create(CommandPacketView::Create(packet_bytes_view)));

  ASSERT(view.IsValid());
  ASSERT_EQ(LeScanType::ACTIVE, view.GetLeScanType());
  ASSERT_EQ(0x12, view.GetLeScanInterval());
  ASSERT_EQ(0x12, view.GetLeScanWindow());
  ASSERT_EQ(AddressType::RANDOM_DEVICE_ADDRESS, view.GetOwnAddressType());
  ASSERT_EQ(LeSetScanningFilterPolicy::ACCEPT_ALL, view.GetScanningFilterPolicy());
}

DEFINE_AND_INSTANTIATE_LeSetScanParametersReflectionTest(le_set_scan_parameters);

std::vector<uint8_t> le_set_scan_enable{
    0x0c, 0x20, 0x02, 0x01, 0x00,
};
TEST(HciPacketsTest, testLeSetScanEnable) {
  PacketView<kLittleEndian> packet_bytes_view(std::make_shared<std::vector<uint8_t>>(le_set_scan_enable));
  auto view = LeSetScanEnableView::Create(LeScanningCommandView::Create(CommandPacketView::Create(packet_bytes_view)));

  ASSERT(view.IsValid());
  ASSERT_EQ(Enable::ENABLED, view.GetLeScanEnable());
  ASSERT_EQ(Enable::DISABLED, view.GetFilterDuplicates());
}

DEFINE_AND_INSTANTIATE_LeSetScanEnableReflectionTest(le_set_scan_enable);

std::vector<uint8_t> le_get_vendor_capabilities{
    0x53,
    0xfd,
    0x00,
};
TEST(HciPacketsTest, testLeGetVendorCapabilities) {
  PacketView<kLittleEndian> packet_bytes_view(std::make_shared<std::vector<uint8_t>>(le_get_vendor_capabilities));
  auto view =
      LeGetVendorCapabilitiesView::Create(VendorCommandView::Create(CommandPacketView::Create(packet_bytes_view)));

  ASSERT(view.IsValid());
}

DEFINE_AND_INSTANTIATE_LeGetVendorCapabilitiesReflectionTest(le_get_vendor_capabilities);

std::vector<uint8_t> le_get_vendor_capabilities_complete{
    0x0e, 0x0c, 0x01, 0x53, 0xfd, 0x00, 0x05, 0x01, 0x00, 0x04, 0x80, 0x01, 0x10, 0x01,
};
TEST(HciPacketsTest, testLeGetVendorCapabilitiesComplete) {
  PacketView<kLittleEndian> packet_bytes_view(
      std::make_shared<std::vector<uint8_t>>(le_get_vendor_capabilities_complete));
  auto view = LeGetVendorCapabilitiesCompleteView::Create(
      CommandCompleteView::Create(EventPacketView::Create(packet_bytes_view)));

  ASSERT(view.IsValid());
  auto base_capabilities = view.GetBaseVendorCapabilities();
  ASSERT_EQ(5, base_capabilities.max_advt_instances_);
  ASSERT_EQ(1, base_capabilities.offloaded_resolution_of_private_address_);
  ASSERT_EQ(1024, base_capabilities.total_scan_results_storage_);
  ASSERT_EQ(128, base_capabilities.max_irk_list_sz_);
  ASSERT_EQ(1, base_capabilities.filtering_support_);
  ASSERT_EQ(16, base_capabilities.max_filter_);
  ASSERT_EQ(1, base_capabilities.activity_energy_info_support_);
}

DEFINE_AND_INSTANTIATE_LeGetVendorCapabilitiesCompleteReflectionTest(le_get_vendor_capabilities_complete);

std::vector<uint8_t> le_set_extended_scan_parameters{
    0x41, 0x20, 0x08, 0x01, 0x00, 0x01, 0x01, 0x12, 0x00, 0x12, 0x00,
};

TEST(HciPacketsTest, testLeSetExtendedScanParameters) {
  PacketView<kLittleEndian> packet_bytes_view(std::make_shared<std::vector<uint8_t>>(le_set_extended_scan_parameters));
  auto view = LeSetExtendedScanParametersView::Create(
      LeScanningCommandView::Create(CommandPacketView::Create(packet_bytes_view)));

  ASSERT(view.IsValid());
  ASSERT_EQ(1, view.GetScanningPhys());
  auto params = view.GetParameters();
  ASSERT_EQ(1, params.size());
  ASSERT_EQ(LeScanType::ACTIVE, params[0].le_scan_type_);
  ASSERT_EQ(18, params[0].le_scan_interval_);
  ASSERT_EQ(18, params[0].le_scan_window_);
}

std::vector<uint8_t> le_set_extended_scan_parameters_6553{
    0x41, 0x20, 0x08, 0x01, 0x00, 0x01, 0x01, 0x99, 0x19, 0x99, 0x19,
};

TEST(HciPacketsTest, testLeSetExtendedScanParameters_6553) {
  PacketView<kLittleEndian> packet_bytes_view(
      std::make_shared<std::vector<uint8_t>>(le_set_extended_scan_parameters_6553));
  auto view = LeSetExtendedScanParametersView::Create(
      LeScanningCommandView::Create(CommandPacketView::Create(packet_bytes_view)));

  ASSERT(view.IsValid());
  ASSERT_EQ(1, view.GetScanningPhys());
  auto params = view.GetParameters();
  ASSERT_EQ(1, params.size());
  ASSERT_EQ(LeScanType::ACTIVE, params[0].le_scan_type_);
  ASSERT_EQ(6553, params[0].le_scan_interval_);
  ASSERT_EQ(6553, params[0].le_scan_window_);
}

DEFINE_AND_INSTANTIATE_LeSetExtendedScanParametersReflectionTest(le_set_extended_scan_parameters,
                                                                 le_set_extended_scan_parameters_6553);

std::vector<uint8_t> le_set_extended_scan_parameters_complete{
    0x0e, 0x04, 0x01, 0x41, 0x20, 0x00,
};
DEFINE_AND_INSTANTIATE_LeSetExtendedScanParametersCompleteReflectionTest(le_set_extended_scan_parameters_complete);

std::vector<uint8_t> le_set_extended_scan_enable{
    0x42, 0x20, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
};

TEST(HciPacketsTest, testLeSetExtendedScanEnable) {
  PacketView<kLittleEndian> packet_bytes_view(std::make_shared<std::vector<uint8_t>>(le_set_extended_scan_enable));
  auto view =
      LeSetExtendedScanEnableView::Create(LeScanningCommandView::Create(CommandPacketView::Create(packet_bytes_view)));

  ASSERT(view.IsValid());
  ASSERT_EQ(FilterDuplicates::DISABLED, view.GetFilterDuplicates());
  ASSERT_EQ(Enable::ENABLED, view.GetEnable());
  ASSERT_EQ(0, view.GetDuration());
  ASSERT_EQ(0, view.GetPeriod());
}

std::vector<uint8_t> le_set_extended_scan_enable_disable{
    0x42, 0x20, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
};

TEST(HciPacketsTest, testLeSetExtendedScanEnableDisable) {
  PacketView<kLittleEndian> packet_bytes_view(
      std::make_shared<std::vector<uint8_t>>(le_set_extended_scan_enable_disable));
  auto view =
      LeSetExtendedScanEnableView::Create(LeScanningCommandView::Create(CommandPacketView::Create(packet_bytes_view)));

  ASSERT(view.IsValid());
  ASSERT_EQ(FilterDuplicates::ENABLED, view.GetFilterDuplicates());
  ASSERT_EQ(Enable::DISABLED, view.GetEnable());
  ASSERT_EQ(0, view.GetDuration());
  ASSERT_EQ(0, view.GetPeriod());
}

DEFINE_AND_INSTANTIATE_LeSetExtendedScanEnableReflectionTest(le_set_extended_scan_enable,
                                                             le_set_extended_scan_enable_disable);

std::vector<uint8_t> le_set_extended_scan_enable_complete{
    0x0e, 0x04, 0x01, 0x42, 0x20, 0x00,
};
DEFINE_AND_INSTANTIATE_LeSetExtendedScanEnableCompleteReflectionTest(le_set_extended_scan_enable_complete);

std::vector<uint8_t> le_extended_create_connection = {
    0x43, 0x20, 0x2a, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x08,
    0x30, 0x00, 0x18, 0x00, 0x28, 0x00, 0x00, 0x00, 0xf4, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x30, 0x00, 0x18, 0x00, 0x28, 0x00, 0x00, 0x00, 0xf4, 0x01, 0x00, 0x00, 0x00, 0x00};
DEFINE_AND_INSTANTIATE_LeExtendedCreateConnectionReflectionTest(le_extended_create_connection);

TEST(HciPacketsTest, testLeExtendedCreateConnection) {
  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(le_extended_create_connection);
  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = LeExtendedCreateConnectionView::Create(
      LeConnectionManagementCommandView::Create(CommandPacketView::Create(packet_bytes_view)));
  ASSERT_TRUE(view.IsValid());
}

std::vector<uint8_t> le_set_extended_advertising_random_address = {
    0x35, 0x20, 0x07, 0x00, 0x77, 0x58, 0xeb, 0xd3, 0x1c, 0x6e,
};

TEST(HciPacketsTest, testLeSetExtendedAdvertisingRandomAddress) {
  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(le_set_extended_advertising_random_address);
  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = LeSetExtendedAdvertisingRandomAddressView::Create(
      LeAdvertisingCommandView::Create(CommandPacketView::Create(packet_bytes_view)));
  ASSERT_TRUE(view.IsValid());
  uint8_t random_address_bytes[] = {0x77, 0x58, 0xeb, 0xd3, 0x1c, 0x6e};
  ASSERT_EQ(0, view.GetAdvertisingHandle());
  ASSERT_EQ(Address(random_address_bytes), view.GetAdvertisingRandomAddress());
}
DEFINE_AND_INSTANTIATE_LeSetExtendedAdvertisingRandomAddressReflectionTest(le_set_extended_advertising_random_address);

std::vector<uint8_t> le_set_extended_advertising_random_address_complete{
    0x0e, 0x04, 0x01, 0x35, 0x20, 0x00,
};
DEFINE_AND_INSTANTIATE_LeSetExtendedAdvertisingRandomAddressCompleteReflectionTest(
    le_set_extended_advertising_random_address_complete);

std::vector<uint8_t> le_set_extended_advertising_data{
    0x37, 0x20, 0x12, 0x00, 0x03, 0x01, 0x0e, 0x02, 0x01, 0x02, 0x0a,
    0x09, 0x50, 0x69, 0x78, 0x65, 0x6c, 0x20, 0x33, 0x20, 0x58,
};
TEST(HciPacketsTest, testLeSetExtendedAdvertisingData) {
  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(le_set_extended_advertising_data);
  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = LeSetExtendedAdvertisingDataRawView::Create(
      LeAdvertisingCommandView::Create(CommandPacketView::Create(packet_bytes_view)));
  ASSERT_TRUE(view.IsValid());
  ASSERT_EQ(0, view.GetAdvertisingHandle());
  ASSERT_EQ(Operation::COMPLETE_ADVERTISEMENT, view.GetOperation());
  ASSERT_EQ(FragmentPreference::CONTROLLER_SHOULD_NOT, view.GetFragmentPreference());
  std::vector<uint8_t> advertising_data{
      0x02, 0x01, 0x02, 0x0a, 0x09, 0x50, 0x69, 0x78, 0x65, 0x6c, 0x20, 0x33, 0x20, 0x58,
  };
  ASSERT_EQ(advertising_data, view.GetAdvertisingData());
}

DEFINE_AND_INSTANTIATE_LeSetExtendedAdvertisingDataRawReflectionTest(le_set_extended_advertising_data);

std::vector<uint8_t> le_set_extended_advertising_data_complete{
    0x0e, 0x04, 0x01, 0x37, 0x20, 0x00,
};
DEFINE_AND_INSTANTIATE_LeSetExtendedAdvertisingDataCompleteReflectionTest(le_set_extended_advertising_data_complete);

std::vector<uint8_t> le_set_extended_advertising_parameters_set_0{
    0x36, 0x20, 0x19, 0x00, 0x13, 0x00, 0x90, 0x01, 0x00, 0xc2, 0x01, 0x00, 0x07, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf9, 0x01, 0x00, 0x01, 0x01, 0x00,
};
TEST(HciPacketsTest, testLeSetExtendedAdvertisingParametersLegacySet0) {
  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(le_set_extended_advertising_parameters_set_0);
  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = LeSetExtendedAdvertisingLegacyParametersView::Create(
      LeAdvertisingCommandView::Create(CommandPacketView::Create(packet_bytes_view)));
  ASSERT_TRUE(view.IsValid());
  ASSERT_EQ(0, view.GetAdvertisingHandle());
  ASSERT_EQ(400, view.GetPrimaryAdvertisingIntervalMin());
  ASSERT_EQ(450, view.GetPrimaryAdvertisingIntervalMax());
  ASSERT_EQ(0x7, view.GetPrimaryAdvertisingChannelMap());
  ASSERT_EQ(OwnAddressType::RANDOM_DEVICE_ADDRESS, view.GetOwnAddressType());
  ASSERT_EQ(PeerAddressType::PUBLIC_DEVICE_OR_IDENTITY_ADDRESS, view.GetPeerAddressType());
  ASSERT_EQ(Address::kEmpty, view.GetPeerAddress());
  ASSERT_EQ(AdvertisingFilterPolicy::ALL_DEVICES, view.GetAdvertisingFilterPolicy());
  ASSERT_EQ(1, view.GetAdvertisingSid());
  ASSERT_EQ(Enable::DISABLED, view.GetScanRequestNotificationEnable());
}

std::vector<uint8_t> le_set_extended_advertising_parameters_set_1{
    0x36, 0x20, 0x19, 0x01, 0x13, 0x00, 0x90, 0x01, 0x00, 0xc2, 0x01, 0x00, 0x07, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf9, 0x01, 0x00, 0x01, 0x01, 0x00,
};
TEST(HciPacketsTest, testLeSetExtendedAdvertisingParametersSet1) {
  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(le_set_extended_advertising_parameters_set_1);
  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = LeSetExtendedAdvertisingLegacyParametersView::Create(
      LeAdvertisingCommandView::Create(CommandPacketView::Create(packet_bytes_view)));
  ASSERT_TRUE(view.IsValid());
  ASSERT_EQ(1, view.GetAdvertisingHandle());
  ASSERT_EQ(400, view.GetPrimaryAdvertisingIntervalMin());
  ASSERT_EQ(450, view.GetPrimaryAdvertisingIntervalMax());
  ASSERT_EQ(0x7, view.GetPrimaryAdvertisingChannelMap());
  ASSERT_EQ(OwnAddressType::RANDOM_DEVICE_ADDRESS, view.GetOwnAddressType());
  ASSERT_EQ(PeerAddressType::PUBLIC_DEVICE_OR_IDENTITY_ADDRESS, view.GetPeerAddressType());
  ASSERT_EQ(Address::kEmpty, view.GetPeerAddress());
  ASSERT_EQ(AdvertisingFilterPolicy::ALL_DEVICES, view.GetAdvertisingFilterPolicy());
  ASSERT_EQ(1, view.GetAdvertisingSid());
  ASSERT_EQ(Enable::DISABLED, view.GetScanRequestNotificationEnable());
}

DEFINE_AND_INSTANTIATE_LeSetExtendedAdvertisingLegacyParametersReflectionTest(
    le_set_extended_advertising_parameters_set_0, le_set_extended_advertising_parameters_set_1);

std::vector<uint8_t> le_set_extended_advertising_parameters_complete{0x0e, 0x05, 0x01, 0x36, 0x20, 0x00, 0xf5};
TEST(HciPacketsTest, testLeSetExtendedAdvertisingParametersComplete) {
  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(le_set_extended_advertising_parameters_complete);
  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = LeSetExtendedAdvertisingParametersCompleteView::Create(
      CommandCompleteView::Create(EventPacketView::Create(packet_bytes_view)));
  ASSERT_TRUE(view.IsValid());
  ASSERT_EQ(static_cast<uint8_t>(-11), view.GetSelectedTxPower());
}

DEFINE_AND_INSTANTIATE_LeSetExtendedAdvertisingParametersCompleteReflectionTest(
    le_set_extended_advertising_parameters_complete);

std::vector<uint8_t> le_remove_advertising_set_1{
    0x3c,
    0x20,
    0x01,
    0x01,
};
TEST(HciPacketsTest, testLeRemoveAdvertisingSet1) {
  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(le_remove_advertising_set_1);
  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = LeRemoveAdvertisingSetView::Create(
      LeAdvertisingCommandView::Create(CommandPacketView::Create(packet_bytes_view)));
  ASSERT_TRUE(view.IsValid());
  ASSERT_EQ(1, view.GetAdvertisingHandle());
}

DEFINE_AND_INSTANTIATE_LeRemoveAdvertisingSetReflectionTest(le_remove_advertising_set_1);

std::vector<uint8_t> le_remove_advertising_set_complete{
    0x0e, 0x04, 0x01, 0x3c, 0x20, 0x00,
};
DEFINE_AND_INSTANTIATE_LeRemoveAdvertisingSetCompleteReflectionTest(le_remove_advertising_set_complete);

std::vector<uint8_t> le_set_extended_advertising_disable_1{
    0x39, 0x20, 0x06, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00,
};
TEST(HciPacketsTest, testLeSetExtendedAdvertisingDisable1) {
  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(le_set_extended_advertising_disable_1);
  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = LeSetExtendedAdvertisingDisableView::Create(
      LeAdvertisingCommandView::Create(CommandPacketView::Create(packet_bytes_view)));
  ASSERT_TRUE(view.IsValid());
  auto disabled_set = view.GetDisabledSets();
  ASSERT_EQ(1, disabled_set.size());
  ASSERT_EQ(1, disabled_set[0].advertising_handle_);
}

DEFINE_AND_INSTANTIATE_LeSetExtendedAdvertisingDisableReflectionTest(le_set_extended_advertising_disable_1);

std::vector<uint8_t> le_set_extended_advertising_enable_complete{
    0x0e, 0x04, 0x01, 0x39, 0x20, 0x00,
};
DEFINE_AND_INSTANTIATE_LeSetExtendedAdvertisingEnableCompleteReflectionTest(
    le_set_extended_advertising_enable_complete);

TEST(HciPacketsTest, testLeSetAdvertisingDataBuilderLength) {
  GapData gap_data;
  gap_data.data_type_ = GapDataType::COMPLETE_LOCAL_NAME;
  gap_data.data_ = std::vector<uint8_t>({'A', ' ', 'g', 'o', 'o', 'd', ' ', 'n', 'a', 'm', 'e'});
  auto builder = LeSetAdvertisingDataBuilder::Create({gap_data});
  ASSERT_EQ(2 /*opcode*/ + 1 /* parameter size */ + 1 /* data_length */ + 31 /* data */, builder->size());

  auto packet_bytes = std::make_shared<std::vector<uint8_t>>();
  packet_bytes->reserve(builder->size());
  BitInserter bit_inserter(*packet_bytes);
  builder->Serialize(bit_inserter);
  auto command_view = LeAdvertisingCommandView::Create(CommandPacketView::Create(packet_bytes));
  ASSERT(command_view.IsValid());
  ASSERT_EQ(1 /* data_length */ + 31 /* data */, command_view.GetPayload().size());
  auto view = LeSetAdvertisingDataView::Create(command_view);
  ASSERT(view.IsValid());
}

TEST(HciPacketsTest, testLeSetScanResponseDataBuilderLength) {
  GapData gap_data;
  gap_data.data_type_ = GapDataType::COMPLETE_LOCAL_NAME;
  gap_data.data_ = std::vector<uint8_t>({'A', ' ', 'g', 'o', 'o', 'd', ' ', 'n', 'a', 'm', 'e'});
  auto builder = LeSetScanResponseDataBuilder::Create({gap_data});
  ASSERT_EQ(2 /*opcode*/ + 1 /* parameter size */ + 1 /*data_length */ + 31 /* data */, builder->size());

  auto packet_bytes = std::make_shared<std::vector<uint8_t>>();
  packet_bytes->reserve(builder->size());
  BitInserter bit_inserter(*packet_bytes);
  builder->Serialize(bit_inserter);
  auto command_view = LeAdvertisingCommandView::Create(CommandPacketView::Create(packet_bytes));
  ASSERT(command_view.IsValid());
  ASSERT_EQ(1 /* data_length */ + 31 /* data */, command_view.GetPayload().size());
  auto view = LeSetScanResponseDataView::Create(command_view);
  ASSERT(view.IsValid());
}

TEST(HciPacketsTest, testLeMultiAdvSetAdvertisingDataBuilderLength) {
  GapData gap_data;
  gap_data.data_type_ = GapDataType::COMPLETE_LOCAL_NAME;
  gap_data.data_ = std::vector<uint8_t>({'A', ' ', 'g', 'o', 'o', 'd', ' ', 'n', 'a', 'm', 'e'});
  uint8_t set = 3;
  auto builder = LeMultiAdvtSetDataBuilder::Create({gap_data}, set);
  ASSERT_EQ(2 /*opcode*/ + 1 /* parameter size */ + 1 /* data_length */ + 31 /* data */ + 1 /* set */, builder->size());

  auto packet_bytes = std::make_shared<std::vector<uint8_t>>();
  packet_bytes->reserve(builder->size());
  BitInserter bit_inserter(*packet_bytes);
  builder->Serialize(bit_inserter);
  auto command_view =
      LeMultiAdvtView::Create(LeAdvertisingCommandView::Create(CommandPacketView::Create(packet_bytes)));
  ASSERT(command_view.IsValid());
  EXPECT_EQ(1 /* data_length */ + 31 /* data */ + 1 /* set */, command_view.GetPayload().size());
  auto view = LeMultiAdvtSetDataView::Create(command_view);
  ASSERT(view.IsValid());
}

TEST(HciPacketsTest, testLeMultiAdvSetScanResponseDataBuilderLength) {
  GapData gap_data;
  gap_data.data_type_ = GapDataType::COMPLETE_LOCAL_NAME;
  gap_data.data_ = std::vector<uint8_t>({'A', ' ', 'g', 'o', 'o', 'd', ' ', 'n', 'a', 'm', 'e'});
  uint8_t set = 3;
  auto builder = LeMultiAdvtSetScanRespBuilder::Create({gap_data}, set);
  EXPECT_EQ(2 /*opcode*/ + 1 /* parameter size */ + 1 /*data_length */ + 31 /* data */ + 1 /* set */, builder->size());

  auto packet_bytes = std::make_shared<std::vector<uint8_t>>();
  packet_bytes->reserve(builder->size());
  BitInserter bit_inserter(*packet_bytes);
  builder->Serialize(bit_inserter);
  auto command_view =
      LeMultiAdvtView::Create(LeAdvertisingCommandView::Create(CommandPacketView::Create(packet_bytes)));
  ASSERT(command_view.IsValid());
  ASSERT_EQ(1 /* data_length */ + 31 /* data */ + 1 /* set */, command_view.GetPayload().size());
  auto view = LeMultiAdvtSetScanRespView::Create(command_view);
  ASSERT(view.IsValid());
}

std::vector<uint8_t> controller_bqr = {0x5e, 0xfd, 0x07, 0x00, 0x1f, 0x00, 0x07, 0x00, 0x88, 0x13};
DEFINE_AND_INSTANTIATE_ControllerBqrReflectionTest(controller_bqr);

std::vector<uint8_t> controller_bqr_complete = {0x0e, 0x08, 0x01, 0x5e, 0xfd, 0x00, 0x1f, 0x00, 0x07, 0x00};
DEFINE_AND_INSTANTIATE_ControllerBqrCompleteReflectionTest(controller_bqr_complete);

std::vector<uint8_t> bqr_monitor_mode_event = {
    0xff, 0x31, 0x58, 0x01, 0x10, 0x02, 0x00, 0x00, 0x07, 0xd5, 0x00, 0x14, 0x00, 0x40, 0x1f, 0xed, 0x41,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x3c, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
DEFINE_AND_INSTANTIATE_BqrMonitorModeEventReflectionTest(bqr_monitor_mode_event);

std::vector<uint8_t> bqr_approach_lsto_event = {
    0xff, 0x48, 0x58, 0x02, 0x10, 0x02, 0x00, 0x01, 0x09, 0xaf, 0x00, 0x2d, 0x00, 0x00, 0x7d, 0x94, 0xe9, 0x03, 0x01,
    0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x30, 0xa8, 0x0f, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x81, 0x9b, 0xf2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xcc, 0xcc, 0xcc, 0xcc, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x4e, 0x11, 0x00, 0x0c, 0x54, 0x10, 0x00};
DEFINE_AND_INSTANTIATE_BqrApproachLstoEventReflectionTest(bqr_approach_lsto_event);

std::vector<uint8_t> bqr_a2dp_audio_choppy_event = {
    0xff, 0x41, 0x58, 0x03, 0x19, 0x09, 0x00, 0x00, 0x07, 0xcb, 0x00, 0x3a, 0x01, 0x40, 0x1f, 0x7e, 0xce,
    0x58, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x7e, 0xce, 0x58,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0xd1, 0x57, 0x00, 0x30, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0d, 0xce, 0x58, 0x00, 0x3a, 0xce, 0x58, 0x00, 0x01, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x01};
DEFINE_AND_INSTANTIATE_BqrA2dpAudioChoppyEventReflectionTest(bqr_a2dp_audio_choppy_event);

std::vector<uint8_t> bqr_sco_voice_choppy_event = {
    0xff, 0x4a, 0x58, 0x04, 0x09, 0x08, 0x00, 0x00, 0x08, 0xbf, 0x00, 0x03, 0x00, 0x40, 0x1f, 0x92, 0x6c, 0x0a, 0x0d,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x06, 0x02, 0x02, 0x0b,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00};
DEFINE_AND_INSTANTIATE_BqrScoVoiceChoppyEventReflectionTest(bqr_sco_voice_choppy_event);

std::vector<uint8_t> bqr_root_inflammation_event = {0xff, 0x04, 0x58, 0x05, 0x00, 0xfe};
DEFINE_AND_INSTANTIATE_BqrRootInflammationEventReflectionTest(bqr_root_inflammation_event);

std::vector<uint8_t> bqr_lmp_ll_message_trace_event = {0xff, 0x11, 0x58, 0x11, 0x03, 0x00, 0x01, 0xff, 0x11, 0x55,
                                                       0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55};
DEFINE_AND_INSTANTIATE_BqrLmpLlMessageTraceEventReflectionTest(bqr_lmp_ll_message_trace_event);

std::vector<uint8_t> bqr_bt_scheduling_trace_event = {0xff, 0x1d, 0x58, 0x12, 0x05, 0x00, 0x02, 0xd9, 0xae, 0x08, 0x01,
                                                      0x05, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
                                                      0x00, 0x01, 0x0c, 0x00, 0x36, 0x3c, 0x00, 0x00, 0x00};
DEFINE_AND_INSTANTIATE_BqrBtSchedulingTraceEventReflectionTest(bqr_bt_scheduling_trace_event);

}  // namespace hci
}  // namespace bluetooth
