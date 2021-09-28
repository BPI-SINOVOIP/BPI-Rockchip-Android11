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

#include "common/bidi_queue.h"

#include <future>

#include "common/bind.h"
#include "gtest/gtest.h"
#include "os/handler.h"
#include "os/thread.h"

using ::bluetooth::os::Thread;
using ::bluetooth::os::Handler;

namespace bluetooth {
namespace common {
namespace {

class BidiQueueTest : public ::testing::Test {
 protected:
  void SetUp() override {
    up_thread_ = new Thread("up_thread", Thread::Priority::NORMAL);
    up_handler_ = new Handler(up_thread_);
    down_thread_ = new Thread("down_thread", Thread::Priority::NORMAL);
    down_handler_ = new Handler(down_thread_);
  }

  void TearDown() override {
    delete up_handler_;
    delete up_thread_;
    delete down_handler_;
    delete down_thread_;
  }

  Thread* up_thread_;
  Handler* up_handler_;
  Thread* down_thread_;
  Handler* down_handler_;
};

class A {
};

class B {
};

template <typename TA, typename TB>
class TestBidiQueueEnd {
 public:
  explicit TestBidiQueueEnd(BidiQueueEnd<TA, TB>* end, Handler* handler)
      : handler_(handler), end_(end) {}

  ~TestBidiQueueEnd() {
    handler_->Clear();
  }

  std::promise<void>* Send(TA* value) {
    std::promise<void>* promise = new std::promise<void>();
    handler_->Post(BindOnce(&TestBidiQueueEnd<TA, TB>::handle_send, common::Unretained(this), common::Unretained(value),
                            common::Unretained(promise)));
    return promise;
  }

  std::promise<TB*>* Receive() {
    std::promise<TB*>* promise = new std::promise<TB*>();
    handler_->Post(
        BindOnce(&TestBidiQueueEnd<TA, TB>::handle_receive, common::Unretained(this), common::Unretained(promise)));

    return promise;
  }

  void handle_send(TA* value, std::promise<void>* promise) {
    end_->RegisterEnqueue(handler_, Bind(&TestBidiQueueEnd<TA, TB>::handle_register_enqueue, common::Unretained(this),
                                         common::Unretained(value), common::Unretained(promise)));
  }

  std::unique_ptr<TA> handle_register_enqueue(TA* value, std::promise<void>* promise) {
    end_->UnregisterEnqueue();
    promise->set_value();
    return std::unique_ptr<TA>(value);
  }

  void handle_receive(std::promise<TB*>* promise) {
    end_->RegisterDequeue(handler_, Bind(&TestBidiQueueEnd<TA, TB>::handle_register_dequeue, common::Unretained(this),
                                         common::Unretained(promise)));
  }

  void handle_register_dequeue(std::promise<TB*>* promise) {
    end_->UnregisterDequeue();
    promise->set_value(end_->TryDequeue().get());
  }

 private:
  Handler* handler_;
  BidiQueueEnd<TA, TB>* end_;
};

TEST_F(BidiQueueTest, simple_test) {
  BidiQueue<A, B> queue(100);
  TestBidiQueueEnd<B, A> test_up(queue.GetUpEnd(), up_handler_);
  TestBidiQueueEnd<A, B> test_down(queue.GetDownEnd(), down_handler_);

  auto sending_b = new B();
  auto promise_sending_b = test_up.Send(sending_b);
  promise_sending_b->get_future().wait();
  auto promise_receive_b = test_down.Receive();
  EXPECT_EQ(promise_receive_b->get_future().get(), sending_b);
  delete promise_receive_b;
  delete promise_sending_b;

  auto sending_a = new A();
  auto promise_sending_a = test_down.Send(sending_a);
  promise_sending_a->get_future().wait();
  auto promise_receive_a = test_up.Receive();
  EXPECT_EQ(promise_receive_a->get_future().get(), sending_a);
  delete promise_receive_a;
  delete promise_sending_a;
}

}  // namespace
}  // namespace os
}  // namespace bluetooth
