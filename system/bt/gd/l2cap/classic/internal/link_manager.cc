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
#include "l2cap/classic/internal/link.h"
#include "l2cap/internal/scheduler_fifo.h"
#include "os/log.h"

#include "l2cap/classic/internal/link_manager.h"

namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {

void LinkManager::ConnectFixedChannelServices(hci::Address device,
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
  auto* link = GetLink(device);
  if (link != nullptr) {
    // If device already have an ACL connection
    // Check if all registered services have an allocated channel and allocate one if not already allocated
    int num_new_channels = 0;
    for (auto& fixed_channel_service : fixed_channel_services) {
      if (link->IsFixedChannelAllocated(fixed_channel_service.first)) {
        // This channel is already allocated for this link, do not allocated twice
        continue;
      }
      if (fixed_channel_service.first == kClassicPairingTriggerCid) {
        this->TriggerPairing(link);
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
  auto pending_link = pending_links_.find(device);
  if (pending_link == pending_links_.end()) {
    // Create pending link if not exist
    pending_links_.try_emplace(device);
    pending_link = pending_links_.find(device);
  }
  pending_link->second.pending_fixed_channel_connections_.push_back(std::move(pending_fixed_channel_connection));
  // Then create new ACL connection
  acl_manager_->CreateConnection(device);
}

void LinkManager::ConnectDynamicChannelServices(
    hci::Address device, Link::PendingDynamicChannelConnection pending_dynamic_channel_connection, Psm psm) {
  auto* link = GetLink(device);
  if (link == nullptr) {
    acl_manager_->CreateConnection(device);
    if (pending_dynamic_channels_.find(device) != pending_dynamic_channels_.end()) {
      pending_dynamic_channels_[device].push_back(psm);
      pending_dynamic_channels_callbacks_[device].push_back(std::move(pending_dynamic_channel_connection));
    } else {
      pending_dynamic_channels_[device] = {psm};
      pending_dynamic_channels_callbacks_[device].push_back(std::move(pending_dynamic_channel_connection));
    }
    return;
  }
  if (dynamic_channel_service_manager_->GetService(psm)->GetSecurityPolicy().RequiresAuthentication() &&
      !link->IsAuthenticated()) {
    link->AddChannelPendingingAuthentication(
        {psm, link->ReserveDynamicChannel(), std::move(pending_dynamic_channel_connection)});
    link->Authenticate();
    return;
  }
  link->SendConnectionRequest(psm, link->ReserveDynamicChannel(), std::move(pending_dynamic_channel_connection));
}

Link* LinkManager::GetLink(const hci::Address device) {
  if (links_.find(device) == links_.end()) {
    return nullptr;
  }
  return &links_.find(device)->second;
}

void LinkManager::TriggerPairing(Link* link) {
  if (!link->IsAuthenticated()) {
    link->Authenticate();
  }
  link->ReadRemoteVersionInformation();
  link->ReadRemoteSupportedFeatures();
  link->ReadRemoteExtendedFeatures();
  link->ReadClockOffset();
}

void LinkManager::OnConnectSuccess(std::unique_ptr<hci::AclConnection> acl_connection) {
  // Same link should not be connected twice
  hci::Address device = acl_connection->GetAddress();
  ASSERT_LOG(GetLink(device) == nullptr, "%s is connected twice without disconnection",
             acl_connection->GetAddress().ToString().c_str());
  // Register ACL disconnection callback in LinkManager so that we can clean up link resource properly
  acl_connection->RegisterDisconnectCallback(
      common::BindOnce(&LinkManager::OnDisconnect, common::Unretained(this), device), l2cap_handler_);
  links_.try_emplace(device, l2cap_handler_, std::move(acl_connection), parameter_provider_,
                     dynamic_channel_service_manager_, fixed_channel_service_manager_);
  auto* link = GetLink(device);
  ASSERT(link != nullptr);
  link->SendInformationRequest(InformationRequestInfoType::EXTENDED_FEATURES_SUPPORTED);
  link->SendInformationRequest(InformationRequestInfoType::FIXED_CHANNELS_SUPPORTED);

  // Allocate and distribute channels for all registered fixed channel services
  auto fixed_channel_services = fixed_channel_service_manager_->GetRegisteredServices();
  for (auto& fixed_channel_service : fixed_channel_services) {
    auto fixed_channel_impl = link->AllocateFixedChannel(fixed_channel_service.first, SecurityPolicy());
    fixed_channel_service.second->NotifyChannelCreation(
        std::make_unique<FixedChannel>(fixed_channel_impl, l2cap_handler_));
    if (fixed_channel_service.first == kClassicPairingTriggerCid) {
      this->TriggerPairing(link);
    }
  }
  if (pending_dynamic_channels_.find(device) != pending_dynamic_channels_.end()) {
    for (Psm psm : pending_dynamic_channels_[device]) {
      auto& callbacks = pending_dynamic_channels_callbacks_[device].front();
      link->SendConnectionRequest(psm, link->ReserveDynamicChannel(), std::move(callbacks));
      pending_dynamic_channels_callbacks_[device].pop_front();
    }
    pending_dynamic_channels_.erase(device);
    pending_dynamic_channels_callbacks_.erase(device);
  }
  // Remove device from pending links list, if any
  auto pending_link = pending_links_.find(device);
  if (pending_link == pending_links_.end()) {
    // This an incoming connection, exit
    return;
  }
  // This is an outgoing connection, remove entry in pending link list
  pending_links_.erase(pending_link);
}

void LinkManager::OnConnectFail(hci::Address device, hci::ErrorCode reason) {
  // Notify all pending links for this device
  auto pending_link = pending_links_.find(device);
  if (pending_link == pending_links_.end()) {
    // There is no pending link, exit
    LOG_DEBUG("Connection to %s failed without a pending link; reason: %s", device.ToString().c_str(),
              hci::ErrorCodeText(reason).c_str());
    if (pending_dynamic_channels_callbacks_.find(device) != pending_dynamic_channels_callbacks_.end()) {
      for (Link::PendingDynamicChannelConnection& callbacks : pending_dynamic_channels_callbacks_[device]) {
        callbacks.handler_->Post(common::BindOnce(std::move(callbacks.on_fail_callback_),
                                                  DynamicChannelManager::ConnectionResult{
                                                      .hci_error = hci::ErrorCode::CONNECTION_TIMEOUT,
                                                  }));
      }
      pending_dynamic_channels_.erase(device);
      pending_dynamic_channels_callbacks_.erase(device);
    }
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

void LinkManager::OnDisconnect(hci::Address device, hci::ErrorCode status) {
  auto* link = GetLink(device);
  ASSERT_LOG(link != nullptr, "Device %s is disconnected with reason 0x%x, but not in local database",
             device.ToString().c_str(), static_cast<uint8_t>(status));
  link->OnAclDisconnected(status);
  links_.erase(device);
}

}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
