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

#include "hidl_handle_driver/VtsHidlHandleDriver.h"

#include <errno.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

using namespace std;

static constexpr const char* kTestFilePath = "/data/local/tmp/test.txt";

namespace android {
namespace vts {

// Unit test to test operations on hidl_memory_driver.
class HidlHandleDriverUnitTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // Register handle objects.
    client1_id_ = handle_driver_.CreateFileHandle(string(kTestFilePath),
                                                  O_RDWR | O_CREAT | O_TRUNC,
                                                  S_IRWXG, vector<int>());
    client2_id_ = handle_driver_.CreateFileHandle(string(kTestFilePath),
                                                  O_RDONLY, 0, vector<int>());
    ASSERT_NE(client1_id_, -1);
    ASSERT_NE(client2_id_, -1);
  }

  virtual void TearDown() {
    ASSERT_TRUE(handle_driver_.UnregisterHidlHandle(client1_id_));
    ASSERT_TRUE(handle_driver_.UnregisterHidlHandle(client2_id_));
    // Delete files for testing.
    remove(kTestFilePath);
  }

  VtsHidlHandleDriver handle_driver_;
  int client1_id_;
  int client2_id_;
};

// Tests trying to write to an invalid handle object.
TEST_F(HidlHandleDriverUnitTest, InvalidHandleId) {
  // Invalid ID: 42, tries to read 10 bytes.
  ASSERT_EQ(handle_driver_.WriteFile(42, nullptr, 10), -1);
}

// Tests reader doesn't have the permission to edit test.txt.
TEST_F(HidlHandleDriverUnitTest, ReaderInvalidWrite) {
  char write_data[10];
  ASSERT_EQ(handle_driver_.WriteFile(client2_id_, write_data, 10), -1);
}

// Tests unregistering a handle and using that handle after returns error.
TEST_F(HidlHandleDriverUnitTest, UnregisterHandle) {
  int new_id = handle_driver_.CreateFileHandle(string(kTestFilePath), O_RDONLY,
                                               0, vector<int>());
  // Reading 0 bytes should work, because handle object is found.
  ASSERT_EQ(handle_driver_.ReadFile(new_id, nullptr, 0), 0);

  // Now unregister the handle.
  ASSERT_TRUE(handle_driver_.UnregisterHidlHandle(new_id));
  // Read 0 bytes again, this time should fail because handle object is deleted.
  ASSERT_EQ(handle_driver_.ReadFile(new_id, nullptr, 0), -1);
}

// Tests simple read/write operations on the same file from two clients.
TEST_F(HidlHandleDriverUnitTest, SimpleReadWrite) {
  string write_data = "Hello World!";

  // Writer writes to test.txt.
  ASSERT_EQ(handle_driver_.WriteFile(
                client1_id_, static_cast<const void*>(write_data.c_str()),
                write_data.length()),
            write_data.length());

  // Reader reads from test.txt.
  char read_data[write_data.length()];
  ASSERT_EQ(handle_driver_.ReadFile(client2_id_, static_cast<void*>(read_data),
                                    write_data.length()),
            write_data.length());

  ASSERT_TRUE(write_data == string(read_data, write_data.length()));
}

// Tests large read/write interaction between reader and writer.
// Writer keeps adding "abcd" at the end of the file.
// After every iteration, reader should read back one extra repeated "abcd".
TEST_F(HidlHandleDriverUnitTest, LargeReadWrite) {
  static constexpr size_t NUM_ITERS = 10;
  const string write_data = "abcd";
  string curr_correct_data = "";
  char read_data[write_data.length() * NUM_ITERS];
  char* curr_read_data_ptr = read_data;

  for (int i = 0; i < NUM_ITERS; i++) {
    // Writer writes to test1.txt.
    ASSERT_EQ(handle_driver_.WriteFile(
                  client1_id_, static_cast<const void*>(write_data.c_str()),
                  write_data.length()),
              write_data.length());

    // Reader reads from test1.txt.
    ASSERT_EQ(handle_driver_.ReadFile(client2_id_,
                                      static_cast<void*>(curr_read_data_ptr),
                                      write_data.length()),
              write_data.length());

    string curr_read_data = string(read_data, write_data.length() * (i + 1));
    curr_correct_data += write_data;
    curr_read_data_ptr += write_data.length();
    ASSERT_TRUE(curr_read_data == curr_correct_data);
  }
}

}  // namespace vts
}  // namespace android
