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
#include "hci/address.h"
#include "l2cap/classic/dynamic_channel_manager.h"
#include "l2cap/classic/fixed_channel_manager.h"
#include "l2cap/classic/internal/dynamic_channel_service_manager_impl.h"
#include "l2cap/classic/internal/fixed_channel_service_manager_impl.h"
#include "l2cap/classic/internal/link.h"
#include "l2cap/internal/parameter_provider.h"
#include "l2cap/internal/scheduler.h"
#include "os/handler.h"

namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {

class LinkManager : public hci::ConnectionCallbacks {
 public:
  LinkManager(os::Handler* l2cap_handler, hci::AclManager* acl_manager,
              FixedChannelServiceManagerImpl* fixed_channel_service_manager,
              DynamicChannelServiceManagerImpl* dynamic_channel_service_manager,
              l2cap::internal::ParameterProvider* parameter_provider)
      : l2cap_handler_(l2cap_handler), acl_manager_(acl_manager),
        fixed_channel_service_manager_(fixed_channel_service_manager),
        dynamic_channel_service_manager_(dynamic_channel_service_manager), parameter_provider_(parameter_provider) {
    acl_manager_->RegisterCallbacks(this, l2cap_handler_);
  }

  struct PendingFixedChannelConnection {
    os::Handler* handler_;
    FixedChannelManager::OnConnectionFailureCallback on_fail_callback_;
  };

  struct PendingLink {
    std::vector<PendingFixedChannelConnection> pending_fixed_channel_connections_;
  };

  // ACL methods

  Link* GetLink(hci::Address device);
  void OnConnectSuccess(std::unique_ptr<hci::AclConnection> acl_connection) override;
  void OnConnectFail(hci::Address device, hci::ErrorCode reason) override;
  void OnDisconnect(hci::Address device, hci::ErrorCode status);

  // FixedChannelManager methods

  void ConnectFixedChannelServices(hci::Address device, PendingFixedChannelConnection pending_fixed_channel_connection);

  // DynamicChannelManager methods

  void ConnectDynamicChannelServices(hci::Address device,
                                     Link::PendingDynamicChannelConnection pending_dynamic_channel_connection, Psm psm);

 private:
  void TriggerPairing(Link* link);

  // Dependencies
  os::Handler* l2cap_handler_;
  hci::AclManager* acl_manager_;
  FixedChannelServiceManagerImpl* fixed_channel_service_manager_;
  DynamicChannelServiceManagerImpl* dynamic_channel_service_manager_;
  l2cap::internal::ParameterProvider* parameter_provider_;

  // Internal states
  std::unordered_map<hci::Address, PendingLink> pending_links_;
  std::unordered_map<hci::Address, Link> links_;
  std::unordered_map<hci::Address, std::list<Psm>> pending_dynamic_channels_;
  std::unordered_map<hci::Address, std::list<Link::PendingDynamicChannelConnection>>
      pending_dynamic_channels_callbacks_;
  DISALLOW_COPY_AND_ASSIGN(LinkManager);
};

}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
