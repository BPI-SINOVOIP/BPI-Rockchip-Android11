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

#include "stack_manager.h"

#include <chrono>
#include <future>
#include <queue>

#include "common/bind.h"
#include "hal/hci_hal.h"
#include "module.h"
#include "os/handler.h"
#include "os/log.h"
#include "os/thread.h"

using ::bluetooth::os::Handler;
using ::bluetooth::os::Thread;

namespace bluetooth {

void StackManager::StartUp(ModuleList* modules, Thread* stack_thread) {
  management_thread_ = new Thread("management_thread", Thread::Priority::NORMAL);
  handler_ = new Handler(management_thread_);

  std::promise<void> promise;
  auto future = promise.get_future();
  handler_->Post(common::BindOnce(&StackManager::handle_start_up, common::Unretained(this), modules, stack_thread,
                                  std::move(promise)));

  auto init_status = future.wait_for(std::chrono::seconds(3));
  ASSERT_LOG(init_status == std::future_status::ready, "Can't start stack");

  LOG_INFO("init complete");
}

void StackManager::handle_start_up(ModuleList* modules, Thread* stack_thread, std::promise<void> promise) {
  registry_.Start(modules, stack_thread);
  promise.set_value();
}

void StackManager::ShutDown() {
  std::promise<void> promise;
  auto future = promise.get_future();
  handler_->Post(common::BindOnce(&StackManager::handle_shut_down, common::Unretained(this), std::move(promise)));

  auto stop_status = future.wait_for(std::chrono::seconds(3));
  ASSERT_LOG(stop_status == std::future_status::ready, "Can't stop stack");

  handler_->Clear();
  handler_->WaitUntilStopped(std::chrono::milliseconds(20));
  delete handler_;
  delete management_thread_;
}

void StackManager::handle_shut_down(std::promise<void> promise) {
  registry_.StopAll();
  promise.set_value();
}

}  // namespace bluetooth
