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
#include "l2cap/classic/internal/fixed_channel_impl.h"

#include "common/testing/bind_test_util.h"
#include "l2cap/cid.h"
#include "l2cap/classic/internal/link_mock.h"
#include "l2cap/internal/parameter_provider_mock.h"
#include "os/handler.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {

using l2cap::internal::testing::MockParameterProvider;
using ::testing::_;
using testing::MockLink;
using ::testing::Return;

class L2capClassicFixedChannelImplTest : public ::testing::Test {
 public:
  static void SyncHandler(os::Handler* handler) {
    std::promise<void> promise;
    auto future = promise.get_future();
    handler->Post(common::BindOnce(&std::promise<void>::set_value, common::Unretained(&promise)));
    future.wait_for(std::chrono::seconds(1));
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

TEST_F(L2capClassicFixedChannelImplTest, get_device) {
  MockParameterProvider mock_parameter_provider;
  EXPECT_CALL(mock_parameter_provider, GetClassicLinkIdleDisconnectTimeout())
      .WillRepeatedly(Return(std::chrono::seconds(5)));
  testing::MockAclConnection* mock_acl_connection = new testing::MockAclConnection();
  EXPECT_CALL(*mock_acl_connection, GetAddress()).Times(1);
  EXPECT_CALL(*mock_acl_connection, GetAddressType()).Times(1);
  EXPECT_CALL(*mock_acl_connection, RegisterCallbacks(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*mock_acl_connection, UnregisterCallbacks(_)).Times(1);
  MockLink mock_classic_link(l2cap_handler_, &mock_parameter_provider,
                             std::unique_ptr<testing::MockAclConnection>(mock_acl_connection));
  hci::AddressWithType device{hci::Address{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}},
                              hci::AddressType::PUBLIC_IDENTITY_ADDRESS};
  EXPECT_CALL(mock_classic_link, GetDevice()).WillRepeatedly(Return(device));
  FixedChannelImpl fixed_channel_impl(kSmpBrCid, &mock_classic_link, l2cap_handler_);
  EXPECT_EQ(device.GetAddress(), fixed_channel_impl.GetDevice());
}

TEST_F(L2capClassicFixedChannelImplTest, close_triggers_callback) {
  MockParameterProvider mock_parameter_provider;
  EXPECT_CALL(mock_parameter_provider, GetClassicLinkIdleDisconnectTimeout())
      .WillRepeatedly(Return(std::chrono::seconds(5)));
  testing::MockAclConnection* mock_acl_connection = new testing::MockAclConnection();
  EXPECT_CALL(*mock_acl_connection, GetAddress()).Times(1);
  EXPECT_CALL(*mock_acl_connection, GetAddressType()).Times(1);
  EXPECT_CALL(*mock_acl_connection, RegisterCallbacks(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*mock_acl_connection, UnregisterCallbacks(_)).Times(1);
  MockLink mock_classic_link(l2cap_handler_, &mock_parameter_provider,
                             std::unique_ptr<testing::MockAclConnection>(mock_acl_connection));
  hci::AddressWithType device{hci::Address{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}},
                              hci::AddressType::PUBLIC_IDENTITY_ADDRESS};
  EXPECT_CALL(mock_classic_link, GetDevice()).WillRepeatedly(Return(device));
  FixedChannelImpl fixed_channel_impl(kSmpBrCid, &mock_classic_link, l2cap_handler_);

  // Register on close callback
  auto user_handler = std::make_unique<os::Handler>(thread_);
  hci::ErrorCode my_status = hci::ErrorCode::SUCCESS;
  fixed_channel_impl.RegisterOnCloseCallback(
      user_handler.get(), common::testing::BindLambdaForTesting([&](hci::ErrorCode status) { my_status = status; }));

  // Channel closure should trigger such callback
  fixed_channel_impl.OnClosed(hci::ErrorCode::REMOTE_USER_TERMINATED_CONNECTION);
  SyncHandler(user_handler.get());
  EXPECT_EQ(hci::ErrorCode::REMOTE_USER_TERMINATED_CONNECTION, my_status);

  user_handler->Clear();
}

TEST_F(L2capClassicFixedChannelImplTest, register_callback_after_close_should_call_immediately) {
  MockParameterProvider mock_parameter_provider;
  EXPECT_CALL(mock_parameter_provider, GetClassicLinkIdleDisconnectTimeout())
      .WillRepeatedly(Return(std::chrono::seconds(5)));
  testing::MockAclConnection* mock_acl_connection = new testing::MockAclConnection();
  EXPECT_CALL(*mock_acl_connection, GetAddress()).Times(1);
  EXPECT_CALL(*mock_acl_connection, GetAddressType()).Times(1);
  EXPECT_CALL(*mock_acl_connection, RegisterCallbacks(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*mock_acl_connection, UnregisterCallbacks(_)).Times(1);
  MockLink mock_classic_link(l2cap_handler_, &mock_parameter_provider,
                             std::unique_ptr<testing::MockAclConnection>(mock_acl_connection));
  hci::AddressWithType device{hci::Address{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}},
                              hci::AddressType::PUBLIC_IDENTITY_ADDRESS};
  EXPECT_CALL(mock_classic_link, GetDevice()).WillRepeatedly(Return(device));
  FixedChannelImpl fixed_channel_impl(kSmpBrCid, &mock_classic_link, l2cap_handler_);

  // Channel closure should do nothing
  fixed_channel_impl.OnClosed(hci::ErrorCode::REMOTE_USER_TERMINATED_CONNECTION);

  // Register on close callback should trigger callback immediately
  auto user_handler = std::make_unique<os::Handler>(thread_);
  hci::ErrorCode my_status = hci::ErrorCode::SUCCESS;
  fixed_channel_impl.RegisterOnCloseCallback(
      user_handler.get(), common::testing::BindLambdaForTesting([&](hci::ErrorCode status) { my_status = status; }));
  SyncHandler(user_handler.get());
  EXPECT_EQ(hci::ErrorCode::REMOTE_USER_TERMINATED_CONNECTION, my_status);

  user_handler->Clear();
}

TEST_F(L2capClassicFixedChannelImplTest, close_twice_should_fail) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  MockParameterProvider mock_parameter_provider;
  EXPECT_CALL(mock_parameter_provider, GetClassicLinkIdleDisconnectTimeout())
      .WillRepeatedly(Return(std::chrono::seconds(5)));
  testing::MockAclConnection* mock_acl_connection = new testing::MockAclConnection();
  EXPECT_CALL(*mock_acl_connection, GetAddress()).Times(1);
  EXPECT_CALL(*mock_acl_connection, GetAddressType()).Times(1);
  EXPECT_CALL(*mock_acl_connection, RegisterCallbacks(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*mock_acl_connection, UnregisterCallbacks(_)).Times(1);
  MockLink mock_classic_link(l2cap_handler_, &mock_parameter_provider,
                             std::unique_ptr<testing::MockAclConnection>(mock_acl_connection));
  hci::AddressWithType device{hci::Address{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}},
                              hci::AddressType::PUBLIC_IDENTITY_ADDRESS};
  EXPECT_CALL(mock_classic_link, GetDevice()).WillRepeatedly(Return(device));
  FixedChannelImpl fixed_channel_impl(kSmpBrCid, &mock_classic_link, l2cap_handler_);

  // Register on close callback
  auto user_handler = std::make_unique<os::Handler>(thread_);
  hci::ErrorCode my_status = hci::ErrorCode::SUCCESS;
  fixed_channel_impl.RegisterOnCloseCallback(
      user_handler.get(), common::testing::BindLambdaForTesting([&](hci::ErrorCode status) { my_status = status; }));

  // Channel closure should trigger such callback
  fixed_channel_impl.OnClosed(hci::ErrorCode::REMOTE_USER_TERMINATED_CONNECTION);
  SyncHandler(user_handler.get());
  EXPECT_EQ(hci::ErrorCode::REMOTE_USER_TERMINATED_CONNECTION, my_status);

  // 2nd OnClose() callback should fail
  EXPECT_DEATH(fixed_channel_impl.OnClosed(hci::ErrorCode::PAGE_TIMEOUT), ".*assertion \'!closed_\' failed.*");

  user_handler->Clear();
}

TEST_F(L2capClassicFixedChannelImplTest, multiple_registration_should_fail) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  MockParameterProvider mock_parameter_provider;
  EXPECT_CALL(mock_parameter_provider, GetClassicLinkIdleDisconnectTimeout())
      .WillRepeatedly(Return(std::chrono::seconds(5)));
  testing::MockAclConnection* mock_acl_connection = new testing::MockAclConnection();
  EXPECT_CALL(*mock_acl_connection, GetAddress()).Times(1);
  EXPECT_CALL(*mock_acl_connection, GetAddressType()).Times(1);
  EXPECT_CALL(*mock_acl_connection, RegisterCallbacks(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*mock_acl_connection, UnregisterCallbacks(_)).Times(1);
  MockLink mock_classic_link(l2cap_handler_, &mock_parameter_provider,
                             std::unique_ptr<testing::MockAclConnection>(mock_acl_connection));
  hci::AddressWithType device{hci::Address{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}},
                              hci::AddressType::PUBLIC_IDENTITY_ADDRESS};
  EXPECT_CALL(mock_classic_link, GetDevice()).WillRepeatedly(Return(device));
  FixedChannelImpl fixed_channel_impl(kSmpBrCid, &mock_classic_link, l2cap_handler_);

  // Register on close callback
  auto user_handler = std::make_unique<os::Handler>(thread_);
  hci::ErrorCode my_status = hci::ErrorCode::SUCCESS;
  fixed_channel_impl.RegisterOnCloseCallback(
      user_handler.get(), common::testing::BindLambdaForTesting([&](hci::ErrorCode status) { my_status = status; }));

  EXPECT_DEATH(fixed_channel_impl.RegisterOnCloseCallback(user_handler.get(),
                                                          common::BindOnce([](hci::ErrorCode status) { FAIL(); })),
               ".*OnCloseCallback can only be registered once.*");

  user_handler->Clear();
}

TEST_F(L2capClassicFixedChannelImplTest, call_acquire_before_registration_should_fail) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  MockParameterProvider mock_parameter_provider;
  EXPECT_CALL(mock_parameter_provider, GetClassicLinkIdleDisconnectTimeout())
      .WillRepeatedly(Return(std::chrono::seconds(5)));
  testing::MockAclConnection* mock_acl_connection = new testing::MockAclConnection();
  EXPECT_CALL(*mock_acl_connection, GetAddress()).Times(1);
  EXPECT_CALL(*mock_acl_connection, GetAddressType()).Times(1);
  EXPECT_CALL(*mock_acl_connection, RegisterCallbacks(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*mock_acl_connection, UnregisterCallbacks(_)).Times(1);
  MockLink mock_classic_link(l2cap_handler_, &mock_parameter_provider,
                             std::unique_ptr<testing::MockAclConnection>(mock_acl_connection));
  hci::AddressWithType device{hci::Address{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}},
                              hci::AddressType::PUBLIC_IDENTITY_ADDRESS};
  EXPECT_CALL(mock_classic_link, GetDevice()).WillRepeatedly(Return(device));
  FixedChannelImpl fixed_channel_impl(kSmpBrCid, &mock_classic_link, l2cap_handler_);
  EXPECT_DEATH(fixed_channel_impl.Acquire(), ".*Must register OnCloseCallback before calling any methods.*");
}

TEST_F(L2capClassicFixedChannelImplTest, call_release_before_registration_should_fail) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  MockParameterProvider mock_parameter_provider;
  EXPECT_CALL(mock_parameter_provider, GetClassicLinkIdleDisconnectTimeout())
      .WillRepeatedly(Return(std::chrono::seconds(5)));
  testing::MockAclConnection* mock_acl_connection = new testing::MockAclConnection();
  EXPECT_CALL(*mock_acl_connection, GetAddress()).Times(1);
  EXPECT_CALL(*mock_acl_connection, GetAddressType()).Times(1);
  EXPECT_CALL(*mock_acl_connection, RegisterCallbacks(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*mock_acl_connection, UnregisterCallbacks(_)).Times(1);
  MockLink mock_classic_link(l2cap_handler_, &mock_parameter_provider,
                             std::unique_ptr<testing::MockAclConnection>(mock_acl_connection));
  hci::AddressWithType device{hci::Address{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}},
                              hci::AddressType::PUBLIC_IDENTITY_ADDRESS};
  EXPECT_CALL(mock_classic_link, GetDevice()).WillRepeatedly(Return(device));
  FixedChannelImpl fixed_channel_impl(kSmpBrCid, &mock_classic_link, l2cap_handler_);
  EXPECT_DEATH(fixed_channel_impl.Release(), ".*Must register OnCloseCallback before calling any methods.*");
}

TEST_F(L2capClassicFixedChannelImplTest, test_acquire_release_channel) {
  MockParameterProvider mock_parameter_provider;
  EXPECT_CALL(mock_parameter_provider, GetClassicLinkIdleDisconnectTimeout())
      .WillRepeatedly(Return(std::chrono::seconds(5)));
  EXPECT_CALL(mock_parameter_provider, GetClassicLinkIdleDisconnectTimeout())
      .WillRepeatedly(Return(std::chrono::seconds(5)));
  testing::MockAclConnection* mock_acl_connection = new testing::MockAclConnection();
  EXPECT_CALL(*mock_acl_connection, GetAddress()).Times(1);
  EXPECT_CALL(*mock_acl_connection, GetAddressType()).Times(1);
  EXPECT_CALL(*mock_acl_connection, RegisterCallbacks(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*mock_acl_connection, UnregisterCallbacks(_)).Times(1);
  MockLink mock_classic_link(l2cap_handler_, &mock_parameter_provider,
                             std::unique_ptr<testing::MockAclConnection>(mock_acl_connection));
  hci::AddressWithType device{hci::Address{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}},
                              hci::AddressType::PUBLIC_IDENTITY_ADDRESS};
  EXPECT_CALL(mock_classic_link, GetDevice()).WillRepeatedly(Return(device));
  FixedChannelImpl fixed_channel_impl(kSmpBrCid, &mock_classic_link, l2cap_handler_);

  // Register on close callback
  auto user_handler = std::make_unique<os::Handler>(thread_);
  hci::ErrorCode my_status = hci::ErrorCode::SUCCESS;
  fixed_channel_impl.RegisterOnCloseCallback(
      user_handler.get(),
      common::testing::BindLambdaForTesting([&my_status](hci::ErrorCode status) { my_status = status; }));

  // Default should be false
  EXPECT_FALSE(fixed_channel_impl.IsAcquired());

  // Should be called 2 times after Acquire() and Release()
  EXPECT_CALL(mock_classic_link, RefreshRefCount()).Times(2);

  fixed_channel_impl.Acquire();
  EXPECT_TRUE(fixed_channel_impl.IsAcquired());

  fixed_channel_impl.Release();
  EXPECT_FALSE(fixed_channel_impl.IsAcquired());

  user_handler->Clear();
}

TEST_F(L2capClassicFixedChannelImplTest, test_acquire_after_close) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  MockParameterProvider mock_parameter_provider;
  EXPECT_CALL(mock_parameter_provider, GetClassicLinkIdleDisconnectTimeout())
      .WillRepeatedly(Return(std::chrono::seconds(5)));
  testing::MockAclConnection* mock_acl_connection = new testing::MockAclConnection();
  EXPECT_CALL(*mock_acl_connection, GetAddress()).Times(1);
  EXPECT_CALL(*mock_acl_connection, GetAddressType()).Times(1);
  EXPECT_CALL(*mock_acl_connection, RegisterCallbacks(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*mock_acl_connection, UnregisterCallbacks(_)).Times(1);
  MockLink mock_classic_link(l2cap_handler_, &mock_parameter_provider,
                             std::unique_ptr<testing::MockAclConnection>(mock_acl_connection));
  hci::AddressWithType device{hci::Address{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}},
                              hci::AddressType::PUBLIC_IDENTITY_ADDRESS};
  EXPECT_CALL(mock_classic_link, GetDevice()).WillRepeatedly(Return(device));
  FixedChannelImpl fixed_channel_impl(kSmpBrCid, &mock_classic_link, l2cap_handler_);

  // Register on close callback
  auto user_handler = std::make_unique<os::Handler>(thread_);
  hci::ErrorCode my_status = hci::ErrorCode::SUCCESS;
  std::promise<void> promise;
  auto future = promise.get_future();
  fixed_channel_impl.RegisterOnCloseCallback(user_handler.get(),
                                             common::testing::BindLambdaForTesting([&](hci::ErrorCode status) {
                                               my_status = status;
                                               promise.set_value();
                                             }));

  // Channel closure should trigger such callback
  fixed_channel_impl.OnClosed(hci::ErrorCode::REMOTE_USER_TERMINATED_CONNECTION);
  SyncHandler(user_handler.get());
  auto future_status = future.wait_for(std::chrono::seconds(1));
  EXPECT_EQ(future_status, std::future_status::ready);
  EXPECT_EQ(hci::ErrorCode::REMOTE_USER_TERMINATED_CONNECTION, my_status);

  // Release or Acquire after closing should crash
  EXPECT_CALL(mock_classic_link, RefreshRefCount()).Times(0);
  EXPECT_FALSE(fixed_channel_impl.IsAcquired());
  EXPECT_DEATH(fixed_channel_impl.Acquire(), ".*Must register OnCloseCallback before calling any methods.*");

  user_handler->Clear();
}

}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
