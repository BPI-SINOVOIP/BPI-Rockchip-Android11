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

#include "hci/acl_manager.h"

#include <gmock/gmock.h>

// Unit test interfaces
namespace bluetooth {
namespace hci {
namespace testing {

class MockAclConnection : public AclConnection {
 public:
  MOCK_METHOD(Address, GetAddress, (), (const, override));
  MOCK_METHOD(AddressType, GetAddressType, (), (const, override));
  MOCK_METHOD(void, RegisterDisconnectCallback,
              (common::OnceCallback<void(ErrorCode)> on_disconnect, os::Handler* handler), (override));
  MOCK_METHOD(bool, Disconnect, (DisconnectReason reason), (override));
  MOCK_METHOD(void, Finish, (), (override));
  MOCK_METHOD(void, RegisterCallbacks, (ConnectionManagementCallbacks * callbacks, os::Handler* handler), (override));
  MOCK_METHOD(void, UnregisterCallbacks, (ConnectionManagementCallbacks * callbacks), (override));

  QueueUpEnd* GetAclQueueEnd() const override {
    return acl_queue_.GetUpEnd();
  }
  mutable common::BidiQueue<PacketView<kLittleEndian>, BasePacketBuilder> acl_queue_{10};
};

class MockAclManager : public AclManager {
 public:
  MOCK_METHOD(void, RegisterCallbacks, (ConnectionCallbacks * callbacks, os::Handler* handler), (override));
  MOCK_METHOD(void, RegisterLeCallbacks, (LeConnectionCallbacks * callbacks, os::Handler* handler), (override));
  MOCK_METHOD(void, CreateConnection, (Address address), (override));
  MOCK_METHOD(void, CreateLeConnection, (AddressWithType address_with_type), (override));
  MOCK_METHOD(void, CancelConnect, (Address address), (override));
};

}  // namespace testing
}  // namespace hci
}  // namespace bluetooth
