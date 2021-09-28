/*
 * Copyright 2020 The Android Open Source Project
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

#include <base/logging.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <queue>

#include "hci/src/packet_fragmenter.cc"
#include "osi/test/AllocationTestHarness.h"

extern void allocation_tracker_uninit(void);

enum kPacketOrder {
  kStart = 1,
  kContinuation = 2,
};

struct AclPacketHeader {
  struct {
    uint16_t handle : 12;
    uint16_t continuation : 1;
    uint16_t start : 1;
    uint16_t reserved : 2;
  } s;
  uint16_t length;

  uint16_t GetRawHandle() const { return *(uint16_t*)(this); }

  uint16_t GetHandle() const { return s.handle; }
  uint16_t GetLength() const { return length; }
} __attribute__((packed));

struct L2capPacketHeader {
  uint16_t length;
  uint16_t cid;
} __attribute__((packed));

struct AclL2capPacketHeader {
  struct AclPacketHeader acl_header;
  struct L2capPacketHeader l2cap_header;
} __attribute__((packed));

namespace {

constexpr uint16_t kHandle = 0x123;
constexpr uint16_t kCid = 0x4567;
constexpr uint16_t kMaxPacketSize = BT_DEFAULT_BUFFER_SIZE - sizeof(BT_HDR) -
                                    L2CAP_HEADER_SIZE - HCI_ACL_PREAMBLE_SIZE;
constexpr size_t kTypicalPacketSizes[] = {
    1, 2, 3, 4, 8, 16, 32, 64, 127, 128, 129, 256, 1024, 2048, kMaxPacketSize};
constexpr size_t kNumberOfTypicalPacketSizes =
    sizeof(kTypicalPacketSizes) / sizeof(kTypicalPacketSizes[0]);

void FreeBuffer(BT_HDR* bt_hdr) { osi_free(bt_hdr); }

struct TestMutables {
  struct {
    int access_count_{0};
  } fragmented;
  struct {
    int access_count_{0};
    std::queue<std::unique_ptr<BT_HDR, decltype(&FreeBuffer)>> queue;
  } reassembled;
  struct {
    int access_count_{0};
  } transmit_finished;
};

TestMutables test_state_;

void OnFragmented(BT_HDR* packet, bool send_transmit_finished) {
  test_state_.fragmented.access_count_++;
}

void OnReassembled(BT_HDR* packet) {
  test_state_.reassembled.access_count_++;
  test_state_.reassembled.queue.push(
      std::unique_ptr<BT_HDR, decltype(&FreeBuffer)>(packet, &FreeBuffer));
}

void OnTransmitFinished(BT_HDR* packet, bool all_fragments_sent) {
  test_state_.transmit_finished.access_count_++;
}

packet_fragmenter_callbacks_t result_callbacks = {
    .fragmented = OnFragmented,
    .reassembled = OnReassembled,
    .transmit_finished = OnTransmitFinished,
};

AclPacketHeader* AclHeader(BT_HDR* packet) {
  return (AclPacketHeader*)packet->data;
}
L2capPacketHeader* L2capHeader(BT_HDR* packet) {
  return &((AclL2capPacketHeader*)packet->data)->l2cap_header;
}

uint8_t* Data(BT_HDR* packet) {
  AclPacketHeader* acl_header =
      reinterpret_cast<AclPacketHeader*>(packet->data);
  return acl_header->s.start
             ? (uint8_t*)(packet->data + sizeof(AclL2capPacketHeader))
             : (uint8_t*)(packet->data + sizeof(AclPacketHeader));
}

}  // namespace

// Needed for linkage
const controller_t* controller_get_interface() { return nullptr; }

/**
 * Test class to test selected functionality in hci/src/hci_layer.cc
 */
class HciPacketFragmenterTest : public AllocationTestHarness {
 protected:
  void SetUp() override {
    AllocationTestHarness::SetUp();
    // Disable our allocation tracker to allow ASAN full range
    allocation_tracker_uninit();
    packet_fragmenter_ = packet_fragmenter_get_interface();
    packet_fragmenter_->init(&result_callbacks);
    test_state_ = TestMutables();
  }

  void TearDown() override {
    FlushPartialPackets();
    while (!test_state_.reassembled.queue.empty()) {
      test_state_.reassembled.queue.pop();
    }
    packet_fragmenter_->cleanup();
    AllocationTestHarness::TearDown();
  }
  const packet_fragmenter_t* packet_fragmenter_;

  // Start acl packet
  BT_HDR* AllocateL2capPacket(size_t l2cap_length,
                              const std::vector<uint8_t> data) const {
    auto packet =
        AllocateAclPacket(data.size() + sizeof(L2capPacketHeader), kStart);
    L2capHeader(packet)->length = l2cap_length;
    L2capHeader(packet)->cid = kCid;
    std::copy(data.cbegin(), data.cend(), Data(packet));
    return packet;
  }

  // Continuation acl packet
  BT_HDR* AllocateL2capPacket(const std::vector<uint8_t> data) const {
    auto packet = AllocateAclPacket(data.size(), kContinuation);
    std::copy(data.cbegin(), data.cend(), Data(packet));
    return packet;
  }

  const std::vector<uint8_t> CreateData(size_t size) const {
    CHECK(size > 0);
    std::vector<uint8_t> v(size);
    uint8_t sum = 0;
    for (size_t s = 0; s < size; s++) {
      sum += v[s] = s;
    }
    v[0] = (~sum + 1);  // First byte has sum complement
    return v;
  }

  // Verify packet integrity
  bool VerifyData(const uint8_t* data, size_t size) const {
    CHECK(size > 0);
    uint8_t sum = 0;
    for (size_t s = 0; s < size; s++) {
      sum += data[s];
    }
    return sum == 0;
  }

 private:
  BT_HDR* AllocateAclPacket(size_t acl_length,
                            kPacketOrder packet_order) const {
    BT_HDR* packet = AllocatePacket(sizeof(AclPacketHeader) + acl_length,
                                    MSG_HC_TO_STACK_HCI_ACL);
    AclHeader(packet)->s.handle = kHandle;
    AclHeader(packet)->length = acl_length;
    switch (packet_order) {
      case kStart:
        AclHeader(packet)->s.start = 1;
        break;
      case kContinuation:
        AclHeader(packet)->s.continuation = 1;
        break;
    }
    return packet;
  }

  BT_HDR* AllocatePacket(size_t packet_length, uint16_t event_mask) const {
    BT_HDR* packet =
        static_cast<BT_HDR*>(osi_calloc(sizeof(BT_HDR) + packet_length));
    packet->event = event_mask;
    packet->len = static_cast<uint16_t>(packet_length);
    return packet;
  }

  void FlushPartialPackets() const {
    while (!partial_packets.empty()) {
      BT_HDR* partial_packet = partial_packets.at(kHandle);
      partial_packets.erase(kHandle);
      osi_free(partial_packet);
    }
  }
};

TEST_F(HciPacketFragmenterTest, TestStruct_Handle) {
  AclPacketHeader acl_header;
  memset(&acl_header, 0, sizeof(acl_header));

  for (uint16_t h = 0; h < UINT16_MAX; h++) {
    acl_header.s.handle = h;
    CHECK(acl_header.GetHandle() == (h & HANDLE_MASK));
    CHECK(acl_header.s.continuation == 0);
    CHECK(acl_header.s.start == 0);
    CHECK(acl_header.s.reserved == 0);

    CHECK((acl_header.GetRawHandle() & HANDLE_MASK) == (h & HANDLE_MASK));
    GET_BOUNDARY_FLAG(acl_header.GetRawHandle() == 0);
  }
}

TEST_F(HciPacketFragmenterTest, TestStruct_Continuation) {
  AclPacketHeader acl_header;
  memset(&acl_header, 0, sizeof(acl_header));

  for (uint16_t h = 0; h < UINT16_MAX; h++) {
    acl_header.s.continuation = h;
    CHECK(acl_header.GetHandle() == 0);
    CHECK(acl_header.s.continuation == (h & 0x1));
    CHECK(acl_header.s.start == 0);
    CHECK(acl_header.s.reserved == 0);

    CHECK((acl_header.GetRawHandle() & HANDLE_MASK) == 0);
    GET_BOUNDARY_FLAG(acl_header.GetRawHandle() == (h & 0x3));
  }
}

TEST_F(HciPacketFragmenterTest, TestStruct_Start) {
  AclPacketHeader acl_header;
  memset(&acl_header, 0, sizeof(acl_header));

  for (uint16_t h = 0; h < UINT16_MAX; h++) {
    acl_header.s.start = h;
    CHECK(acl_header.GetHandle() == 0);
    CHECK(acl_header.s.continuation == 0);
    CHECK(acl_header.s.start == (h & 0x1));
    CHECK(acl_header.s.reserved == 0);

    CHECK((acl_header.GetRawHandle() & HANDLE_MASK) == 0);
    GET_BOUNDARY_FLAG(acl_header.GetRawHandle() == (h & 0x3));
  }
}

TEST_F(HciPacketFragmenterTest, TestStruct_Reserved) {
  AclPacketHeader acl_header;
  memset(&acl_header, 0, sizeof(acl_header));

  for (uint16_t h = 0; h < UINT16_MAX; h++) {
    acl_header.s.reserved = h;
    CHECK(acl_header.GetHandle() == 0);
    CHECK(acl_header.s.continuation == 0);
    CHECK(acl_header.s.start == 0);
    CHECK(acl_header.s.reserved == (h & 0x3));
  }
}
TEST_F(HciPacketFragmenterTest, CreateAndVerifyPackets) {
  const size_t size_check[] = {1,  2,   3,   4,   8,   16,   32,
                               64, 127, 128, 129, 256, 1024, 0xfff0};
  const std::vector<size_t> sizes(
      size_check, size_check + sizeof(size_check) / sizeof(size_check[0]));

  for (const auto packet_size : sizes) {
    const std::vector<uint8_t> data = CreateData(packet_size);
    uint8_t buf[packet_size];
    std::copy(data.cbegin(), data.cend(), buf);
    CHECK(VerifyData(buf, packet_size));
  }
}

TEST_F(HciPacketFragmenterTest, OnePacket_Immediate) {
  const std::vector<size_t> sizes(
      kTypicalPacketSizes, kTypicalPacketSizes + kNumberOfTypicalPacketSizes);

  int reassembled_access_count = 0;
  for (const auto packet_size : sizes) {
    const std::vector<uint8_t> data = CreateData(packet_size);
    reassemble_and_dispatch(AllocateL2capPacket(data.size(), data));

    CHECK(partial_packets.size() == 0);
    CHECK(test_state_.reassembled.access_count_ == ++reassembled_access_count);
    auto packet = std::move(test_state_.reassembled.queue.front());
    test_state_.reassembled.queue.pop();
    CHECK(VerifyData(Data(packet.get()), packet_size));
  }
}

TEST_F(HciPacketFragmenterTest, OnePacket_ImmediateTooBig) {
  const size_t packet_size = kMaxPacketSize + 1;
  const std::vector<uint8_t> data = CreateData(packet_size);
  reassemble_and_dispatch(AllocateL2capPacket(data.size(), data));

  CHECK(partial_packets.size() == 0);
  CHECK(test_state_.reassembled.access_count_ == 0);
}

TEST_F(HciPacketFragmenterTest, ThreePackets_Immediate) {
  const size_t packet_size = 512;
  const std::vector<uint8_t> data = CreateData(packet_size);
  reassemble_and_dispatch(AllocateL2capPacket(data.size(), data));
  reassemble_and_dispatch(AllocateL2capPacket(data.size(), data));
  reassemble_and_dispatch(AllocateL2capPacket(data.size(), data));
  CHECK(partial_packets.size() == 0);
  CHECK(test_state_.reassembled.access_count_ == 3);
}

TEST_F(HciPacketFragmenterTest, OnePacket_SplitTwo) {
  const std::vector<size_t> sizes(
      kTypicalPacketSizes, kTypicalPacketSizes + kNumberOfTypicalPacketSizes);

  int reassembled_access_count = 0;
  for (auto packet_size : sizes) {
    const std::vector<uint8_t> data = CreateData(packet_size);
    const std::vector<uint8_t> part1(data.cbegin(),
                                     data.cbegin() + packet_size / 2);
    reassemble_and_dispatch(AllocateL2capPacket(data.size(), part1));

    CHECK(partial_packets.size() == 1);
    CHECK(test_state_.reassembled.access_count_ == reassembled_access_count);

    const std::vector<uint8_t> part2(data.cbegin() + packet_size / 2,
                                     data.cend());
    reassemble_and_dispatch(AllocateL2capPacket(part2));

    CHECK(partial_packets.size() == 0);
    CHECK(test_state_.reassembled.access_count_ == ++reassembled_access_count);

    auto packet = std::move(test_state_.reassembled.queue.front());
    test_state_.reassembled.queue.pop();
    CHECK(VerifyData(Data(packet.get()), packet_size));
  }
}

TEST_F(HciPacketFragmenterTest, OnePacket_SplitALot) {
  const size_t packet_size = 512;
  const size_t stride = 2;

  const std::vector<uint8_t> data = CreateData(packet_size);
  const std::vector<uint8_t> first_part(data.cbegin(), data.cbegin() + stride);
  reassemble_and_dispatch(AllocateL2capPacket(data.size(), first_part));
  CHECK(partial_packets.size() == 1);

  for (size_t i = 2; i < packet_size - stride; i += stride) {
    const std::vector<uint8_t> middle_part(data.cbegin() + i,
                                           data.cbegin() + i + stride);
    reassemble_and_dispatch(AllocateL2capPacket(middle_part));
  }
  CHECK(partial_packets.size() == 1);
  CHECK(test_state_.reassembled.access_count_ == 0);

  const std::vector<uint8_t> last_part(data.cbegin() + packet_size - stride,
                                       data.cend());
  reassemble_and_dispatch(AllocateL2capPacket(last_part));

  CHECK(partial_packets.size() == 0);
  CHECK(test_state_.reassembled.access_count_ == 1);
  auto packet = std::move(test_state_.reassembled.queue.front());
  CHECK(VerifyData(Data(packet.get()), packet_size));
}

TEST_F(HciPacketFragmenterTest, TwoPacket_InvalidLength) {
  const size_t packet_size = UINT16_MAX;
  const std::vector<uint8_t> data = CreateData(packet_size);
  const std::vector<uint8_t> first_part(data.cbegin(),
                                        data.cbegin() + packet_size / 2);
  reassemble_and_dispatch(AllocateL2capPacket(data.size(), first_part));

  CHECK(partial_packets.size() == 0);
  CHECK(test_state_.reassembled.access_count_ == 0);

  const std::vector<uint8_t> second_part(data.cbegin() + packet_size / 2,
                                         data.cend());
  reassemble_and_dispatch(AllocateL2capPacket(second_part));

  CHECK(partial_packets.size() == 0);
  CHECK(test_state_.reassembled.access_count_ == 0);
}

TEST_F(HciPacketFragmenterTest, TwoPacket_HugeBogusSecond) {
  const size_t packet_size = kMaxPacketSize;
  const std::vector<uint8_t> data = CreateData(UINT16_MAX);
  const std::vector<uint8_t> first_part(data.cbegin(),
                                        data.cbegin() + packet_size - 1);
  reassemble_and_dispatch(AllocateL2capPacket(packet_size, first_part));

  CHECK(partial_packets.size() == 1);
  CHECK(test_state_.reassembled.access_count_ == 0);

  const std::vector<uint8_t> second_part(data.cbegin() + packet_size - 1,
                                         data.cend());
  reassemble_and_dispatch(AllocateL2capPacket(second_part));

  CHECK(partial_packets.size() == 0);
  CHECK(test_state_.reassembled.access_count_ == 1);
}
