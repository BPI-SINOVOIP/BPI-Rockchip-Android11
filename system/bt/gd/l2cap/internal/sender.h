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

#include <string>
#include <unordered_map>

#include "common/bidi_queue.h"
#include "common/bind.h"
#include "data_controller.h"
#include "l2cap/cid.h"
#include "l2cap/classic/internal/channel_configuration_state.h"
#include "l2cap/internal/channel_impl.h"
#include "l2cap/internal/data_controller.h"
#include "l2cap/l2cap_packets.h"
#include "l2cap/mtu.h"
#include "os/handler.h"
#include "os/queue.h"
#include "packet/base_packet_builder.h"
#include "packet/packet_view.h"

namespace bluetooth {
namespace l2cap {
namespace internal {
class Scheduler;
class ILink;

/**
 * A middle layer between L2CAP channel and outgoing packet scheduler.
 * Fetches data (SDU) from an L2CAP channel queue end, handles L2CAP segmentation, and gives data to L2CAP scheduler.
 */
class Sender {
 public:
  using UpperEnqueue = packet::PacketView<packet::kLittleEndian>;
  using UpperDequeue = packet::BasePacketBuilder;
  using UpperQueueDownEnd = common::BidiQueueEnd<UpperEnqueue, UpperDequeue>;

  enum class ChannelMode {
    BASIC = 0,
    ERTM = 3,
    LE_CREDIT_BASED = 10,
  };

  Sender(os::Handler* handler, ILink* link, Scheduler* scheduler, std::shared_ptr<ChannelImpl> channel);
  Sender(os::Handler* handler, ILink* link, Scheduler* scheduler, std::shared_ptr<ChannelImpl> channel,
         ChannelMode mode);
  ~Sender();

  /**
   * Callback from scheduler to indicate that scheduler already dequeued a packet from sender's queue.
   * Segmenter can continue dequeuing from channel queue end.
   */
  void OnPacketSent();

  /**
   * Called by the scheduler to return the next PDU to be sent
   */
  std::unique_ptr<UpperDequeue> GetNextPacket();

  void UpdateClassicConfiguration(classic::internal::ChannelConfigurationState config);
  DataController* GetDataController();

 private:
  os::Handler* handler_;
  ILink* link_;
  UpperQueueDownEnd* queue_end_;
  Scheduler* scheduler_;
  const Cid channel_id_;
  const Cid remote_channel_id_;
  bool is_dequeue_registered_ = false;
  RetransmissionAndFlowControlModeOption mode_ = RetransmissionAndFlowControlModeOption::L2CAP_BASIC;
  std::unique_ptr<DataController> data_controller_;

  void try_register_dequeue();
  void dequeue_callback();
};
}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
