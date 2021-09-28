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

#include <cstdint>

#include "common/bidi_queue.h"
#include "l2cap/cid.h"
#include "l2cap/classic/dynamic_channel_configuration_option.h"
#include "l2cap/internal/channel_impl.h"
#include "l2cap/internal/data_controller.h"
#include "l2cap/internal/sender.h"
#include "l2cap/l2cap_packets.h"
#include "packet/base_packet_builder.h"
#include "packet/packet_view.h"

namespace bluetooth {
namespace l2cap {
namespace internal {

/**
 * Handle the scheduling of packets through the l2cap stack.
 * For each attached channel, dequeue its outgoing packets and enqueue it to the given LinkQueueUpEnd, according to some
 * policy (cid).
 *
 * Note: If a channel cannot dequeue from ChannelQueueDownEnd so that the buffer for incoming packet is full, further
 * incoming packets will be dropped.
 */
class Scheduler {
 public:
  using UpperEnqueue = packet::PacketView<packet::kLittleEndian>;
  using UpperDequeue = packet::BasePacketBuilder;
  using UpperQueueDownEnd = common::BidiQueueEnd<UpperEnqueue, UpperDequeue>;
  using LowerEnqueue = UpperDequeue;
  using LowerDequeue = UpperEnqueue;
  using LowerQueueUpEnd = common::BidiQueueEnd<LowerEnqueue, LowerDequeue>;

  /**
   * Callback from the sender to indicate that the scheduler could dequeue number_packets from it
   */
  virtual void OnPacketsReady(Cid cid, int number_packets) {}

  virtual ~Scheduler() = default;
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
