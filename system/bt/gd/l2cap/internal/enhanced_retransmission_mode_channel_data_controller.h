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
#include "l2cap/internal/scheduler.h"
#include "l2cap/l2cap_packets.h"
#include "l2cap/mtu.h"
#include "os/handler.h"
#include "os/queue.h"
#include "packet/base_packet_builder.h"
#include "packet/packet_view.h"
#include "packet/raw_builder.h"

namespace bluetooth {
namespace l2cap {
namespace internal {
class ILink;

class ErtmController : public DataController {
 public:
  using UpperEnqueue = packet::PacketView<packet::kLittleEndian>;
  using UpperDequeue = packet::BasePacketBuilder;
  using UpperQueueDownEnd = common::BidiQueueEnd<UpperEnqueue, UpperDequeue>;
  ErtmController(ILink* link, Cid cid, Cid remote_cid, UpperQueueDownEnd* channel_queue_end, os::Handler* handler,
                 Scheduler* scheduler);
  ~ErtmController();
  // Segmentation is handled here
  void OnSdu(std::unique_ptr<packet::BasePacketBuilder> sdu) override;
  void OnPdu(packet::PacketView<true> pdu) override;
  std::unique_ptr<packet::BasePacketBuilder> GetNextPacket() override;
  void EnableFcs(bool enabled) override;
  void SetRetransmissionAndFlowControlOptions(const RetransmissionAndFlowControlConfigurationOption& option) override;

 private:
  ILink* link_;
  Cid cid_;
  Cid remote_cid_;
  os::EnqueueBuffer<UpperEnqueue> enqueue_buffer_;
  os::Handler* handler_;
  std::queue<std::unique_ptr<packet::BasePacketBuilder>> pdu_queue_;
  Scheduler* scheduler_;

  // Configuration options
  bool fcs_enabled_ = false;
  uint16_t local_tx_window_ = 10;
  uint16_t local_max_transmit_ = 20;
  uint16_t local_retransmit_timeout_ms_ = 2000;
  uint16_t local_monitor_timeout_ms_ = 12000;

  uint16_t remote_tx_window_ = 10;
  uint16_t remote_mps_ = 1010;

  uint16_t size_each_packet_ =
      (remote_mps_ - 4 /* basic L2CAP header */ - 2 /* SDU length */ - 2 /* Extended control */ - 2 /* FCS */);

  class PacketViewForReassembly : public packet::PacketView<kLittleEndian> {
   public:
    PacketViewForReassembly(const PacketView& packetView) : PacketView(packetView) {}
    void AppendPacketView(packet::PacketView<kLittleEndian> to_append) {
      Append(to_append);
    }
  };

  class CopyablePacketBuilder : public packet::BasePacketBuilder {
   public:
    CopyablePacketBuilder(std::shared_ptr<packet::RawBuilder> builder) : builder_(std::move(builder)) {}

    void Serialize(BitInserter& it) const override;

    size_t size() const override;

   private:
    std::shared_ptr<packet::RawBuilder> builder_;
  };

  PacketViewForReassembly reassembly_stage_{std::make_shared<std::vector<uint8_t>>()};
  SegmentationAndReassembly sar_state_ = SegmentationAndReassembly::END;
  uint16_t remaining_sdu_continuation_packet_size_ = 0;

  void stage_for_reassembly(SegmentationAndReassembly sar, uint16_t sdu_size,
                            const packet::PacketView<kLittleEndian>& payload);
  void send_pdu(std::unique_ptr<packet::BasePacketBuilder> pdu);

  void close_channel();

  void on_pdu_no_fcs(const packet::PacketView<true>& pdu);
  void on_pdu_fcs(const packet::PacketView<true>& pdu);

  struct impl;
  std::unique_ptr<impl> pimpl_;
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
