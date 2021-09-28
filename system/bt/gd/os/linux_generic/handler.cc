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
#include <unistd.h>
#include <cstring>

#include "common/bind.h"
#include "common/callback.h"
#include "os/log.h"
#include "os/reactor.h"
#include "os/utils.h"

#ifndef EFD_SEMAPHORE
#define EFD_SEMAPHORE 1
#endif

namespace bluetooth {
namespace os {

Handler::Handler(Thread* thread)
    : tasks_(new std::queue<OnceClosure>()), thread_(thread), fd_(eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK)) {
  ASSERT(fd_ != -1);
  reactable_ = thread_->GetReactor()->Register(fd_, common::Bind(&Handler::handle_next_event, common::Unretained(this)),
                                               common::Closure());
}

Handler::~Handler() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ASSERT_LOG(was_cleared(), "Handlers must be cleared before they are destroyed");
  }

  int close_status;
  RUN_NO_INTR(close_status = close(fd_));
  ASSERT(close_status != -1);
}

void Handler::Post(OnceClosure closure) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (was_cleared()) {
      return;
    }
    tasks_->emplace(std::move(closure));
  }
  uint64_t val = 1;
  auto write_result = eventfd_write(fd_, val);
  ASSERT(write_result != -1);
}

void Handler::Clear() {
  std::queue<OnceClosure>* tmp = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ASSERT_LOG(!was_cleared(), "Handlers must only be cleared once");
    std::swap(tasks_, tmp);
  }
  delete tmp;

  uint64_t val;
  while (eventfd_read(fd_, &val) == 0) {
  }

  thread_->GetReactor()->Unregister(reactable_);
  reactable_ = nullptr;
}

void Handler::WaitUntilStopped(std::chrono::milliseconds timeout) {
  ASSERT(reactable_ == nullptr);
  ASSERT(thread_->GetReactor()->WaitForUnregisteredReactable(timeout));
}

void Handler::handle_next_event() {
  common::OnceClosure closure;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t val = 0;
    auto read_result = eventfd_read(fd_, &val);

    if (was_cleared()) {
      return;
    }
    ASSERT_LOG(read_result != -1, "eventfd read error %d %s", errno, strerror(errno));

    closure = std::move(tasks_->front());
    tasks_->pop();
  }
  std::move(closure).Run();
}

}  // namespace os
}  // namespace bluetooth
