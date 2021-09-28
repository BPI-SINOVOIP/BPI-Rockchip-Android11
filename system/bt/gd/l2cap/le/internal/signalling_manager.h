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
#include <queue>
#include <vector>

#include "l2cap/cid.h"
#include "l2cap/internal/data_pipeline_manager.h"
#include "l2cap/internal/dynamic_channel_allocator.h"
#include "l2cap/l2cap_packets.h"
#include "l2cap/le/internal/dynamic_channel_service_manager_impl.h"
#include "l2cap/le/internal/fixed_channel_impl.h"
#include "l2cap/le/internal/fixed_channel_service_manager_impl.h"
#include "l2cap/mtu.h"
#include "l2cap/psm.h"
#include "l2cap/signal_id.h"
#include "os/alarm.h"
#include "os/handler.h"
#include "os/queue.h"
#include "packet/raw_builder.h"

namespace bluetooth {
namespace l2cap {
namespace le {
namespace internal {

struct PendingCommand {
  SignalId signal_id_ = kInvalidSignalId;
  LeCommandCode command_code_;
  Psm psm_;
  Cid source_cid_;
  Cid destination_cid_;
  Mtu mtu_;
  uint16_t mps_;
  uint16_t credits_;
};

class Link;

class LeSignallingManager {
 public:
  LeSignallingManager(os::Handler* handler, Link* link, l2cap::internal::DataPipelineManager* data_pipeline_manager,
                      DynamicChannelServiceManagerImpl* dynamic_service_manager,
                      l2cap::internal::DynamicChannelAllocator* channel_allocator);

  virtual ~LeSignallingManager();

  void SendConnectionRequest(Psm psm, Cid local_cid, Mtu mtu);

  void SendDisconnectRequest(Cid local_cid, Cid remote_cid);

  void SendConnectionParameterUpdateRequest(uint16_t interval_min, uint16_t interval_max, uint16_t slave_latency,
                                            uint16_t timeout_multiplier);

  void SendConnectionParameterUpdateResponse(SignalId signal_id, ConnectionParameterUpdateResponseResult result);

  void SendCredit(Cid local_cid, uint16_t credits);

  void CancelAlarm();

  void OnCommandReject(LeCommandRejectView command_reject_view);

  void OnConnectionParameterUpdateRequest(uint16_t interval_min, uint16_t interval_max, uint16_t slave_latency,
                                          uint16_t timeout_multiplier);
  void OnConnectionParameterUpdateResponse(ConnectionParameterUpdateResponseResult result);

  void OnConnectionRequest(SignalId signal_id, Psm psm, Cid remote_cid, Mtu mtu, uint16_t mps,
                           uint16_t initial_credits);

  void OnConnectionResponse(SignalId signal_id, Cid remote_cid, Mtu mtu, uint16_t mps, uint16_t initial_credits,
                            LeCreditBasedConnectionResponseResult result);

  void OnDisconnectionRequest(SignalId signal_id, Cid cid, Cid remote_cid);

  void OnDisconnectionResponse(SignalId signal_id, Cid cid, Cid remote_cid);

  void OnCredit(Cid remote_cid, uint16_t credits);

 private:
  void on_incoming_packet();
  void send_connection_response(SignalId signal_id, Cid local_cid, Mtu mtu, uint16_t mps, uint16_t initial_credit,
                                LeCreditBasedConnectionResponseResult result);
  void on_command_timeout();
  void handle_send_next_command();

  os::Handler* handler_;
  Link* link_;
  l2cap::internal::DataPipelineManager* data_pipeline_manager_;
  std::shared_ptr<le::internal::FixedChannelImpl> signalling_channel_;
  DynamicChannelServiceManagerImpl* dynamic_service_manager_;
  l2cap::internal::DynamicChannelAllocator* channel_allocator_;
  std::unique_ptr<os::EnqueueBuffer<packet::BasePacketBuilder>> enqueue_buffer_;
  std::queue<PendingCommand> pending_commands_;
  PendingCommand command_just_sent_;
  os::Alarm alarm_;
  SignalId next_signal_id_ = kInitialSignalId;
};

}  // namespace internal
}  // namespace le
}  // namespace l2cap
}  // namespace bluetooth
