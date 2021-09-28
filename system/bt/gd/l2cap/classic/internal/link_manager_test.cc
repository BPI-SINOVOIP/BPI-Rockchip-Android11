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

#include "l2cap/classic/internal/link_manager.h"

#include <future>
#include <thread>

#include "common/bind.h"
#include "common/testing/bind_test_util.h"
#include "dynamic_channel_service_manager_impl_mock.h"
#include "hci/acl_manager_mock.h"
#include "hci/address.h"
#include "l2cap/cid.h"
#include "l2cap/classic/fixed_channel_manager.h"
#include "l2cap/classic/internal/fixed_channel_service_impl_mock.h"
#include "l2cap/classic/internal/fixed_channel_service_manager_impl_mock.h"
#include "l2cap/internal/parameter_provider_mock.h"
#include "os/handler.h"
#include "os/thread.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {
namespace {

using hci::testing::MockAclConnection;
using hci::testing::MockAclManager;
using l2cap::internal::testing::MockParameterProvider;
using ::testing::_;  // Matcher to any value
using ::testing::ByMove;
using ::testing::DoAll;
using testing::MockDynamicChannelServiceManagerImpl;
using testing::MockFixedChannelServiceImpl;
using testing::MockFixedChannelServiceManagerImpl;
using ::testing::Return;
using ::testing::SaveArg;

constexpr static auto kTestIdleDisconnectTimeoutLong = std::chrono::milliseconds(1000);
constexpr static auto kTestIdleDisconnectTimeoutShort = std::chrono::milliseconds(1000);

class L2capClassicLinkManagerTest : public ::testing::Test {
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
    mock_parameter_provider_ = new MockParameterProvider;
    EXPECT_CALL(*mock_parameter_provider_, GetClassicLinkIdleDisconnectTimeout)
        .WillRepeatedly(Return(kTestIdleDisconnectTimeoutLong));
  }

  void TearDown() override {
    delete mock_parameter_provider_;
    l2cap_handler_->Clear();
    delete l2cap_handler_;
    delete thread_;
  }

  os::Thread* thread_ = nullptr;
  os::Handler* l2cap_handler_ = nullptr;
  MockParameterProvider* mock_parameter_provider_ = nullptr;
};

TEST_F(L2capClassicLinkManagerTest, connect_fixed_channel_service_without_acl) {
  MockFixedChannelServiceManagerImpl mock_classic_fixed_channel_service_manager;
  MockDynamicChannelServiceManagerImpl mock_classic_dynamic_channel_service_manager;
  MockAclManager mock_acl_manager;
  hci::Address device{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
  auto user_handler = std::make_unique<os::Handler>(thread_);

  // Step 1: Verify callback registration with HCI
  hci::ConnectionCallbacks* hci_connection_callbacks = nullptr;
  os::Handler* hci_callback_handler = nullptr;
  EXPECT_CALL(mock_acl_manager, RegisterCallbacks(_, _))
      .WillOnce(DoAll(SaveArg<0>(&hci_connection_callbacks), SaveArg<1>(&hci_callback_handler)));
  LinkManager classic_link_manager(l2cap_handler_, &mock_acl_manager, &mock_classic_fixed_channel_service_manager,
                                   &mock_classic_dynamic_channel_service_manager, mock_parameter_provider_);
  EXPECT_EQ(hci_connection_callbacks, &classic_link_manager);
  EXPECT_EQ(hci_callback_handler, l2cap_handler_);

  // Register fake services
  MockFixedChannelServiceImpl mock_service_1, mock_service_2;
  std::vector<std::pair<Cid, FixedChannelServiceImpl*>> results;
  results.emplace_back(kSmpBrCid, &mock_service_1);
  results.emplace_back(kConnectionlessCid, &mock_service_2);
  EXPECT_CALL(mock_classic_fixed_channel_service_manager, GetRegisteredServices()).WillRepeatedly(Return(results));

  // Step 2: Connect to fixed channel without ACL connection should trigger ACL connection process
  EXPECT_CALL(mock_acl_manager, CreateConnection(device)).Times(1);
  LinkManager::PendingFixedChannelConnection pending_fixed_channel_connection{
      .handler_ = user_handler.get(),
      .on_fail_callback_ = common::BindOnce([](FixedChannelManager::ConnectionResult result) { FAIL(); })};
  classic_link_manager.ConnectFixedChannelServices(device, std::move(pending_fixed_channel_connection));

  // Step 3: ACL connection success event should trigger channel creation for all registered services

  std::unique_ptr<MockAclConnection> acl_connection = std::make_unique<MockAclConnection>();
  EXPECT_CALL(*acl_connection, GetAddress()).WillRepeatedly(Return(device));
  EXPECT_CALL(*acl_connection, GetAddressType()).WillRepeatedly(Return(hci::AddressType::PUBLIC_DEVICE_ADDRESS));
  EXPECT_CALL(*acl_connection, RegisterCallbacks(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*acl_connection, RegisterDisconnectCallback(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*acl_connection, UnregisterCallbacks(_)).Times(1);
  std::unique_ptr<FixedChannel> channel_1, channel_2;
  std::promise<void> promise_1, promise_2;
  auto future_1 = promise_1.get_future();
  auto future_2 = promise_2.get_future();
  EXPECT_CALL(mock_service_1, NotifyChannelCreation(_))
      .WillOnce([&channel_1, &promise_1](std::unique_ptr<FixedChannel> channel) {
        channel_1 = std::move(channel);
        promise_1.set_value();
      });
  EXPECT_CALL(mock_service_2, NotifyChannelCreation(_))
      .WillOnce([&channel_2, &promise_2](std::unique_ptr<FixedChannel> channel) {
        channel_2 = std::move(channel);
        promise_2.set_value();
      });
  hci_callback_handler->Post(common::BindOnce(&hci::ConnectionCallbacks::OnConnectSuccess,
                                              common::Unretained(hci_connection_callbacks), std::move(acl_connection)));
  SyncHandler(hci_callback_handler);
  auto future_1_status = future_1.wait_for(kTestIdleDisconnectTimeoutShort);
  EXPECT_EQ(future_1_status, std::future_status::ready);
  EXPECT_NE(channel_1, nullptr);
  auto future_2_status = future_2.wait_for(kTestIdleDisconnectTimeoutShort);
  EXPECT_EQ(future_2_status, std::future_status::ready);
  EXPECT_NE(channel_2, nullptr);

  // Step 4: Calling ConnectServices() to the same device will no trigger another connection attempt
  FixedChannelManager::ConnectionResult my_result;
  LinkManager::PendingFixedChannelConnection pending_fixed_channel_connection_2{
      .handler_ = user_handler.get(),
      .on_fail_callback_ = common::testing::BindLambdaForTesting(
          [&my_result](FixedChannelManager::ConnectionResult result) { my_result = result; })};
  classic_link_manager.ConnectFixedChannelServices(device, std::move(pending_fixed_channel_connection_2));
  SyncHandler(user_handler.get());
  EXPECT_EQ(my_result.connection_result_code,
            FixedChannelManager::ConnectionResultCode::FAIL_ALL_SERVICES_HAVE_CHANNEL);

  // Step 5: Register new service will cause new channels to be created during ConnectServices()
  MockFixedChannelServiceImpl mock_service_3;
  results.emplace_back(kSmpBrCid + 1, &mock_service_3);
  EXPECT_CALL(mock_classic_fixed_channel_service_manager, GetRegisteredServices()).WillRepeatedly(Return(results));
  LinkManager::PendingFixedChannelConnection pending_fixed_channel_connection_3{
      .handler_ = user_handler.get(),
      .on_fail_callback_ = common::BindOnce([](FixedChannelManager::ConnectionResult result) { FAIL(); })};
  std::unique_ptr<FixedChannel> channel_3;
  EXPECT_CALL(mock_service_3, NotifyChannelCreation(_)).WillOnce([&channel_3](std::unique_ptr<FixedChannel> channel) {
    channel_3 = std::move(channel);
  });
  classic_link_manager.ConnectFixedChannelServices(device, std::move(pending_fixed_channel_connection_3));
  EXPECT_NE(channel_3, nullptr);

  user_handler->Clear();

  classic_link_manager.OnDisconnect(device, hci::ErrorCode::SUCCESS);
}

TEST_F(L2capClassicLinkManagerTest, connect_fixed_channel_service_without_acl_with_no_service) {
  MockFixedChannelServiceManagerImpl mock_classic_fixed_channel_service_manager;
  MockAclManager mock_acl_manager;
  hci::Address device{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
  auto user_handler = std::make_unique<os::Handler>(thread_);

  // Step 1: Verify callback registration with HCI
  hci::ConnectionCallbacks* hci_connection_callbacks = nullptr;
  os::Handler* hci_callback_handler = nullptr;
  EXPECT_CALL(mock_acl_manager, RegisterCallbacks(_, _))
      .WillOnce(DoAll(SaveArg<0>(&hci_connection_callbacks), SaveArg<1>(&hci_callback_handler)));
  LinkManager classic_link_manager(l2cap_handler_, &mock_acl_manager, &mock_classic_fixed_channel_service_manager,
                                   nullptr, mock_parameter_provider_);
  EXPECT_EQ(hci_connection_callbacks, &classic_link_manager);
  EXPECT_EQ(hci_callback_handler, l2cap_handler_);

  // Make sure no service is registered
  std::vector<std::pair<Cid, FixedChannelServiceImpl*>> results;
  EXPECT_CALL(mock_classic_fixed_channel_service_manager, GetRegisteredServices()).WillRepeatedly(Return(results));

  // Step 2: Connect to fixed channel without any service registered will result in failure
  EXPECT_CALL(mock_acl_manager, CreateConnection(device)).Times(0);
  FixedChannelManager::ConnectionResult my_result;
  LinkManager::PendingFixedChannelConnection pending_fixed_channel_connection{
      .handler_ = user_handler.get(),
      .on_fail_callback_ = common::testing::BindLambdaForTesting(
          [&my_result](FixedChannelManager::ConnectionResult result) { my_result = result; })};
  classic_link_manager.ConnectFixedChannelServices(device, std::move(pending_fixed_channel_connection));
  SyncHandler(user_handler.get());
  EXPECT_EQ(my_result.connection_result_code, FixedChannelManager::ConnectionResultCode::FAIL_NO_SERVICE_REGISTERED);

  user_handler->Clear();
}

TEST_F(L2capClassicLinkManagerTest, connect_fixed_channel_service_without_acl_with_hci_failure) {
  MockFixedChannelServiceManagerImpl mock_classic_fixed_channel_service_manager;
  MockAclManager mock_acl_manager;
  hci::Address device{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
  auto user_handler = std::make_unique<os::Handler>(thread_);

  // Step 1: Verify callback registration with HCI
  hci::ConnectionCallbacks* hci_connection_callbacks = nullptr;
  os::Handler* hci_callback_handler = nullptr;
  EXPECT_CALL(mock_acl_manager, RegisterCallbacks(_, _))
      .WillOnce(DoAll(SaveArg<0>(&hci_connection_callbacks), SaveArg<1>(&hci_callback_handler)));
  LinkManager classic_link_manager(l2cap_handler_, &mock_acl_manager, &mock_classic_fixed_channel_service_manager,
                                   nullptr, mock_parameter_provider_);
  EXPECT_EQ(hci_connection_callbacks, &classic_link_manager);
  EXPECT_EQ(hci_callback_handler, l2cap_handler_);

  // Register fake services
  MockFixedChannelServiceImpl mock_service_1;
  std::vector<std::pair<Cid, FixedChannelServiceImpl*>> results;
  results.emplace_back(kSmpBrCid, &mock_service_1);
  EXPECT_CALL(mock_classic_fixed_channel_service_manager, GetRegisteredServices()).WillRepeatedly(Return(results));

  // Step 2: Connect to fixed channel without ACL connection should trigger ACL connection process
  EXPECT_CALL(mock_acl_manager, CreateConnection(device)).Times(1);
  FixedChannelManager::ConnectionResult my_result;
  LinkManager::PendingFixedChannelConnection pending_fixed_channel_connection{
      .handler_ = user_handler.get(),
      .on_fail_callback_ = common::testing::BindLambdaForTesting(
          [&my_result](FixedChannelManager::ConnectionResult result) { my_result = result; })};
  classic_link_manager.ConnectFixedChannelServices(device, std::move(pending_fixed_channel_connection));

  // Step 3: ACL connection failure event should trigger connection failure callback
  EXPECT_CALL(mock_service_1, NotifyChannelCreation(_)).Times(0);
  hci_callback_handler->Post(common::BindOnce(&hci::ConnectionCallbacks::OnConnectFail,
                                              common::Unretained(hci_connection_callbacks), device,
                                              hci::ErrorCode::PAGE_TIMEOUT));
  SyncHandler(hci_callback_handler);
  SyncHandler(user_handler.get());
  EXPECT_EQ(my_result.connection_result_code, FixedChannelManager::ConnectionResultCode::FAIL_HCI_ERROR);
  EXPECT_EQ(my_result.hci_error, hci::ErrorCode::PAGE_TIMEOUT);

  user_handler->Clear();
}

TEST_F(L2capClassicLinkManagerTest, not_acquiring_channels_should_disconnect_acl_after_timeout) {
  EXPECT_CALL(*mock_parameter_provider_, GetClassicLinkIdleDisconnectTimeout)
      .WillRepeatedly(Return(kTestIdleDisconnectTimeoutShort));
  MockFixedChannelServiceManagerImpl mock_classic_fixed_channel_service_manager;
  MockAclManager mock_acl_manager;
  hci::Address device{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
  auto user_handler = std::make_unique<os::Handler>(thread_);

  // Step 1: Verify callback registration with HCI
  hci::ConnectionCallbacks* hci_connection_callbacks = nullptr;
  os::Handler* hci_callback_handler = nullptr;
  EXPECT_CALL(mock_acl_manager, RegisterCallbacks(_, _))
      .WillOnce(DoAll(SaveArg<0>(&hci_connection_callbacks), SaveArg<1>(&hci_callback_handler)));
  LinkManager classic_link_manager(l2cap_handler_, &mock_acl_manager, &mock_classic_fixed_channel_service_manager,
                                   nullptr, mock_parameter_provider_);
  EXPECT_EQ(hci_connection_callbacks, &classic_link_manager);
  EXPECT_EQ(hci_callback_handler, l2cap_handler_);

  // Register fake services
  MockFixedChannelServiceImpl mock_service_1, mock_service_2;
  std::vector<std::pair<Cid, FixedChannelServiceImpl*>> results;
  results.emplace_back(kSmpBrCid, &mock_service_1);
  results.emplace_back(kConnectionlessCid, &mock_service_2);
  EXPECT_CALL(mock_classic_fixed_channel_service_manager, GetRegisteredServices()).WillRepeatedly(Return(results));

  // Step 2: Connect to fixed channel without ACL connection should trigger ACL connection process
  EXPECT_CALL(mock_acl_manager, CreateConnection(device)).Times(1);
  LinkManager::PendingFixedChannelConnection pending_fixed_channel_connection{
      .handler_ = user_handler.get(),
      .on_fail_callback_ = common::BindOnce([](FixedChannelManager::ConnectionResult result) { FAIL(); })};
  classic_link_manager.ConnectFixedChannelServices(device, std::move(pending_fixed_channel_connection));

  // Step 3: ACL connection success event should trigger channel creation for all registered services
  auto* raw_acl_connection = new MockAclConnection();
  std::unique_ptr<MockAclConnection> acl_connection(raw_acl_connection);
  EXPECT_CALL(*acl_connection, GetAddress()).WillRepeatedly(Return(device));
  EXPECT_CALL(*acl_connection, GetAddressType()).WillRepeatedly(Return(hci::AddressType::PUBLIC_DEVICE_ADDRESS));
  EXPECT_CALL(*acl_connection, RegisterCallbacks(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*acl_connection, RegisterDisconnectCallback(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*acl_connection, UnregisterCallbacks(_)).Times(1);
  std::unique_ptr<FixedChannel> channel_1, channel_2;
  std::promise<void> promise_1, promise_2;
  auto future_1 = promise_1.get_future();
  auto future_2 = promise_2.get_future();
  EXPECT_CALL(mock_service_1, NotifyChannelCreation(_))
      .WillOnce([&channel_1, &promise_1](std::unique_ptr<FixedChannel> channel) {
        channel_1 = std::move(channel);
        promise_1.set_value();
      });
  EXPECT_CALL(mock_service_2, NotifyChannelCreation(_))
      .WillOnce([&channel_2, &promise_2](std::unique_ptr<FixedChannel> channel) {
        channel_2 = std::move(channel);
        promise_2.set_value();
      });
  hci_callback_handler->Post(common::BindOnce(&hci::ConnectionCallbacks::OnConnectSuccess,
                                              common::Unretained(hci_connection_callbacks), std::move(acl_connection)));
  SyncHandler(hci_callback_handler);
  auto future_1_status = future_1.wait_for(kTestIdleDisconnectTimeoutShort);
  EXPECT_EQ(future_1_status, std::future_status::ready);
  EXPECT_NE(channel_1, nullptr);
  auto future_2_status = future_2.wait_for(kTestIdleDisconnectTimeoutShort);
  EXPECT_EQ(future_2_status, std::future_status::ready);
  EXPECT_NE(channel_2, nullptr);
  hci::ErrorCode status_1 = hci::ErrorCode::SUCCESS;
  channel_1->RegisterOnCloseCallback(
      user_handler.get(), common::testing::BindLambdaForTesting([&](hci::ErrorCode status) { status_1 = status; }));
  hci::ErrorCode status_2 = hci::ErrorCode::SUCCESS;
  channel_2->RegisterOnCloseCallback(
      user_handler.get(), common::testing::BindLambdaForTesting([&](hci::ErrorCode status) { status_2 = status; }));

  // Step 4: Leave channel IDLE long enough, they will disconnect
  EXPECT_CALL(*raw_acl_connection, Disconnect(hci::DisconnectReason::REMOTE_USER_TERMINATED_CONNECTION)).Times(1);
  std::this_thread::sleep_for(kTestIdleDisconnectTimeoutShort * 1.2);

  // Step 5: Link disconnect will trigger all callbacks
  classic_link_manager.OnDisconnect(device, hci::ErrorCode::CONNECTION_TERMINATED_BY_LOCAL_HOST);
  SyncHandler(user_handler.get());
  EXPECT_EQ(hci::ErrorCode::CONNECTION_TERMINATED_BY_LOCAL_HOST, status_1);
  EXPECT_EQ(hci::ErrorCode::CONNECTION_TERMINATED_BY_LOCAL_HOST, status_2);

  user_handler->Clear();
}

TEST_F(L2capClassicLinkManagerTest, acquiring_channels_should_not_disconnect_acl_after_timeout) {
  EXPECT_CALL(*mock_parameter_provider_, GetClassicLinkIdleDisconnectTimeout)
      .WillRepeatedly(Return(kTestIdleDisconnectTimeoutShort));
  MockFixedChannelServiceManagerImpl mock_classic_fixed_channel_service_manager;
  MockAclManager mock_acl_manager;
  hci::Address device{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
  auto user_handler = std::make_unique<os::Handler>(thread_);

  // Step 1: Verify callback registration with HCI
  hci::ConnectionCallbacks* hci_connection_callbacks = nullptr;
  os::Handler* hci_callback_handler = nullptr;
  EXPECT_CALL(mock_acl_manager, RegisterCallbacks(_, _))
      .WillOnce(DoAll(SaveArg<0>(&hci_connection_callbacks), SaveArg<1>(&hci_callback_handler)));
  LinkManager classic_link_manager(l2cap_handler_, &mock_acl_manager, &mock_classic_fixed_channel_service_manager,
                                   nullptr, mock_parameter_provider_);
  EXPECT_EQ(hci_connection_callbacks, &classic_link_manager);
  EXPECT_EQ(hci_callback_handler, l2cap_handler_);

  // Register fake services
  MockFixedChannelServiceImpl mock_service_1, mock_service_2;
  std::vector<std::pair<Cid, FixedChannelServiceImpl*>> results;
  results.emplace_back(kSmpBrCid, &mock_service_1);
  results.emplace_back(kConnectionlessCid, &mock_service_2);
  EXPECT_CALL(mock_classic_fixed_channel_service_manager, GetRegisteredServices()).WillRepeatedly(Return(results));

  // Step 2: Connect to fixed channel without ACL connection should trigger ACL connection process
  EXPECT_CALL(mock_acl_manager, CreateConnection(device)).Times(1);
  LinkManager::PendingFixedChannelConnection pending_fixed_channel_connection{
      .handler_ = user_handler.get(),
      .on_fail_callback_ = common::BindOnce([](FixedChannelManager::ConnectionResult result) { FAIL(); })};
  classic_link_manager.ConnectFixedChannelServices(device, std::move(pending_fixed_channel_connection));

  // Step 3: ACL connection success event should trigger channel creation for all registered services
  auto* raw_acl_connection = new MockAclConnection();
  std::unique_ptr<MockAclConnection> acl_connection(raw_acl_connection);
  EXPECT_CALL(*acl_connection, GetAddress()).WillRepeatedly(Return(device));
  EXPECT_CALL(*acl_connection, GetAddressType()).WillRepeatedly(Return(hci::AddressType::PUBLIC_DEVICE_ADDRESS));
  EXPECT_CALL(*acl_connection, RegisterCallbacks(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*acl_connection, RegisterDisconnectCallback(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*acl_connection, UnregisterCallbacks(_)).Times(1);
  std::unique_ptr<FixedChannel> channel_1, channel_2;
  std::promise<void> promise_1, promise_2;
  auto future_1 = promise_1.get_future();
  auto future_2 = promise_2.get_future();
  EXPECT_CALL(mock_service_1, NotifyChannelCreation(_))
      .WillOnce([&channel_1, &promise_1](std::unique_ptr<FixedChannel> channel) {
        channel_1 = std::move(channel);
        promise_1.set_value();
      });
  EXPECT_CALL(mock_service_2, NotifyChannelCreation(_))
      .WillOnce([&channel_2, &promise_2](std::unique_ptr<FixedChannel> channel) {
        channel_2 = std::move(channel);
        promise_2.set_value();
      });
  hci_callback_handler->Post(common::BindOnce(&hci::ConnectionCallbacks::OnConnectSuccess,
                                              common::Unretained(hci_connection_callbacks), std::move(acl_connection)));
  SyncHandler(hci_callback_handler);
  auto future_1_status = future_1.wait_for(kTestIdleDisconnectTimeoutShort);
  EXPECT_EQ(future_1_status, std::future_status::ready);
  EXPECT_NE(channel_1, nullptr);
  auto future_2_status = future_2.wait_for(kTestIdleDisconnectTimeoutShort);
  EXPECT_EQ(future_2_status, std::future_status::ready);
  EXPECT_NE(channel_2, nullptr);
  hci::ErrorCode status_1 = hci::ErrorCode::SUCCESS;
  channel_1->RegisterOnCloseCallback(
      user_handler.get(), common::testing::BindLambdaForTesting([&](hci::ErrorCode status) { status_1 = status; }));
  hci::ErrorCode status_2 = hci::ErrorCode::SUCCESS;
  channel_2->RegisterOnCloseCallback(
      user_handler.get(), common::testing::BindLambdaForTesting([&](hci::ErrorCode status) { status_2 = status; }));

  channel_1->Acquire();

  // Step 4: Leave channel IDLE, it won't disconnect to due acquired channel 1
  EXPECT_CALL(*raw_acl_connection, Disconnect(hci::DisconnectReason::REMOTE_USER_TERMINATED_CONNECTION)).Times(0);
  std::this_thread::sleep_for(kTestIdleDisconnectTimeoutShort * 2);

  // Step 5: Link disconnect will trigger all callbacks
  classic_link_manager.OnDisconnect(device, hci::ErrorCode::CONNECTION_TERMINATED_BY_LOCAL_HOST);
  SyncHandler(user_handler.get());
  EXPECT_EQ(hci::ErrorCode::CONNECTION_TERMINATED_BY_LOCAL_HOST, status_1);
  EXPECT_EQ(hci::ErrorCode::CONNECTION_TERMINATED_BY_LOCAL_HOST, status_2);

  user_handler->Clear();
}

TEST_F(L2capClassicLinkManagerTest, acquiring_and_releasing_channels_should_eventually_disconnect_acl) {
  EXPECT_CALL(*mock_parameter_provider_, GetClassicLinkIdleDisconnectTimeout)
      .WillRepeatedly(Return(kTestIdleDisconnectTimeoutShort));
  MockFixedChannelServiceManagerImpl mock_classic_fixed_channel_service_manager;
  MockAclManager mock_acl_manager;
  hci::Address device{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
  auto user_handler = std::make_unique<os::Handler>(thread_);

  // Step 1: Verify callback registration with HCI
  hci::ConnectionCallbacks* hci_connection_callbacks = nullptr;
  os::Handler* hci_callback_handler = nullptr;
  EXPECT_CALL(mock_acl_manager, RegisterCallbacks(_, _))
      .WillOnce(DoAll(SaveArg<0>(&hci_connection_callbacks), SaveArg<1>(&hci_callback_handler)));
  LinkManager classic_link_manager(l2cap_handler_, &mock_acl_manager, &mock_classic_fixed_channel_service_manager,
                                   nullptr, mock_parameter_provider_);
  EXPECT_EQ(hci_connection_callbacks, &classic_link_manager);
  EXPECT_EQ(hci_callback_handler, l2cap_handler_);

  // Register fake services
  MockFixedChannelServiceImpl mock_service_1, mock_service_2;
  std::vector<std::pair<Cid, FixedChannelServiceImpl*>> results;
  results.emplace_back(kSmpBrCid, &mock_service_1);
  results.emplace_back(kConnectionlessCid, &mock_service_2);
  EXPECT_CALL(mock_classic_fixed_channel_service_manager, GetRegisteredServices()).WillRepeatedly(Return(results));

  // Step 2: Connect to fixed channel without ACL connection should trigger ACL connection process
  EXPECT_CALL(mock_acl_manager, CreateConnection(device)).Times(1);
  LinkManager::PendingFixedChannelConnection pending_fixed_channel_connection{
      .handler_ = user_handler.get(),
      .on_fail_callback_ = common::BindOnce([](FixedChannelManager::ConnectionResult result) { FAIL(); })};
  classic_link_manager.ConnectFixedChannelServices(device, std::move(pending_fixed_channel_connection));

  // Step 3: ACL connection success event should trigger channel creation for all registered services
  auto* raw_acl_connection = new MockAclConnection();
  std::unique_ptr<MockAclConnection> acl_connection(raw_acl_connection);
  EXPECT_CALL(*acl_connection, GetAddress()).WillRepeatedly(Return(device));
  EXPECT_CALL(*acl_connection, GetAddressType()).WillRepeatedly(Return(hci::AddressType::PUBLIC_DEVICE_ADDRESS));
  EXPECT_CALL(*acl_connection, RegisterCallbacks(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*acl_connection, RegisterDisconnectCallback(_, l2cap_handler_)).Times(1);
  EXPECT_CALL(*acl_connection, UnregisterCallbacks(_)).Times(1);
  std::unique_ptr<FixedChannel> channel_1, channel_2;
  std::promise<void> promise_1, promise_2;
  auto future_1 = promise_1.get_future();
  auto future_2 = promise_2.get_future();
  EXPECT_CALL(mock_service_1, NotifyChannelCreation(_))
      .WillOnce([&channel_1, &promise_1](std::unique_ptr<FixedChannel> channel) {
        channel_1 = std::move(channel);
        promise_1.set_value();
      });
  EXPECT_CALL(mock_service_2, NotifyChannelCreation(_))
      .WillOnce([&channel_2, &promise_2](std::unique_ptr<FixedChannel> channel) {
        channel_2 = std::move(channel);
        promise_2.set_value();
      });
  hci_callback_handler->Post(common::BindOnce(&hci::ConnectionCallbacks::OnConnectSuccess,
                                              common::Unretained(hci_connection_callbacks), std::move(acl_connection)));
  SyncHandler(hci_callback_handler);
  auto future_1_status = future_1.wait_for(kTestIdleDisconnectTimeoutShort);
  EXPECT_EQ(future_1_status, std::future_status::ready);
  EXPECT_NE(channel_1, nullptr);
  auto future_2_status = future_2.wait_for(kTestIdleDisconnectTimeoutShort);
  EXPECT_EQ(future_2_status, std::future_status::ready);
  EXPECT_NE(channel_2, nullptr);
  hci::ErrorCode status_1 = hci::ErrorCode::SUCCESS;
  channel_1->RegisterOnCloseCallback(
      user_handler.get(), common::testing::BindLambdaForTesting([&](hci::ErrorCode status) { status_1 = status; }));
  hci::ErrorCode status_2 = hci::ErrorCode::SUCCESS;
  channel_2->RegisterOnCloseCallback(
      user_handler.get(), common::testing::BindLambdaForTesting([&](hci::ErrorCode status) { status_2 = status; }));

  channel_1->Acquire();

  // Step 4: Leave channel IDLE, it won't disconnect to due acquired channel 1
  EXPECT_CALL(*raw_acl_connection, Disconnect(hci::DisconnectReason::REMOTE_USER_TERMINATED_CONNECTION)).Times(0);
  std::this_thread::sleep_for(kTestIdleDisconnectTimeoutShort * 2);

  // Step 5: Leave channel IDLE long enough, they will disconnect
  channel_1->Release();
  EXPECT_CALL(*raw_acl_connection, Disconnect(hci::DisconnectReason::REMOTE_USER_TERMINATED_CONNECTION)).Times(1);
  std::this_thread::sleep_for(kTestIdleDisconnectTimeoutShort * 1.2);

  // Step 6: Link disconnect will trigger all callbacks
  classic_link_manager.OnDisconnect(device, hci::ErrorCode::CONNECTION_TERMINATED_BY_LOCAL_HOST);
  SyncHandler(user_handler.get());
  EXPECT_EQ(hci::ErrorCode::CONNECTION_TERMINATED_BY_LOCAL_HOST, status_1);
  EXPECT_EQ(hci::ErrorCode::CONNECTION_TERMINATED_BY_LOCAL_HOST, status_2);

  user_handler->Clear();
}

}  // namespace
}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
