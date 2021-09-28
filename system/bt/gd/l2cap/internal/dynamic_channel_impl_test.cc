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

#include "l2cap/internal/dynamic_channel_impl.h"

#include "common/testing/bind_test_util.h"
#include "l2cap/cid.h"
#include "l2cap/classic/internal/link_mock.h"
#include "l2cap/internal/parameter_provider_mock.h"
#include "os/handler.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace bluetooth {
namespace l2cap {
namespace internal {

using classic::internal::testing::MockLink;
using l2cap::internal::testing::MockParameterProvider;
using ::testing::Return;

class L2capClassicDynamicChannelImplTest : public ::testing::Test {
 public:
  static void SyncHandler(os::Handler* handler) {
    std::promise<void> promise;
    auto future = promise.get_future();
    handler->Post(common::BindOnce(&std::promise<void>::set_value, common::Unretained(&promise)));
    future.wait_for(std::chrono::milliseconds(3));
  }

 protected:
  void SetUp() override {
    thread_ = new os::Thread("test_thread", os::Thread::Priority::NORMAL);
    l2cap_handler_ = new os::Handler(thread_);
  }

  void TearDown() override {
    l2cap_handler_->Clear();
    delete l2cap_handler_;
    delete thread_;
  }

  os::Thread* thread_ = nullptr;
  os::Handler* l2cap_handler_ = nullptr;
};

TEST_F(L2capClassicDynamicChannelImplTest, get_device) {
  MockParameterProvider mock_parameter_provider;
  MockLink mock_classic_link(l2cap_handler_, &mock_parameter_provider);
  const hci::AddressWithType device{{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}}, hci::AddressType::PUBLIC_IDENTITY_ADDRESS};
  EXPECT_CALL(mock_classic_link, GetDevice()).WillRepeatedly(Return(device));
  DynamicChannelImpl dynamic_channel_impl(0x01, kFirstDynamicChannel, kFirstDynamicChannel, &mock_classic_link,
                                          l2cap_handler_);
  EXPECT_EQ(device.GetAddress(), dynamic_channel_impl.GetDevice());
}

TEST_F(L2capClassicDynamicChannelImplTest, close_triggers_callback) {
  MockParameterProvider mock_parameter_provider;
  MockLink mock_classic_link(l2cap_handler_, &mock_parameter_provider);
  const hci::AddressWithType device{{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}}, hci::AddressType::PUBLIC_IDENTITY_ADDRESS};
  EXPECT_CALL(mock_classic_link, GetDevice()).WillRepeatedly(Return(device));
  DynamicChannelImpl dynamic_channel_impl(0x01, kFirstDynamicChannel, kFirstDynamicChannel, &mock_classic_link,
                                          l2cap_handler_);

  // Register on close callback
  auto user_handler = std::make_unique<os::Handler>(thread_);
  hci::ErrorCode my_status = hci::ErrorCode::SUCCESS;
  dynamic_channel_impl.RegisterOnCloseCallback(
      user_handler.get(), common::testing::BindLambdaForTesting([&](hci::ErrorCode status) { my_status = status; }));

  // Channel closure should trigger such callback
  dynamic_channel_impl.OnClosed(hci::ErrorCode::REMOTE_USER_TERMINATED_CONNECTION);
  SyncHandler(user_handler.get());
  EXPECT_EQ(hci::ErrorCode::REMOTE_USER_TERMINATED_CONNECTION, my_status);

  user_handler->Clear();
}

TEST_F(L2capClassicDynamicChannelImplTest, register_callback_after_close_should_call_immediately) {
  MockParameterProvider mock_parameter_provider;
  MockLink mock_classic_link(l2cap_handler_, &mock_parameter_provider);
  const hci::AddressWithType device{{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}}, hci::AddressType::PUBLIC_IDENTITY_ADDRESS};
  EXPECT_CALL(mock_classic_link, GetDevice()).WillRepeatedly(Return(device));
  DynamicChannelImpl dynamic_channel_impl(0x01, kFirstDynamicChannel, kFirstDynamicChannel, &mock_classic_link,
                                          l2cap_handler_);

  // Channel closure should do nothing
  dynamic_channel_impl.OnClosed(hci::ErrorCode::REMOTE_USER_TERMINATED_CONNECTION);

  // Register on close callback should trigger callback immediately
  auto user_handler = std::make_unique<os::Handler>(thread_);
  hci::ErrorCode my_status = hci::ErrorCode::SUCCESS;
  dynamic_channel_impl.RegisterOnCloseCallback(
      user_handler.get(), common::testing::BindLambdaForTesting([&](hci::ErrorCode status) { my_status = status; }));
  SyncHandler(user_handler.get());
  EXPECT_EQ(hci::ErrorCode::REMOTE_USER_TERMINATED_CONNECTION, my_status);

  user_handler->Clear();
}

TEST_F(L2capClassicDynamicChannelImplTest, close_twice_should_fail) {
  MockParameterProvider mock_parameter_provider;
  MockLink mock_classic_link(l2cap_handler_, &mock_parameter_provider);
  const hci::AddressWithType device{{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}}, hci::AddressType::PUBLIC_IDENTITY_ADDRESS};
  EXPECT_CALL(mock_classic_link, GetDevice()).WillRepeatedly(Return(device));
  DynamicChannelImpl dynamic_channel_impl(0x01, kFirstDynamicChannel, kFirstDynamicChannel, &mock_classic_link,
                                          l2cap_handler_);

  // Register on close callback
  auto user_handler = std::make_unique<os::Handler>(thread_);
  hci::ErrorCode my_status = hci::ErrorCode::SUCCESS;
  dynamic_channel_impl.RegisterOnCloseCallback(
      user_handler.get(), common::testing::BindLambdaForTesting([&](hci::ErrorCode status) { my_status = status; }));

  // Channel closure should trigger such callback
  dynamic_channel_impl.OnClosed(hci::ErrorCode::REMOTE_USER_TERMINATED_CONNECTION);
  SyncHandler(user_handler.get());
  EXPECT_EQ(hci::ErrorCode::REMOTE_USER_TERMINATED_CONNECTION, my_status);

  // 2nd OnClose() callback should fail
  EXPECT_DEATH(dynamic_channel_impl.OnClosed(hci::ErrorCode::PAGE_TIMEOUT), ".*OnClosed.*");

  user_handler->Clear();
}

TEST_F(L2capClassicDynamicChannelImplTest, multiple_registeration_should_fail) {
  MockParameterProvider mock_parameter_provider;
  MockLink mock_classic_link(l2cap_handler_, &mock_parameter_provider);
  const hci::AddressWithType device{{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}}, hci::AddressType::PUBLIC_IDENTITY_ADDRESS};
  EXPECT_CALL(mock_classic_link, GetDevice()).WillRepeatedly(Return(device));
  DynamicChannelImpl dynamic_channel_impl(0x01, kFirstDynamicChannel, kFirstDynamicChannel, &mock_classic_link,
                                          l2cap_handler_);

  // Register on close callback
  auto user_handler = std::make_unique<os::Handler>(thread_);
  hci::ErrorCode my_status = hci::ErrorCode::SUCCESS;
  dynamic_channel_impl.RegisterOnCloseCallback(
      user_handler.get(), common::testing::BindLambdaForTesting([&](hci::ErrorCode status) { my_status = status; }));

  EXPECT_DEATH(dynamic_channel_impl.RegisterOnCloseCallback(user_handler.get(),
                                                            common::BindOnce([](hci::ErrorCode status) { FAIL(); })),
               ".*RegisterOnCloseCallback.*");

  user_handler->Clear();
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
