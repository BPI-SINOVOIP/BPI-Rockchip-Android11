// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef IORAP_SRC_COMMON_ASYNC_POOL_H_
#define IORAP_SRC_COMMON_ASYNC_POOL_H_

#include <atomic>
#include <vector>
#include <deque>
#include <future>

#include <condition_variable>
#include <future>

namespace iorap::common {

class AsyncPool {
  std::atomic<bool> shutting_down_{false};
  std::deque<std::future<void>> futures_;

  std::mutex mutex_;
  std::condition_variable cond_var_;

 public:

  // Any threads calling 'Join' should eventually unblock
  // once all functors have run to completition.
  void Shutdown() {
    shutting_down_ = true;

    cond_var_.notify_all();
  }

  // Block forever until Shutdown is called *and* all
  // functors passed to 'LaunchAsync' have run to completition.
  void Join() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
      // Pop all items eagerly
      while (true) {
        auto it = futures_.begin();
        if (it == futures_.end()) {
          break;
        }

        std::future<void> future = std::move(*it);
        futures_.pop_front();

        lock.unlock();  // do not stall callers of LaunchAsync
        future.get();
        lock.lock();
      }

      if (shutting_down_) {
        break;
      }

      // Wait until we either get more items or ask to be shutdown.
      cond_var_.wait(lock);
    }
  }

  // Execute the functor 'u' in a new thread asynchronously.
  // Using this spawns a new thread each time to immediately begin
  // async execution.
  template <typename T>
  void LaunchAsync(T&& u) {
    auto future = std::async(std::launch::async, std::forward<T>(u));

    {
      std::unique_lock<std::mutex> lock(mutex_);
      futures_.push_back(std::move(future));
    }
    cond_var_.notify_one();
  }
};

}  // namespace iorap::common

#endif  // IORAP_SRC_COMMON_ASYNC_POOL_H_
