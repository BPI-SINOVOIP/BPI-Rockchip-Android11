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

#include <future>

#include <gtest/gtest.h>

#include "common/bind.h"
#include "l2cap/cid.h"
#include "l2cap/classic/dynamic_channel_manager.h"
#include "l2cap/classic/dynamic_channel_service.h"
#include "l2cap/classic/internal/dynamic_channel_service_manager_impl.h"
#include "os/handler.h"
#include "os/thread.h"

namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {
namespace {

class L2capDynamicServiceManagerTest : public ::testing::Test {
 public:
  ~L2capDynamicServiceManagerTest() override = default;

  void OnServiceRegistered(bool expect_success, DynamicChannelManager::RegistrationResult result,
                           std::unique_ptr<DynamicChannelService> user_service) {
    EXPECT_EQ(result == DynamicChannelManager::RegistrationResult::SUCCESS, expect_success);
    service_registered_ = expect_success;
  }

 protected:
  void SetUp() override {
    thread_ = new os::Thread("test_thread", os::Thread::Priority::NORMAL);
    user_handler_ = new os::Handler(thread_);
    l2cap_handler_ = new os::Handler(thread_);
    manager_ = new DynamicChannelServiceManagerImpl{l2cap_handler_};
  }

  void TearDown() override {
    delete manager_;
    l2cap_handler_->Clear();
    delete l2cap_handler_;
    user_handler_->Clear();
    delete user_handler_;
    delete thread_;
  }

  void sync_user_handler() {
    std::promise<void> promise;
    auto future = promise.get_future();
    user_handler_->Post(common::BindOnce(&std::promise<void>::set_value, common::Unretained(&promise)));
    auto future_status = future.wait_for(std::chrono::seconds(1));
    EXPECT_EQ(future_status, std::future_status::ready);
  }

  DynamicChannelServiceManagerImpl* manager_ = nullptr;
  os::Thread* thread_ = nullptr;
  os::Handler* user_handler_ = nullptr;
  os::Handler* l2cap_handler_ = nullptr;

  bool service_registered_ = false;
};

TEST_F(L2capDynamicServiceManagerTest, register_and_unregister_classic_dynamic_channel) {
  DynamicChannelServiceImpl::PendingRegistration pending_registration{
      .user_handler_ = user_handler_,
      .on_registration_complete_callback_ =
          common::BindOnce(&L2capDynamicServiceManagerTest::OnServiceRegistered, common::Unretained(this), true)};
  Cid cid = kSmpBrCid;
  EXPECT_FALSE(manager_->IsServiceRegistered(cid));
  manager_->Register(cid, std::move(pending_registration));
  EXPECT_TRUE(manager_->IsServiceRegistered(cid));
  sync_user_handler();
  EXPECT_TRUE(service_registered_);
  manager_->Unregister(cid, common::BindOnce([] {}), user_handler_);
  EXPECT_FALSE(manager_->IsServiceRegistered(cid));
}

TEST_F(L2capDynamicServiceManagerTest, register_classic_dynamic_channel_bad_cid) {
  DynamicChannelServiceImpl::PendingRegistration pending_registration{
      .user_handler_ = user_handler_,
      .on_registration_complete_callback_ =
          common::BindOnce(&L2capDynamicServiceManagerTest::OnServiceRegistered, common::Unretained(this), false)};
  Cid cid = 0x1000;
  EXPECT_FALSE(manager_->IsServiceRegistered(cid));
  manager_->Register(cid, std::move(pending_registration));
  EXPECT_FALSE(manager_->IsServiceRegistered(cid));
  sync_user_handler();
  EXPECT_FALSE(service_registered_);
}

}  // namespace
}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
