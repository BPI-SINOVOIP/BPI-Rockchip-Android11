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

#include "l2cap/le/internal/fixed_channel_service_manager_impl.h"

#include <future>

#include "common/bind.h"
#include "l2cap/cid.h"
#include "l2cap/le/fixed_channel_manager.h"
#include "l2cap/le/fixed_channel_service.h"
#include "os/handler.h"
#include "os/thread.h"

#include <gtest/gtest.h>

namespace bluetooth {
namespace l2cap {
namespace le {
namespace internal {

class L2capLeServiceManagerTest : public ::testing::Test {
 public:
  ~L2capLeServiceManagerTest() override = default;

  void OnServiceRegistered(bool expect_success, FixedChannelManager::RegistrationResult result,
                           std::unique_ptr<FixedChannelService> user_service) {
    EXPECT_EQ(result == FixedChannelManager::RegistrationResult::SUCCESS, expect_success);
    service_registered_ = expect_success;
  }

 protected:
  void SetUp() override {
    manager_ = new FixedChannelServiceManagerImpl{nullptr};
    thread_ = new os::Thread("test_thread", os::Thread::Priority::NORMAL);
    user_handler_ = new os::Handler(thread_);
  }

  void TearDown() override {
    user_handler_->Clear();
    delete user_handler_;
    delete thread_;
    delete manager_;
  }

  void sync_user_handler() {
    std::promise<void> promise;
    auto future = promise.get_future();
    user_handler_->Post(common::BindOnce(&std::promise<void>::set_value, common::Unretained(&promise)));
    future.wait_for(std::chrono::milliseconds(3));
  }

  FixedChannelServiceManagerImpl* manager_ = nullptr;
  os::Thread* thread_ = nullptr;
  os::Handler* user_handler_ = nullptr;

  bool service_registered_ = false;
};

TEST_F(L2capLeServiceManagerTest, register_and_unregister_le_fixed_channel) {
  FixedChannelServiceImpl::PendingRegistration pending_registration{
      .user_handler_ = user_handler_,
      .on_registration_complete_callback_ =
          common::BindOnce(&L2capLeServiceManagerTest::OnServiceRegistered, common::Unretained(this), true)};
  Cid cid = kSmpBrCid;
  EXPECT_FALSE(manager_->IsServiceRegistered(cid));
  manager_->Register(cid, std::move(pending_registration));
  EXPECT_TRUE(manager_->IsServiceRegistered(cid));
  sync_user_handler();
  EXPECT_TRUE(service_registered_);
  manager_->Unregister(cid, common::BindOnce([] {}), user_handler_);
  EXPECT_FALSE(manager_->IsServiceRegistered(cid));
}

TEST_F(L2capLeServiceManagerTest, register_le_fixed_channel_bad_cid) {
  FixedChannelServiceImpl::PendingRegistration pending_registration{
      .user_handler_ = user_handler_,
      .on_registration_complete_callback_ =
          common::BindOnce(&L2capLeServiceManagerTest::OnServiceRegistered, common::Unretained(this), false)};
  Cid cid = 0x1000;
  EXPECT_FALSE(manager_->IsServiceRegistered(cid));
  manager_->Register(cid, std::move(pending_registration));
  EXPECT_FALSE(manager_->IsServiceRegistered(cid));
  sync_user_handler();
  EXPECT_FALSE(service_registered_);
}

}  // namespace internal
}  // namespace le
}  // namespace l2cap
}  // namespace bluetooth
