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
#include <stdint.h>

#include "common/message_loop_thread.h"
#include "hci/src/hci_layer.cc"
#include "hci_internals.h"
#include "osi/include/allocator.h"
#include "osi/include/osi.h"
#include "osi/test/AllocationTestHarness.h"
#include "osi/test/test_stubs.h"
#include "stack/include/bt_types.h"
#include "stack/include/hcidefs.h"

extern void allocation_tracker_uninit(void);

allocator_t buffer_allocator_ = {
    .alloc = osi_malloc,
    .free = osi_free,
};

void monitor_socket(int ctrl_fd, int fd) {
  LOG(INFO) << __func__ << " UNIMPLEMENTED";
}
void hci_initialize() { LOG(INFO) << __func__ << " UNIMPLEMENTED"; }
void hci_close() { LOG(INFO) << __func__ << " UNIMPLEMENTED"; }
void hci_transmit(BT_HDR* packet) { LOG(INFO) << __func__ << " UNIMPLEMENTED"; }
int hci_open_firmware_log_file() { return INVALID_FD; }
void hci_close_firmware_log_file(int fd) {}
void hci_log_firmware_debug_packet(int fd, BT_HDR* packet) {}
const allocator_t* buffer_allocator_get_interface() {
  return &buffer_allocator_;
}

/**
 * Test class to test selected functionality in hci/src/hci_layer.cc
 */
class HciLayerTest : public AllocationTestHarness {
 protected:
  void SetUp() override {
    AllocationTestHarness::SetUp();
    // Disable our allocation tracker to allow ASAN full range
    allocation_tracker_uninit();
    commands_pending_response = list_new(NULL);
    buffer_allocator = &buffer_allocator_;
  }

  void TearDown() override {
    list_free(commands_pending_response);
    AllocationTestHarness::TearDown();
  }

  BT_HDR* AllocateHciEventPacket(size_t packet_length) const {
    return AllocatePacket(packet_length, MSG_HC_TO_STACK_HCI_EVT);
  }

  uint8_t* GetPayloadPointer(BT_HDR* packet) const {
    return static_cast<uint8_t*>(packet->data);
  }

 private:
  BT_HDR* AllocatePacket(size_t packet_length, uint16_t event) const {
    BT_HDR* packet =
        static_cast<BT_HDR*>(osi_calloc(sizeof(BT_HDR) + packet_length));
    packet->offset = 0;
    packet->len = packet_length;
    packet->layer_specific = 0;
    packet->event = MSG_HC_TO_STACK_HCI_EVT;
    return packet;
  }
};

TEST_F(HciLayerTest, FilterIncomingEvent) {
  {
    BT_HDR* packet = AllocateHciEventPacket(3);

    auto p = GetPayloadPointer(packet);
    *p++ = HCI_COMMAND_STATUS_EVT;
    *p++ = 0x0;  // length

    CHECK(filter_incoming_event(packet));
  }

  {
    BT_HDR* packet = AllocateHciEventPacket(3);

    auto p = GetPayloadPointer(packet);
    *p++ = HCI_COMMAND_STATUS_EVT;
    *p++ = 0x1;  // length
    *p++ = 0xff;

    CHECK(filter_incoming_event(packet));
  }

  {
    BT_HDR* packet = AllocateHciEventPacket(6);

    auto p = GetPayloadPointer(packet);
    *p++ = HCI_COMMAND_STATUS_EVT;
    *p++ = 0x04;  // length
    *p++ = 0x00;  // status
    *p++ = 0x01;  // credits
    *p++ = 0x34;  // opcode0
    *p++ = 0x12;  // opcode1

    CHECK(filter_incoming_event(packet));
  }
}
