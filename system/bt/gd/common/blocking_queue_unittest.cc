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

#include "common/blocking_queue.h"

#include <thread>

#include <gtest/gtest.h>

namespace bluetooth {
namespace common {
namespace {
class BlockingQueueTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_TRUE(queue_.empty());
  }

  // Postcondition for each test case: clear the blocking queue
  void TearDown() override {
    EXPECT_TRUE(queue_.empty());
  }

  BlockingQueue<int> queue_;
};

TEST_F(BlockingQueueTest, initial_empty) {
  EXPECT_TRUE(queue_.empty());
}

TEST_F(BlockingQueueTest, same_thread_push_and_pop) {
  int data = 1;
  queue_.push(data);
  EXPECT_FALSE(queue_.empty());
  EXPECT_EQ(queue_.take(), data);
  EXPECT_TRUE(queue_.empty());
}

TEST_F(BlockingQueueTest, same_thread_push_and_pop_sequential) {
  for (int data = 0; data < 10; data++) {
    queue_.push(data);
    EXPECT_FALSE(queue_.empty());
    EXPECT_EQ(queue_.take(), data);
    EXPECT_TRUE(queue_.empty());
  }
}

TEST_F(BlockingQueueTest, same_thread_push_and_pop_batch) {
  for (int data = 0; data < 10; data++) {
    queue_.push(data);
  }
  EXPECT_FALSE(queue_.empty());
  for (int data = 0; data < 10; data++) {
    EXPECT_EQ(queue_.take(), data);
  }
  EXPECT_TRUE(queue_.empty());
}

TEST_F(BlockingQueueTest, clear_queue) {
  for (int data = 0; data < 10; data++) {
    queue_.push(data);
  }
  EXPECT_FALSE(queue_.empty());
  queue_.clear();
  EXPECT_TRUE(queue_.empty());
}

TEST_F(BlockingQueueTest, wait_for_non_empty) {
  int data = 1;
  std::thread waiter_thread([this, data] { EXPECT_EQ(queue_.take(), data); });
  queue_.push(data);
  waiter_thread.join();
  EXPECT_TRUE(queue_.empty());
}

TEST_F(BlockingQueueTest, wait_to_take_fail) {
  EXPECT_FALSE(queue_.wait_to_take(std::chrono::milliseconds(3)));
}

TEST_F(BlockingQueueTest, wait_to_take_after_non_empty) {
  int data = 1;
  queue_.push(data);
  EXPECT_TRUE(queue_.wait_to_take(std::chrono::milliseconds(3)));
  queue_.clear();
}

TEST_F(BlockingQueueTest, wait_to_take_before_non_empty) {
  int data = 1;
  std::thread waiter_thread([this] { EXPECT_TRUE(queue_.wait_to_take(std::chrono::milliseconds(3))); });
  queue_.push(data);
  waiter_thread.join();
  queue_.clear();
}

TEST_F(BlockingQueueTest, wait_for_non_empty_batch) {
  std::thread waiter_thread([this] {
    for (int data = 0; data < 10; data++) {
      EXPECT_EQ(queue_.take(), data);
    }
  });
  for (int data = 0; data < 10; data++) {
    queue_.push(data);
  }
  waiter_thread.join();
  EXPECT_TRUE(queue_.empty());
}

class VectorBlockingQueueTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_TRUE(queue_.empty());
  }

  // Postcondition for each test case: clear the blocking queue
  void TearDown() override {
    EXPECT_TRUE(queue_.empty());
  }

  BlockingQueue<std::vector<uint8_t>> queue_;
};

TEST_F(VectorBlockingQueueTest, same_thread_push_and_pop) {
  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6};
  queue_.push(data);
  EXPECT_FALSE(queue_.empty());
  EXPECT_EQ(queue_.take(), data);
  EXPECT_TRUE(queue_.empty());
}

}  // namespace
}  // namespace common
}  // namespace bluetooth
