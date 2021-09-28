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

#include "os/alarm.h"

#include <future>

#include "common/bind.h"
#include "gtest/gtest.h"

namespace bluetooth {
namespace os {
namespace {

using common::BindOnce;

class AlarmTest : public ::testing::Test {
 protected:
  void SetUp() override {
    thread_ = new Thread("test_thread", Thread::Priority::NORMAL);
    handler_ = new Handler(thread_);
    alarm_ = new Alarm(handler_);
  }

  void TearDown() override {
    delete alarm_;
    handler_->Clear();
    delete handler_;
    delete thread_;
  }
  Alarm* alarm_;

 private:
  Handler* handler_;
  Thread* thread_;
};

TEST_F(AlarmTest, cancel_while_not_armed) {
  alarm_->Cancel();
}

TEST_F(AlarmTest, schedule) {
  std::promise<void> promise;
  auto future = promise.get_future();
  auto before = std::chrono::steady_clock::now();
  int delay_ms = 10;
  int delay_error_ms = 3;
  alarm_->Schedule(BindOnce(&std::promise<void>::set_value, common::Unretained(&promise)),
                   std::chrono::milliseconds(delay_ms));
  future.get();
  auto after = std::chrono::steady_clock::now();
  auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(after - before);
  ASSERT_NEAR(duration_ms.count(), delay_ms, delay_error_ms);
}

TEST_F(AlarmTest, cancel_alarm) {
  alarm_->Schedule(BindOnce([]() { ASSERT_TRUE(false) << "Should not happen"; }), std::chrono::milliseconds(3));
  alarm_->Cancel();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

TEST_F(AlarmTest, cancel_alarm_from_callback) {
  alarm_->Schedule(BindOnce(&Alarm::Cancel, common::Unretained(alarm_)), std::chrono::milliseconds(1));
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

TEST_F(AlarmTest, schedule_while_alarm_armed) {
  alarm_->Schedule(BindOnce([]() { ASSERT_TRUE(false) << "Should not happen"; }), std::chrono::milliseconds(1));
  std::promise<void> promise;
  auto future = promise.get_future();
  alarm_->Schedule(BindOnce(&std::promise<void>::set_value, common::Unretained(&promise)),
                   std::chrono::milliseconds(10));
  future.get();
}

TEST_F(AlarmTest, delete_while_alarm_armed) {
  alarm_->Schedule(BindOnce([]() { ASSERT_TRUE(false) << "Should not happen"; }), std::chrono::milliseconds(1));
  delete alarm_;
  alarm_ = nullptr;
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

}  // namespace
}  // namespace os
}  // namespace bluetooth
