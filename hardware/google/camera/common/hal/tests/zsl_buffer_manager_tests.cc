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

#define LOG_TAG "ZslBufferManagerTests"
#include <log/log.h>

#include <gtest/gtest.h>
#include <zsl_buffer_manager.h>

namespace android {
namespace google_camera_hal {

static const uint32_t kDataBytes = 256;
static const uint32_t kNumEntries = 10;
static const uint32_t kMaxBufferDepth = 16;

static constexpr HalBufferDescriptor kRawBufferDescriptor = {
    .width = 4032,
    .height = 3024,
    .format = HAL_PIXEL_FORMAT_RAW10,
    .immediate_num_buffers = kMaxBufferDepth,
    .max_num_buffers = kMaxBufferDepth,
};

static void SetMetadata(std::unique_ptr<HalCameraMetadata>& hal_metadata) {
  // Set current BOOT_TIME timestamp in nanoseconds
  struct timespec ts;
  if (clock_gettime(CLOCK_BOOTTIME, &ts) == 0) {
    static const int64_t kNsPerSec = 1000000000;
    int64_t buffer_timestamp = ts.tv_sec * kNsPerSec + ts.tv_nsec;
    status_t res =
        hal_metadata->Set(ANDROID_SENSOR_TIMESTAMP, &buffer_timestamp, 1);
    ASSERT_EQ(res, OK) << "Set ANDROID_SENSOR_TIMESTAMP failed";
  }
}

// Test ZslBufferManager AllocateBuffer.
TEST(ZslBufferManagerTests, AllocateBuffer) {
  auto manager = std::make_unique<ZslBufferManager>();
  ASSERT_NE(manager, nullptr) << "Create ZslBufferManager failed.";

  status_t res = manager->AllocateBuffers(kRawBufferDescriptor);
  ASSERT_EQ(res, OK) << "AllocateBuffers failed: " << strerror(res);
}

// Test ZslBufferManager GetEmptyBuffer.
TEST(ZslBufferManagerTests, GetEmptyBuffer) {
  auto manager = std::make_unique<ZslBufferManager>();
  ASSERT_NE(manager, nullptr) << "Create ZslBufferManager failed.";

  status_t res = manager->AllocateBuffers(kRawBufferDescriptor);
  ASSERT_EQ(res, OK) << "AllocateBuffers failed: " << strerror(res);
  buffer_handle_t buffer;
  for (uint32_t i = 0; i < kMaxBufferDepth; i++) {
    buffer = manager->GetEmptyBuffer();
    ASSERT_NE(buffer, kInvalidBufferHandle) << "GetEmptyBuffer failed: " << i;
  }
  // If get buffer number is greater than allocation, it will get nullptr.
  buffer = manager->GetEmptyBuffer();
  ASSERT_EQ(buffer, kInvalidBufferHandle) << "GetEmptyBuffer is not nullptr";
}

// Test ZslBufferManager fill buffers.
// For realtime pipeline case, get the buffer
// and then return buffer with metadata.
TEST(ZslBufferManagerTests, FillBuffers) {
  static const uint32_t kTestCycle = 50;
  auto manager = std::make_unique<ZslBufferManager>();
  ASSERT_NE(manager, nullptr) << "Creating ZslBufferManager failed.";

  status_t res = manager->AllocateBuffers(kRawBufferDescriptor);
  ASSERT_EQ(res, OK) << "AllocateBuffers failed: " << strerror(res);

  // Fill the zsl buffers
  for (uint32_t i = 0; i < kTestCycle; i++) {
    // Get empty buffer and check whether buffer is nullptr or not
    buffer_handle_t empty_buffer = manager->GetEmptyBuffer();
    ASSERT_NE(empty_buffer, kInvalidBufferHandle)
        << "GetEmptyBuffer failed at: " << i;

    StreamBuffer stream_buffer;
    stream_buffer.buffer = empty_buffer;
    // Return the buffer with metadata.
    res = manager->ReturnFilledBuffer(i, stream_buffer);
    ASSERT_EQ(res, OK) << "ReturnFilledBuffer failed: " << strerror(res);
    auto metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
    res = manager->ReturnMetadata(i, metadata.get());
    ASSERT_EQ(res, OK) << "ReturnMetadata failed: " << strerror(res);
  }
}

// Test ZslBufferManager GetMostRecentZslBuffers and ReturnZslBuffers
// For offline pipeline case, get most recent filled zsl buffers
// and then return zsl buffers.
TEST(ZslBufferManagerTests, GetRecentBuffers) {
  static const uint32_t kTestCycle = 2;
  static const uint32_t kGetTotalBufferNum = 10;
  static const uint32_t kRequireMinBufferNum = 3;
  auto manager = std::make_unique<ZslBufferManager>();
  ASSERT_NE(manager, nullptr) << "Creating ZslBufferManager failed.";

  status_t res = manager->AllocateBuffers(kRawBufferDescriptor);
  ASSERT_EQ(res, OK) << "AllocateBuffers failed: " << strerror(res);

  StreamBuffer stream_buffer[kMaxBufferDepth];

  // Fill the zsl buffers
  uint32_t frame_index = 0;
  for (uint32_t j = 0; j < kTestCycle; j++) {
    // Fill the zsl buffers
    for (uint32_t i = 0; i < kMaxBufferDepth; i++) {
      // Get empty buffer and check whether buffer is nullptr or not
      buffer_handle_t empty_buffer = manager->GetEmptyBuffer();
      ASSERT_NE(empty_buffer, kInvalidBufferHandle)
          << "GetEmptyBuffer failed at: " << i;
      stream_buffer[i].buffer = empty_buffer;
      // Return the buffer with metadata.
      res = manager->ReturnFilledBuffer(frame_index, stream_buffer[i]);
      ASSERT_EQ(res, OK) << "ReturnFilledBuffer failed: " << strerror(res);

      auto metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
      SetMetadata(metadata);
      res = manager->ReturnMetadata(frame_index, metadata.get());
      ASSERT_EQ(res, OK) << "ReturnMetadata failed: " << strerror(res);
      frame_index++;
    }

    // Get filled zsl buffers and return filled buffers
    std::vector<ZslBufferManager::ZslBuffer> filled_buffers;
    manager->GetMostRecentZslBuffers(&filled_buffers, kGetTotalBufferNum,
                                     kRequireMinBufferNum);
    ASSERT_EQ(filled_buffers.size(), (uint32_t)kGetTotalBufferNum)
        << "GetMostRecentZslBuffers failed.";

    // Check the zsl buffer frame number meet the most recent
    // For the test case (kTestCycle = 0), fill frame_number (0 ~ 15)
    // And GetMostRecentZslBuffers will get frame_number (6 ~ 15)
    for (uint32_t k = 0; k < kGetTotalBufferNum; k++) {
      ASSERT_EQ(filled_buffers[k].frame_number,
                frame_index - kGetTotalBufferNum + k)
          << "GetMostRecentZslBuffers data failed at " << k;
    }
    manager->ReturnZslBuffers(std::move(filled_buffers));
  }
}

// Test ZslBufferManager ReturnMetadata.
// If allocated_metadata_ size is greater than kMaxAllcatedMetadataSize(100),
// ReturnMetadata() will return error and not allocate new metadata.
TEST(ZslBufferManagerTests, ReturnMetadata) {
  static const uint32_t kTestCycle = 100;
  auto manager = std::make_unique<ZslBufferManager>();
  ASSERT_NE(manager, nullptr) << "Creating ZslBufferManager failed.";
  status_t res;
  // Normal ReturnMetadata
  // when zsl buffer manager allocated_metadata_ size < 100
  for (uint32_t i = 0; i < kTestCycle; i++) {
    auto metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
    res = manager->ReturnMetadata(i, metadata.get());
    ASSERT_EQ(res, OK) << "ReturnMetadata failed: " << strerror(res)
                       << " at:" << i;
  }

  // Abnormal ReturnMetadata
  // when zsl buffer manager allocated_metadata_ size >= 100
  for (uint32_t i = kTestCycle; i < kTestCycle + 20; i++) {
    auto metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
    res = manager->ReturnMetadata(i, metadata.get());
    ASSERT_EQ(res, OK) << "ReturnMetadata failed: " << strerror(res)
                       << " at:" << i;
  }
}

TEST(ZslBufferManagerTests, PendingBuffer) {
  auto manager = std::make_unique<ZslBufferManager>();
  ASSERT_NE(manager, nullptr) << "Creating ZslBufferManager failed.";
  bool empty = manager->IsPendingBufferEmpty();
  ASSERT_EQ(empty, true) << "Pending buffer is not empty.";

  std::vector<ZslBufferManager::ZslBuffer> filled_buffers;
  ZslBufferManager::ZslBuffer zsl_buffer;
  filled_buffers.push_back(std::move(zsl_buffer));
  manager->AddPendingBuffers(filled_buffers);

  // Pending buffer is not empty after call AddPendingBuffers.
  empty = manager->IsPendingBufferEmpty();
  ASSERT_EQ(empty, false) << "Pending buffer is empty after AddPendingBuffers.";

  status_t res = manager->CleanPendingBuffers(&filled_buffers);
  ASSERT_EQ(res, OK) << "CleanPendingBuffers failed.";

  empty = manager->IsPendingBufferEmpty();
  ASSERT_EQ(empty, true)
      << "Pending buffer is not empty after CleanPendingBuffers.";
}

}  // namespace google_camera_hal
}  // namespace android
