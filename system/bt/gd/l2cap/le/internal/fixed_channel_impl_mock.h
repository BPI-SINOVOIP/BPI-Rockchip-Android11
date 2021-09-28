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

#include "l2cap/le/internal/fixed_channel_impl.h"

#include <gmock/gmock.h>

// Unit test interfaces
namespace bluetooth {
namespace l2cap {
namespace le {
namespace internal {
namespace testing {

class MockFixedChannelImpl : public FixedChannelImpl {
 public:
  MOCK_METHOD(void, RegisterOnCloseCallback,
              (os::Handler * user_handler, FixedChannel::OnCloseCallback on_close_callback), (override));
  MOCK_METHOD(void, Acquire, (), (override));
  MOCK_METHOD(void, Release, (), (override));
  MOCK_METHOD(bool, IsAcquired, (), (override, const));
  MOCK_METHOD(void, OnClosed, (hci::ErrorCode status), (override));
};

}  // namespace testing
}  // namespace internal
}  // namespace le
}  // namespace l2cap
}  // namespace bluetooth