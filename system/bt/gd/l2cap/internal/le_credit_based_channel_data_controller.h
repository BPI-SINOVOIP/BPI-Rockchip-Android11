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

#include <memory>
#include <unordered_map>
#include <utility>

#include "common/bidi_queue.h"
#include "l2cap/cid.h"
#include "l2cap/internal/channel_impl.h"
#include "l2cap/internal/data_controller.h"
#include "l2cap/internal/ilink.h"
#include "l2cap/internal/scheduler.h"
#include "l2cap/l2cap_packets.h"
#include "l2cap/mtu.h"
#include "os/handler.h"
#include "os/queue.h"
#include "packet/base_packet_builder.h"
#include "packet/packet_view.h"

namespace bluetooth {
namespace l2cap {
namespace internal {

class LeCreditBasedDataController : public DataController {
 public:
  using UpperEnqueue = packet::PacketView<packet::kLittleEndian>;
  using UpperDequeue = packet::BasePacketBuilder;
  using UpperQueueDownEnd = common::BidiQueueEnd<UpperEnqueue, UpperDequeue>;
  LeCreditBasedDataController(ILink* link, Cid cid, Cid remote_cid, UpperQueueDownEnd* channel_queue_end,
                              os::Handler* handler, Scheduler* scheduler);

  void OnSdu(std::unique_ptr<packet::BasePacketBuilder> sdu) override;
  void OnPdu(packet::PacketView<true> pdu) override;
  std::unique_ptr<packet::BasePacketBuilder> GetNextPacket() override;

  void EnableFcs(bool enabled) override {}
  void SetRetransmissionAndFlowControlOptions(const RetransmissionAndFlowControlConfigurationOption& option) override {}

  // TODO: Set MTU and MPS from signalling channel
  void SetMtu(Mtu mtu);
  void SetMps(uint16_t mps);
  // TODO: Handle credits
  void OnCredit(uint16_t credits);

 private:
  Cid cid_;
  Cid remote_cid_;
  os::EnqueueBuffer<UpperEnqueue> enqueue_buffer_;
  os::Handler* handler_;
  std::queue<std::unique_ptr<packet::BasePacketBuilder>> pdu_queue_;
  Scheduler* scheduler_;
  ILink* link_;
  Mtu mtu_ = 512;
  uint16_t mps_ = 251;
  uint16_t credits_ = 0;
  uint16_t pending_frames_count_ = 0;

  class PacketViewForReassembly : public packet::PacketView<kLittleEndian> {
   public:
    PacketViewForReassembly(const PacketView& packetView) : PacketView(packetView) {}
    void AppendPacketView(packet::PacketView<kLittleEndian> to_append) {
      Append(to_append);
    }
  };
  PacketViewForReassembly reassembly_stage_{std::make_shared<std::vector<uint8_t>>()};
  uint16_t remaining_sdu_continuation_packet_size_ = 0;
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
