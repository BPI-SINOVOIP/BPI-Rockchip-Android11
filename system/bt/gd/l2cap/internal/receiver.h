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
#include "l2cap/internal/scheduler.h"
#include "l2cap/l2cap_packets.h"
#include "l2cap/mtu.h"
#include "os/queue.h"
#include "packet/base_packet_builder.h"
#include "packet/packet_view.h"

namespace bluetooth {
namespace l2cap {
namespace internal {

class DataPipelineManager;

/**
 * Handle receiving L2CAP PDUs from link queue and distribute them into into channel data controllers.
 * Dequeue incoming packets from LinkQueueUpEnd, and enqueue it to ChannelQueueDownEnd. Note: If a channel
 * cannot dequeue from ChannelQueueDownEnd so that the buffer for incoming packet is full, further incoming packets will
 * be dropped.
 * The Reassembler keeps the reference to ChannelImpl objects, because it needs to check channel mode and parameters.
 * The Reassembler also keeps the reference to Scheduler, to get Segmenter and send signals (Tx, Rx seq) to it.
 */
class Receiver {
 public:
  using UpperEnqueue = packet::PacketView<packet::kLittleEndian>;
  using UpperDequeue = packet::BasePacketBuilder;
  using UpperQueueDownEnd = common::BidiQueueEnd<UpperEnqueue, UpperDequeue>;
  using LowerEnqueue = UpperDequeue;
  using LowerDequeue = UpperEnqueue;
  using LowerQueueUpEnd = common::BidiQueueEnd<LowerEnqueue, LowerDequeue>;

  Receiver(LowerQueueUpEnd* link_queue_up_end, os::Handler* handler, DataPipelineManager* data_pipeline_manager);
  ~Receiver();

 private:
  LowerQueueUpEnd* link_queue_up_end_;
  os::Handler* handler_;
  DataPipelineManager* data_pipeline_manager_;

  void link_queue_dequeue_callback();
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
