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

#include <chrono>
#include <memory>

#include "hci/acl_manager.h"
#include "l2cap/classic/dynamic_channel_manager.h"
#include "l2cap/classic/internal/fixed_channel_impl.h"
#include "l2cap/classic/internal/link.h"
#include "l2cap/internal/parameter_provider.h"
#include "os/alarm.h"

namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {

Link::Link(os::Handler* l2cap_handler, std::unique_ptr<hci::AclConnection> acl_connection,
           l2cap::internal::ParameterProvider* parameter_provider,
           DynamicChannelServiceManagerImpl* dynamic_service_manager,
           FixedChannelServiceManagerImpl* fixed_service_manager)
    : l2cap_handler_(l2cap_handler), acl_connection_(std::move(acl_connection)),
      data_pipeline_manager_(l2cap_handler, this, acl_connection_->GetAclQueueEnd()),
      parameter_provider_(parameter_provider), dynamic_service_manager_(dynamic_service_manager),
      fixed_service_manager_(fixed_service_manager),
      signalling_manager_(l2cap_handler_, this, &data_pipeline_manager_, dynamic_service_manager_,
                          &dynamic_channel_allocator_, fixed_service_manager_) {
  ASSERT(l2cap_handler_ != nullptr);
  ASSERT(acl_connection_ != nullptr);
  ASSERT(parameter_provider_ != nullptr);
  link_idle_disconnect_alarm_.Schedule(common::BindOnce(&Link::Disconnect, common::Unretained(this)),
                                       parameter_provider_->GetClassicLinkIdleDisconnectTimeout());
  acl_connection_->RegisterCallbacks(this, l2cap_handler_);
}

Link::~Link() {
  acl_connection_->UnregisterCallbacks(this);
}

void Link::OnAclDisconnected(hci::ErrorCode status) {
  signalling_manager_.CancelAlarm();
  fixed_channel_allocator_.OnAclDisconnected(status);
  dynamic_channel_allocator_.OnAclDisconnected(status);
  DynamicChannelManager::ConnectionResult result{
      .connection_result_code = DynamicChannelManager::ConnectionResultCode::FAIL_HCI_ERROR,
      .hci_error = status,
      .l2cap_connection_response_result = ConnectionResponseResult::SUCCESS,
  };
  while (!local_cid_to_pending_dynamic_channel_connection_map_.empty()) {
    auto entry = local_cid_to_pending_dynamic_channel_connection_map_.begin();
    NotifyChannelFail(entry->first, result);
  }
}

void Link::Disconnect() {
  acl_connection_->Disconnect(hci::DisconnectReason::REMOTE_USER_TERMINATED_CONNECTION);
}

void Link::Encrypt() {
  acl_connection_->SetConnectionEncryption(hci::Enable::ENABLED);
}

void Link::Authenticate() {
  acl_connection_->AuthenticationRequested();
}

bool Link::IsAuthenticated() const {
  return encryption_enabled_ != hci::EncryptionEnabled::OFF;
}

void Link::ReadRemoteVersionInformation() {
  acl_connection_->ReadRemoteVersionInformation();
}

void Link::ReadRemoteSupportedFeatures() {
  acl_connection_->ReadRemoteSupportedFeatures();
}

void Link::ReadRemoteExtendedFeatures() {
  acl_connection_->ReadRemoteExtendedFeatures();
}

void Link::ReadClockOffset() {
  acl_connection_->ReadClockOffset();
}

std::shared_ptr<FixedChannelImpl> Link::AllocateFixedChannel(Cid cid, SecurityPolicy security_policy) {
  auto channel = fixed_channel_allocator_.AllocateChannel(cid, security_policy);
  data_pipeline_manager_.AttachChannel(cid, channel, l2cap::internal::DataPipelineManager::ChannelMode::BASIC);
  return channel;
}

bool Link::IsFixedChannelAllocated(Cid cid) {
  return fixed_channel_allocator_.IsChannelAllocated(cid);
}

Cid Link::ReserveDynamicChannel() {
  return dynamic_channel_allocator_.ReserveChannel();
}

void Link::SendConnectionRequest(Psm psm, Cid local_cid) {
  signalling_manager_.SendConnectionRequest(psm, local_cid);
}

void Link::SendConnectionRequest(Psm psm, Cid local_cid,
                                 PendingDynamicChannelConnection pending_dynamic_channel_connection) {
  local_cid_to_pending_dynamic_channel_connection_map_[local_cid] = std::move(pending_dynamic_channel_connection);
  signalling_manager_.SendConnectionRequest(psm, local_cid);
}

void Link::OnOutgoingConnectionRequestFail(Cid local_cid) {
  if (local_cid_to_pending_dynamic_channel_connection_map_.find(local_cid) !=
      local_cid_to_pending_dynamic_channel_connection_map_.end()) {
    DynamicChannelManager::ConnectionResult result{
        .connection_result_code = DynamicChannelManager::ConnectionResultCode::FAIL_HCI_ERROR,
        .hci_error = hci::ErrorCode::CONNECTION_TIMEOUT,
        .l2cap_connection_response_result = ConnectionResponseResult::SUCCESS,
    };
    NotifyChannelFail(local_cid, result);
  }
  dynamic_channel_allocator_.FreeChannel(local_cid);
}

void Link::SendDisconnectionRequest(Cid local_cid, Cid remote_cid) {
  signalling_manager_.SendDisconnectionRequest(local_cid, remote_cid);
}

void Link::SendInformationRequest(InformationRequestInfoType type) {
  signalling_manager_.SendInformationRequest(type);
}

std::shared_ptr<l2cap::internal::DynamicChannelImpl> Link::AllocateDynamicChannel(Psm psm, Cid remote_cid,
                                                                                  SecurityPolicy security_policy) {
  auto channel = dynamic_channel_allocator_.AllocateChannel(psm, remote_cid, security_policy);
  if (channel != nullptr) {
    data_pipeline_manager_.AttachChannel(channel->GetCid(), channel,
                                         l2cap::internal::DataPipelineManager::ChannelMode::BASIC);
    RefreshRefCount();
  }
  channel->local_initiated_ = false;
  return channel;
}

std::shared_ptr<l2cap::internal::DynamicChannelImpl> Link::AllocateReservedDynamicChannel(
    Cid reserved_cid, Psm psm, Cid remote_cid, SecurityPolicy security_policy) {
  auto channel = dynamic_channel_allocator_.AllocateReservedChannel(reserved_cid, psm, remote_cid, security_policy);
  if (channel != nullptr) {
    data_pipeline_manager_.AttachChannel(channel->GetCid(), channel,
                                         l2cap::internal::DataPipelineManager::ChannelMode::BASIC);
    RefreshRefCount();
  }
  channel->local_initiated_ = true;
  return channel;
}

classic::DynamicChannelConfigurationOption Link::GetConfigurationForInitialConfiguration(Cid cid) {
  ASSERT(local_cid_to_pending_dynamic_channel_connection_map_.find(cid) !=
         local_cid_to_pending_dynamic_channel_connection_map_.end());
  return local_cid_to_pending_dynamic_channel_connection_map_[cid].configuration_;
}

void Link::FreeDynamicChannel(Cid cid) {
  if (dynamic_channel_allocator_.FindChannelByCid(cid) == nullptr) {
    return;
  }
  data_pipeline_manager_.DetachChannel(cid);
  dynamic_channel_allocator_.FreeChannel(cid);
  RefreshRefCount();
}

void Link::RefreshRefCount() {
  int ref_count = 0;
  ref_count += fixed_channel_allocator_.GetRefCount();
  ref_count += dynamic_channel_allocator_.NumberOfChannels();
  ASSERT_LOG(ref_count >= 0, "ref_count %d is less than 0", ref_count);
  if (ref_count > 0) {
    link_idle_disconnect_alarm_.Cancel();
  } else {
    link_idle_disconnect_alarm_.Schedule(common::BindOnce(&Link::Disconnect, common::Unretained(this)),
                                         parameter_provider_->GetClassicLinkIdleDisconnectTimeout());
  }
}

void Link::NotifyChannelCreation(Cid cid, std::unique_ptr<DynamicChannel> user_channel) {
  ASSERT(local_cid_to_pending_dynamic_channel_connection_map_.find(cid) !=
         local_cid_to_pending_dynamic_channel_connection_map_.end());
  auto& pending_dynamic_channel_connection = local_cid_to_pending_dynamic_channel_connection_map_[cid];
  pending_dynamic_channel_connection.handler_->Post(
      common::BindOnce(std::move(pending_dynamic_channel_connection.on_open_callback_), std::move(user_channel)));
  local_cid_to_pending_dynamic_channel_connection_map_.erase(cid);
}

void Link::NotifyChannelFail(Cid cid, DynamicChannelManager::ConnectionResult result) {
  ASSERT(local_cid_to_pending_dynamic_channel_connection_map_.find(cid) !=
         local_cid_to_pending_dynamic_channel_connection_map_.end());
  auto& pending_dynamic_channel_connection = local_cid_to_pending_dynamic_channel_connection_map_[cid];
  pending_dynamic_channel_connection.handler_->Post(
      common::BindOnce(std::move(pending_dynamic_channel_connection.on_fail_callback_), result));
  local_cid_to_pending_dynamic_channel_connection_map_.erase(cid);
}

void Link::SetRemoteConnectionlessMtu(Mtu mtu) {
  remote_connectionless_mtu_ = mtu;
}

Mtu Link::GetRemoteConnectionlessMtu() const {
  return remote_connectionless_mtu_;
}

void Link::SetRemoteSupportsErtm(bool supported) {
  remote_supports_ertm_ = supported;
}

bool Link::GetRemoteSupportsErtm() const {
  return remote_supports_ertm_;
}

void Link::SetRemoteSupportsFcs(bool supported) {
  remote_supports_fcs_ = supported;
}

bool Link::GetRemoteSupportsFcs() const {
  return remote_supports_fcs_;
}

void Link::AddChannelPendingingAuthentication(PendingAuthenticateDynamicChannelConnection pending_channel) {
  pending_channel_list_.push_back(std::move(pending_channel));
}

void Link::OnConnectionPacketTypeChanged(uint16_t packet_type) {
  LOG_DEBUG("UNIMPLEMENTED %s packet_type:%x", __func__, packet_type);
}

void Link::OnAuthenticationComplete() {
  if (!pending_channel_list_.empty()) {
    acl_connection_->SetConnectionEncryption(hci::Enable::ENABLED);
  }
}

void Link::OnEncryptionChange(hci::EncryptionEnabled enabled) {
  encryption_enabled_ = enabled;
  if (encryption_enabled_ == hci::EncryptionEnabled::OFF) {
    LOG_DEBUG("Encryption has changed to disabled");
    return;
  }
  LOG_DEBUG("Encryption has changed to enabled .. restarting channels:%zd", pending_channel_list_.size());

  for (auto& channel : pending_channel_list_) {
    local_cid_to_pending_dynamic_channel_connection_map_[channel.cid_] =
        std::move(channel.pending_dynamic_channel_connection_);
    signalling_manager_.SendConnectionRequest(channel.psm_, channel.cid_);
  }
  pending_channel_list_.clear();
}

void Link::OnChangeConnectionLinkKeyComplete() {
  LOG_DEBUG("UNIMPLEMENTED %s", __func__);
}

void Link::OnReadClockOffsetComplete(uint16_t clock_offset) {
  LOG_DEBUG("UNIMPLEMENTED %s clock_offset:%d", __func__, clock_offset);
}

void Link::OnModeChange(hci::Mode current_mode, uint16_t interval) {
  LOG_DEBUG("UNIMPLEMENTED %s mode:%s interval:%d", __func__, hci::ModeText(current_mode).c_str(), interval);
}

void Link::OnQosSetupComplete(hci::ServiceType service_type, uint32_t token_rate, uint32_t peak_bandwidth,
                              uint32_t latency, uint32_t delay_variation) {
  LOG_DEBUG("UNIMPLEMENTED %s service_type:%s token_rate:%d peak_bandwidth:%d latency:%d delay_varitation:%d", __func__,
            hci::ServiceTypeText(service_type).c_str(), token_rate, peak_bandwidth, latency, delay_variation);
}
void Link::OnFlowSpecificationComplete(hci::FlowDirection flow_direction, hci::ServiceType service_type,
                                       uint32_t token_rate, uint32_t token_bucket_size, uint32_t peak_bandwidth,
                                       uint32_t access_latency) {
  LOG_DEBUG(
      "UNIMPLEMENTED %s flow_direction:%s service_type:%s token_rate:%d token_bucket_size:%d peak_bandwidth:%d "
      "access_latency:%d",
      __func__, hci::FlowDirectionText(flow_direction).c_str(), hci::ServiceTypeText(service_type).c_str(), token_rate,
      token_bucket_size, peak_bandwidth, access_latency);
}
void Link::OnFlushOccurred() {
  LOG_DEBUG("UNIMPLEMENTED %s", __func__);
}
void Link::OnRoleDiscoveryComplete(hci::Role current_role) {
  LOG_DEBUG("UNIMPLEMENTED %s current_role:%s", __func__, hci::RoleText(current_role).c_str());
}
void Link::OnReadLinkPolicySettingsComplete(uint16_t link_policy_settings) {
  LOG_DEBUG("UNIMPLEMENTED %s link_policy_settings:0x%x", __func__, link_policy_settings);
}
void Link::OnReadAutomaticFlushTimeoutComplete(uint16_t flush_timeout) {
  LOG_DEBUG("UNIMPLEMENTED %s flush_timeout:%d", __func__, flush_timeout);
}
void Link::OnReadTransmitPowerLevelComplete(uint8_t transmit_power_level) {
  LOG_DEBUG("UNIMPLEMENTED %s transmit_power_level:%d", __func__, transmit_power_level);
}
void Link::OnReadLinkSupervisionTimeoutComplete(uint16_t link_supervision_timeout) {
  LOG_DEBUG("UNIMPLEMENTED %s link_supervision_timeout:%d", __func__, link_supervision_timeout);
}
void Link::OnReadFailedContactCounterComplete(uint16_t failed_contact_counter) {
  LOG_DEBUG("UNIMPLEMENTED %sfailed_contact_counter:%hu", __func__, failed_contact_counter);
}
void Link::OnReadLinkQualityComplete(uint8_t link_quality) {
  LOG_DEBUG("UNIMPLEMENTED %s link_quality:%hhu", __func__, link_quality);
}
void Link::OnReadAfhChannelMapComplete(hci::AfhMode afh_mode, std::array<uint8_t, 10> afh_channel_map) {
  LOG_DEBUG("UNIMPLEMENTED %s afh_mode:%s", __func__, hci::AfhModeText(afh_mode).c_str());
}
void Link::OnReadRssiComplete(uint8_t rssi) {
  LOG_DEBUG("UNIMPLEMENTED %s rssi:%hhd", __func__, rssi);
}
void Link::OnReadClockComplete(uint32_t clock, uint16_t accuracy) {
  LOG_DEBUG("UNIMPLEMENTED %s clock:%u accuracy:%hu", __func__, clock, accuracy);
}

}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
