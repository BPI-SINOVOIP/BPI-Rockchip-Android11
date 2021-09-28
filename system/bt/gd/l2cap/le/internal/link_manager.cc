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
#include <memory>
#include <unordered_map>

#include "hci/acl_manager.h"
#include "hci/address.h"
#include "l2cap/internal/scheduler_fifo.h"
#include "l2cap/le/internal/link.h"
#include "os/handler.h"
#include "os/log.h"

#include "l2cap/le/internal/link_manager.h"

namespace bluetooth {
namespace l2cap {
namespace le {
namespace internal {

void LinkManager::ConnectFixedChannelServices(hci::AddressWithType address_with_type,
                                              PendingFixedChannelConnection pending_fixed_channel_connection) {
  // Check if there is any service registered
  auto fixed_channel_services = fixed_channel_service_manager_->GetRegisteredServices();
  if (fixed_channel_services.empty()) {
    // If so, return error
    pending_fixed_channel_connection.handler_->Post(common::BindOnce(
        std::move(pending_fixed_channel_connection.on_fail_callback_),
        FixedChannelManager::ConnectionResult{
            .connection_result_code = FixedChannelManager::ConnectionResultCode::FAIL_NO_SERVICE_REGISTERED}));
    return;
  }
  // Otherwise, check if device has an ACL connection
  auto* link = GetLink(address_with_type);
  if (link != nullptr) {
    // If device already have an ACL connection
    // Check if all registered services have an allocated channel and allocate one if not already allocated
    int num_new_channels = 0;
    for (auto& fixed_channel_service : fixed_channel_services) {
      if (link->IsFixedChannelAllocated(fixed_channel_service.first)) {
        // This channel is already allocated for this link, do not allocated twice
        continue;
      }
      // Allocate channel for newly registered fixed channels
      auto fixed_channel_impl = link->AllocateFixedChannel(fixed_channel_service.first, SecurityPolicy());
      fixed_channel_service.second->NotifyChannelCreation(
          std::make_unique<FixedChannel>(fixed_channel_impl, l2cap_handler_));
      num_new_channels++;
    }
    // Declare connection failure if no new channels are created
    if (num_new_channels == 0) {
      pending_fixed_channel_connection.handler_->Post(common::BindOnce(
          std::move(pending_fixed_channel_connection.on_fail_callback_),
          FixedChannelManager::ConnectionResult{
              .connection_result_code = FixedChannelManager::ConnectionResultCode::FAIL_ALL_SERVICES_HAVE_CHANNEL}));
    }
    // No need to create ACL connection, return without saving any pending connections
    return;
  }
  // If not, create new ACL connection
  // Add request to pending link list first
  auto pending_link = pending_links_.find(address_with_type);
  if (pending_link == pending_links_.end()) {
    // Create pending link if not exist
    pending_links_.try_emplace(address_with_type);
    pending_link = pending_links_.find(address_with_type);
  }
  pending_link->second.pending_fixed_channel_connections_.push_back(std::move(pending_fixed_channel_connection));
  // Then create new ACL connection
  acl_manager_->CreateLeConnection(address_with_type);
}

void LinkManager::ConnectDynamicChannelServices(
    hci::AddressWithType device, Link::PendingDynamicChannelConnection pending_dynamic_channel_connection, Psm psm) {
  auto* link = GetLink(device);
  if (link == nullptr) {
    acl_manager_->CreateLeConnection(device);
    pending_dynamic_channels_[device].push_back(std::make_pair(psm, std::move(pending_dynamic_channel_connection)));
    return;
  }
  link->SendConnectionRequest(psm, std::move(pending_dynamic_channel_connection));
}

Link* LinkManager::GetLink(hci::AddressWithType address_with_type) {
  if (links_.find(address_with_type) == links_.end()) {
    return nullptr;
  }
  return &links_.find(address_with_type)->second;
}

void LinkManager::OnLeConnectSuccess(hci::AddressWithType connecting_address_with_type,
                                     std::unique_ptr<hci::AclConnection> acl_connection) {
  // Same link should not be connected twice
  hci::AddressWithType connected_address_with_type(acl_connection->GetAddress(), acl_connection->GetAddressType());
  ASSERT_LOG(GetLink(connected_address_with_type) == nullptr, "%s is connected twice without disconnection",
             acl_connection->GetAddress().ToString().c_str());
  // Register ACL disconnection callback in LinkManager so that we can clean up link resource properly
  acl_connection->RegisterDisconnectCallback(
      common::BindOnce(&LinkManager::OnDisconnect, common::Unretained(this), connected_address_with_type),
      l2cap_handler_);
  links_.try_emplace(connected_address_with_type, l2cap_handler_, std::move(acl_connection), parameter_provider_,
                     dynamic_channel_service_manager_, fixed_channel_service_manager_);
  auto* link = GetLink(connected_address_with_type);
  // Allocate and distribute channels for all registered fixed channel services
  auto fixed_channel_services = fixed_channel_service_manager_->GetRegisteredServices();
  for (auto& fixed_channel_service : fixed_channel_services) {
    auto fixed_channel_impl = link->AllocateFixedChannel(fixed_channel_service.first, SecurityPolicy());
    fixed_channel_service.second->NotifyChannelCreation(
        std::make_unique<FixedChannel>(fixed_channel_impl, l2cap_handler_));
  }
  if (pending_dynamic_channels_.find(connected_address_with_type) != pending_dynamic_channels_.end()) {
    for (auto& psm_callback : pending_dynamic_channels_[connected_address_with_type]) {
      link->SendConnectionRequest(psm_callback.first, std::move(psm_callback.second));
    }
    pending_dynamic_channels_.erase(connected_address_with_type);
  }

  // Remove device from pending links list, if any
  auto pending_link = pending_links_.find(connecting_address_with_type);
  if (pending_link == pending_links_.end()) {
    // This an incoming connection, exit
    return;
  }
  // This is an outgoing connection, remove entry in pending link list
  pending_links_.erase(pending_link);
}

void LinkManager::OnLeConnectFail(hci::AddressWithType address_with_type, hci::ErrorCode reason) {
  // Notify all pending links for this device
  auto pending_link = pending_links_.find(address_with_type);
  if (pending_link == pending_links_.end()) {
    // There is no pending link, exit
    LOG_DEBUG("Connection to %s failed without a pending link", address_with_type.ToString().c_str());
    return;
  }
  for (auto& pending_fixed_channel_connection : pending_link->second.pending_fixed_channel_connections_) {
    pending_fixed_channel_connection.handler_->Post(common::BindOnce(
        std::move(pending_fixed_channel_connection.on_fail_callback_),
        FixedChannelManager::ConnectionResult{
            .connection_result_code = FixedChannelManager::ConnectionResultCode::FAIL_HCI_ERROR, .hci_error = reason}));
  }
  // Remove entry in pending link list
  pending_links_.erase(pending_link);
}

void LinkManager::OnDisconnect(hci::AddressWithType address_with_type, hci::ErrorCode status) {
  auto* link = GetLink(address_with_type);
  ASSERT_LOG(link != nullptr, "Device %s is disconnected with reason 0x%x, but not in local database",
             address_with_type.ToString().c_str(), static_cast<uint8_t>(status));
  link->OnAclDisconnected(status);
  links_.erase(address_with_type);
}

}  // namespace internal
}  // namespace le
}  // namespace l2cap
}  // namespace bluetooth
