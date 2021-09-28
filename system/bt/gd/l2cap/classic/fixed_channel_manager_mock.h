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

#include "l2cap/classic/fixed_channel_manager.h"

#include <gmock/gmock.h>

// Unit test interfaces
namespace bluetooth {
namespace l2cap {
namespace classic {
namespace testing {

class MockFixedChannelManager : public FixedChannelManager {
 public:
  MockFixedChannelManager() : FixedChannelManager(nullptr, nullptr, nullptr){};
  MOCK_METHOD(bool, ConnectServices,
              (hci::Address device, OnConnectionFailureCallback on_fail_callback, os::Handler* handler), (override));
  MOCK_METHOD(bool, RegisterService,
              (Cid cid, const SecurityPolicy& security_policy, OnRegistrationCompleteCallback on_registration_complete,
               OnConnectionOpenCallback on_connection_open, os::Handler* handler),
              (override));
};

}  // namespace testing
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
