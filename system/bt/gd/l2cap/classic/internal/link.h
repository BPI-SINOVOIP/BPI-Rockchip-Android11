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

#include "hci/acl_manager.h"
#include "l2cap/classic/dynamic_channel_configuration_option.h"
#include "l2cap/classic/internal/dynamic_channel_service_manager_impl.h"
#include "l2cap/classic/internal/fixed_channel_impl.h"
#include "l2cap/classic/internal/fixed_channel_service_manager_impl.h"
#include "l2cap/internal/data_pipeline_manager.h"
#include "l2cap/internal/dynamic_channel_allocator.h"
#include "l2cap/internal/dynamic_channel_impl.h"
#include "l2cap/internal/fixed_channel_allocator.h"
#include "l2cap/internal/ilink.h"
#include "l2cap/internal/parameter_provider.h"
#include "os/alarm.h"
#include "os/handler.h"
#include "signalling_manager.h"

namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {

class Link : public l2cap::internal::ILink, public hci::ConnectionManagementCallbacks {
 public:
  Link(os::Handler* l2cap_handler, std::unique_ptr<hci::AclConnection> acl_connection,
       l2cap::internal::ParameterProvider* parameter_provider,
       DynamicChannelServiceManagerImpl* dynamic_service_manager,
       FixedChannelServiceManagerImpl* fixed_service_manager);
  ~Link();

  hci::AddressWithType GetDevice() override {
    return {acl_connection_->GetAddress(), acl_connection_->GetAddressType()};
  }

  struct PendingDynamicChannelConnection {
    os::Handler* handler_;
    DynamicChannelManager::OnConnectionOpenCallback on_open_callback_;
    DynamicChannelManager::OnConnectionFailureCallback on_fail_callback_;
    classic::DynamicChannelConfigurationOption configuration_;
  };

  struct PendingAuthenticateDynamicChannelConnection {
    Psm psm_;
    Cid cid_;
    PendingDynamicChannelConnection pending_dynamic_channel_connection_;
  };

  // ACL methods

  virtual void OnAclDisconnected(hci::ErrorCode status);

  virtual void Disconnect();

  virtual void Encrypt();

  virtual void Authenticate();

  virtual bool IsAuthenticated() const;

  virtual void ReadRemoteVersionInformation();

  virtual void ReadRemoteSupportedFeatures();

  virtual void ReadRemoteExtendedFeatures();

  virtual void ReadClockOffset();

  // FixedChannel methods

  std::shared_ptr<FixedChannelImpl> AllocateFixedChannel(Cid cid, SecurityPolicy security_policy);

  virtual bool IsFixedChannelAllocated(Cid cid);

  // DynamicChannel methods

  virtual Cid ReserveDynamicChannel();

  virtual void SendConnectionRequest(Psm psm, Cid local_cid);
  virtual void SendConnectionRequest(Psm psm, Cid local_cid,
                                     PendingDynamicChannelConnection pending_dynamic_channel_connection);

  // Invoked by signalling manager to indicate an outgoing connection request failed and link shall free resources
  virtual void OnOutgoingConnectionRequestFail(Cid local_cid);

  virtual void SendInformationRequest(InformationRequestInfoType type);

  virtual void SendDisconnectionRequest(Cid local_cid, Cid remote_cid) override;

  virtual std::shared_ptr<l2cap::internal::DynamicChannelImpl> AllocateDynamicChannel(Psm psm, Cid remote_cid,
                                                                                      SecurityPolicy security_policy);

  virtual std::shared_ptr<l2cap::internal::DynamicChannelImpl> AllocateReservedDynamicChannel(
      Cid reserved_cid, Psm psm, Cid remote_cid, SecurityPolicy security_policy);

  virtual classic::DynamicChannelConfigurationOption GetConfigurationForInitialConfiguration(Cid cid);

  virtual void FreeDynamicChannel(Cid cid);

  // Check how many channels are acquired or in use, if zero, start tear down timer, if non-zero, cancel tear down timer
  virtual void RefreshRefCount();

  virtual void NotifyChannelCreation(Cid cid, std::unique_ptr<DynamicChannel> channel);
  virtual void NotifyChannelFail(Cid cid, DynamicChannelManager::ConnectionResult result);

  // Information received from signaling channel
  virtual void SetRemoteConnectionlessMtu(Mtu mtu);
  virtual Mtu GetRemoteConnectionlessMtu() const;
  virtual void SetRemoteSupportsErtm(bool supported);
  virtual bool GetRemoteSupportsErtm() const;
  virtual void SetRemoteSupportsFcs(bool supported);
  virtual bool GetRemoteSupportsFcs() const;

  virtual std::string ToString() {
    return GetDevice().ToString();
  }

  void SendLeCredit(Cid local_cid, uint16_t credit) override {}

  void AddChannelPendingingAuthentication(PendingAuthenticateDynamicChannelConnection pending_channel);

  // ConnectionManagementCallbacks
  virtual void OnConnectionPacketTypeChanged(uint16_t packet_type) override;
  virtual void OnAuthenticationComplete() override;
  virtual void OnEncryptionChange(hci::EncryptionEnabled enabled) override;
  virtual void OnChangeConnectionLinkKeyComplete() override;
  virtual void OnReadClockOffsetComplete(uint16_t clock_offset) override;
  virtual void OnModeChange(hci::Mode current_mode, uint16_t interval) override;
  virtual void OnQosSetupComplete(hci::ServiceType service_type, uint32_t token_rate, uint32_t peak_bandwidth,
                                  uint32_t latency, uint32_t delay_variation) override;
  virtual void OnFlowSpecificationComplete(hci::FlowDirection flow_direction, hci::ServiceType service_type,
                                           uint32_t token_rate, uint32_t token_bucket_size, uint32_t peak_bandwidth,
                                           uint32_t access_latency) override;
  virtual void OnFlushOccurred() override;
  virtual void OnRoleDiscoveryComplete(hci::Role current_role) override;
  virtual void OnReadLinkPolicySettingsComplete(uint16_t link_policy_settings) override;
  virtual void OnReadAutomaticFlushTimeoutComplete(uint16_t flush_timeout) override;
  virtual void OnReadTransmitPowerLevelComplete(uint8_t transmit_power_level) override;
  virtual void OnReadLinkSupervisionTimeoutComplete(uint16_t link_supervision_timeout) override;
  virtual void OnReadFailedContactCounterComplete(uint16_t failed_contact_counter) override;
  virtual void OnReadLinkQualityComplete(uint8_t link_quality) override;
  virtual void OnReadAfhChannelMapComplete(hci::AfhMode afh_mode, std::array<uint8_t, 10> afh_channel_map) override;
  virtual void OnReadRssiComplete(uint8_t rssi) override;
  virtual void OnReadClockComplete(uint32_t clock, uint16_t accuracy) override;

 private:
  os::Handler* l2cap_handler_;
  l2cap::internal::FixedChannelAllocator<FixedChannelImpl, Link> fixed_channel_allocator_{this, l2cap_handler_};
  l2cap::internal::DynamicChannelAllocator dynamic_channel_allocator_{this, l2cap_handler_};
  std::unique_ptr<hci::AclConnection> acl_connection_;
  l2cap::internal::DataPipelineManager data_pipeline_manager_;
  l2cap::internal::ParameterProvider* parameter_provider_;
  DynamicChannelServiceManagerImpl* dynamic_service_manager_;
  FixedChannelServiceManagerImpl* fixed_service_manager_;
  ClassicSignallingManager signalling_manager_;
  std::unordered_map<Cid, PendingDynamicChannelConnection> local_cid_to_pending_dynamic_channel_connection_map_;
  os::Alarm link_idle_disconnect_alarm_{l2cap_handler_};
  Mtu remote_connectionless_mtu_ = kMinimumClassicMtu;
  bool remote_supports_ertm_ = false;
  bool remote_supports_fcs_ = false;
  hci::EncryptionEnabled encryption_enabled_ = hci::EncryptionEnabled::OFF;
  std::list<Link::PendingAuthenticateDynamicChannelConnection> pending_channel_list_;
  DISALLOW_COPY_AND_ASSIGN(Link);
};

}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
