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

#include "fmq_driver/VtsFmqDriver.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <fmq/MessageQueue.h>
#include <gtest/gtest.h>

using android::hardware::kSynchronizedReadWrite;
using android::hardware::kUnsynchronizedWrite;
using namespace std;

namespace android {
namespace vts {

// A test that initializes a single writer and a single reader.
class SyncReadWrites : public ::testing::Test {
 protected:
  virtual void SetUp() {
    static constexpr size_t NUM_ELEMS = 2048;

    // initialize a writer
    writer_id_ = manager_.CreateFmq<uint16_t, kSynchronizedReadWrite>(
        "uint16_t", NUM_ELEMS, false);
    ASSERT_NE(writer_id_, -1);

    // initialize a reader
    reader_id_ = manager_.CreateFmq<uint16_t, kSynchronizedReadWrite>(
        "uint16_t", writer_id_);
    ASSERT_NE(reader_id_, -1);
  }

  virtual void TearDown() {}

  VtsFmqDriver manager_;
  QueueId writer_id_;
  QueueId reader_id_;
};

// A test that initializes a single writer and a single reader.
// TODO: add tests for blocking between multiple queues later when there is more
// use case.
class BlockingReadWrites : public ::testing::Test {
 protected:
  virtual void SetUp() {
    static constexpr size_t NUM_ELEMS = 2048;

    // initialize a writer
    writer_id_ = manager_.CreateFmq<uint16_t, kSynchronizedReadWrite>(
        "uint16_t", NUM_ELEMS, true);
    ASSERT_NE(writer_id_, -1);

    // initialize a reader
    reader_id_ = manager_.CreateFmq<uint16_t, kSynchronizedReadWrite>(
        "uint16_t", writer_id_);
    ASSERT_NE(reader_id_, -1);
  }

  virtual void TearDown() {}

  VtsFmqDriver manager_;
  QueueId writer_id_;
  QueueId reader_id_;
};

// A test that initializes a single writer and two readers.
class UnsynchronizedWrites : public ::testing::Test {
 protected:
  virtual void SetUp() {
    static constexpr size_t NUM_ELEMS = 2048;

    // initialize a writer
    writer_id_ = manager_.CreateFmq<uint16_t, kUnsynchronizedWrite>(
        "uint16_t", NUM_ELEMS, false);
    ASSERT_NE(writer_id_, -1);

    // initialize two readers
    reader_id1_ = manager_.CreateFmq<uint16_t, kUnsynchronizedWrite>(
        "uint16_t", writer_id_);
    reader_id2_ = manager_.CreateFmq<uint16_t, kUnsynchronizedWrite>(
        "uint16_t", writer_id_);
    ASSERT_NE(reader_id1_, -1);
    ASSERT_NE(reader_id2_, -1);
  }

  virtual void TearDown() {}

  VtsFmqDriver manager_;
  QueueId writer_id_;
  QueueId reader_id1_;
  QueueId reader_id2_;
};

// Tests the reader and writer are set up correctly.
TEST_F(SyncReadWrites, SetupBasicTest) {
  static constexpr size_t NUM_ELEMS = 2048;

  // check if the writer has a valid queue
  ASSERT_TRUE((manager_.IsValid<uint16_t, kSynchronizedReadWrite>("uint16_t",
                                                                  writer_id_)));

  // check queue size on writer side
  size_t writer_queue_size;
  ASSERT_TRUE((manager_.GetQuantumCount<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", writer_id_, &writer_queue_size)));
  ASSERT_EQ(NUM_ELEMS, writer_queue_size);

  // check queue element size on writer side
  size_t writer_elem_size;
  ASSERT_TRUE((manager_.GetQuantumSize<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", writer_id_, &writer_elem_size)));
  ASSERT_EQ(sizeof(uint16_t), writer_elem_size);

  // check space available for writer
  size_t writer_available_writes;
  ASSERT_TRUE((manager_.AvailableToWrite<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", writer_id_, &writer_available_writes)));
  ASSERT_EQ(NUM_ELEMS, writer_available_writes);

  // check if the reader has a valid queue
  ASSERT_TRUE((manager_.IsValid<uint16_t, kSynchronizedReadWrite>("uint16_t",
                                                                  reader_id_)));

  // check queue size on reader side
  size_t reader_queue_size;
  ASSERT_TRUE((manager_.GetQuantumCount<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", reader_id_, &reader_queue_size)));
  ASSERT_EQ(NUM_ELEMS, reader_queue_size);

  // check queue element size on reader side
  size_t reader_elem_size;
  ASSERT_TRUE((manager_.GetQuantumSize<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", reader_id_, &reader_elem_size)));
  ASSERT_EQ(sizeof(uint16_t), reader_elem_size);

  // check items available for reader
  size_t reader_available_reads;
  ASSERT_TRUE((manager_.AvailableToRead<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", reader_id_, &reader_available_reads)));
  ASSERT_EQ(0, reader_available_reads);
}

// util method to initialize data
void InitData(uint16_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    data[i] = rand() % 100 + 1;  // a random value between 1 and 100
  }
}

// Tests a basic writer and reader interaction.
// Reader reads back data written by the writer correctly.
TEST_F(SyncReadWrites, ReadWriteSuccessTest) {
  static constexpr size_t DATA_SIZE = 64;
  uint16_t write_data[DATA_SIZE];
  uint16_t read_data[DATA_SIZE];

  // initialize the data to transfer
  InitData(write_data, DATA_SIZE);

  // writer should succeed
  ASSERT_TRUE((manager_.WriteFmq<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", writer_id_, write_data, DATA_SIZE)));

  // reader should succeed
  ASSERT_TRUE((manager_.ReadFmq<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", reader_id_, read_data, DATA_SIZE)));

  // check if the data is read back correctly
  ASSERT_EQ(0, memcmp(write_data, read_data, DATA_SIZE * sizeof(uint16_t)));
}

// Tests reading from an empty queue.
TEST_F(SyncReadWrites, ReadEmpty) {
  static constexpr size_t DATA_SIZE = 64;
  uint16_t read_data[DATA_SIZE];

  // attempt to read from an empty queue
  ASSERT_FALSE((manager_.ReadFmq<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", reader_id_, read_data, DATA_SIZE)));
}

// Tests writing to a full queue.
TEST_F(SyncReadWrites, WriteFull) {
  static constexpr size_t DATA_SIZE = 2048;
  uint16_t write_data[DATA_SIZE];
  uint16_t read_data[DATA_SIZE];

  // initialize the data to transfer
  InitData(write_data, DATA_SIZE);

  // This write succeeds, filling up the queue
  ASSERT_TRUE((manager_.WriteFmq<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", writer_id_, write_data, DATA_SIZE)));

  // This write fails, queue is full
  ASSERT_FALSE((manager_.WriteFmq<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", writer_id_, write_data, DATA_SIZE)));

  // checks space available is 0
  size_t writer_available_writes;
  ASSERT_TRUE((manager_.AvailableToWrite<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", writer_id_, &writer_available_writes)));
  ASSERT_EQ(0, writer_available_writes);

  // reader succeeds, reads the entire queue back correctly
  ASSERT_TRUE((manager_.ReadFmq<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", reader_id_, read_data, DATA_SIZE)));

  ASSERT_EQ(0, memcmp(write_data, read_data, DATA_SIZE * sizeof(uint16_t)));
}

// Attempt to write more than the size of the queue.
TEST_F(SyncReadWrites, WriteTooLarge) {
  static constexpr size_t LARGE_DATA_SIZE = 2049;
  uint16_t write_data[LARGE_DATA_SIZE];

  // initialize data to transfer
  InitData(write_data, LARGE_DATA_SIZE);

  // write more than the queue size
  ASSERT_FALSE((manager_.WriteFmq<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", writer_id_, write_data, LARGE_DATA_SIZE)));
}

// Pass the wrong type.
TEST_F(SyncReadWrites, WrongType) {
  static constexpr size_t DATA_SIZE = 2;
  uint16_t write_data[DATA_SIZE];

  // initialize data to transfer
  InitData(write_data, DATA_SIZE);

  // attempt to write uint32_t type
  ASSERT_FALSE((manager_.WriteFmq<uint16_t, kSynchronizedReadWrite>(
      "uint32_t", writer_id_, write_data, DATA_SIZE)));
}

// Tests consecutive interaction between writer and reader.
// Reader immediately reads back what writer writes.
TEST_F(SyncReadWrites, ConsecutiveReadWrite) {
  static constexpr size_t DATA_SIZE = 64;
  static constexpr size_t BATCH_SIZE = 10;
  uint16_t read_data[DATA_SIZE];
  uint16_t write_data[DATA_SIZE];

  // 10 consecutive writes and reads
  for (size_t i = 0; i < BATCH_SIZE; i++) {
    InitData(write_data, DATA_SIZE);
    ASSERT_TRUE((manager_.WriteFmq<uint16_t, kSynchronizedReadWrite>(
        "uint16_t", writer_id_, write_data, DATA_SIZE)));

    ASSERT_TRUE((manager_.ReadFmq<uint16_t, kSynchronizedReadWrite>(
        "uint16_t", reader_id_, read_data, DATA_SIZE)));
    ASSERT_EQ(0, memcmp(write_data, read_data, DATA_SIZE * sizeof(uint16_t)));
  }

  // no more available to read
  size_t reader_available_reads;
  ASSERT_TRUE((manager_.AvailableToRead<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", reader_id_, &reader_available_reads)));
  ASSERT_EQ(0, reader_available_reads);
}

// Tests reader waiting for data to be available.
// Writer waits for 0.05s and writes the data.
// Reader blocks for at most 0.1s and reads the data if it is available.
TEST_F(BlockingReadWrites, ReadWriteSuccess) {
  static constexpr size_t DATA_SIZE = 64;

  uint16_t read_data[DATA_SIZE];
  uint16_t write_data[DATA_SIZE];

  // initialize data to transfer
  InitData(write_data, DATA_SIZE);

  pid_t pid = fork();
  if (pid == 0) {  // child process is a reader, blocking for at most 0.1s.
    ASSERT_TRUE((manager_.ReadFmqBlocking<uint16_t, kSynchronizedReadWrite>(
        "uint16_t", reader_id_, read_data, DATA_SIZE, 100 * 1000000)));
    ASSERT_EQ(0, memcmp(write_data, read_data, DATA_SIZE * sizeof(uint16_t)));
    exit(0);
  } else if (pid > 0) {
    // parent process is a writer, waits for 0.05s and writes.
    struct timespec writer_wait_time = {0, 50 * 1000000};
    nanosleep(&writer_wait_time, NULL);
    ASSERT_TRUE((manager_.WriteFmqBlocking<uint16_t, kSynchronizedReadWrite>(
        "uint16_t", writer_id_, write_data, DATA_SIZE, 1000000)));
  }
}

// Tests reader blocking times out.
TEST_F(BlockingReadWrites, BlockingTimeOut) {
  static constexpr size_t DATA_SIZE = 64;
  uint16_t read_data[DATA_SIZE];

  // block for 0.05s, timeout
  ASSERT_FALSE((manager_.ReadFmqBlocking<uint16_t, kSynchronizedReadWrite>(
      "uint16_t", reader_id_, read_data, DATA_SIZE, 50 * 1000000)));
}

// Tests two readers can both read back what writer writes correctly.
TEST_F(UnsynchronizedWrites, ReadWriteSuccess) {
  static constexpr size_t DATA_SIZE = 64;
  uint16_t write_data[DATA_SIZE];
  uint16_t read_data1[DATA_SIZE];
  uint16_t read_data2[DATA_SIZE];

  // initialize data to transfer
  InitData(write_data, DATA_SIZE);

  // writer writes 64 items
  ASSERT_TRUE((manager_.WriteFmq<uint16_t, kUnsynchronizedWrite>(
      "uint16_t", writer_id_, write_data, DATA_SIZE)));

  // reader 1 reads back data correctly
  ASSERT_TRUE((manager_.ReadFmq<uint16_t, kUnsynchronizedWrite>(
      "uint16_t", reader_id1_, read_data1, DATA_SIZE)));
  ASSERT_EQ(0, memcmp(write_data, read_data1, DATA_SIZE * sizeof(uint16_t)));

  // reader 2 reads back data correctly
  ASSERT_TRUE((manager_.ReadFmq<uint16_t, kUnsynchronizedWrite>(
      "uint16_t", reader_id2_, read_data2, DATA_SIZE)));
  ASSERT_EQ(0, memcmp(write_data, read_data2, DATA_SIZE * sizeof(uint16_t)));
}

// Tests that blocking is not allowed on unsynchronized queue.
TEST_F(UnsynchronizedWrites, IllegalBlocking) {
  static constexpr size_t DATA_SIZE = 64;
  uint16_t write_data[DATA_SIZE];

  // initialize data to transfer
  InitData(write_data, DATA_SIZE);

  // should fail immediately, instead of blocking
  ASSERT_FALSE((manager_.WriteFmqBlocking<uint16_t, kUnsynchronizedWrite>(
      "uint16_t", writer_id_, write_data, DATA_SIZE, 1000000)));
}

}  // namespace vts
}  // namespace android
