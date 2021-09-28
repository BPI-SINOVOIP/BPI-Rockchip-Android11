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

#include <stdint.h>

#include <string>

#include <android-base/file.h>
#include <gtest/gtest.h>

#include "NativeInfo.h"

class NativeInfoTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tmp_file_ = new TemporaryFile();
    ASSERT_TRUE(tmp_file_->fd != -1);
  }

  void TearDown() override {
    delete tmp_file_;
  }

 TemporaryFile* tmp_file_ = nullptr;
};

TEST_F(NativeInfoTest, no_matching) {
  std::string smaps_data =
      "b6f1a000-b6f1c000 rw-p 00000000 00:00 0          [anon:thread signal stack]\n"
      "Size:                  8 kB\n"
      "Rss:                   12 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:           [anon:thread signal stack]\n";
  ASSERT_TRUE(TEMP_FAILURE_RETRY(
      write(tmp_file_->fd, smaps_data.c_str(), smaps_data.size())) != -1);
  ASSERT_TRUE(lseek(tmp_file_->fd, 0, SEEK_SET) != off_t(-1));

  size_t rss_bytes = 1;
  size_t va_bytes = 1;
  NativeGetInfo(tmp_file_->fd, &rss_bytes, &va_bytes);
  ASSERT_EQ(0U, rss_bytes);
  ASSERT_EQ(0U, va_bytes);
}

TEST_F(NativeInfoTest, multiple_anons) {
  std::string smaps_data =
      "b6f1a000-b6f1c000 rw-p 00000000 00:00 0          [anon:libc_malloc]\n"
      "Size:                  8 kB\n"
      "Rss:                   12 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:           [anon:libc_malloc]\n"
      "b6f1e000-b6f1f000 rw-p 00000000 00:00 0          [anon:libc_malloc]\n"
      "Size:                  8 kB\n"
      "Rss:                   20 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:           [anon:libc_malloc]\n"
      "b6f2e000-b6f2f000 rw-p 00000000 00:00 0\n"
      "Size:                  8 kB\n"
      "Rss:                   24 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:\n";
  ASSERT_TRUE(TEMP_FAILURE_RETRY(
      write(tmp_file_->fd, smaps_data.c_str(), smaps_data.size())) != -1);
  ASSERT_TRUE(lseek(tmp_file_->fd, 0, SEEK_SET) != off_t(-1));

  size_t rss_bytes = 1;
  size_t va_bytes = 1;
  NativeGetInfo(tmp_file_->fd, &rss_bytes, &va_bytes);
  ASSERT_EQ(32768U, rss_bytes);
  ASSERT_EQ(12288U, va_bytes);
}

TEST_F(NativeInfoTest, multiple_heaps) {
  std::string smaps_data =
      "b6f1a000-b6f1c000 rw-p 00000000 00:00 0          [heap]\n"
      "Size:                  8 kB\n"
      "Rss:                   24 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:           [heap]\n"
      "b6f1e000-b6f1f000 rw-p 00000000 00:00 0          [heap]\n"
      "Size:                  8 kB\n"
      "Rss:                   20 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:           [heap]\n"
      "b6f2e000-b6f2f000 rw-p 00000000 00:00 0\n"
      "Size:                  8 kB\n"
      "Rss:                   24 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:\n";
  ASSERT_TRUE(TEMP_FAILURE_RETRY(
      write(tmp_file_->fd, smaps_data.c_str(), smaps_data.size())) != -1);
  ASSERT_TRUE(lseek(tmp_file_->fd, 0, SEEK_SET) != off_t(-1));

  size_t rss_bytes = 1;
  size_t va_bytes = 1;
  NativeGetInfo(tmp_file_->fd, &rss_bytes, &va_bytes);
  ASSERT_EQ(45056U, rss_bytes);
  ASSERT_EQ(12288U, va_bytes);
}

TEST_F(NativeInfoTest, mix_heap_anon) {
  std::string smaps_data =
      "b6f1a000-b6f1c000 rw-p 00000000 00:00 0          [heap]\n"
      "Size:                  8 kB\n"
      "Rss:                   32 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:           [heap]\n"
      "b6f1e000-b6f1f000 rw-p 00000000 00:00 0          [anon:skip]\n"
      "Size:                  8 kB\n"
      "Rss:                   32 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:           [anon:skip]\n"
      "b6f2e000-b6f2f000 rw-p 00000000 00:00 0          [anon:libc_malloc]\n"
      "Size:                  8 kB\n"
      "Rss:                   40 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:           [anon:libc_malloc]\n"
      "b6f3e000-b6f3f000 rw-p 00000000 00:00 0\n"
      "Size:                  8 kB\n"
      "Rss:                   24 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:\n"
      "b6f4e000-b6f6f000 rw-p 00000000 00:00 0          [anon:scudo:test]\n"
      "Size:                  8 kB\n"
      "Rss:                   52 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:           [anon:scudo:test]\n"
      "b8f4c000-b8f4d000 rw-p 00000000 00:00 0          [anon:GWP-ASan Guard Page]\n"
      "Size:                  4 kB\n"
      "Rss:                   0 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:           [anon:GWP-ASan Guard Page]\n"
      "b8f4d000-b8f4e000 rw-p 00000000 00:00 0          [anon:GWP-ASan Alive Slot]\n"
      "Size:                  4 kB\n"
      "Rss:                   4 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:           [anon:GWP-ASan Alive Slot]\n"
      "b8f4e000-b8f4f000 rw-p 00000000 00:00 0          [anon:GWP-ASan Guard Page]\n"
      "Size:                  4 kB\n"
      "Rss:                   0 kB\n"
      "Pss:                   0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Private_Clean:         0 kB\n"
      "Private_Dirty:         0 kB\n"
      "Referenced:            0 kB\n"
      "Anonymous:             0 kB\n"
      "AnonHugePages:         0 kB\n"
      "Swap:                  0 kB\n"
      "KernelPageSize:        4 kB\n"
      "MMUPageSize:           4 kB\n"
      "Locked:                0 kB\n"
      "Name:           [anon:GWP-ASan Guard Page]\n";

  ASSERT_TRUE(TEMP_FAILURE_RETRY(
      write(tmp_file_->fd, smaps_data.c_str(), smaps_data.size())) != -1);
  ASSERT_TRUE(lseek(tmp_file_->fd, 0, SEEK_SET) != off_t(-1));

  size_t rss_bytes = 1;
  size_t va_bytes = 1;
  NativeGetInfo(tmp_file_->fd, &rss_bytes, &va_bytes);
  EXPECT_EQ(131072U, rss_bytes);
  EXPECT_EQ(159744U, va_bytes);
}
