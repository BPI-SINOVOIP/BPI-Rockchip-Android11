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

#include "l2cap/le/internal/signalling_manager.h"

#include <chrono>

#include "common/bind.h"
#include "l2cap/internal/data_pipeline_manager.h"
#include "l2cap/internal/dynamic_channel_impl.h"
#include "l2cap/internal/le_credit_based_channel_data_controller.h"
#include "l2cap/l2cap_packets.h"
#include "l2cap/le/internal/link.h"
#include "os/log.h"
#include "packet/raw_builder.h"

namespace bluetooth {
namespace l2cap {
namespace le {
namespace internal {

static constexpr auto kTimeout = std::chrono::seconds(3);

LeSignallingManager::LeSignallingManager(os::Handler* handler, Link* link,
                                         l2cap::internal::DataPipelineManager* data_pipeline_manager,
                                         DynamicChannelServiceManagerImpl* dynamic_service_manager,
                                         l2cap::internal::DynamicChannelAllocator* channel_allocator)
    : handler_(handler), link_(link), data_pipeline_manager_(data_pipeline_manager),
      dynamic_service_manager_(dynamic_service_manager), channel_allocator_(channel_allocator), alarm_(handler) {
  ASSERT(handler_ != nullptr);
  ASSERT(link_ != nullptr);
  signalling_channel_ = link_->AllocateFixedChannel(kLeSignallingCid, {});
  signalling_channel_->GetQueueUpEnd()->RegisterDequeue(
      handler_, common::Bind(&LeSignallingManager::on_incoming_packet, common::Unretained(this)));
  enqueue_buffer_ =
      std::make_unique<os::EnqueueBuffer<packet::BasePacketBuilder>>(signalling_channel_->GetQueueUpEnd());
}

LeSignallingManager::~LeSignallingManager() {
  enqueue_buffer_.reset();
  signalling_channel_->GetQueueUpEnd()->UnregisterDequeue();
  signalling_channel_ = nullptr;
}

void LeSignallingManager::SendConnectionRequest(Psm psm, Cid local_cid, Mtu mtu) {
  PendingCommand pending_command = {
      next_signal_id_, LeCommandCode::LE_CREDIT_BASED_CONNECTION_REQUEST, psm, local_cid, {}, mtu, link_->GetMps(),
      link_->GetInitialCredit()};
  next_signal_id_++;
  pending_commands_.push(pending_command);
  if (pending_commands_.size() == 1) {
    handle_send_next_command();
  }
}

void LeSignallingManager::SendDisconnectRequest(Cid local_cid, Cid remote_cid) {
  PendingCommand pending_command = {
      next_signal_id_, LeCommandCode::DISCONNECTION_REQUEST, {}, local_cid, remote_cid, {}, {}, {}};
  next_signal_id_++;
  pending_commands_.push(pending_command);
  if (pending_commands_.size() == 1) {
    handle_send_next_command();
  }
}

void LeSignallingManager::SendConnectionParameterUpdateRequest(uint16_t interval_min, uint16_t interval_max,
                                                               uint16_t slave_latency, uint16_t timeout_multiplier) {
  LOG_ERROR("Not implemented");
}

void LeSignallingManager::SendConnectionParameterUpdateResponse(SignalId signal_id,
                                                                ConnectionParameterUpdateResponseResult result) {
  auto builder = ConnectionParameterUpdateResponseBuilder::Create(signal_id.Value(), result);
  enqueue_buffer_->Enqueue(std::move(builder), handler_);
}

void LeSignallingManager::SendCredit(Cid local_cid, uint16_t credits) {
  auto builder = LeFlowControlCreditBuilder::Create(next_signal_id_.Value(), local_cid, credits);
  next_signal_id_++;
  enqueue_buffer_->Enqueue(std::move(builder), handler_);
}

void LeSignallingManager::CancelAlarm() {
  alarm_.Cancel();
}

void LeSignallingManager::OnCommandReject(LeCommandRejectView command_reject_view) {
  auto signal_id = command_reject_view.GetIdentifier();
  if (signal_id != command_just_sent_.signal_id_ || command_just_sent_.command_code_ != command_reject_view.GetCode()) {
    LOG_WARN("Unexpected response: no pending request");
    return;
  }
  alarm_.Cancel();
  handle_send_next_command();

  LOG_WARN("Command rejected");
}

void LeSignallingManager::OnConnectionParameterUpdateRequest(uint16_t interval_min, uint16_t interval_max,
                                                             uint16_t slave_latency, uint16_t timeout_multiplier) {
  LOG_ERROR("Not implemented");
}

void LeSignallingManager::OnConnectionParameterUpdateResponse(ConnectionParameterUpdateResponseResult result) {
  LOG_ERROR("Not implemented");
}

void LeSignallingManager::OnConnectionRequest(SignalId signal_id, Psm psm, Cid remote_cid, Mtu mtu, uint16_t mps,
                                              uint16_t initial_credits) {
  if (!IsPsmValid(psm)) {
    LOG_WARN("Invalid psm received from remote psm:%d remote_cid:%d", psm, remote_cid);
    send_connection_response(signal_id, kInvalidCid, 0, 0, 0,
                             LeCreditBasedConnectionResponseResult::LE_PSM_NOT_SUPPORTED);
    return;
  }

  if (remote_cid == kInvalidCid) {
    LOG_WARN("Invalid remote cid received from remote psm:%d remote_cid:%d", psm, remote_cid);
    send_connection_response(signal_id, kInvalidCid, 0, 0, 0,
                             LeCreditBasedConnectionResponseResult::INVALID_SOURCE_CID);
    return;
  }

  if (channel_allocator_->IsPsmUsed(psm)) {
    LOG_WARN("Psm already exists");
    send_connection_response(signal_id, kInvalidCid, 0, 0, 0,
                             LeCreditBasedConnectionResponseResult::LE_PSM_NOT_SUPPORTED);
    return;
  }

  if (!dynamic_service_manager_->IsServiceRegistered(psm)) {
    LOG_INFO("Service for this psm (%d) is not registered", psm);
    send_connection_response(signal_id, kInvalidCid, 0, 0, 0,
                             LeCreditBasedConnectionResponseResult::LE_PSM_NOT_SUPPORTED);
    return;
  }

  auto* service = dynamic_service_manager_->GetService(psm);
  auto config = service->GetConfigOption();
  auto local_mtu = config.mtu;
  auto local_mps = link_->GetMps();

  auto new_channel = link_->AllocateDynamicChannel(psm, remote_cid, {});
  if (new_channel == nullptr) {
    LOG_WARN("Can't allocate dynamic channel");
    send_connection_response(signal_id, kInvalidCid, 0, 0, 0,
                             LeCreditBasedConnectionResponseResult::NO_RESOURCES_AVAILABLE);

    return;
  }
  send_connection_response(signal_id, remote_cid, local_mtu, local_mps, link_->GetInitialCredit(),
                           LeCreditBasedConnectionResponseResult::SUCCESS);
  auto* data_controller = reinterpret_cast<l2cap::internal::LeCreditBasedDataController*>(
      data_pipeline_manager_->GetDataController(new_channel->GetCid()));
  data_controller->SetMtu(std::min(mtu, local_mtu));
  data_controller->SetMps(std::min(mps, local_mps));
  data_controller->OnCredit(initial_credits);
  auto user_channel = std::make_unique<DynamicChannel>(new_channel, handler_);
  dynamic_service_manager_->GetService(psm)->NotifyChannelCreation(std::move(user_channel));
}

void LeSignallingManager::OnConnectionResponse(SignalId signal_id, Cid remote_cid, Mtu mtu, uint16_t mps,
                                               uint16_t initial_credits, LeCreditBasedConnectionResponseResult result) {
  if (signal_id != command_just_sent_.signal_id_) {
    LOG_WARN("Unexpected response: no pending request");
    return;
  }
  if (command_just_sent_.command_code_ != LeCommandCode::LE_CREDIT_BASED_CONNECTION_REQUEST) {
    LOG_WARN("Unexpected response: no pending request");
    return;
  }
  alarm_.Cancel();
  command_just_sent_.signal_id_ = kInitialSignalId;
  if (result != LeCreditBasedConnectionResponseResult::SUCCESS) {
    LOG_WARN("Connection failed: %s", LeCreditBasedConnectionResponseResultText(result).data());
    link_->OnOutgoingConnectionRequestFail(command_just_sent_.source_cid_);
    handle_send_next_command();
    return;
  }
  auto new_channel =
      link_->AllocateReservedDynamicChannel(command_just_sent_.source_cid_, command_just_sent_.psm_, remote_cid, {});
  if (new_channel == nullptr) {
    LOG_WARN("Can't allocate dynamic channel");
    link_->OnOutgoingConnectionRequestFail(command_just_sent_.source_cid_);
    handle_send_next_command();
    return;
  }
  auto* data_controller = reinterpret_cast<l2cap::internal::LeCreditBasedDataController*>(
      data_pipeline_manager_->GetDataController(new_channel->GetCid()));
  data_controller->SetMtu(std::min(mtu, command_just_sent_.mtu_));
  data_controller->SetMps(std::min(mps, command_just_sent_.mps_));
  data_controller->OnCredit(initial_credits);
  std::unique_ptr<DynamicChannel> user_channel = std::make_unique<DynamicChannel>(new_channel, handler_);
  dynamic_service_manager_->GetService(command_just_sent_.psm_)->NotifyChannelCreation(std::move(user_channel));
}

void LeSignallingManager::OnDisconnectionRequest(SignalId signal_id, Cid cid, Cid remote_cid) {
  auto channel = channel_allocator_->FindChannelByCid(cid);
  if (channel == nullptr) {
    LOG_WARN("Disconnect request for an unknown channel");
    return;
  }
  if (channel->GetRemoteCid() != remote_cid) {
    LOG_WARN("Disconnect request for an unmatching channel");
    return;
  }
  auto builder = LeDisconnectionResponseBuilder::Create(signal_id.Value(), cid, remote_cid);
  enqueue_buffer_->Enqueue(std::move(builder), handler_);
  channel->OnClosed(hci::ErrorCode::SUCCESS);
  link_->FreeDynamicChannel(cid);
}

void LeSignallingManager::OnDisconnectionResponse(SignalId signal_id, Cid cid, Cid remote_cid) {
  if (signal_id != command_just_sent_.signal_id_ ||
      command_just_sent_.command_code_ != LeCommandCode::DISCONNECTION_REQUEST) {
    LOG_WARN("Unexpected response: no pending request");
    return;
  }
  if (command_just_sent_.source_cid_ != cid || command_just_sent_.destination_cid_ != remote_cid) {
    LOG_WARN("Unexpected response: cid doesn't match. Expected scid %d dcid %d, got scid %d dcid %d",
             command_just_sent_.source_cid_, command_just_sent_.destination_cid_, cid, remote_cid);
    handle_send_next_command();
    return;
  }
  alarm_.Cancel();
  command_just_sent_.signal_id_ = kInitialSignalId;
  auto channel = channel_allocator_->FindChannelByCid(cid);
  if (channel == nullptr) {
    LOG_WARN("Disconnect response for an unknown channel");
    handle_send_next_command();
    return;
  }

  channel->OnClosed(hci::ErrorCode::SUCCESS);
  link_->FreeDynamicChannel(cid);
  handle_send_next_command();
}

void LeSignallingManager::OnCredit(Cid remote_cid, uint16_t credits) {
  auto channel = channel_allocator_->FindChannelByRemoteCid(remote_cid);
  if (channel == nullptr) {
    LOG_WARN("Received credit for invalid cid %d", channel->GetCid());
    return;
  }
  auto* data_controller = reinterpret_cast<l2cap::internal::LeCreditBasedDataController*>(
      data_pipeline_manager_->GetDataController(channel->GetCid()));
  data_controller->OnCredit(credits);
}

void LeSignallingManager::on_incoming_packet() {
  auto packet = signalling_channel_->GetQueueUpEnd()->TryDequeue();
  LeControlView control_packet_view = LeControlView::Create(*packet);
  if (!control_packet_view.IsValid()) {
    LOG_WARN("Invalid signalling packet received");
    return;
  }
  auto code = control_packet_view.GetCode();
  switch (code) {
    case LeCommandCode::COMMAND_REJECT: {
      LeCommandRejectView command_reject_view = LeCommandRejectView::Create(control_packet_view);
      if (!command_reject_view.IsValid()) {
        return;
      }
      OnCommandReject(command_reject_view);
      return;
    }

    case LeCommandCode::CONNECTION_PARAMETER_UPDATE_REQUEST: {
      ConnectionParameterUpdateRequestView parameter_update_req_view =
          ConnectionParameterUpdateRequestView::Create(control_packet_view);
      if (!parameter_update_req_view.IsValid()) {
        return;
      }
      OnConnectionParameterUpdateRequest(
          parameter_update_req_view.GetIntervalMin(), parameter_update_req_view.GetIntervalMax(),
          parameter_update_req_view.GetSlaveLatency(), parameter_update_req_view.GetTimeoutMultiplier());
      return;
    }
    case LeCommandCode::CONNECTION_PARAMETER_UPDATE_RESPONSE: {
      ConnectionParameterUpdateResponseView parameter_update_rsp_view =
          ConnectionParameterUpdateResponseView::Create(control_packet_view);
      if (!parameter_update_rsp_view.IsValid()) {
        return;
      }
      OnConnectionParameterUpdateResponse(parameter_update_rsp_view.GetResult());
      return;
    }
    case LeCommandCode::LE_CREDIT_BASED_CONNECTION_REQUEST: {
      LeCreditBasedConnectionRequestView connection_request_view =
          LeCreditBasedConnectionRequestView::Create(control_packet_view);
      if (!connection_request_view.IsValid()) {
        return;
      }
      OnConnectionRequest(connection_request_view.GetIdentifier(), connection_request_view.GetLePsm(),
                          connection_request_view.GetSourceCid(), connection_request_view.GetMtu(),
                          connection_request_view.GetMps(), connection_request_view.GetInitialCredits());
      return;
    }
    case LeCommandCode::LE_CREDIT_BASED_CONNECTION_RESPONSE: {
      LeCreditBasedConnectionResponseView connection_response_view =
          LeCreditBasedConnectionResponseView::Create(control_packet_view);
      if (!connection_response_view.IsValid()) {
        return;
      }
      OnConnectionResponse(connection_response_view.GetIdentifier(), connection_response_view.GetDestinationCid(),
                           connection_response_view.GetMtu(), connection_response_view.GetMps(),
                           connection_response_view.GetInitialCredits(), connection_response_view.GetResult());
      return;
    }
    case LeCommandCode::LE_FLOW_CONTROL_CREDIT: {
      LeFlowControlCreditView credit_view = LeFlowControlCreditView::Create(control_packet_view);
      if (!credit_view.IsValid()) {
        return;
      }
      OnCredit(credit_view.GetCid(), credit_view.GetCredits());
      return;
    }
    case LeCommandCode::DISCONNECTION_REQUEST: {
      LeDisconnectionRequestView disconnection_request_view = LeDisconnectionRequestView::Create(control_packet_view);
      if (!disconnection_request_view.IsValid()) {
        return;
      }
      OnDisconnectionRequest(disconnection_request_view.GetIdentifier(), disconnection_request_view.GetDestinationCid(),
                             disconnection_request_view.GetSourceCid());
      return;
    }
    case LeCommandCode::DISCONNECTION_RESPONSE: {
      LeDisconnectionResponseView disconnection_response_view =
          LeDisconnectionResponseView::Create(control_packet_view);
      if (!disconnection_response_view.IsValid()) {
        return;
      }
      OnDisconnectionResponse(disconnection_response_view.GetIdentifier(),
                              disconnection_response_view.GetDestinationCid(),
                              disconnection_response_view.GetSourceCid());
      return;
    }
    default:
      LOG_WARN("Unhandled event 0x%x", static_cast<int>(code));
      auto builder = LeCommandRejectNotUnderstoodBuilder::Create(control_packet_view.GetIdentifier());
      enqueue_buffer_->Enqueue(std::move(builder), handler_);
      return;
  }
}

void LeSignallingManager::send_connection_response(SignalId signal_id, Cid local_cid, Mtu mtu, uint16_t mps,
                                                   uint16_t initial_credit,
                                                   LeCreditBasedConnectionResponseResult result) {
  auto builder =
      LeCreditBasedConnectionResponseBuilder::Create(signal_id.Value(), local_cid, mtu, mps, initial_credit, result);
  enqueue_buffer_->Enqueue(std::move(builder), handler_);
}

void LeSignallingManager::on_command_timeout() {
  LOG_WARN("Response time out");
  if (command_just_sent_.signal_id_ == kInvalidSignalId) {
    LOG_ERROR("No pending command");
    return;
  }
  switch (command_just_sent_.command_code_) {
    case LeCommandCode::CONNECTION_PARAMETER_UPDATE_REQUEST: {
      link_->OnOutgoingConnectionRequestFail(command_just_sent_.source_cid_);
      break;
    }
    default:
      break;
  }
  handle_send_next_command();
}

void LeSignallingManager::handle_send_next_command() {
  command_just_sent_.signal_id_ = kInvalidSignalId;
  if (pending_commands_.empty()) {
    return;
  }

  command_just_sent_ = pending_commands_.front();
  pending_commands_.pop();
  switch (command_just_sent_.command_code_) {
    case LeCommandCode::LE_CREDIT_BASED_CONNECTION_REQUEST: {
      auto builder = LeCreditBasedConnectionRequestBuilder::Create(
          command_just_sent_.signal_id_.Value(), command_just_sent_.psm_, command_just_sent_.source_cid_,
          command_just_sent_.mtu_, command_just_sent_.mps_, command_just_sent_.credits_);
      enqueue_buffer_->Enqueue(std::move(builder), handler_);
      alarm_.Schedule(common::BindOnce(&LeSignallingManager::on_command_timeout, common::Unretained(this)), kTimeout);
      break;
    }
    case LeCommandCode::DISCONNECTION_REQUEST: {
      auto builder = LeDisconnectionRequestBuilder::Create(
          command_just_sent_.signal_id_.Value(), command_just_sent_.destination_cid_, command_just_sent_.source_cid_);
      enqueue_buffer_->Enqueue(std::move(builder), handler_);
      alarm_.Schedule(common::BindOnce(&LeSignallingManager::on_command_timeout, common::Unretained(this)), kTimeout);
      break;
    }
    default: {
      LOG_WARN("Unsupported command code 0x%x", static_cast<int>(command_just_sent_.command_code_));
    }
  }
}
}  // namespace internal
}  // namespace le
}  // namespace l2cap
}  // namespace bluetooth
