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
#include "l2cap/classic/internal/channel_configuration_state.h"
#include "l2cap/classic/internal/dynamic_channel_service_manager_impl.h"
#include "l2cap/classic/internal/fixed_channel_impl.h"
#include "l2cap/classic/internal/fixed_channel_service_manager_impl.h"
#include "l2cap/internal/data_pipeline_manager.h"
#include "l2cap/internal/dynamic_channel_allocator.h"
#include "l2cap/l2cap_packets.h"
#include "l2cap/psm.h"
#include "l2cap/signal_id.h"
#include "os/alarm.h"
#include "os/handler.h"
#include "os/queue.h"
#include "packet/raw_builder.h"

namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {

struct PendingCommand {
  SignalId signal_id_ = kInvalidSignalId;
  CommandCode command_code_;
  Psm psm_;
  Cid source_cid_;
  Cid destination_cid_;
  InformationRequestInfoType info_type_;
  std::vector<std::unique_ptr<ConfigurationOption>> config_;
};

class Link;

class ClassicSignallingManager {
 public:
  ClassicSignallingManager(os::Handler* handler, Link* link,
                           l2cap::internal::DataPipelineManager* data_pipeline_manager,
                           classic::internal::DynamicChannelServiceManagerImpl* dynamic_service_manager,
                           l2cap::internal::DynamicChannelAllocator* channel_allocator,
                           classic::internal::FixedChannelServiceManagerImpl* fixed_service_manager);

  virtual ~ClassicSignallingManager();

  void OnCommandReject(CommandRejectView command_reject_view);

  void SendConnectionRequest(Psm psm, Cid local_cid);

  void SendConfigurationRequest(Cid remote_cid, std::vector<std::unique_ptr<ConfigurationOption>> config);

  void SendDisconnectionRequest(Cid local_cid, Cid remote_cid);

  void SendInformationRequest(InformationRequestInfoType type);

  void SendEchoRequest(std::unique_ptr<packet::RawBuilder> payload);

  void CancelAlarm();

  void OnConnectionRequest(SignalId signal_id, Psm psm, Cid remote_cid);

  void OnConnectionResponse(SignalId signal_id, Cid remote_cid, Cid cid, ConnectionResponseResult result,
                            ConnectionResponseStatus status);

  void OnDisconnectionRequest(SignalId signal_id, Cid cid, Cid remote_cid);

  void OnDisconnectionResponse(SignalId signal_id, Cid cid, Cid remote_cid);

  void OnConfigurationRequest(SignalId signal_id, Cid cid, Continuation is_continuation,
                              std::vector<std::unique_ptr<ConfigurationOption>>);

  void OnConfigurationResponse(SignalId signal_id, Cid cid, Continuation is_continuation,
                               ConfigurationResponseResult result, std::vector<std::unique_ptr<ConfigurationOption>>);

  void OnEchoRequest(SignalId signal_id, const PacketView<kLittleEndian>& packet);

  void OnEchoResponse(SignalId signal_id, const PacketView<kLittleEndian>& packet);

  void OnInformationRequest(SignalId signal_id, InformationRequestInfoType type);

  void OnInformationResponse(SignalId signal_id, const InformationResponseView& response);

 private:
  void on_incoming_packet();
  void send_connection_response(SignalId signal_id, Cid remote_cid, Cid local_cid, ConnectionResponseResult result,
                                ConnectionResponseStatus status);
  void on_command_timeout();
  void handle_send_next_command();

  os::Handler* handler_;
  Link* link_;
  l2cap::internal::DataPipelineManager* data_pipeline_manager_;
  std::shared_ptr<classic::internal::FixedChannelImpl> signalling_channel_;
  DynamicChannelServiceManagerImpl* dynamic_service_manager_;
  l2cap::internal::DynamicChannelAllocator* channel_allocator_;
  FixedChannelServiceManagerImpl* fixed_service_manager_;
  std::unique_ptr<os::EnqueueBuffer<packet::BasePacketBuilder>> enqueue_buffer_;
  std::queue<PendingCommand> pending_commands_;
  PendingCommand command_just_sent_;
  os::Alarm alarm_;
  SignalId next_signal_id_ = kInitialSignalId;
  std::unordered_map<Cid, ChannelConfigurationState> channel_configuration_;
};

}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
