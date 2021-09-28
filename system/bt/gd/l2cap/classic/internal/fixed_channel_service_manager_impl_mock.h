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

#include "l2cap/classic/internal/fixed_channel_impl.h"
#include "l2cap/classic/internal/fixed_channel_service_manager_impl.h"

#include <gmock/gmock.h>

// Unit test interfaces
namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {
namespace testing {

class MockFixedChannelServiceManagerImpl : public FixedChannelServiceManagerImpl {
 public:
  MockFixedChannelServiceManagerImpl() : FixedChannelServiceManagerImpl(nullptr) {}
  MOCK_METHOD(void, Register, (Cid cid, FixedChannelServiceImpl::PendingRegistration pending_registration), (override));
  MOCK_METHOD(void, Unregister, (Cid cid, FixedChannelService::OnUnregisteredCallback callback, os::Handler* handler),
              (override));
  MOCK_METHOD(bool, IsServiceRegistered, (Cid cid), (const, override));
  MOCK_METHOD(FixedChannelServiceImpl*, GetService, (Cid cid), (override));
  MOCK_METHOD((std::vector<std::pair<Cid, FixedChannelServiceImpl*>>), GetRegisteredServices, (), (override));
};

}  // namespace testing
}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth