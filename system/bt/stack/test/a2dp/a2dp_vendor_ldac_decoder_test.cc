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
#include <stdio.h>
#include <cstdint>

#include "stack/include/ldacBT_bco_for_fluoride.h"

#include "osi/test/AllocationTestHarness.h"
#undef LOG_TAG
#include "stack/a2dp/a2dp_vendor_ldac_decoder.cc"

extern void allocation_tracker_uninit(void);
namespace {

uint8_t* Data(BT_HDR* packet) { return packet->data + packet->offset; }

}  // namespace

/**
 * Test class to test selected functionality in stack/a2dp
 */
class A2dpStackTest : public AllocationTestHarness {
 protected:
  void SetUp() override {
    AllocationTestHarness::SetUp();
    // Disable our allocation tracker to allow ASAN full range
    allocation_tracker_uninit();
  }

  void TearDown() override { AllocationTestHarness::TearDown(); }

  BT_HDR* AllocateL2capPacket(const std::vector<uint8_t> data) const {
    auto packet = AllocatePacket(data.size());
    std::copy(data.cbegin(), data.cend(), Data(packet));
    return packet;
  }

 private:
  BT_HDR* AllocatePacket(size_t packet_length) const {
    BT_HDR* packet =
        static_cast<BT_HDR*>(osi_calloc(sizeof(BT_HDR) + packet_length));
    packet->len = packet_length;
    return packet;
  }
};

TEST_F(A2dpStackTest, DecodePacket_ZeroLength) {
  const std::vector<uint8_t> data;
  BT_HDR* p_buf = AllocateL2capPacket(data);
  CHECK(!a2dp_vendor_ldac_decoder_decode_packet(p_buf));
  osi_free(p_buf);
}
