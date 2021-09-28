/*
 * Copyright 2018 The Android Open Source Project
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

#include "acl_connection_handler.h"

#include "os/log.h"

#include "hci/address.h"

using std::shared_ptr;

namespace test_vendor_lib {

using ::bluetooth::hci::Address;
using ::bluetooth::hci::AddressType;
using ::bluetooth::hci::AddressWithType;

bool AclConnectionHandler::HasHandle(uint16_t handle) const {
  if (acl_connections_.count(handle) == 0) {
    return false;
  }
  return true;
}

uint16_t AclConnectionHandler::GetUnusedHandle() {
  while (acl_connections_.count(last_handle_) == 1) {
    last_handle_ = (last_handle_ + 1) % acl::kReservedHandle;
  }
  uint16_t unused_handle = last_handle_;
  last_handle_ = (last_handle_ + 1) % acl::kReservedHandle;
  return unused_handle;
}

bool AclConnectionHandler::CreatePendingConnection(
    Address addr, bool authenticate_on_connect) {
  if (classic_connection_pending_) {
    return false;
  }
  classic_connection_pending_ = true;
  pending_connection_address_ = addr;
  authenticate_pending_classic_connection_ = authenticate_on_connect;
  return true;
}

bool AclConnectionHandler::HasPendingConnection(Address addr) const {
  return classic_connection_pending_ && pending_connection_address_ == addr;
}

bool AclConnectionHandler::AuthenticatePendingConnection() const {
  return authenticate_pending_classic_connection_;
}

bool AclConnectionHandler::CancelPendingConnection(Address addr) {
  if (!classic_connection_pending_ || pending_connection_address_ != addr) {
    return false;
  }
  classic_connection_pending_ = false;
  pending_connection_address_ = Address::kEmpty;
  return true;
}

bool AclConnectionHandler::CreatePendingLeConnection(AddressWithType addr) {
  bool device_connected = false;
  for (auto pair : acl_connections_) {
    auto connection = std::get<AclConnection>(pair);
    if (connection.GetAddress() == addr) {
      device_connected = true;
    }
  }
  if (device_connected) {
    LOG_INFO("%s: %s is already connected", __func__, addr.ToString().c_str());
    return false;
  }
  if (le_connection_pending_) {
    LOG_INFO("%s: connection already pending", __func__);
    return false;
  }
  le_connection_pending_ = true;
  pending_le_connection_address_ = addr;
  return true;
}

bool AclConnectionHandler::HasPendingLeConnection(AddressWithType addr) const {
  return le_connection_pending_ && pending_le_connection_address_ == addr;
}

bool AclConnectionHandler::CancelPendingLeConnection(AddressWithType addr) {
  if (!le_connection_pending_ || pending_le_connection_address_ != addr) {
    return false;
  }
  le_connection_pending_ = false;
  pending_le_connection_address_ =
      AddressWithType{Address::kEmpty, AddressType::PUBLIC_DEVICE_ADDRESS};
  return true;
}

uint16_t AclConnectionHandler::CreateConnection(Address addr,
                                                Address own_addr) {
  if (CancelPendingConnection(addr)) {
    uint16_t handle = GetUnusedHandle();
    acl_connections_.emplace(
        handle,
        AclConnection{
            AddressWithType{addr, AddressType::PUBLIC_DEVICE_ADDRESS},
            AddressWithType{own_addr, AddressType::PUBLIC_DEVICE_ADDRESS},
            Phy::Type::BR_EDR});
    return handle;
  }
  return acl::kReservedHandle;
}

uint16_t AclConnectionHandler::CreateLeConnection(AddressWithType addr,
                                                  AddressWithType own_addr) {
  if (CancelPendingLeConnection(addr)) {
    uint16_t handle = GetUnusedHandle();
    acl_connections_.emplace(
        handle, AclConnection{addr, own_addr, Phy::Type::LOW_ENERGY});
    return handle;
  }
  return acl::kReservedHandle;
}

bool AclConnectionHandler::Disconnect(uint16_t handle) {
  return acl_connections_.erase(handle) > 0;
}

uint16_t AclConnectionHandler::GetHandle(AddressWithType addr) const {
  for (auto pair : acl_connections_) {
    if (std::get<AclConnection>(pair).GetAddress() == addr) {
      return std::get<0>(pair);
    }
  }
  return acl::kReservedHandle;
}

uint16_t AclConnectionHandler::GetHandleOnlyAddress(
    bluetooth::hci::Address addr) const {
  for (auto pair : acl_connections_) {
    if (std::get<AclConnection>(pair).GetAddress().GetAddress() == addr) {
      return std::get<0>(pair);
    }
  }
  return acl::kReservedHandle;
}

AddressWithType AclConnectionHandler::GetAddress(uint16_t handle) const {
  ASSERT_LOG(HasHandle(handle), "Handle unknown %hd", handle);
  return acl_connections_.at(handle).GetAddress();
}

AddressWithType AclConnectionHandler::GetOwnAddress(uint16_t handle) const {
  ASSERT_LOG(HasHandle(handle), "Handle unknown %hd", handle);
  return acl_connections_.at(handle).GetOwnAddress();
}

void AclConnectionHandler::Encrypt(uint16_t handle) {
  if (!HasHandle(handle)) {
    return;
  }
  acl_connections_.at(handle).Encrypt();
}

bool AclConnectionHandler::IsEncrypted(uint16_t handle) const {
  if (!HasHandle(handle)) {
    return false;
  }
  return acl_connections_.at(handle).IsEncrypted();
}

void AclConnectionHandler::SetAddress(uint16_t handle,
                                      AddressWithType address) {
  if (!HasHandle(handle)) {
    return;
  }
  auto connection = acl_connections_.at(handle);
  connection.SetAddress(address);
}

Phy::Type AclConnectionHandler::GetPhyType(uint16_t handle) const {
  if (!HasHandle(handle)) {
    return Phy::Type::BR_EDR;
  }
  return acl_connections_.at(handle).GetPhyType();
}

}  // namespace test_vendor_lib
