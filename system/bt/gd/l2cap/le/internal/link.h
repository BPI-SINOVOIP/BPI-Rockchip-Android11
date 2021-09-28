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

#include <chrono>
#include <memory>

#include "hci/acl_manager.h"
#include "l2cap/internal/data_pipeline_manager.h"
#include "l2cap/internal/dynamic_channel_allocator.h"
#include "l2cap/internal/fixed_channel_allocator.h"
#include "l2cap/internal/ilink.h"
#include "l2cap/internal/parameter_provider.h"
#include "l2cap/le/dynamic_channel_manager.h"
#include "l2cap/le/internal/dynamic_channel_service_manager_impl.h"
#include "l2cap/le/internal/fixed_channel_impl.h"
#include "l2cap/le/internal/fixed_channel_service_manager_impl.h"
#include "l2cap/le/internal/signalling_manager.h"
#include "os/alarm.h"

namespace bluetooth {
namespace l2cap {
namespace le {
namespace internal {

class Link : public l2cap::internal::ILink {
 public:
  Link(os::Handler* l2cap_handler, std::unique_ptr<hci::AclConnection> acl_connection,
       l2cap::internal::ParameterProvider* parameter_provider,
       DynamicChannelServiceManagerImpl* dynamic_service_manager,
       FixedChannelServiceManagerImpl* fixed_service_manager);

  ~Link() override = default;

  inline hci::AddressWithType GetDevice() override {
    return {acl_connection_->GetAddress(), acl_connection_->GetAddressType()};
  }

  struct PendingDynamicChannelConnection {
    os::Handler* handler_;
    DynamicChannelManager::OnConnectionOpenCallback on_open_callback_;
    DynamicChannelManager::OnConnectionFailureCallback on_fail_callback_;
    le::DynamicChannelConfigurationOption configuration_;
  };

  inline virtual hci::Role GetRole() {
    return acl_connection_->GetRole();
  }

  inline virtual hci::AclConnection* GetAclConnection() {
    return acl_connection_.get();
  }

  // ACL methods

  virtual void OnAclDisconnected(hci::ErrorCode status);

  virtual void Disconnect();

  // Handles connection parameter update request from remote
  virtual void UpdateConnectionParameter(SignalId signal_id, uint16_t conn_interval_min, uint16_t conn_interval_max,
                                         uint16_t conn_latency, uint16_t supervision_timeout);

  // FixedChannel methods

  virtual std::shared_ptr<FixedChannelImpl> AllocateFixedChannel(Cid cid, SecurityPolicy security_policy);

  virtual bool IsFixedChannelAllocated(Cid cid);

  // DynamicChannel methods

  virtual Cid ReserveDynamicChannel();

  virtual void SendConnectionRequest(Psm psm, PendingDynamicChannelConnection pending_dynamic_channel_connection);

  void SendDisconnectionRequest(Cid local_cid, Cid remote_cid) override;

  // Invoked by signalling manager to indicate an outgoing connection request failed and link shall free resources
  virtual void OnOutgoingConnectionRequestFail(Cid local_cid);

  virtual std::shared_ptr<l2cap::internal::DynamicChannelImpl> AllocateDynamicChannel(Psm psm, Cid remote_cid,
                                                                                      SecurityPolicy security_policy);

  virtual std::shared_ptr<l2cap::internal::DynamicChannelImpl> AllocateReservedDynamicChannel(
      Cid reserved_cid, Psm psm, Cid remote_cid, SecurityPolicy security_policy);

  virtual DynamicChannelConfigurationOption GetConfigurationForInitialConfiguration(Cid cid);

  virtual void FreeDynamicChannel(Cid cid);

  // Check how many channels are acquired or in use, if zero, start tear down timer, if non-zero, cancel tear down timer
  virtual void RefreshRefCount();

  void NotifyChannelCreation(Cid cid, std::unique_ptr<DynamicChannel> user_channel);
  void NotifyChannelFail(Cid cid);

  virtual std::string ToString() {
    return GetDevice().ToString();
  }

  virtual uint16_t GetMps() const;

  virtual uint16_t GetInitialCredit() const;

  void SendLeCredit(Cid local_cid, uint16_t credit) override;

 private:
  os::Handler* l2cap_handler_;
  l2cap::internal::FixedChannelAllocator<FixedChannelImpl, Link> fixed_channel_allocator_{this, l2cap_handler_};
  l2cap::internal::DynamicChannelAllocator dynamic_channel_allocator_{this, l2cap_handler_};
  std::unique_ptr<hci::AclConnection> acl_connection_;
  l2cap::internal::DataPipelineManager data_pipeline_manager_;
  l2cap::internal::ParameterProvider* parameter_provider_;
  DynamicChannelServiceManagerImpl* dynamic_service_manager_;
  LeSignallingManager signalling_manager_;
  std::unordered_map<Cid, PendingDynamicChannelConnection> local_cid_to_pending_dynamic_channel_connection_map_;
  os::Alarm link_idle_disconnect_alarm_{l2cap_handler_};
  DISALLOW_COPY_AND_ASSIGN(Link);

  void on_connection_update_complete(SignalId signal_id, hci::ErrorCode error_code);
};

}  // namespace internal
}  // namespace le
}  // namespace l2cap
}  // namespace bluetooth
