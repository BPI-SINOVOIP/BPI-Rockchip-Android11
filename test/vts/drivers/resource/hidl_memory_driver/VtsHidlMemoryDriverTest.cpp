//
// Copyright 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "hidl_memory_driver/VtsHidlMemoryDriver.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <gtest/gtest.h>

using namespace std;

namespace android {
namespace vts {

// Unit test to test operations on hidl_memory_driver.
class HidlMemoryDriverUnitTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    mem_id_ = mem_driver_.Allocate(100);
    ASSERT_NE(mem_id_, -1);
  }

  virtual void TearDown() {}

  VtsHidlMemoryDriver mem_driver_;
  int mem_id_;
};

// Helper method to initialize an array of random integers.
void InitData(int* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    data[i] = rand() % 100 + 1;
  }
}

// Tests trying to read from an invalid memory object.
TEST_F(HidlMemoryDriverUnitTest, InvalidMemId) {
  ASSERT_FALSE(mem_driver_.Read(42));
}

// Tests GetSize() method.
TEST_F(HidlMemoryDriverUnitTest, GetSizeTest) {
  size_t mem_size;
  ASSERT_TRUE(mem_driver_.GetSize(mem_id_, &mem_size));
  ASSERT_EQ(100, mem_size);
}

// Tests writing to the memory and reading the same data back.
TEST_F(HidlMemoryDriverUnitTest, SimpleWriteRead) {
  string write_data = "abcdef";
  // Write into the memory.
  ASSERT_TRUE(mem_driver_.Update(mem_id_));
  ASSERT_TRUE(mem_driver_.UpdateBytes(mem_id_, write_data.c_str(),
                                      write_data.length()));
  ASSERT_TRUE(mem_driver_.Commit(mem_id_));

  // Read from the memory.
  char read_data[write_data.length()];
  ASSERT_TRUE(mem_driver_.Read(mem_id_));
  ASSERT_TRUE(mem_driver_.ReadBytes(mem_id_, read_data, write_data.length()));
  ASSERT_TRUE(mem_driver_.Commit(mem_id_));

  // Check read data.
  ASSERT_EQ(0, strncmp(write_data.c_str(), read_data, write_data.length()));
}

// Tests consecutive writes and reads using integers.
// For each of the 5 iterations, write 5 integers into
// different chunks of memory, and read them back.
TEST_F(HidlMemoryDriverUnitTest, LargeWriteRead) {
  int write_data[5];
  int read_data[5];

  // Every write shifts 5 * sizeof(int) bytes of region.
  // Every iteration writes 5 integers.
  for (int i = 0; i < 100; i += 5 * sizeof(int)) {
    InitData(write_data, 5);
    // Write 5 integers, which is equivalent to writing 5 * sizeof(int) bytes.
    ASSERT_TRUE(mem_driver_.UpdateRange(mem_id_, i, 5 * sizeof(int)));
    ASSERT_TRUE(mem_driver_.UpdateBytes(
        mem_id_, reinterpret_cast<char*>(write_data), 5 * sizeof(int), i));
    ASSERT_TRUE(mem_driver_.Commit(mem_id_));

    // Read the integers back.
    ASSERT_TRUE(mem_driver_.ReadRange(mem_id_, i, 5 * sizeof(int)));
    ASSERT_TRUE(mem_driver_.ReadBytes(
        mem_id_, reinterpret_cast<char*>(read_data), 5 * sizeof(int), i));
    ASSERT_TRUE(mem_driver_.Commit(mem_id_));

    ASSERT_EQ(0, memcmp(write_data, read_data, 5 * sizeof(int)));
  }
}

// Tests writing into different regions in the memory buffer.
// Writer requests the beginning of the first half and
// the beginning of the second half of the buffer.
// It writes to the second half, commits, and reads the data back.
// Then it writes to the first half, commits, and reads the data back.
TEST_F(HidlMemoryDriverUnitTest, WriteTwoRegionsInOneBuffer) {
  string write_data1 = "abcdef";
  string write_data2 = "ghijklmno";
  char read_data1[write_data1.length()];
  char read_data2[write_data2.length()];

  // Request writes to two separate regions in the same buffer.
  // One from the start, the second from offset 50.
  ASSERT_TRUE(mem_driver_.UpdateRange(mem_id_, 0, write_data1.length()));
  ASSERT_TRUE(mem_driver_.UpdateRange(mem_id_, 50, write_data2.length()));
  // Update the second region.
  ASSERT_TRUE(mem_driver_.UpdateBytes(mem_id_, write_data2.c_str(),
                                      write_data2.length(), 50));
  ASSERT_TRUE(mem_driver_.Commit(mem_id_));

  // Read from the second region.
  ASSERT_TRUE(mem_driver_.Read(mem_id_));
  ASSERT_TRUE(
      mem_driver_.ReadBytes(mem_id_, read_data2, write_data2.length(), 50));
  ASSERT_TRUE(mem_driver_.Commit(mem_id_));
  ASSERT_EQ(0, strncmp(read_data2, write_data2.c_str(), write_data2.length()));

  // Update the first region.
  ASSERT_TRUE(mem_driver_.UpdateBytes(mem_id_, write_data1.c_str(),
                                      write_data1.length()));
  ASSERT_TRUE(mem_driver_.Commit(mem_id_));

  // Read from the first region.
  ASSERT_TRUE(mem_driver_.Read(mem_id_));
  ASSERT_TRUE(mem_driver_.ReadBytes(mem_id_, read_data1, write_data1.length()));
  ASSERT_TRUE(mem_driver_.Commit(mem_id_));
  ASSERT_EQ(0, strncmp(read_data1, write_data1.c_str(), write_data1.length()));
}

}  // namespace vts
}  // namespace android
