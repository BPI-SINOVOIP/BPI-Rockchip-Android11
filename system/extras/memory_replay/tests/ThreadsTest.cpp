/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <gtest/gtest.h>

#include "Alloc.h"
#include "Pointers.h"
#include "Thread.h"
#include "Threads.h"

TEST(ThreadsTest, single_thread) {
  Pointers pointers(2);

  Threads threads(&pointers, 1);
  Thread* thread = threads.CreateThread(900);
  ASSERT_TRUE(thread != nullptr);
  ASSERT_EQ(1U, threads.num_threads());

  Thread* found_thread = threads.FindThread(900);
  ASSERT_EQ(thread, found_thread);

  AllocEntry thread_done = {.type = THREAD_DONE};
  thread->SetAllocEntry(&thread_done);

  thread->SetPending();

  threads.Finish(thread);

  ASSERT_EQ(0U, threads.num_threads());
}

TEST(ThreadsTest, multiple_threads) {
  Pointers pointers(4);

  Threads threads(&pointers, 1);
  Thread* thread1 = threads.CreateThread(900);
  ASSERT_TRUE(thread1 != nullptr);
  ASSERT_EQ(1U, threads.num_threads());

  Thread* thread2 = threads.CreateThread(901);
  ASSERT_TRUE(thread2 != nullptr);
  ASSERT_EQ(2U, threads.num_threads());

  Thread* thread3 = threads.CreateThread(902);
  ASSERT_TRUE(thread3 != nullptr);
  ASSERT_EQ(3U, threads.num_threads());

  Thread* found_thread1 = threads.FindThread(900);
  ASSERT_EQ(thread1, found_thread1);

  Thread* found_thread2 = threads.FindThread(901);
  ASSERT_EQ(thread2, found_thread2);

  Thread* found_thread3 = threads.FindThread(902);
  ASSERT_EQ(thread3, found_thread3);

  AllocEntry thread_done = {.type = THREAD_DONE};
  thread1->SetAllocEntry(&thread_done);
  thread2->SetAllocEntry(&thread_done);
  thread3->SetAllocEntry(&thread_done);

  thread1->SetPending();
  threads.Finish(thread1);
  ASSERT_EQ(2U, threads.num_threads());

  thread3->SetPending();
  threads.Finish(thread3);
  ASSERT_EQ(1U, threads.num_threads());

  thread2->SetPending();
  threads.Finish(thread2);
  ASSERT_EQ(0U, threads.num_threads());
}

TEST(ThreadsTest, verify_quiesce) {
  Pointers pointers(4);

  Threads threads(&pointers, 1);
  Thread* thread = threads.CreateThread(900);
  ASSERT_TRUE(thread != nullptr);
  ASSERT_EQ(1U, threads.num_threads());

  // If WaitForAllToQuiesce is not correct, then this should provoke an error
  // since we are overwriting the action data while it's being used.
  constexpr size_t kAllocEntries = 512;
  std::vector<AllocEntry> mallocs(kAllocEntries);
  std::vector<AllocEntry> frees(kAllocEntries);
  for (size_t i = 0; i < kAllocEntries; i++) {
    mallocs[i].type = MALLOC;
    mallocs[i].ptr = 0x1234 + i;
    mallocs[i].size = 100;
    thread->SetAllocEntry(&mallocs[i]);
    thread->SetPending();
    threads.WaitForAllToQuiesce();

    frees[i].type = FREE;
    frees[i].ptr = 0x1234 + i;
    thread->SetAllocEntry(&frees[i]);
    thread->SetPending();
    threads.WaitForAllToQuiesce();
  }

  AllocEntry thread_done = {.type = THREAD_DONE};
  thread->SetAllocEntry(&thread_done);
  thread->SetPending();
  threads.Finish(thread);
  ASSERT_EQ(0U, threads.num_threads());
}

static void TestTooManyThreads() {
  Pointers pointers(4);

  Threads threads(&pointers, 1);
  for (size_t i = 0; i <= threads.max_threads(); i++) {
    Thread* thread = threads.CreateThread(900+i);
    ASSERT_EQ(thread, threads.FindThread(900+i));
  }
}

TEST(ThreadsTest, too_many_threads) {
  ASSERT_EXIT(TestTooManyThreads(), ::testing::ExitedWithCode(1), "");
}
