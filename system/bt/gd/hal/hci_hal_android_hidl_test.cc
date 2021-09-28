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

#include "hal/hci_hal.h"

#include <chrono>
#include <future>

#include <gtest/gtest.h>

#include "os/thread.h"

using ::bluetooth::os::Thread;

namespace bluetooth {
namespace hal {
namespace {

class HciHalHidlTest : public ::testing::Test {
 protected:
  void SetUp() override {
    thread_ = new Thread("test_thread", Thread::Priority::NORMAL);
  }

  void TearDown() override {
    delete thread_;
  }

  ModuleRegistry fake_registry_;
  Thread* thread_;
};

TEST_F(HciHalHidlTest, init_and_close) {
  fake_registry_.Start<HciHal>(thread_);
  fake_registry_.StopAll();
}
}  // namespace
}  // namespace hal
}  // namespace bluetooth
