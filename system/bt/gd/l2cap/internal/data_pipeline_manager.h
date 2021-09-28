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
#include "l2cap/internal/receiver.h"
#include "l2cap/internal/scheduler.h"
#include "l2cap/internal/scheduler_fifo.h"
#include "l2cap/l2cap_packets.h"
#include "l2cap/mtu.h"
#include "os/handler.h"
#include "os/log.h"
#include "os/queue.h"
#include "packet/base_packet_builder.h"
#include "packet/packet_view.h"

namespace bluetooth {
namespace l2cap {
namespace internal {
class ILink;

/**
 * Manages data pipeline from channel queue end to link queue end, per link.
 * Contains a Scheduler and Receiver per link.
 * Contains a Sender and its corresponding DataController per attached channel.
 */
class DataPipelineManager {
 public:
  using UpperEnqueue = packet::PacketView<packet::kLittleEndian>;
  using UpperDequeue = packet::BasePacketBuilder;
  using UpperQueueDownEnd = common::BidiQueueEnd<UpperEnqueue, UpperDequeue>;
  using LowerEnqueue = UpperDequeue;
  using LowerDequeue = UpperEnqueue;
  using LowerQueueUpEnd = common::BidiQueueEnd<LowerEnqueue, LowerDequeue>;

  DataPipelineManager(os::Handler* handler, ILink* link, LowerQueueUpEnd* link_queue_up_end)
      : handler_(handler), link_(link), scheduler_(std::make_unique<Fifo>(this, link_queue_up_end, handler)),
        receiver_(link_queue_up_end, handler, this) {}

  using ChannelMode = Sender::ChannelMode;

  virtual void AttachChannel(Cid cid, std::shared_ptr<ChannelImpl> channel, ChannelMode mode);
  virtual void DetachChannel(Cid cid);
  virtual DataController* GetDataController(Cid cid);
  virtual void OnPacketSent(Cid cid);
  virtual void UpdateClassicConfiguration(Cid cid, classic::internal::ChannelConfigurationState config);
  virtual ~DataPipelineManager() = default;

 private:
  os::Handler* handler_;
  ILink* link_;
  std::unique_ptr<Scheduler> scheduler_;
  Receiver receiver_;
  std::unordered_map<Cid, Sender> sender_map_;
};
}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
