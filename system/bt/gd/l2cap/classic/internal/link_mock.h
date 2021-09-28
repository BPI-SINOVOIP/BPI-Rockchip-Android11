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
#include "hci/address.h"
#include "l2cap/classic/internal/link.h"
#include "l2cap/internal/scheduler_mock.h"

#include <gmock/gmock.h>

// Unit test interfaces
namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {
namespace testing {

using hci::testing::MockAclConnection;

class MockLink : public Link {
 public:
  explicit MockLink(os::Handler* handler, l2cap::internal::ParameterProvider* parameter_provider)
      : Link(handler, std::make_unique<MockAclConnection>(), parameter_provider, nullptr, nullptr){};
  explicit MockLink(os::Handler* handler, l2cap::internal::ParameterProvider* parameter_provider,
                    std::unique_ptr<hci::AclConnection> acl_connection)
      : Link(handler, std::move(acl_connection), parameter_provider, nullptr, nullptr){};
  MOCK_METHOD(hci::AddressWithType, GetDevice, (), (override));
  MOCK_METHOD(void, OnAclDisconnected, (hci::ErrorCode status), (override));
  MOCK_METHOD(void, Disconnect, (), (override));
  MOCK_METHOD(std::shared_ptr<l2cap::internal::DynamicChannelImpl>, AllocateDynamicChannel,
              (Psm psm, Cid cid, SecurityPolicy security_policy), (override));
  MOCK_METHOD(bool, IsFixedChannelAllocated, (Cid cid), (override));
  MOCK_METHOD(void, RefreshRefCount, (), (override));
};

}  // namespace testing
}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth