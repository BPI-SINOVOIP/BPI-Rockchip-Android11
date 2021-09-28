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

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>

namespace bluetooth {
namespace common {

template <typename T>
class BlockingQueue {
 public:
  void push(T data) {
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.push(std::move(data));
    if (queue_.size() == 1) {
      not_empty_.notify_all();
    }
  };

  T take() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (queue_.empty()) {
      not_empty_.wait(lock);
    }
    T data = queue_.front();
    queue_.pop();
    return data;
  };

  // Returns true if take() will not block within a time period
  bool wait_to_take(std::chrono::milliseconds time) {
    std::unique_lock<std::mutex> lock(mutex_);
    while (queue_.empty()) {
      if (not_empty_.wait_for(lock, time) == std::cv_status::timeout) {
        return false;
      }
    }
    return true;
  }

  bool empty() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return queue_.empty();
  };

  void clear() {
    std::unique_lock<std::mutex> lock(mutex_);
    std::queue<T> empty;
    std::swap(queue_, empty);
  };

 private:
  std::queue<T> queue_;
  mutable std::mutex mutex_;
  std::condition_variable not_empty_;
};

}  // namespace common
}  // namespace bluetooth
