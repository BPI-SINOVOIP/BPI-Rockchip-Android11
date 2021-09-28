/*
 * Copyright 2020 The Android Open Source Project
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

#include "l2cap/classic/internal/link.h"

#include "hci/acl_manager_mock.h"
#include "hci/address.h"
#include "l2cap/classic/internal/dynamic_channel_service_manager_impl_mock.h"
#include "l2cap/classic/internal/fixed_channel_service_manager_impl_mock.h"
#include "l2cap/internal/parameter_provider_mock.h"

#include <gmock/gmock-nice-strict.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::NiceMock;

namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {
namespace {

constexpr Psm kPsm = 123;
constexpr Cid kCid = 456;

using classic::internal::testing::MockDynamicChannelServiceManagerImpl;
using hci::testing::MockAclConnection;
using l2cap::internal::testing::MockParameterProvider;
using testing::MockFixedChannelServiceManagerImpl;

class L2capClassicLinkTest : public ::testing::Test {
 public:
  void OnOpen(std::unique_ptr<DynamicChannel> channel) {
    on_open_promise_.set_value();
  }

  void OnFail(DynamicChannelManager::ConnectionResult result) {
    on_fail_promise_.set_value();
  }

  void OnDequeueCallbackForTest() {
    std::unique_ptr<BasePacketBuilder> data = raw_acl_connection_->acl_queue_.GetDownEnd()->TryDequeue();
    if (data != nullptr) {
      dequeue_promise_.set_value();
    }
  }

  void EnqueueCallbackForTest() {
    raw_acl_connection_->acl_queue_.GetDownEnd()->RegisterDequeue(
        handler_, common::Bind(&L2capClassicLinkTest::OnDequeueCallbackForTest, common::Unretained(this)));
  }

  void DequeueCallback() {
    raw_acl_connection_->acl_queue_.GetDownEnd()->UnregisterDequeue();
  }

 protected:
  void SetUp() override {
    thread_ = new os::Thread("test_thread", os::Thread::Priority::NORMAL);
    handler_ = new os::Handler(thread_);
    signalling_handler_ = new os::Handler(thread_);

    raw_acl_connection_ = new NiceMock<MockAclConnection>();
    link_ = new Link(signalling_handler_, std::unique_ptr<MockAclConnection>(raw_acl_connection_),
                     &mock_parameter_provider_, &mock_classic_dynamic_channel_service_manager_,
                     &mock_classic_fixed_channel_service_manager_);
  }

  void TearDown() override {
    delete link_;

    signalling_handler_->Clear();
    delete signalling_handler_;

    handler_->Clear();
    delete handler_;

    delete thread_;
  }

  os::Thread* thread_ = nullptr;
  os::Handler* handler_ = nullptr;
  os::Handler* signalling_handler_ = nullptr;

  MockAclConnection* raw_acl_connection_ = nullptr;
  std::unique_ptr<MockAclConnection> acl_connection_;

  NiceMock<MockParameterProvider> mock_parameter_provider_;
  MockFixedChannelServiceManagerImpl mock_classic_fixed_channel_service_manager_;
  MockDynamicChannelServiceManagerImpl mock_classic_dynamic_channel_service_manager_;

  std::promise<void> on_open_promise_;
  std::promise<void> on_fail_promise_;
  std::promise<void> dequeue_promise_;

  Link* link_;
};

TEST_F(L2capClassicLinkTest, pending_channels_get_notified_on_acl_disconnect) {
  EnqueueCallbackForTest();

  Link::PendingDynamicChannelConnection pending_dynamic_channel_connection{
      .handler_ = handler_,
      .on_open_callback_ = common::Bind(&L2capClassicLinkTest::OnOpen, common::Unretained(this)),
      .on_fail_callback_ = common::Bind(&L2capClassicLinkTest::OnFail, common::Unretained(this)),
      .configuration_ = DynamicChannelConfigurationOption(),
  };
  auto future = on_fail_promise_.get_future();

  link_->SendConnectionRequest(kPsm, kCid, std::move(pending_dynamic_channel_connection));
  link_->OnAclDisconnected(hci::ErrorCode::UNKNOWN_HCI_COMMAND);
  future.wait();

  auto dequeue_future = dequeue_promise_.get_future();
  dequeue_future.wait();
  DequeueCallback();
}

}  // namespace
}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
