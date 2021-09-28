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

#include "os/queue.h"

#include <sys/eventfd.h>
#include <future>
#include <unordered_map>

#include "common/bind.h"
#include "gtest/gtest.h"
#include "os/reactor.h"

namespace bluetooth {
namespace os {
namespace {

constexpr int kQueueSize = 10;
constexpr int kHalfOfQueueSize = kQueueSize / 2;
constexpr int kDoubleOfQueueSize = kQueueSize * 2;
constexpr int kQueueSizeOne = 1;

class QueueTest : public ::testing::Test {
 protected:
  void SetUp() override {
    enqueue_thread_ = new Thread("enqueue_thread", Thread::Priority::NORMAL);
    enqueue_handler_ = new Handler(enqueue_thread_);
    dequeue_thread_ = new Thread("dequeue_thread", Thread::Priority::NORMAL);
    dequeue_handler_ = new Handler(dequeue_thread_);
  }
  void TearDown() override {
    enqueue_handler_->Clear();
    delete enqueue_handler_;
    delete enqueue_thread_;
    dequeue_handler_->Clear();
    delete dequeue_handler_;
    delete dequeue_thread_;
    enqueue_handler_ = nullptr;
    enqueue_thread_ = nullptr;
    dequeue_handler_ = nullptr;
    dequeue_thread_ = nullptr;
  }

  Thread* enqueue_thread_;
  Handler* enqueue_handler_;
  Thread* dequeue_thread_;
  Handler* dequeue_handler_;
};

class TestEnqueueEnd {
 public:
  explicit TestEnqueueEnd(Queue<std::string>* queue, Handler* handler)
      : count(0), handler_(handler), queue_(queue), delay_(0) {}

  ~TestEnqueueEnd() {}

  void RegisterEnqueue(std::unordered_map<int, std::promise<int>>* promise_map) {
    promise_map_ = promise_map;
    handler_->Post(common::BindOnce(&TestEnqueueEnd::handle_register_enqueue, common::Unretained(this)));
  }

  void UnregisterEnqueue() {
    std::promise<void> promise;
    auto future = promise.get_future();

    handler_->Post(
        common::BindOnce(&TestEnqueueEnd::handle_unregister_enqueue, common::Unretained(this), std::move(promise)));
    future.wait();
  }

  std::unique_ptr<std::string> EnqueueCallbackForTest() {
    if (delay_ != 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_));
    }

    count++;
    std::unique_ptr<std::string> data = std::move(buffer_.front());
    buffer_.pop();
    std::string copy = *data;
    if (buffer_.empty()) {
      queue_->UnregisterEnqueue();
    }

    auto pair = promise_map_->find(buffer_.size());
    if (pair != promise_map_->end()) {
      pair->second.set_value(pair->first);
      promise_map_->erase(pair->first);
    }
    return data;
  }

  void setDelay(int value) {
    delay_ = value;
  }

  std::queue<std::unique_ptr<std::string>> buffer_;
  int count;

 private:
  Handler* handler_;
  Queue<std::string>* queue_;
  std::unordered_map<int, std::promise<int>>* promise_map_;
  int delay_;

  void handle_register_enqueue() {
    queue_->RegisterEnqueue(handler_, common::Bind(&TestEnqueueEnd::EnqueueCallbackForTest, common::Unretained(this)));
  }

  void handle_unregister_enqueue(std::promise<void> promise) {
    queue_->UnregisterEnqueue();
    promise.set_value();
  }
};

class TestDequeueEnd {
 public:
  explicit TestDequeueEnd(Queue<std::string>* queue, Handler* handler, int capacity)
      : count(0), handler_(handler), queue_(queue), capacity_(capacity), delay_(0) {}

  ~TestDequeueEnd() {}

  void RegisterDequeue(std::unordered_map<int, std::promise<int>>* promise_map) {
    promise_map_ = promise_map;
    handler_->Post(common::BindOnce(&TestDequeueEnd::handle_register_dequeue, common::Unretained(this)));
  }

  void UnregisterDequeue() {
    std::promise<void> promise;
    auto future = promise.get_future();

    handler_->Post(
        common::BindOnce(&TestDequeueEnd::handle_unregister_dequeue, common::Unretained(this), std::move(promise)));
    future.wait();
  }

  void DequeueCallbackForTest() {
    if (delay_ != 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_));
    }

    count++;
    std::unique_ptr<std::string> data = queue_->TryDequeue();
    buffer_.push(std::move(data));

    if (buffer_.size() == capacity_) {
      queue_->UnregisterDequeue();
    }

    auto pair = promise_map_->find(buffer_.size());
    if (pair != promise_map_->end()) {
      pair->second.set_value(pair->first);
      promise_map_->erase(pair->first);
    }
  }

  void setDelay(int value) {
    delay_ = value;
  }

  std::queue<std::unique_ptr<std::string>> buffer_;
  int count;

 private:
  Handler* handler_;
  Queue<std::string>* queue_;
  std::unordered_map<int, std::promise<int>>* promise_map_;
  int capacity_;
  int delay_;

  void handle_register_dequeue() {
    queue_->RegisterDequeue(handler_, common::Bind(&TestDequeueEnd::DequeueCallbackForTest, common::Unretained(this)));
  }

  void handle_unregister_dequeue(std::promise<void> promise) {
    queue_->UnregisterDequeue();
    promise.set_value();
  }
};

// Enqueue end level : 0 -> queue is full, 1 - >  queue isn't full
// Dequeue end level : 0 -> queue is empty, 1 - >  queue isn't empty

// Test 1 : Queue is empty

// Enqueue end level : 1
// Dequeue end level : 0
// Test 1-1 EnqueueCallback should continually be invoked when queue isn't full
TEST_F(QueueTest, register_enqueue_with_empty_queue) {
  Queue<std::string> queue(kQueueSize);
  TestEnqueueEnd test_enqueue_end(&queue, enqueue_handler_);

  // Push kQueueSize data to enqueue_end buffer
  for (int i = 0; i < kQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }
  EXPECT_EQ(test_enqueue_end.buffer_.size(), (size_t)kQueueSize);

  // Register enqueue and expect data move to Queue
  std::unordered_map<int, std::promise<int>> enqueue_promise_map;
  enqueue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(0), std::forward_as_tuple());
  auto enqueue_future = enqueue_promise_map[0].get_future();
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);
  enqueue_future.wait();
  EXPECT_EQ(enqueue_future.get(), 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

// Enqueue end level : 1
// Dequeue end level : 0
// Test 1-2 DequeueCallback shouldn't be invoked when queue is empty
TEST_F(QueueTest, register_dequeue_with_empty_queue) {
  Queue<std::string> queue(kQueueSize);
  TestDequeueEnd test_dequeue_end(&queue, dequeue_handler_, kQueueSize);

  // Register dequeue, DequeueCallback shouldn't be invoked
  std::unordered_map<int, std::promise<int>> dequeue_promise_map;
  test_dequeue_end.RegisterDequeue(&dequeue_promise_map);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_EQ(test_dequeue_end.count, 0);

  test_dequeue_end.UnregisterDequeue();
}

// Test 2 : Queue is full

// Enqueue end level : 0
// Dequeue end level : 1
// Test 2-1 EnqueueCallback shouldn't be invoked when queue is full
TEST_F(QueueTest, register_enqueue_with_full_queue) {
  Queue<std::string> queue(kQueueSize);
  TestEnqueueEnd test_enqueue_end(&queue, enqueue_handler_);

  // make Queue full
  for (int i = 0; i < kQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }
  std::unordered_map<int, std::promise<int>> enqueue_promise_map;
  enqueue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(0), std::forward_as_tuple());
  auto enqueue_future = enqueue_promise_map[0].get_future();
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);
  enqueue_future.wait();
  EXPECT_EQ(enqueue_future.get(), 0);

  // push some data to enqueue_end buffer and register enqueue;
  for (int i = 0; i < kHalfOfQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);

  // EnqueueCallback shouldn't be invoked
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_EQ(test_enqueue_end.buffer_.size(), (size_t)kHalfOfQueueSize);
  EXPECT_EQ(test_enqueue_end.count, kQueueSize);

  test_enqueue_end.UnregisterEnqueue();
}

// Enqueue end level : 0
// Dequeue end level : 1
// Test 2-2 DequeueCallback should continually be invoked when queue isn't empty
TEST_F(QueueTest, register_dequeue_with_full_queue) {
  Queue<std::string> queue(kQueueSize);
  TestEnqueueEnd test_enqueue_end(&queue, enqueue_handler_);
  TestDequeueEnd test_dequeue_end(&queue, dequeue_handler_, kDoubleOfQueueSize);

  // make Queue full
  for (int i = 0; i < kQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }
  std::unordered_map<int, std::promise<int>> enqueue_promise_map;
  enqueue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(0), std::forward_as_tuple());
  auto enqueue_future = enqueue_promise_map[0].get_future();
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);
  enqueue_future.wait();
  EXPECT_EQ(enqueue_future.get(), 0);

  // Register dequeue and expect data move to dequeue end buffer
  std::unordered_map<int, std::promise<int>> dequeue_promise_map;
  dequeue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(kQueueSize), std::forward_as_tuple());
  auto dequeue_future = dequeue_promise_map[kQueueSize].get_future();
  test_dequeue_end.RegisterDequeue(&dequeue_promise_map);
  dequeue_future.wait();
  EXPECT_EQ(dequeue_future.get(), kQueueSize);

  test_dequeue_end.UnregisterDequeue();
}

// Test 3 : Queue is non-empty and non-full

// Enqueue end level : 1
// Dequeue end level : 1
// Test 3-1 Register enqueue with half empty queue, EnqueueCallback should continually be invoked
TEST_F(QueueTest, register_enqueue_with_half_empty_queue) {
  Queue<std::string> queue(kQueueSize);
  TestEnqueueEnd test_enqueue_end(&queue, enqueue_handler_);

  // make Queue half empty
  for (int i = 0; i < kHalfOfQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }
  std::unordered_map<int, std::promise<int>> enqueue_promise_map;
  enqueue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(0), std::forward_as_tuple());
  auto enqueue_future = enqueue_promise_map[0].get_future();
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);
  enqueue_future.wait();
  EXPECT_EQ(enqueue_future.get(), 0);

  // push some data to enqueue_end buffer and register enqueue;
  for (int i = 0; i < kHalfOfQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }

  // Register enqueue and expect data move to Queue
  enqueue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(0), std::forward_as_tuple());
  enqueue_future = enqueue_promise_map[0].get_future();
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);
  enqueue_future.wait();
  EXPECT_EQ(enqueue_future.get(), 0);
}

// Enqueue end level : 1
// Dequeue end level : 1
// Test 3-2 Register dequeue with half empty queue, DequeueCallback should continually be invoked
TEST_F(QueueTest, register_dequeue_with_half_empty_queue) {
  Queue<std::string> queue(kQueueSize);
  TestEnqueueEnd test_enqueue_end(&queue, enqueue_handler_);
  TestDequeueEnd test_dequeue_end(&queue, dequeue_handler_, kQueueSize);

  // make Queue half empty
  for (int i = 0; i < kHalfOfQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }
  std::unordered_map<int, std::promise<int>> enqueue_promise_map;
  enqueue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(0), std::forward_as_tuple());
  auto enqueue_future = enqueue_promise_map[0].get_future();
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);
  enqueue_future.wait();
  EXPECT_EQ(enqueue_future.get(), 0);

  // Register dequeue and expect data move to dequeue end buffer
  std::unordered_map<int, std::promise<int>> dequeue_promise_map;
  dequeue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(kHalfOfQueueSize),
                              std::forward_as_tuple());
  auto dequeue_future = dequeue_promise_map[kHalfOfQueueSize].get_future();
  test_dequeue_end.RegisterDequeue(&dequeue_promise_map);
  dequeue_future.wait();
  EXPECT_EQ(dequeue_future.get(), kHalfOfQueueSize);

  test_dequeue_end.UnregisterDequeue();
}

// Dynamic level test

// Test 4 : Queue becomes full during test, EnqueueCallback should stop to be invoked

// Enqueue end level : 1 -> 0
// Dequeue end level : 1
// Test 4-1 Queue becomes full due to only register EnqueueCallback
TEST_F(QueueTest, queue_becomes_full_enqueue_callback_only) {
  Queue<std::string> queue(kQueueSize);
  TestEnqueueEnd test_enqueue_end(&queue, enqueue_handler_);

  // push double of kQueueSize to enqueue end buffer
  for (int i = 0; i < kDoubleOfQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }

  // Register enqueue and expect kQueueSize data move to Queue
  std::unordered_map<int, std::promise<int>> enqueue_promise_map;
  enqueue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(kQueueSize), std::forward_as_tuple());
  auto enqueue_future = enqueue_promise_map[kQueueSize].get_future();
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);
  enqueue_future.wait();
  EXPECT_EQ(enqueue_future.get(), kQueueSize);

  // EnqueueCallback shouldn't be invoked and buffer size stay in kQueueSize
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_EQ(test_enqueue_end.buffer_.size(), (size_t)kQueueSize);
  EXPECT_EQ(test_enqueue_end.count, kQueueSize);

  test_enqueue_end.UnregisterEnqueue();
}

// Enqueue end level : 1 -> 0
// Dequeue end level : 1
// Test 4-2 Queue becomes full due to DequeueCallback unregister during test
TEST_F(QueueTest, queue_becomes_full_dequeue_callback_unregister) {
  Queue<std::string> queue(kQueueSize);
  TestEnqueueEnd test_enqueue_end(&queue, enqueue_handler_);
  TestDequeueEnd test_dequeue_end(&queue, dequeue_handler_, kHalfOfQueueSize);

  // push double of kQueueSize to enqueue end buffer
  for (int i = 0; i < kDoubleOfQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }

  // Register dequeue
  std::unordered_map<int, std::promise<int>> dequeue_promise_map;
  dequeue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(kHalfOfQueueSize),
                              std::forward_as_tuple());
  auto dequeue_future = dequeue_promise_map[kHalfOfQueueSize].get_future();
  test_dequeue_end.RegisterDequeue(&dequeue_promise_map);

  // Register enqueue
  std::unordered_map<int, std::promise<int>> enqueue_promise_map;
  enqueue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(kHalfOfQueueSize),
                              std::forward_as_tuple());
  auto enqueue_future = enqueue_promise_map[kHalfOfQueueSize].get_future();
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);

  // Dequeue end will unregister when buffer size is kHalfOfQueueSize
  dequeue_future.wait();
  EXPECT_EQ(dequeue_future.get(), kHalfOfQueueSize);

  // EnqueueCallback shouldn't be invoked and buffer size stay in kHalfOfQueueSize
  enqueue_future.wait();
  EXPECT_EQ(enqueue_future.get(), kHalfOfQueueSize);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_EQ(test_enqueue_end.buffer_.size(), (size_t)kHalfOfQueueSize);
  EXPECT_EQ(test_enqueue_end.count, kQueueSize + kHalfOfQueueSize);

  test_enqueue_end.UnregisterEnqueue();
}

// Enqueue end level : 1 -> 0
// Dequeue end level : 1
// Test 4-3 Queue becomes full due to DequeueCallback is slower
TEST_F(QueueTest, queue_becomes_full_dequeue_callback_slower) {
  Queue<std::string> queue(kQueueSize);
  TestEnqueueEnd test_enqueue_end(&queue, enqueue_handler_);
  TestDequeueEnd test_dequeue_end(&queue, dequeue_handler_, kDoubleOfQueueSize);

  // push double of kDoubleOfQueueSize to enqueue end buffer
  for (int i = 0; i < kDoubleOfQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }

  // Set 20 ms delay for callback and register dequeue
  std::unordered_map<int, std::promise<int>> dequeue_promise_map;
  test_dequeue_end.setDelay(20);
  auto dequeue_future = dequeue_promise_map[kHalfOfQueueSize].get_future();
  test_dequeue_end.RegisterDequeue(&dequeue_promise_map);

  // Register enqueue
  std::unordered_map<int, std::promise<int>> enqueue_promise_map;
  enqueue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(0), std::forward_as_tuple());
  auto enqueue_future = enqueue_promise_map[0].get_future();
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);

  // Wait for enqueue buffer empty and expect queue is full
  enqueue_future.wait();
  EXPECT_EQ(enqueue_future.get(), 0);
  EXPECT_GE(test_dequeue_end.buffer_.size(), kQueueSize - 1);

  test_dequeue_end.UnregisterDequeue();
}

// Enqueue end level : 0 -> 1
// Dequeue end level : 1 -> 0
// Test 5 Queue becomes full and non empty at same time.
TEST_F(QueueTest, queue_becomes_full_and_non_empty_at_same_time) {
  Queue<std::string> queue(kQueueSizeOne);
  TestEnqueueEnd test_enqueue_end(&queue, enqueue_handler_);
  TestDequeueEnd test_dequeue_end(&queue, dequeue_handler_, kDoubleOfQueueSize);

  // push double of kQueueSize to enqueue end buffer
  for (int i = 0; i < kQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }

  // Register dequeue
  std::unordered_map<int, std::promise<int>> dequeue_promise_map;
  dequeue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(kQueueSize), std::forward_as_tuple());
  auto dequeue_future = dequeue_promise_map[kQueueSize].get_future();
  test_dequeue_end.RegisterDequeue(&dequeue_promise_map);

  // Register enqueue
  std::unordered_map<int, std::promise<int>> enqueue_promise_map;
  auto enqueue_future = enqueue_promise_map[0].get_future();
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);

  // Wait for all data move from enqueue end buffer to dequeue end buffer
  dequeue_future.wait();
  EXPECT_EQ(dequeue_future.get(), kQueueSize);

  test_dequeue_end.UnregisterDequeue();
}

// Enqueue end level : 1 -> 0
// Dequeue end level : 1
// Test 6 Queue becomes not full during test, EnqueueCallback should start to be invoked
TEST_F(QueueTest, queue_becomes_non_full_during_test) {
  Queue<std::string> queue(kQueueSize);
  TestEnqueueEnd test_enqueue_end(&queue, enqueue_handler_);
  TestDequeueEnd test_dequeue_end(&queue, dequeue_handler_, kQueueSize * 3);

  // make Queue full
  for (int i = 0; i < kDoubleOfQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }
  std::unordered_map<int, std::promise<int>> enqueue_promise_map;
  enqueue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(kQueueSize), std::forward_as_tuple());
  enqueue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(0), std::forward_as_tuple());
  auto enqueue_future = enqueue_promise_map[kQueueSize].get_future();
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);
  enqueue_future.wait();
  EXPECT_EQ(enqueue_future.get(), kQueueSize);

  // Expect kQueueSize data block in enqueue end buffer
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_EQ(test_enqueue_end.buffer_.size(), kQueueSize);

  // Register dequeue
  std::unordered_map<int, std::promise<int>> dequeue_promise_map;
  test_dequeue_end.RegisterDequeue(&dequeue_promise_map);

  // Expect enqueue end will empty
  enqueue_future = enqueue_promise_map[0].get_future();
  enqueue_future.wait();
  EXPECT_EQ(enqueue_future.get(), 0);

  test_dequeue_end.UnregisterDequeue();
}

// Enqueue end level : 0 -> 1
// Dequeue end level : 1 -> 0
// Test 7 Queue becomes non full and empty at same time. (Exactly same as Test 5)
TEST_F(QueueTest, queue_becomes_non_full_and_empty_at_same_time) {
  Queue<std::string> queue(kQueueSizeOne);
  TestEnqueueEnd test_enqueue_end(&queue, enqueue_handler_);
  TestDequeueEnd test_dequeue_end(&queue, dequeue_handler_, kDoubleOfQueueSize);

  // push double of kQueueSize to enqueue end buffer
  for (int i = 0; i < kQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }

  // Register dequeue
  std::unordered_map<int, std::promise<int>> dequeue_promise_map;
  dequeue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(kQueueSize), std::forward_as_tuple());
  auto dequeue_future = dequeue_promise_map[kQueueSize].get_future();
  test_dequeue_end.RegisterDequeue(&dequeue_promise_map);

  // Register enqueue
  std::unordered_map<int, std::promise<int>> enqueue_promise_map;
  auto enqueue_future = enqueue_promise_map[0].get_future();
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);

  // Wait for all data move from enqueue end buffer to dequeue end buffer
  dequeue_future.wait();
  EXPECT_EQ(dequeue_future.get(), kQueueSize);

  test_dequeue_end.UnregisterDequeue();
}

// Test 8 : Queue becomes empty during test, DequeueCallback should stop to be invoked

// Enqueue end level : 1
// Dequeue end level : 1 -> 0
// Test 8-1 Queue becomes empty due to only register DequeueCallback
TEST_F(QueueTest, queue_becomes_empty_dequeue_callback_only) {
  Queue<std::string> queue(kQueueSize);
  TestEnqueueEnd test_enqueue_end(&queue, enqueue_handler_);
  TestDequeueEnd test_dequeue_end(&queue, dequeue_handler_, kHalfOfQueueSize);

  // make Queue half empty
  for (int i = 0; i < kHalfOfQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }
  std::unordered_map<int, std::promise<int>> enqueue_promise_map;
  enqueue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(0), std::forward_as_tuple());
  auto enqueue_future = enqueue_promise_map[0].get_future();
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);
  enqueue_future.wait();
  EXPECT_EQ(enqueue_future.get(), 0);

  // Register dequeue, expect kHalfOfQueueSize data move to dequeue end buffer
  std::unordered_map<int, std::promise<int>> dequeue_promise_map;
  dequeue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(kHalfOfQueueSize),
                              std::forward_as_tuple());
  auto dequeue_future = dequeue_promise_map[kHalfOfQueueSize].get_future();
  test_dequeue_end.RegisterDequeue(&dequeue_promise_map);
  dequeue_future.wait();
  EXPECT_EQ(dequeue_future.get(), kHalfOfQueueSize);

  // Expect DequeueCallback should stop to be invoked
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_EQ(test_dequeue_end.count, kHalfOfQueueSize);
}

// Enqueue end level : 1
// Dequeue end level : 1 -> 0
// Test 8-2 Queue becomes empty due to EnqueueCallback unregister during test
TEST_F(QueueTest, queue_becomes_empty_enqueue_callback_unregister) {
  Queue<std::string> queue(kQueueSize);
  TestEnqueueEnd test_enqueue_end(&queue, enqueue_handler_);
  TestDequeueEnd test_dequeue_end(&queue, dequeue_handler_, kQueueSize);

  // make Queue half empty
  for (int i = 0; i < kHalfOfQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }
  std::unordered_map<int, std::promise<int>> enqueue_promise_map;
  enqueue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(0), std::forward_as_tuple());
  auto enqueue_future = enqueue_promise_map[0].get_future();
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);
  enqueue_future.wait();
  EXPECT_EQ(enqueue_future.get(), 0);

  // push kHalfOfQueueSize to enqueue end buffer and register enqueue.
  for (int i = 0; i < kHalfOfQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);

  // Register dequeue, expect kQueueSize move to dequeue end buffer
  std::unordered_map<int, std::promise<int>> dequeue_promise_map;
  dequeue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(kQueueSize), std::forward_as_tuple());
  auto dequeue_future = dequeue_promise_map[kQueueSize].get_future();
  test_dequeue_end.RegisterDequeue(&dequeue_promise_map);
  dequeue_future.wait();
  EXPECT_EQ(dequeue_future.get(), kQueueSize);

  // Expect DequeueCallback should stop to be invoked
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_EQ(test_dequeue_end.count, kQueueSize);
}

// Enqueue end level : 1
// Dequeue end level : 0 -> 1
// Test 9 Queue becomes not empty during test, DequeueCallback should start to be invoked
TEST_F(QueueTest, queue_becomes_non_empty_during_test) {
  Queue<std::string> queue(kQueueSize);
  TestEnqueueEnd test_enqueue_end(&queue, enqueue_handler_);
  TestDequeueEnd test_dequeue_end(&queue, dequeue_handler_, kQueueSize);

  // Register dequeue
  std::unordered_map<int, std::promise<int>> dequeue_promise_map;
  dequeue_promise_map.emplace(std::piecewise_construct, std::forward_as_tuple(kQueueSize), std::forward_as_tuple());
  test_dequeue_end.RegisterDequeue(&dequeue_promise_map);

  // push kQueueSize data to enqueue end buffer and register enqueue
  for (int i = 0; i < kQueueSize; i++) {
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::to_string(i));
    test_enqueue_end.buffer_.push(std::move(data));
  }
  std::unordered_map<int, std::promise<int>> enqueue_promise_map;
  test_enqueue_end.RegisterEnqueue(&enqueue_promise_map);

  // Expect kQueueSize data move to dequeue end buffer
  auto dequeue_future = dequeue_promise_map[kQueueSize].get_future();
  dequeue_future.wait();
  EXPECT_EQ(dequeue_future.get(), kQueueSize);
}

TEST_F(QueueTest, pass_smart_pointer_and_unregister) {
  Queue<std::string>* queue = new Queue<std::string>(kQueueSize);

  // Enqueue a string
  std::string valid = "Valid String";
  std::shared_ptr<std::string> shared = std::make_shared<std::string>(valid);
  queue->RegisterEnqueue(enqueue_handler_, common::Bind(
                                               [](Queue<std::string>* queue, std::shared_ptr<std::string> shared) {
                                                 queue->UnregisterEnqueue();
                                                 return std::make_unique<std::string>(*shared);
                                               },
                                               common::Unretained(queue), shared));

  // Dequeue the string
  queue->RegisterDequeue(dequeue_handler_, common::Bind(
                                               [](Queue<std::string>* queue, std::string valid) {
                                                 queue->UnregisterDequeue();
                                                 auto answer = *queue->TryDequeue();
                                                 ASSERT_EQ(answer, valid);
                                               },
                                               common::Unretained(queue), valid));

  // Wait for both handlers to finish and delete the Queue
  std::promise<void> promise;
  auto future = promise.get_future();

  enqueue_handler_->Post(common::BindOnce(
      [](os::Handler* dequeue_handler, Queue<std::string>* queue, std::promise<void>* promise) {
        dequeue_handler->Post(common::BindOnce(
            [](Queue<std::string>* queue, std::promise<void>* promise) {
              delete queue;
              promise->set_value();
            },
            common::Unretained(queue), common::Unretained(promise)));
      },
      common::Unretained(dequeue_handler_), common::Unretained(queue), common::Unretained(&promise)));
  future.wait();
}

// Create all threads for death tests in the function that dies
class QueueDeathTest : public ::testing::Test {
 public:
  void RegisterEnqueueAndDelete() {
    Thread* enqueue_thread = new Thread("enqueue_thread", Thread::Priority::NORMAL);
    Handler* enqueue_handler = new Handler(enqueue_thread);
    Queue<std::string>* queue = new Queue<std::string>(kQueueSizeOne);
    queue->RegisterEnqueue(enqueue_handler,
                           common::Bind([]() { return std::make_unique<std::string>("A string to fill the queue"); }));
    delete queue;
  }

  void RegisterDequeueAndDelete() {
    Thread* dequeue_thread = new Thread("dequeue_thread", Thread::Priority::NORMAL);
    Handler* dequeue_handler = new Handler(dequeue_thread);
    Queue<std::string>* queue = new Queue<std::string>(kQueueSizeOne);
    queue->RegisterDequeue(dequeue_handler, common::Bind([](Queue<std::string>* queue) { queue->TryDequeue(); },
                                                         common::Unretained(queue)));
    delete queue;
  }
};

TEST_F(QueueDeathTest, die_if_enqueue_not_unregistered) {
  EXPECT_DEATH(RegisterEnqueueAndDelete(), "nqueue");
}

TEST_F(QueueDeathTest, die_if_dequeue_not_unregistered) {
  EXPECT_DEATH(RegisterDequeueAndDelete(), "equeue");
}

class MockIQueueEnqueue : public IQueueEnqueue<int> {
 public:
  void RegisterEnqueue(Handler* handler, EnqueueCallback callback) override {
    EXPECT_FALSE(registered_);
    registered_ = true;
    handler->Post(common::BindOnce(&MockIQueueEnqueue::handle_register_enqueue, common::Unretained(this), callback));
  }

  void handle_register_enqueue(EnqueueCallback callback) {
    if (dont_handle_register_enqueue_) {
      return;
    }
    while (registered_) {
      std::unique_ptr<int> front = callback.Run();
      queue_.push(*front);
    }
  }

  void UnregisterEnqueue() override {
    EXPECT_TRUE(registered_);
    registered_ = false;
  }

  bool dont_handle_register_enqueue_ = false;
  bool registered_ = false;
  std::queue<int> queue_;
};

class EnqueueBufferTest : public ::testing::Test {
 protected:
  void SetUp() override {
    thread_ = new Thread("test_thread", Thread::Priority::NORMAL);
    handler_ = new Handler(thread_);
  }

  void TearDown() override {
    handler_->Clear();
    delete handler_;
    delete thread_;
  }

  void SynchronizeHandler() {
    std::promise<void> promise;
    auto future = promise.get_future();
    handler_->Post(common::BindOnce([](std::promise<void> promise) { promise.set_value(); }, std::move(promise)));
    future.wait();
  }

  MockIQueueEnqueue enqueue_;
  EnqueueBuffer<int> enqueue_buffer_{&enqueue_};
  Thread* thread_;
  Handler* handler_;
};

TEST_F(EnqueueBufferTest, enqueue) {
  int num_items = 10;
  for (int i = 0; i < num_items; i++) {
    enqueue_buffer_.Enqueue(std::make_unique<int>(i), handler_);
  }
  SynchronizeHandler();
  for (int i = 0; i < num_items; i++) {
    ASSERT_EQ(enqueue_.queue_.front(), i);
    enqueue_.queue_.pop();
  }
  ASSERT_FALSE(enqueue_.registered_);
}

TEST_F(EnqueueBufferTest, clear) {
  enqueue_.dont_handle_register_enqueue_ = true;
  int num_items = 10;
  for (int i = 0; i < num_items; i++) {
    enqueue_buffer_.Enqueue(std::make_unique<int>(i), handler_);
  }
  ASSERT_TRUE(enqueue_.registered_);
  enqueue_buffer_.Clear();
  ASSERT_FALSE(enqueue_.registered_);
}

}  // namespace
}  // namespace os
}  // namespace bluetooth
