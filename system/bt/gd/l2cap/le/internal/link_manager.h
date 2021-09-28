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

#include "os/handler.h"

#include "hci/acl_manager.h"
#include "hci/address.h"
#include "hci/address_with_type.h"
#include "l2cap/internal/parameter_provider.h"
#include "l2cap/internal/scheduler.h"
#include "l2cap/le/fixed_channel_manager.h"
#include "l2cap/le/internal/fixed_channel_service_manager_impl.h"
#include "l2cap/le/internal/link.h"

namespace bluetooth {
namespace l2cap {
namespace le {
namespace internal {

class LinkManager : public hci::LeConnectionCallbacks {
 public:
  LinkManager(os::Handler* l2cap_handler, hci::AclManager* acl_manager, FixedChannelServiceManagerImpl* service_manager,
              l2cap::internal::ParameterProvider* parameter_provider)
      : l2cap_handler_(l2cap_handler), acl_manager_(acl_manager), fixed_channel_service_manager_(service_manager),
        parameter_provider_(parameter_provider) {
    acl_manager_->RegisterLeCallbacks(this, l2cap_handler_);
  }

  struct PendingFixedChannelConnection {
    os::Handler* handler_;
    FixedChannelManager::OnConnectionFailureCallback on_fail_callback_;
  };

  struct PendingLink {
    std::vector<PendingFixedChannelConnection> pending_fixed_channel_connections_;
  };

  // ACL methods

  Link* GetLink(hci::AddressWithType address_with_type);
  void OnLeConnectSuccess(hci::AddressWithType connecting_address_with_type,
                          std::unique_ptr<hci::AclConnection> acl_connection) override;
  void OnLeConnectFail(hci::AddressWithType address_with_type, hci::ErrorCode reason) override;
  void OnDisconnect(hci::AddressWithType address_with_type, hci::ErrorCode status);

  // FixedChannelManager methods

  void ConnectFixedChannelServices(hci::AddressWithType address_with_type,
                                   PendingFixedChannelConnection pending_fixed_channel_connection);

  // DynamicChannelManager methods

  void ConnectDynamicChannelServices(hci::AddressWithType device,
                                     Link::PendingDynamicChannelConnection pending_dynamic_channel_connection, Psm psm);

 private:
  // Dependencies
  os::Handler* l2cap_handler_;
  hci::AclManager* acl_manager_;
  FixedChannelServiceManagerImpl* fixed_channel_service_manager_;
  DynamicChannelServiceManagerImpl* dynamic_channel_service_manager_;
  l2cap::internal::ParameterProvider* parameter_provider_;

  // Internal states
  std::unordered_map<hci::AddressWithType, PendingLink> pending_links_;
  std::unordered_map<hci::AddressWithType, Link> links_;
  std::unordered_map<hci::AddressWithType, std::list<std::pair<Psm, Link::PendingDynamicChannelConnection>>>
      pending_dynamic_channels_;
  DISALLOW_COPY_AND_ASSIGN(LinkManager);
};

}  // namespace internal
}  // namespace le
}  // namespace l2cap
}  // namespace bluetooth
