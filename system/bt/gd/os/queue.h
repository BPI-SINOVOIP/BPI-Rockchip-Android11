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

#include <unistd.h>
#include <functional>
#include <mutex>
#include <queue>

#include "common/bind.h"
#include "common/callback.h"
#include "os/handler.h"
#ifdef OS_LINUX_GENERIC
#include "os/linux_generic/reactive_semaphore.h"
#endif
#include "os/log.h"

namespace bluetooth {
namespace os {

// See documentation for |Queue|
template <typename T>
class IQueueEnqueue {
 public:
  using EnqueueCallback = Callback<std::unique_ptr<T>()>;
  virtual ~IQueueEnqueue() = default;
  virtual void RegisterEnqueue(Handler* handler, EnqueueCallback callback) = 0;
  virtual void UnregisterEnqueue() = 0;
};

// See documentation for |Queue|
template <typename T>
class IQueueDequeue {
 public:
  using DequeueCallback = Callback<void()>;
  virtual ~IQueueDequeue() = default;
  virtual void RegisterDequeue(Handler* handler, DequeueCallback callback) = 0;
  virtual void UnregisterDequeue() = 0;
  virtual std::unique_ptr<T> TryDequeue() = 0;
};

template <typename T>
class Queue : public IQueueEnqueue<T>, public IQueueDequeue<T> {
 public:
  // A function moving data from enqueue end buffer to queue, it will be continually be invoked until queue
  // is full. Enqueue end should make sure buffer isn't empty and UnregisterEnqueue when buffer become empty.
  using EnqueueCallback = Callback<std::unique_ptr<T>()>;
  // A function moving data form queue to dequeue end buffer, it will be continually be invoked until queue
  // is empty. TryDequeue should be use in this function to get data from queue.
  using DequeueCallback = Callback<void()>;
  // Create a queue with |capacity| is the maximum number of messages a queue can contain
  explicit Queue(size_t capacity);
  ~Queue();
  // Register |callback| that will be called on |handler| when the queue is able to enqueue one piece of data.
  // This will cause a crash if handler or callback has already been registered before.
  void RegisterEnqueue(Handler* handler, EnqueueCallback callback) override;
  // Unregister current EnqueueCallback from this queue, this will cause a crash if not registered yet.
  void UnregisterEnqueue() override;
  // Register |callback| that will be called on |handler| when the queue has at least one piece of data ready
  // for dequeue. This will cause a crash if handler or callback has already been registered before.
  void RegisterDequeue(Handler* handler, DequeueCallback callback) override;
  // Unregister current DequeueCallback from this queue, this will cause a crash if not registered yet.
  void UnregisterDequeue() override;

  // Try to dequeue an item from this queue. Return nullptr when there is nothing in the queue.
  std::unique_ptr<T> TryDequeue() override;

 private:
  void EnqueueCallbackInternal(EnqueueCallback callback);
  // An internal queue that holds at most |capacity| pieces of data
  std::queue<std::unique_ptr<T>> queue_;
  // A mutex that guards data in this queue
  std::mutex mutex_;

  class QueueEndpoint {
   public:
#ifdef OS_LINUX_GENERIC
    explicit QueueEndpoint(unsigned int initial_value)
        : reactive_semaphore_(initial_value), handler_(nullptr), reactable_(nullptr) {}
    ReactiveSemaphore reactive_semaphore_;
#endif
    Handler* handler_;
    Reactor::Reactable* reactable_;
  };

  QueueEndpoint enqueue_;
  QueueEndpoint dequeue_;
};

template <typename T>
class EnqueueBuffer {
 public:
  EnqueueBuffer(IQueueEnqueue<T>* queue) : queue_(queue) {}

  void Enqueue(std::unique_ptr<T> t, os::Handler* handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.push(std::move(t));
    if (buffer_.size() == 1) {
      queue_->RegisterEnqueue(handler, common::Bind(&EnqueueBuffer<T>::enqueue_callback, common::Unretained(this)));
    }
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!buffer_.empty()) {
      queue_->UnregisterEnqueue();
      std::queue<std::unique_ptr<T>> empty;
      std::swap(buffer_, empty);
    }
  }

 private:
  std::unique_ptr<T> enqueue_callback() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unique_ptr<T> enqueued_t = std::move(buffer_.front());
    buffer_.pop();
    if (buffer_.empty()) {
      queue_->UnregisterEnqueue();
    }
    return enqueued_t;
  }

  mutable std::mutex mutex_;
  IQueueEnqueue<T>* queue_;
  std::queue<std::unique_ptr<T>> buffer_;
};

#ifdef OS_LINUX_GENERIC
#include "os/linux_generic/queue.tpp"
#endif

}  // namespace os
}  // namespace bluetooth
