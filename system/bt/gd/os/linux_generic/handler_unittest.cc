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

#include "os/handler.h"

#include <sys/eventfd.h>
#include <future>
#include <thread>

#include "common/bind.h"
#include "common/callback.h"
#include "gtest/gtest.h"
#include "os/log.h"

namespace bluetooth {
namespace os {
namespace {

class HandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    thread_ = new Thread("test_thread", Thread::Priority::NORMAL);
    handler_ = new Handler(thread_);
  }
  void TearDown() override {
    delete handler_;
    delete thread_;
  }

  Handler* handler_;
  Thread* thread_;
};

TEST_F(HandlerTest, empty) {
  handler_->Clear();
}

TEST_F(HandlerTest, post_task_invoked) {
  int val = 0;
  std::promise<void> closure_ran;
  auto future = closure_ran.get_future();
  OnceClosure closure = common::BindOnce(
      [](int* val, std::promise<void> closure_ran) {
        *val = *val + 1;
        closure_ran.set_value();
      },
      common::Unretained(&val), std::move(closure_ran));
  handler_->Post(std::move(closure));
  future.wait();
  ASSERT_EQ(val, 1);
  handler_->Clear();
}

TEST_F(HandlerTest, post_task_cleared) {
  int val = 0;
  std::promise<void> closure_started;
  auto closure_started_future = closure_started.get_future();
  std::promise<void> closure_can_continue;
  auto can_continue_future = closure_can_continue.get_future();
  handler_->Post(common::BindOnce(
      [](int* val, std::promise<void> closure_started, std::future<void> can_continue_future) {
        closure_started.set_value();
        *val = *val + 1;
        can_continue_future.wait();
      },
      common::Unretained(&val), std::move(closure_started), std::move(can_continue_future)));
  handler_->Post(common::BindOnce([]() { ASSERT_TRUE(false); }));
  closure_started_future.wait();
  handler_->Clear();
  closure_can_continue.set_value();
  ASSERT_EQ(val, 1);
}

void check_int(std::unique_ptr<int> number, std::shared_ptr<int> to_change) {
  *to_change = *number;
}

TEST_F(HandlerTest, once_callback) {
  auto number = std::make_unique<int>(1);
  auto to_change = std::make_shared<int>(0);
  auto once_callback = common::BindOnce(&check_int, std::move(number), to_change);
  std::move(once_callback).Run();
  EXPECT_EQ(*to_change, 1);
  handler_->Clear();
}

TEST_F(HandlerTest, callback_with_promise) {
  std::promise<void> promise;
  auto future = promise.get_future();
  auto once_callback = common::BindOnce(&std::promise<void>::set_value, common::Unretained(&promise));
  std::move(once_callback).Run();
  future.wait();
  handler_->Clear();
}

// For Death tests, all the threading needs to be done in the ASSERT_DEATH call
class HandlerDeathTest : public ::testing::Test {
 protected:
  void ThreadSetUp() {
    thread_ = new Thread("test_thread", Thread::Priority::NORMAL);
    handler_ = new Handler(thread_);
  }

  void ThreadTearDown() {
    delete handler_;
    delete thread_;
  }

  void ClearTwice() {
    ThreadSetUp();
    handler_->Clear();
    handler_->Clear();
    ThreadTearDown();
  }

  void NotCleared() {
    ThreadSetUp();
    ThreadTearDown();
  }

  Handler* handler_;
  Thread* thread_;
};

TEST_F(HandlerDeathTest, clear_after_handler_cleared) {
  ASSERT_DEATH(ClearTwice(), "Handlers must only be cleared once");
}

TEST_F(HandlerDeathTest, not_cleared_before_destruction) {
  ASSERT_DEATH(NotCleared(), "Handlers must be cleared");
}

}  // namespace
}  // namespace os
}  // namespace bluetooth
