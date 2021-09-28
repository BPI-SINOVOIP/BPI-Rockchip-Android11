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

#define LOG_TAG "ResultDispatcherTests"
#include <log/log.h>

#include <cutils/properties.h>
#include <gtest/gtest.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "result_dispatcher.h"

namespace android {
namespace google_camera_hal {

class ResultDispatcherTests : public ::testing::Test {
 protected:
  // TODO(b/143902331): Test partial results.
  static constexpr uint32_t kPartialResult = 1;
  static constexpr uint32_t kResultWaitTimeMs = 30;

  // Defined a result metadata received from the result dispatcher.
  struct ReceivedResultMetadata {
    uint32_t frame_number = 0;
    std::unique_ptr<HalCameraMetadata> result_metadata;
  };

  // Defined a buffer received from the result dispatcher.
  struct ReceivedBuffer {
    uint32_t frame_number = 0;
    StreamBuffer buffer;
  };

  void SetUp() override {
    // Skip test if product is unsupported.
    char product_name[PROPERTY_VALUE_MAX];
    std::unordered_set<std::string> const supported_product_list{
        "blueline", "crosshatch", "flame", "coral", "needlefish"};
    property_get("ro.build.product", product_name, "");
    bool product_support_test =
        supported_product_list.find(std::string{product_name}) !=
        supported_product_list.end();
    if (!product_support_test) {
      GTEST_SKIP();
    }

    result_dispatcher_ = ResultDispatcher::Create(
        kPartialResult,
        [this](std::unique_ptr<CaptureResult> result) {
          ProcessCaptureResult(std::move(result));
        },
        [this](const NotifyMessage& message) { Notify(message); });

    ASSERT_NE(result_dispatcher_, nullptr)
        << "Creating ResultDispatcher failed";
  }

  // Invoked when receiving a shutter from the result dispatcher.
  void Notify(const NotifyMessage& message) {
    if (message.type != MessageType::kShutter) {
      EXPECT_EQ(message.type, MessageType::kShutter)
          << "Received a non-shutter message.";
      return;
    }

    std::lock_guard<std::mutex> lock(callback_lock_);
    received_shutters_.push_back({message.message.shutter.frame_number,
                                  message.message.shutter.timestamp_ns});
    callback_condition_.notify_one();
  }

  // Invoked when receiving a capture result from the result dispatcher.
  void ProcessCaptureResult(std::unique_ptr<CaptureResult> new_result) {
    if (new_result == nullptr) {
      EXPECT_NE(new_result, nullptr);
      return;
    }

    uint32_t frame_number = new_result->frame_number;

    std::lock_guard<std::mutex> lock(callback_lock_);
    if (new_result->result_metadata != nullptr) {
      ASSERT_EQ(new_result->partial_result, kPartialResult);

      ReceivedResultMetadata metadata;
      metadata.frame_number = frame_number;
      metadata.result_metadata = std::move(new_result->result_metadata);
      received_result_metadata_.push_back(std::move(metadata));
    }

    for (auto& buffer : new_result->output_buffers) {
      ProcessReceivedBuffer(frame_number, buffer);
    }

    for (auto& buffer : new_result->input_buffers) {
      ProcessReceivedBuffer(frame_number, buffer);
    }
    callback_condition_.notify_one();
  }

  // Add a bufffer to the received buffer queue.
  void ProcessReceivedBuffer(uint32_t frame_number, const StreamBuffer& buffer) {
    auto buffers_it = stream_received_buffers_map_.find(buffer.stream_id);
    if (buffers_it == stream_received_buffers_map_.end()) {
      stream_received_buffers_map_.emplace(buffer.stream_id,
                                           std::vector<ReceivedBuffer>{});
      buffers_it = stream_received_buffers_map_.find(buffer.stream_id);
      ASSERT_NE(buffers_it, stream_received_buffers_map_.end());
    }

    buffers_it->second.push_back({frame_number, buffer});
  }

  // Protected by callback_lock_.
  bool IsShutterReceivedLocked(uint32_t frame_number, uint64_t timestamp_ns) {
    for (auto& shutter : received_shutters_) {
      if (shutter.frame_number == frame_number &&
          shutter.timestamp_ns == timestamp_ns) {
        return true;
      }
    }

    return false;
  }

  // Wait for a shutter from the result dispatcher.
  status_t WaitForShutter(uint32_t frame_number, uint64_t timestamp_ns) {
    std::unique_lock<std::mutex> lock(callback_lock_);
    bool received = callback_condition_.wait_for(
        lock, std::chrono::milliseconds(kResultWaitTimeMs),
        [&] { return IsShutterReceivedLocked(frame_number, timestamp_ns); });

    return received ? OK : TIMED_OUT;
  }

  // Protected by callback_lock_.
  bool IsResultMetadataReceivedLocked(uint32_t frame_number) {
    for (auto& metadata : received_result_metadata_) {
      if (metadata.frame_number == frame_number) {
        return true;
      }
    }

    return false;
  }

  status_t WaitForResultMetadata(uint32_t frame_number) {
    std::unique_lock<std::mutex> lock(callback_lock_);
    bool received = callback_condition_.wait_for(
        lock, std::chrono::milliseconds(kResultWaitTimeMs),
        [&] { return IsResultMetadataReceivedLocked(frame_number); });

    return received ? OK : TIMED_OUT;
  }

  // Protected by callback_lock_.
  bool IsOutputBufferReceivedLocked(uint32_t frame_number, int32_t stream_id) {
    auto buffers_it = stream_received_buffers_map_.find(stream_id);
    if (buffers_it == stream_received_buffers_map_.end()) {
      return false;
    }

    for (auto& buffer : buffers_it->second) {
      if (buffer.frame_number == frame_number) {
        return true;
      }
    }

    return false;
  }

  status_t WaitForOuptutBuffer(uint32_t frame_number, int32_t stream_id) {
    std::unique_lock<std::mutex> lock(callback_lock_);
    bool received = callback_condition_.wait_for(
        lock, std::chrono::milliseconds(kResultWaitTimeMs),
        [&] { return IsOutputBufferReceivedLocked(frame_number, stream_id); });

    return received ? OK : TIMED_OUT;
  }

  // Verify received shutters are sorted by frame numbers.
  void VerifyShuttersOrder() {
    std::lock_guard<std::mutex> lock(callback_lock_);

    auto shutter = received_shutters_.begin();
    if (shutter == received_shutters_.end()) {
      return;
    }

    while (1) {
      auto next_shutter = shutter;
      next_shutter++;
      if (next_shutter == received_shutters_.end()) {
        return;
      }

      EXPECT_LT(shutter->frame_number, next_shutter->frame_number);
      shutter = next_shutter;
    }
  }

  // Verify received result metadata are sorted by frame numbers.
  void VerifyResultMetadataOrder() {
    std::lock_guard<std::mutex> lock(callback_lock_);

    auto metadata = received_result_metadata_.begin();
    if (metadata == received_result_metadata_.end()) {
      return;
    }

    while (1) {
      auto next_metadata = metadata;
      next_metadata++;
      if (next_metadata == received_result_metadata_.end()) {
        return;
      }

      EXPECT_LT(metadata->frame_number, next_metadata->frame_number);
      metadata = next_metadata;
    }
  }

  // Verify received buffers are sorted by frame numbers.
  // Protected by callback_lock_.
  void VerifyBuffersOrderLocked(const std::vector<ReceivedBuffer>& buffers) {
    auto buffer = buffers.begin();
    if (buffer == buffers.end()) {
      return;
    }

    while (1) {
      auto next_buffer = buffer;
      next_buffer++;
      if (next_buffer == buffers.end()) {
        return;
      }

      EXPECT_LT(buffer->frame_number, next_buffer->frame_number);
      buffer = next_buffer;
    }
  }

  // Verify received buffers are sorted by frame numbers.
  void VerifyBuffersOrder() {
    std::lock_guard<std::mutex> lock(callback_lock_);

    for (auto buffers : stream_received_buffers_map_) {
      VerifyBuffersOrderLocked(buffers.second);
    }
  }

  // Add pending request to dispatcher in the order of frame numbers, given
  // unordered frame numbers and ordered output buffers.
  void AddPendingRequestsToDispatcher(
      const std::vector<uint32_t>& unordered_frame_numbers,
      const std::vector<std::vector<StreamBuffer>>& ordered_output_buffers = {}) {
    if (ordered_output_buffers.size() > 0) {
      ASSERT_EQ(ordered_output_buffers.size(), unordered_frame_numbers.size());
    }

    std::vector<uint32_t> ordered_frame_numbers = unordered_frame_numbers;
    std::sort(ordered_frame_numbers.begin(), ordered_frame_numbers.end());

    // Add pending requests to result dispatcher.
    for (size_t i = 0; i < ordered_frame_numbers.size(); i++) {
      CaptureRequest request = {};
      request.frame_number = ordered_frame_numbers[i];
      if (ordered_output_buffers.size() > 0) {
        request.output_buffers = ordered_output_buffers[i];
      }

      ASSERT_EQ(result_dispatcher_->AddPendingRequest(request), OK)
          << "Failed to add a pending request for frame "
          << request.frame_number;
    }
  }

  std::unique_ptr<ResultDispatcher> result_dispatcher_;

  std::mutex callback_lock_;
  std::condition_variable callback_condition_;

  // Protected by callback_lock_.
  std::vector<ShutterMessage> received_shutters_;

  // Protected by callback_lock_.
  std::vector<ReceivedResultMetadata> received_result_metadata_;

  // Maps from stream ID to received output buffers.
  // Protected by callback_lock_.
  std::unordered_map<int32_t, std::vector<ReceivedBuffer>>
      stream_received_buffers_map_;
};

TEST_F(ResultDispatcherTests, ShutterOrder) {
  static constexpr uint64_t kFrameDurationNs = 100;

  std::vector<uint32_t> unordered_frame_numbers = {3, 1, 2, 5, 4, 6};
  AddPendingRequestsToDispatcher(unordered_frame_numbers);

  // Add unordered shutters to dispatcher.
  for (auto frame_number : unordered_frame_numbers) {
    EXPECT_EQ(result_dispatcher_->AddShutter(frame_number,
                                             frame_number * kFrameDurationNs),
              OK);
  }

  // Wait for all shutters to be notified.
  for (auto& frame_number : unordered_frame_numbers) {
    EXPECT_EQ(WaitForShutter(frame_number, frame_number * kFrameDurationNs), OK)
        << "Waiting for shutter for frame " << frame_number << " timed out.";
  }

  // Verify the shutters are received in the order of frame numbers.
  VerifyShuttersOrder();
}

TEST_F(ResultDispatcherTests, ResultMetadataOrder) {
  std::vector<uint32_t> unordered_frame_numbers = {4, 2, 1, 3, 6, 5};
  AddPendingRequestsToDispatcher(unordered_frame_numbers);

  // Add unordered result metadata to dispatcher.
  for (auto frame_number : unordered_frame_numbers) {
    static constexpr uint32_t kNumEntries = 10;
    static constexpr uint32_t kDataBytes = 256;

    auto result = std::make_unique<CaptureResult>(CaptureResult({}));
    result->frame_number = frame_number;
    result->partial_result = kPartialResult;
    result->result_metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);

    EXPECT_EQ(result_dispatcher_->AddResult(std::move(result)), OK);
  }

  // Wait for all result metadata to be notified.
  for (auto& frame_number : unordered_frame_numbers) {
    EXPECT_EQ(WaitForResultMetadata(frame_number), OK)
        << "Waiting for result metadata for frame " << frame_number
        << " timed out.";
  }

  // Verify the result metadata are received in the order of frame numbers.
  VerifyResultMetadataOrder();
}

TEST_F(ResultDispatcherTests, OutputBufferOrder) {
  static constexpr int32_t kStreamId = 5;

  std::vector<uint32_t> unordered_frame_numbers = {3, 1, 4, 2, 5, 6};
  std::vector<std::vector<StreamBuffer>> output_buffers;

  for (uint32_t i = 0; i < unordered_frame_numbers.size(); i++) {
    StreamBuffer buffer = {
        .stream_id = kStreamId,
        .buffer_id = i,
    };

    output_buffers.push_back({buffer});
  }

  AddPendingRequestsToDispatcher(unordered_frame_numbers, output_buffers);

  // Add unordered output buffers to dispatcher.
  for (uint32_t i = 0; i < unordered_frame_numbers.size(); i++) {
    auto result = std::make_unique<CaptureResult>();
    result->frame_number = unordered_frame_numbers[i];
    result->partial_result = 0;
    result->output_buffers = output_buffers[i];

    EXPECT_EQ(result_dispatcher_->AddResult(std::move(result)), OK);
  }

  // Wait for all output buffers to be notified.
  for (auto& frame_number : unordered_frame_numbers) {
    EXPECT_EQ(WaitForOuptutBuffer(frame_number, kStreamId), OK)
        << "Waiting for output buffers for frame " << frame_number
        << " timed out.";
  }

  // Verify the buffers are received in the order of frame numbers.
  VerifyBuffersOrder();
}

TEST_F(ResultDispatcherTests, ShutterOrderWithRemovePengingRequest) {
  static constexpr uint64_t kFrameDurationNs = 100;

  std::vector<uint32_t> unordered_frame_numbers = {3, 1, 2, 5, 4, 6};
  AddPendingRequestsToDispatcher(unordered_frame_numbers);

  auto iter = unordered_frame_numbers.begin() + 2;
  auto remove_frame_number = *iter;
  // After erase iter, unordered_frame_numbers = {3, 1, 5, 4, 6};
  unordered_frame_numbers.erase(iter);
  for (auto frame_number : unordered_frame_numbers) {
    EXPECT_EQ(result_dispatcher_->AddShutter(frame_number,
                                             frame_number * kFrameDurationNs),
              OK);
  }

  // Remove pending request for the frame number 2.
  result_dispatcher_->RemovePendingRequest(remove_frame_number);

  // Wait for all shutters to be notified.
  for (auto& frame_number : unordered_frame_numbers) {
    EXPECT_EQ(WaitForShutter(frame_number, frame_number * kFrameDurationNs), OK)
        << "Waiting for shutter for frame " << frame_number << " timed out.";
  }

  // Verify the shutters are received in the order of frame numbers.
  VerifyShuttersOrder();
}

// TODO(b/138960498): Test errors like adding repeated pending requests and
// repeated results.

}  // namespace google_camera_hal
}  // namespace android
