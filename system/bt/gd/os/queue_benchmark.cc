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

#include "benchmark/benchmark.h"

#include <future>

#include "os/handler.h"
#include "os/queue.h"
#include "os/thread.h"

using ::benchmark::State;

namespace bluetooth {
namespace os {

class BM_QueuePerformance : public ::benchmark::Fixture {
 protected:
  void SetUp(State& st) override {
    ::benchmark::Fixture::SetUp(st);
    enqueue_thread_ = new Thread("enqueue_thread", Thread::Priority::NORMAL);
    enqueue_handler_ = new Handler(enqueue_thread_);
    dequeue_thread_ = new Thread("dequeue_thread", Thread::Priority::NORMAL);
    dequeue_handler_ = new Handler(dequeue_thread_);
  }

  void TearDown(State& st) override {
    delete enqueue_handler_;
    delete enqueue_thread_;
    delete dequeue_handler_;
    delete dequeue_thread_;
    enqueue_handler_ = nullptr;
    enqueue_thread_ = nullptr;
    dequeue_handler_ = nullptr;
    dequeue_thread_ = nullptr;
    benchmark::Fixture::TearDown(st);
  }

  Thread* enqueue_thread_;
  Handler* enqueue_handler_;
  Thread* dequeue_thread_;
  Handler* dequeue_handler_;
};

class TestEnqueueEnd {
 public:
  explicit TestEnqueueEnd(int64_t count, Queue<std::string>* queue, Handler* handler, std::promise<void>* promise)
      : count_(count), handler_(handler), queue_(queue), promise_(promise) {}

  void RegisterEnqueue() {
    handler_->Post(common::BindOnce(&TestEnqueueEnd::handle_register_enqueue, common::Unretained(this)));
  }

  void push(std::string data) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      buffer_.push(std::move(data));
    }
    if (buffer_.size() == 1) {
      RegisterEnqueue();
    }
  }

  std::unique_ptr<std::string> EnqueueCallbackForTest() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unique_ptr<std::string> data = std::make_unique<std::string>(std::move(buffer_.front()));
    buffer_.pop();

    if (buffer_.empty()) {
      queue_->UnregisterEnqueue();
    }

    count_--;
    if (count_ == 0) {
      promise_->set_value();
    }

    return data;
  }

  std::queue<std::string> buffer_;
  int64_t count_;

 private:
  Handler* handler_;
  Queue<std::string>* queue_;
  std::promise<void>* promise_;
  std::mutex mutex_;

  void handle_register_enqueue() {
    queue_->RegisterEnqueue(handler_, common::Bind(&TestEnqueueEnd::EnqueueCallbackForTest, common::Unretained(this)));
  }
};

class TestDequeueEnd {
 public:
  explicit TestDequeueEnd(int64_t count, Queue<std::string>* queue, Handler* handler, std::promise<void>* promise)
      : count_(count), handler_(handler), queue_(queue), promise_(promise) {}

  void RegisterDequeue() {
    handler_->Post(common::BindOnce(&TestDequeueEnd::handle_register_dequeue, common::Unretained(this)));
  }

  void DequeueCallbackForTest() {
    std::string data = *(queue_->TryDequeue());
    buffer_.push(data);

    count_--;
    if (count_ == 0) {
      queue_->UnregisterDequeue();
      promise_->set_value();
    }
  }

  std::queue<std::string> buffer_;
  int64_t count_;

 private:
  Handler* handler_;
  Queue<std::string>* queue_;
  std::promise<void>* promise_;

  void handle_register_dequeue() {
    queue_->RegisterDequeue(handler_, common::Bind(&TestDequeueEnd::DequeueCallbackForTest, common::Unretained(this)));
  }
};

BENCHMARK_DEFINE_F(BM_QueuePerformance, send_packet_vary_by_packet_num)(State& state) {
  for (auto _ : state) {
    int64_t num_data_to_send_ = state.range(0);
    Queue<std::string> queue(num_data_to_send_);

    // register dequeue
    std::promise<void> dequeue_promise;
    auto dequeue_future = dequeue_promise.get_future();
    TestDequeueEnd test_dequeue_end(num_data_to_send_, &queue, enqueue_handler_, &dequeue_promise);
    test_dequeue_end.RegisterDequeue();

    // Push data to enqueue end buffer and register enqueue
    std::promise<void> enqueue_promise;
    TestEnqueueEnd test_enqueue_end(num_data_to_send_, &queue, enqueue_handler_, &enqueue_promise);
    for (int i = 0; i < num_data_to_send_; i++) {
      std::string data = std::to_string(1);
      test_enqueue_end.push(std::move(data));
    }
    dequeue_future.wait();
  }

  state.SetBytesProcessed(static_cast<int_fast64_t>(state.iterations()) * state.range(0));
};

BENCHMARK_REGISTER_F(BM_QueuePerformance, send_packet_vary_by_packet_num)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Iterations(100)
    ->UseRealTime();

BENCHMARK_DEFINE_F(BM_QueuePerformance, send_10000_packet_vary_by_packet_size)(State& state) {
  for (auto _ : state) {
    int64_t num_data_to_send_ = 10000;
    int64_t packet_size = state.range(0);
    Queue<std::string> queue(num_data_to_send_);

    // register dequeue
    std::promise<void> dequeue_promise;
    auto dequeue_future = dequeue_promise.get_future();
    TestDequeueEnd test_dequeue_end(num_data_to_send_, &queue, enqueue_handler_, &dequeue_promise);
    test_dequeue_end.RegisterDequeue();

    // Push data to enqueue end buffer and register enqueue
    std::promise<void> enqueue_promise;
    TestEnqueueEnd test_enqueue_end(num_data_to_send_, &queue, enqueue_handler_, &enqueue_promise);
    for (int i = 0; i < num_data_to_send_; i++) {
      std::string data = std::string(packet_size, 'x');
      test_enqueue_end.push(std::move(data));
    }
    dequeue_future.wait();
  }

  state.SetBytesProcessed(static_cast<int_fast64_t>(state.iterations()) * state.range(0) * 10000);
};

BENCHMARK_REGISTER_F(BM_QueuePerformance, send_10000_packet_vary_by_packet_size)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1000)
    ->Iterations(100)
    ->UseRealTime();

}  // namespace os
}  // namespace bluetooth
