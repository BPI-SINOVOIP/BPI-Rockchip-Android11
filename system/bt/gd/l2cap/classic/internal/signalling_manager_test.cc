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

#include "l2cap/classic/internal/signalling_manager.h"

#include "l2cap/classic/internal/dynamic_channel_service_manager_impl_mock.h"
#include "l2cap/classic/internal/fixed_channel_service_manager_impl_mock.h"
#include "l2cap/classic/internal/link_mock.h"
#include "l2cap/internal/parameter_provider_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {
namespace {

class L2capClassicSignallingManagerTest : public ::testing::Test {
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

TEST_F(L2capClassicSignallingManagerTest, precondition) {}

}  // namespace
}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
