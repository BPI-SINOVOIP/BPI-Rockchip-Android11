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

#include "hci/hci_packets.h"

#include <gtest/gtest.h>
#include <memory>

#include "os/log.h"
#include "packet/bit_inserter.h"
#include "packet/raw_builder.h"

using bluetooth::packet::BitInserter;
using bluetooth::packet::RawBuilder;
using std::vector;

namespace {
vector<uint8_t> information_request = {
    0xfe, 0x2e, 0x0a, 0x00, 0x06, 0x00, 0x01, 0x00, 0x0a, 0x02, 0x02, 0x00, 0x02, 0x00,
};
// 0x00, 0x01, 0x02, 0x03, ...
vector<uint8_t> counting_bytes;
// 0xFF, 0xFE, 0xFD, 0xFC, ...
vector<uint8_t> counting_down_bytes;
const size_t count_size = 0x8;

}  // namespace

namespace bluetooth {
namespace hci {

class AclBuilderTest : public ::testing::Test {
 public:
  AclBuilderTest() {
    counting_bytes.reserve(count_size);
    counting_down_bytes.reserve(count_size);
    for (size_t i = 0; i < count_size; i++) {
      counting_bytes.push_back(i);
      counting_down_bytes.push_back(~i);
    }
  }
  ~AclBuilderTest() = default;
};

TEST(AclBuilderTest, buildAclCount) {
  uint16_t handle = 0x0314;
  PacketBoundaryFlag packet_boundary_flag = PacketBoundaryFlag::FIRST_AUTOMATICALLY_FLUSHABLE;
  BroadcastFlag broadcast_flag = BroadcastFlag::ACTIVE_SLAVE_BROADCAST;

  std::unique_ptr<RawBuilder> count_payload = std::make_unique<RawBuilder>();
  count_payload->AddOctets(counting_bytes);
  ASSERT_EQ(counting_bytes.size(), count_payload->size());

  std::unique_ptr<AclPacketBuilder> count_packet =
      AclPacketBuilder::Create(handle, packet_boundary_flag, broadcast_flag, std::move(count_payload));

  ASSERT_EQ(counting_bytes.size() + 4, count_packet->size());

  std::shared_ptr<std::vector<uint8_t>> count_packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*count_packet_bytes);
  count_packet->Serialize(it);

  PacketView<true> count_packet_bytes_view(count_packet_bytes);
  AclPacketView count_packet_view = AclPacketView::Create(count_packet_bytes_view);
  ASSERT_TRUE(count_packet_view.IsValid());

  ASSERT_EQ(handle, count_packet_view.GetHandle());
  ASSERT_EQ(packet_boundary_flag, count_packet_view.GetPacketBoundaryFlag());
  ASSERT_EQ(broadcast_flag, count_packet_view.GetBroadcastFlag());
  PacketView<true> count_view = count_packet_view.GetPayload();

  ASSERT_EQ(count_view.size(), counting_bytes.size());
  for (size_t i = 0; i < count_view.size(); i++) {
    ASSERT_EQ(count_view[i], counting_bytes[i]);
  }
}

TEST(AclBuilderTest, buildAclCountInverted) {
  uint16_t handle = 0x0304;
  PacketBoundaryFlag packet_boundary_flag = PacketBoundaryFlag::CONTINUING_FRAGMENT;
  BroadcastFlag broadcast_flag = BroadcastFlag::POINT_TO_POINT;

  std::unique_ptr<RawBuilder> counting_down_bytes_payload = std::make_unique<RawBuilder>();
  counting_down_bytes_payload->AddOctets(counting_down_bytes);
  ASSERT_EQ(counting_down_bytes.size(), counting_down_bytes_payload->size());

  std::unique_ptr<AclPacketBuilder> counting_down_bytes_packet =
      AclPacketBuilder::Create(handle, packet_boundary_flag, broadcast_flag, std::move(counting_down_bytes_payload));

  ASSERT_EQ(counting_down_bytes.size() + 4, counting_down_bytes_packet->size());

  std::shared_ptr<std::vector<uint8_t>> counting_down_bytes_packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*counting_down_bytes_packet_bytes);
  counting_down_bytes_packet->Serialize(it);
  PacketView<true> counting_down_bytes_packet_bytes_view(counting_down_bytes_packet_bytes);
  AclPacketView counting_down_bytes_packet_view = AclPacketView::Create(counting_down_bytes_packet_bytes_view);
  ASSERT_TRUE(counting_down_bytes_packet_view.IsValid());

  ASSERT_EQ(handle, counting_down_bytes_packet_view.GetHandle());
  ASSERT_EQ(packet_boundary_flag, counting_down_bytes_packet_view.GetPacketBoundaryFlag());
  ASSERT_EQ(broadcast_flag, counting_down_bytes_packet_view.GetBroadcastFlag());
  PacketView<true> counting_down_bytes_view = counting_down_bytes_packet_view.GetPayload();

  ASSERT_EQ(counting_down_bytes_view.size(), counting_down_bytes.size());
  for (size_t i = 0; i < counting_down_bytes_view.size(); i++) {
    ASSERT_EQ(counting_down_bytes_view[i], counting_down_bytes[i]);
  }
}

TEST(AclBuilderTest, buildInformationRequest) {
  uint16_t handle = 0x0efe;
  PacketBoundaryFlag packet_boundary_flag = PacketBoundaryFlag::FIRST_AUTOMATICALLY_FLUSHABLE;
  BroadcastFlag broadcast_flag = BroadcastFlag::POINT_TO_POINT;

  std::vector<uint8_t> payload_bytes(information_request.begin() + 4, information_request.end());
  std::unique_ptr<RawBuilder> payload = std::make_unique<RawBuilder>();
  payload->AddOctets(payload_bytes);
  ASSERT_EQ(payload_bytes.size(), payload->size());

  std::unique_ptr<AclPacketBuilder> packet =
      AclPacketBuilder::Create(handle, packet_boundary_flag, broadcast_flag, std::move(payload));

  ASSERT_EQ(information_request.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);
  PacketView<true> packet_bytes_view(packet_bytes);
  AclPacketView packet_view = AclPacketView::Create(packet_bytes_view);
  ASSERT_TRUE(packet_view.IsValid());

  ASSERT_EQ(packet_bytes->size(), information_request.size());
  for (size_t i = 0; i < packet_bytes->size(); i++) {
    ASSERT_EQ((*packet_bytes)[i], information_request[i]);
  }

  ASSERT_EQ(handle, packet_view.GetHandle());
  ASSERT_EQ(packet_boundary_flag, packet_view.GetPacketBoundaryFlag());
  ASSERT_EQ(broadcast_flag, packet_view.GetBroadcastFlag());
  PacketView<true> payload_view = packet_view.GetPayload();

  ASSERT_EQ(payload_view.size(), payload_bytes.size());
  for (size_t i = 0; i < payload_view.size(); i++) {
    ASSERT_EQ(payload_view[i], payload_bytes[i]);
  }

  ASSERT_EQ(packet_view.size(), information_request.size());
  for (size_t i = 0; i < packet_view.size(); i++) {
    ASSERT_EQ(packet_view[i], information_request[i]);
  }
}

}  // namespace hci
}  // namespace bluetooth
