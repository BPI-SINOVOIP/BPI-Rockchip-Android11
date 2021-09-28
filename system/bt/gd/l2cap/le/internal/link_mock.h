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

#include "hci/acl_manager_mock.h"
#include "hci/address_with_type.h"
#include "l2cap/internal/scheduler_mock.h"
#include "l2cap/le/internal/link.h"

#include <gmock/gmock.h>

// Unit test interfaces
namespace bluetooth {
namespace l2cap {
namespace le {
namespace internal {
namespace testing {

using hci::testing::MockAclConnection;

class MockLink : public Link {
 public:
  explicit MockLink(os::Handler* handler, l2cap::internal::ParameterProvider* parameter_provider,
                    std::unique_ptr<MockAclConnection> mock_acl_connection)
      : Link(handler, std::move(mock_acl_connection), parameter_provider, nullptr, nullptr){};

  MOCK_METHOD(hci::AddressWithType, GetDevice, (), (override));
  MOCK_METHOD(hci::Role, GetRole, (), (override));
  MOCK_METHOD(void, OnAclDisconnected, (hci::ErrorCode status), (override));
  MOCK_METHOD(void, Disconnect, (), (override));
  MOCK_METHOD(std::shared_ptr<FixedChannelImpl>, AllocateFixedChannel, (Cid cid, SecurityPolicy security_policy),
              (override));
  MOCK_METHOD(bool, IsFixedChannelAllocated, (Cid cid), (override));
  MOCK_METHOD(void, RefreshRefCount, (), (override));
};

}  // namespace testing
}  // namespace internal
}  // namespace le
}  // namespace l2cap
}  // namespace bluetooth
