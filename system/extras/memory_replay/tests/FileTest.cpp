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

#include <malloc.h>
#include <stdint.h>

#include <string>

#include <android-base/file.h>
#include <gtest/gtest.h>

#include "Alloc.h"
#include "File.h"

static std::string GetTestDirectory() {
  return android::base::GetExecutableDirectory() + "/tests";
}

static std::string GetTestZip() {
  return GetTestDirectory() + "/test.zip";
}

TEST(FileTest, zip_get_contents) {
  EXPECT_EQ("12345: malloc 0x1000 16\n12345: free 0x1000\n", ZipGetContents(GetTestZip().c_str()));
}

TEST(FileTest, zip_get_contents_bad_file) {
  EXPECT_EQ("", ZipGetContents("/does/not/exist.zip"));
}

TEST(FileTest, get_unwind_info_from_zip_file) {
  // This will allocate, so do it before getting mallinfo.
  std::string file_name = GetTestZip();

  size_t mallinfo_before = mallinfo().uordblks;
  AllocEntry* entries;
  size_t num_entries;
  GetUnwindInfo(file_name.c_str(), &entries, &num_entries);
  size_t mallinfo_after = mallinfo().uordblks;

  // Verify no memory is allocated.
  EXPECT_EQ(mallinfo_after, mallinfo_before);

  ASSERT_EQ(2U, num_entries);
  EXPECT_EQ(12345, entries[0].tid);
  EXPECT_EQ(MALLOC, entries[0].type);
  EXPECT_EQ(0x1000U, entries[0].ptr);
  EXPECT_EQ(16U, entries[0].size);
  EXPECT_EQ(0U, entries[0].u.old_ptr);

  EXPECT_EQ(12345, entries[1].tid);
  EXPECT_EQ(FREE, entries[1].type);
  EXPECT_EQ(0x1000U, entries[1].ptr);
  EXPECT_EQ(0U, entries[1].size);
  EXPECT_EQ(0U, entries[1].u.old_ptr);

  FreeEntries(entries, num_entries);
}

TEST(FileTest, get_unwind_info_bad_zip_file) {
  AllocEntry* entries;
  size_t num_entries;
  EXPECT_DEATH(GetUnwindInfo("/does/not/exist.zip", &entries, &num_entries), "");
}

TEST(FileTest, get_unwind_info_from_text_file) {
  // This will allocate, so do it before getting mallinfo.
  std::string file_name = GetTestDirectory() + "/test.txt";

  size_t mallinfo_before = mallinfo().uordblks;
  AllocEntry* entries;
  size_t num_entries;
  GetUnwindInfo(file_name.c_str(), &entries, &num_entries);
  size_t mallinfo_after = mallinfo().uordblks;

  // Verify no memory is allocated.
  EXPECT_EQ(mallinfo_after, mallinfo_before);

  ASSERT_EQ(2U, num_entries);
  EXPECT_EQ(98765, entries[0].tid);
  EXPECT_EQ(MEMALIGN, entries[0].type);
  EXPECT_EQ(0xa000U, entries[0].ptr);
  EXPECT_EQ(124U, entries[0].size);
  EXPECT_EQ(16U, entries[0].u.align);

  EXPECT_EQ(98765, entries[1].tid);
  EXPECT_EQ(FREE, entries[1].type);
  EXPECT_EQ(0xa000U, entries[1].ptr);
  EXPECT_EQ(0U, entries[1].size);
  EXPECT_EQ(0U, entries[1].u.old_ptr);

  FreeEntries(entries, num_entries);
}

TEST(FileTest, get_unwind_info_bad_file) {
  AllocEntry* entries;
  size_t num_entries;
  EXPECT_DEATH(GetUnwindInfo("/does/not/exist", &entries, &num_entries), "");
}
