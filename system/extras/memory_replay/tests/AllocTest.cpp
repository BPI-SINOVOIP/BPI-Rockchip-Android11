/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <stdint.h>

#include <string>

#include <gtest/gtest.h>

#include "Alloc.h"

TEST(AllocTest, malloc_valid) {
  std::string line = "1234: malloc 0xabd0000 20";
  AllocEntry entry;
  AllocGetData(line, &entry);
  EXPECT_EQ(MALLOC, entry.type);
  EXPECT_EQ(1234, entry.tid);
  EXPECT_EQ(0xabd0000U, entry.ptr);
  EXPECT_EQ(20U, entry.size);
  EXPECT_EQ(0U, entry.u.align);
}

TEST(AllocTest, malloc_invalid) {
  std::string line = "1234: malloc 0xabd0000";
  AllocEntry entry;
  EXPECT_DEATH(AllocGetData(line, &entry), "");

  line = "1234: malloc";
  EXPECT_DEATH(AllocGetData(line, &entry), "");
}

TEST(AllocTest, free_valid) {
  std::string line = "1235: free 0x5000";
  AllocEntry entry;
  AllocGetData(line, &entry);
  EXPECT_EQ(FREE, entry.type);
  EXPECT_EQ(1235, entry.tid);
  EXPECT_EQ(0x5000U, entry.ptr);
  EXPECT_EQ(0U, entry.size);
  EXPECT_EQ(0U, entry.u.align);
}

TEST(AllocTest, free_invalid) {
  std::string line = "1234: free";
  AllocEntry entry;
  EXPECT_DEATH(AllocGetData(line, &entry), "");
}

TEST(AllocTest, calloc_valid) {
  std::string line = "1236: calloc 0x8000 50 30";
  AllocEntry entry;
  AllocGetData(line, &entry);
  EXPECT_EQ(CALLOC, entry.type);
  EXPECT_EQ(1236, entry.tid);
  EXPECT_EQ(0x8000U, entry.ptr);
  EXPECT_EQ(30U, entry.size);
  EXPECT_EQ(50U, entry.u.n_elements);
}

TEST(AllocTest, calloc_invalid) {
  std::string line = "1236: calloc 0x8000 50";
  AllocEntry entry;
  EXPECT_DEATH(AllocGetData(line, &entry), "");

  line = "1236: calloc 0x8000";
  EXPECT_DEATH(AllocGetData(line, &entry), "");

  line = "1236: calloc";
  EXPECT_DEATH(AllocGetData(line, &entry), "");
}

TEST(AllocTest, realloc_valid) {
  std::string line = "1237: realloc 0x9000 0x4000 80";
  AllocEntry entry;
  AllocGetData(line, &entry);
  EXPECT_EQ(REALLOC, entry.type);
  EXPECT_EQ(1237, entry.tid);
  EXPECT_EQ(0x9000U, entry.ptr);
  EXPECT_EQ(80U, entry.size);
  EXPECT_EQ(0x4000U, entry.u.old_ptr);
}

TEST(AllocTest, realloc_invalid) {
  std::string line = "1237: realloc 0x9000 0x4000";
  AllocEntry entry;
  EXPECT_DEATH(AllocGetData(line, &entry), "");

  line = "1237: realloc 0x9000";
  EXPECT_DEATH(AllocGetData(line, &entry), "");

  line = "1237: realloc";
  EXPECT_DEATH(AllocGetData(line, &entry), "");
}

TEST(AllocTest, memalign_valid) {
  std::string line = "1238: memalign 0xa000 16 89";
  AllocEntry entry;
  AllocGetData(line, &entry);
  EXPECT_EQ(MEMALIGN, entry.type);
  EXPECT_EQ(1238, entry.tid);
  EXPECT_EQ(0xa000U, entry.ptr);
  EXPECT_EQ(89U, entry.size);
  EXPECT_EQ(16U, entry.u.align);
}

TEST(AllocTest, memalign_invalid) {
  std::string line = "1238: memalign 0xa000 16";
  AllocEntry entry;
  EXPECT_DEATH(AllocGetData(line, &entry), "");

  line = "1238: memalign 0xa000";
  EXPECT_DEATH(AllocGetData(line, &entry), "");

  line = "1238: memalign";
  EXPECT_DEATH(AllocGetData(line, &entry), "");
}

TEST(AllocTest, thread_done_valid) {
  std::string line = "1239: thread_done 0x0";
  AllocEntry entry;
  AllocGetData(line, &entry);
  EXPECT_EQ(THREAD_DONE, entry.type);
  EXPECT_EQ(1239, entry.tid);
  EXPECT_EQ(0U, entry.ptr);
  EXPECT_EQ(0U, entry.size);
  EXPECT_EQ(0U, entry.u.old_ptr);
}

TEST(AllocTest, thread_done_invalid) {
  std::string line = "1240: thread_done";
  AllocEntry entry;
  EXPECT_DEATH(AllocGetData(line, &entry), "");
}
