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

#include "l2cap/l2cap_packets.h"
#include "packet/base_packet_builder.h"
#include "packet/packet_view.h"

namespace bluetooth {
namespace l2cap {
namespace internal {

class DataController {
 public:
  virtual ~DataController() = default;

  // SDU -> PDUs and notify Scheduler
  virtual void OnSdu(std::unique_ptr<packet::BasePacketBuilder> sdu) = 0;

  // PDUs -> SDU and enqueue to channel queue end
  virtual void OnPdu(packet::PacketView<true> pdu) = 0;

  // Used by Scheduler to get next PDU
  virtual std::unique_ptr<packet::BasePacketBuilder> GetNextPacket() = 0;

  // Set FCS mode. This only applies to some modes (ERTM).
  virtual void EnableFcs(bool enabled) = 0;

  // Set retransmission and flow control. Ignore the mode option because each DataController only handles one mode.
  // This only applies to some modes (ERTM).
  virtual void SetRetransmissionAndFlowControlOptions(
      const RetransmissionAndFlowControlConfigurationOption& option) = 0;
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
