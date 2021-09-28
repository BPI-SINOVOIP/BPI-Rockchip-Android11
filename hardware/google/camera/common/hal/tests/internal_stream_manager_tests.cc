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

#define LOG_TAG "InternalStreamManagerTests"
#include <log/log.h>

#include <gtest/gtest.h>
#include <hal_types.h>
#include <hardware/gralloc.h>
#include <internal_stream_manager.h>

namespace android {
namespace google_camera_hal {

static const uint32_t kDataBytes = 256;
static const uint32_t kNumEntries = 10;

// Preview stream template used in the test.
static constexpr Stream kPreviewStreamTemplate{
    .stream_type = StreamType::kOutput,
    .width = 1920,
    .height = 1080,
    .format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
    .usage = GRALLOC_USAGE_HW_TEXTURE,
    .rotation = StreamRotation::kRotation0,
};

// Video stream template used in the test.
static constexpr Stream kVideoStreamTemplate{
    .stream_type = StreamType::kOutput,
    .width = 3840,
    .height = 2160,
    .format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
    .usage = GRALLOC_USAGE_HW_VIDEO_ENCODER,
    .rotation = StreamRotation::kRotation0,
};

// Raw stream template used in the test.
static constexpr Stream kRawStreamTemplate{
    .stream_type = StreamType::kOutput,
    .width = 4022,
    .height = 3024,
    .format = HAL_PIXEL_FORMAT_RAW10,
    .usage = 0,
    .rotation = StreamRotation::kRotation0,
};

// Preview HAL stream template used in the test.
static constexpr HalStream kPreviewHalStreamTemplate{
    .override_format = HAL_PIXEL_FORMAT_YV12,
    .producer_usage = GRALLOC_USAGE_HW_CAMERA_WRITE,
    .max_buffers = 4,
};

// Video HAL stream template used in the test.
static constexpr HalStream kVideoHalStreamTemplate{
    .override_format = HAL_PIXEL_FORMAT_YV12,
    .producer_usage = GRALLOC_USAGE_HW_CAMERA_WRITE,
    .max_buffers = 4,
};

// Raw HAL stream template used in the test.
static constexpr HalStream kRawHalStreamTemplate{
    .override_format = HAL_PIXEL_FORMAT_RAW10,
    .producer_usage = GRALLOC_USAGE_HW_CAMERA_WRITE,
    .max_buffers = 16,
};

// Additional number of buffers to allocate.
static constexpr uint32_t kNumAdditionalBuffers = 2;

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

TEST(InternalStreamManagerTests, Create) {
  auto stream_manager = InternalStreamManager::Create();
  EXPECT_NE(stream_manager, nullptr);
}

TEST(InternalStreamManagerTests, RegisterNewInternalStream) {
  auto stream_manager = InternalStreamManager::Create();
  ASSERT_NE(stream_manager, nullptr);

  EXPECT_EQ(stream_manager->RegisterNewInternalStream(kPreviewStreamTemplate,
                                                      /*stream_id=*/nullptr),
            BAD_VALUE)
      << "Passing a nullptr stream should fail";

  int32_t preview_stream_id = -1;
  int32_t video_stream_id = -1;

  EXPECT_EQ(stream_manager->RegisterNewInternalStream(kPreviewStreamTemplate,
                                                      &preview_stream_id),
            OK);
  EXPECT_EQ(stream_manager->RegisterNewInternalStream(kVideoStreamTemplate,
                                                      &video_stream_id),
            OK);
  EXPECT_NE(preview_stream_id, video_stream_id);
}

TEST(InternalStreamManagerTests, AllocateBuffers) {
  auto stream_manager = InternalStreamManager::Create();
  ASSERT_NE(stream_manager, nullptr);

  HalStream preview_hal_stream = kPreviewHalStreamTemplate;
  HalStream video_hal_stream = kVideoHalStreamTemplate;

  EXPECT_NE(stream_manager->AllocateBuffers(preview_hal_stream), OK)
      << "Allocating buffers for unregistered stream should fail";

  // Allocate preview and video stream.
  ASSERT_EQ(stream_manager->RegisterNewInternalStream(kPreviewStreamTemplate,
                                                      &preview_hal_stream.id),
            OK);
  EXPECT_EQ(stream_manager->AllocateBuffers(preview_hal_stream), OK);

  ASSERT_EQ(stream_manager->RegisterNewInternalStream(kVideoStreamTemplate,
                                                      &video_hal_stream.id),
            OK);
  EXPECT_EQ(stream_manager->AllocateBuffers(video_hal_stream), OK);

  EXPECT_NE(stream_manager->AllocateBuffers(preview_hal_stream), OK)
      << "Allocating buffers for the same stream again should fail";

  stream_manager->FreeStream(preview_hal_stream.id);
  EXPECT_NE(stream_manager->AllocateBuffers(preview_hal_stream), OK)
      << "Allocating buffers for a freed stream should fail";
}

TEST(InternalStreamManagerTests, FreeStream) {
  auto stream_manager = InternalStreamManager::Create();
  ASSERT_NE(stream_manager, nullptr);

  HalStream preview_hal_stream = kPreviewHalStreamTemplate;

  // Free an invalid stream.
  stream_manager->FreeStream(/*stream_id=*/-1);

  // Free a registered stream.
  ASSERT_EQ(stream_manager->RegisterNewInternalStream(kPreviewStreamTemplate,
                                                      &preview_hal_stream.id),
            OK);
  stream_manager->FreeStream(preview_hal_stream.id);

  // Free an allocated stream.
  ASSERT_EQ(stream_manager->RegisterNewInternalStream(kPreviewStreamTemplate,
                                                      &preview_hal_stream.id),
            OK);
  ASSERT_EQ(stream_manager->AllocateBuffers(preview_hal_stream), OK);
  stream_manager->FreeStream(preview_hal_stream.id);
}

TEST(InternalStreamManagerTests, GetStreamBuffer) {
  auto stream_manager = InternalStreamManager::Create();
  ASSERT_NE(stream_manager, nullptr);

  HalStream preview_hal_stream = kPreviewHalStreamTemplate;

  // Get buffer from an invalid stream.
  StreamBuffer dummy_buffer;
  EXPECT_NE(stream_manager->GetStreamBuffer(/*stream_id=*/-1, &dummy_buffer), OK)
      << "Getting a buffer from an invalid stream should fail";

  // Register and allocate buffers.
  ASSERT_EQ(stream_manager->RegisterNewInternalStream(kPreviewStreamTemplate,
                                                      &preview_hal_stream.id),
            OK);
  ASSERT_EQ(stream_manager->AllocateBuffers(preview_hal_stream,
                                            kNumAdditionalBuffers),
            OK);

  EXPECT_NE(stream_manager->GetStreamBuffer(preview_hal_stream.id, nullptr), OK)
      << "Getting a buffer with nullptr should fail";

  // Verify we can get a number of buffers we allocated.
  uint32_t num_buffers = preview_hal_stream.max_buffers + kNumAdditionalBuffers;
  std::vector<StreamBuffer> buffers(num_buffers);
  for (auto& buffer : buffers) {
    EXPECT_EQ(stream_manager->GetStreamBuffer(preview_hal_stream.id, &buffer),
              OK);
    EXPECT_NE(buffer.buffer, kInvalidBufferHandle) << "Buffer should be valid";
  }
}

TEST(InternalStreamManagerTests, ReturnStreamBuffer) {
  auto stream_manager = InternalStreamManager::Create();
  ASSERT_NE(stream_manager, nullptr);

  HalStream preview_hal_stream = kPreviewHalStreamTemplate;

  // Register and allocate buffers.
  ASSERT_EQ(stream_manager->RegisterNewInternalStream(kPreviewStreamTemplate,
                                                      &preview_hal_stream.id),
            OK);
  ASSERT_EQ(stream_manager->AllocateBuffers(preview_hal_stream), OK);

  // Get all buffers.
  std::vector<StreamBuffer> buffers(preview_hal_stream.max_buffers);
  for (auto& buffer : buffers) {
    ASSERT_EQ(stream_manager->GetStreamBuffer(preview_hal_stream.id, &buffer),
              OK);
  }

  // Return all buffers.
  for (auto& buffer : buffers) {
    EXPECT_EQ(stream_manager->ReturnStreamBuffer(buffer), OK);
  }

  EXPECT_NE(stream_manager->ReturnStreamBuffer(buffers[0]), OK)
      << "Returning the same buffer again should fail";

  StreamBuffer invalid_buffer = {
      .stream_id = -1,
  };
  EXPECT_NE(stream_manager->ReturnStreamBuffer(invalid_buffer), OK)
      << "Returning a invalid buffer should fail";
}

TEST(InternalStreamManagerTests, ReturnFilledBuffer) {
  auto stream_manager = InternalStreamManager::Create();
  ASSERT_NE(stream_manager, nullptr);

  HalStream preview_hal_stream = kPreviewHalStreamTemplate;

  // Register and allocate buffers.
  ASSERT_EQ(stream_manager->RegisterNewInternalStream(kPreviewStreamTemplate,
                                                      &preview_hal_stream.id),
            OK);
  ASSERT_EQ(stream_manager->AllocateBuffers(preview_hal_stream), OK);

  // Get all buffers.
  std::vector<StreamBuffer> buffers(preview_hal_stream.max_buffers);
  for (auto& buffer : buffers) {
    ASSERT_EQ(stream_manager->GetStreamBuffer(preview_hal_stream.id, &buffer),
              OK);
  }

  // Return all filled buffers and metadata.
  uint32_t frame_number = 1;
  int32_t invalid_stream_id = -1;
  for (auto& buffer : buffers) {
    auto metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
    EXPECT_EQ(stream_manager->ReturnFilledBuffer(frame_number, buffer), OK);
    // Return invalid stream id will return BAD_VALUE
    EXPECT_EQ(stream_manager->ReturnMetadata(invalid_stream_id, frame_number,
                                             metadata.get()),
              BAD_VALUE);
    EXPECT_EQ(stream_manager->ReturnMetadata(preview_hal_stream.id,
                                             frame_number, metadata.get()),
              OK);
    frame_number++;
  }
}

TEST(InternalStreamManagerTests, GetMostRecentStreamBuffer) {
  auto stream_manager = InternalStreamManager::Create();
  ASSERT_NE(stream_manager, nullptr);

  HalStream raw_hal_stream = kRawHalStreamTemplate;

  // Register and allocate buffers.
  ASSERT_EQ(stream_manager->RegisterNewInternalStream(kRawStreamTemplate,
                                                      &raw_hal_stream.id),
            OK);
  ASSERT_EQ(stream_manager->AllocateBuffers(raw_hal_stream), OK);

  StreamBuffer stream_buffer[kRawHalStreamTemplate.max_buffers];
  uint32_t frame_index = 0;
  status_t res;
  // Get empty buffer and then fill buffer and metadata
  for (uint32_t i = 0; i < kRawHalStreamTemplate.max_buffers; i++) {
    ASSERT_EQ(
        stream_manager->GetStreamBuffer(raw_hal_stream.id, &stream_buffer[i]),
        OK);

    res = stream_manager->ReturnFilledBuffer(frame_index, stream_buffer[i]);
    ASSERT_EQ(res, OK) << "ReturnFilledBuffer failed: " << strerror(res);

    auto metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
    SetMetadata(metadata);
    res = stream_manager->ReturnMetadata(raw_hal_stream.id, frame_index,
                                         metadata.get());
    ASSERT_EQ(res, OK) << "ReturnMetadata failed: " << strerror(res);
    frame_index++;
  }

  std::vector<StreamBuffer> input_buffers;
  std::vector<std::unique_ptr<HalCameraMetadata>> input_buffer_metadata;
  res = stream_manager->GetMostRecentStreamBuffer(
      raw_hal_stream.id, &input_buffers, &input_buffer_metadata,
      /*payload_frames*/ 1);
  ASSERT_EQ(res, OK) << "GetMostRecentZslBuffers failed.";

  // Pending buffer is not empty after call GetMostRecentStreamBuffer
  bool empty = stream_manager->IsPendingBufferEmpty(raw_hal_stream.id);
  ASSERT_EQ(empty, false) << "Pending buffer is empty";

  res = stream_manager->ReturnZslStreamBuffers(frame_index, raw_hal_stream.id);
  ASSERT_EQ(res, OK) << "ReturnZslStreamBuffers failed.";

  // Pending buffer is empty after call ReturnZslStreamBuffers
  empty = stream_manager->IsPendingBufferEmpty(raw_hal_stream.id);
  ASSERT_EQ(empty, true) << "Pending buffer is not empty";
}

}  // namespace google_camera_hal
}  // namespace android
