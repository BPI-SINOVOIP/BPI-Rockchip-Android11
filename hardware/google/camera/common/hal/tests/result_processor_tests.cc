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

#define LOG_TAG "ResultProcessorTest"
#include <log/log.h>

#include <gtest/gtest.h>
#include <memory>

#include "basic_result_processor.h"

namespace android {
namespace google_camera_hal {

static constexpr native_handle kDummyNativeHandle = {};
static constexpr buffer_handle_t kDummyBufferHandle = &kDummyNativeHandle;

using ResultProcessorCreateFunc =
    std::function<std::unique_ptr<ResultProcessor>()>;

std::unique_ptr<ResultProcessor> CreateBasicResultProcessor() {
  return BasicResultProcessor::Create();
}

// All result processor implementation classes should be added to this
// vector.
std::vector<ResultProcessorCreateFunc> g_result_processor_create_funcs = {
    []() { return CreateBasicResultProcessor(); },
};

TEST(ResultProcessorTest, Create) {
  for (auto& create_func : g_result_processor_create_funcs) {
    auto result_processor = create_func();
    ASSERT_NE(result_processor, nullptr)
        << "Creating a result processor failed";
  }
}

TEST(ResultProcessorTest, SetResultCallback) {
  ProcessCaptureResultFunc process_capture_result =
      ProcessCaptureResultFunc([](std::unique_ptr<CaptureResult> /*result*/) {});
  NotifyFunc notify = NotifyFunc([](const NotifyMessage& /*message*/) {});

  for (auto& create_func : g_result_processor_create_funcs) {
    auto result_processor = create_func();
    ASSERT_NE(result_processor, nullptr)
        << "Creating a result processor failed";

    result_processor->SetResultCallback(process_capture_result, notify);
  }
}

void SendResultsAndMessages(ResultProcessor* result_processor) {
  ProcessBlockNotifyMessage shutter_message = {
      .message = {.type = MessageType::kShutter}};

  ProcessBlockNotifyMessage error_message = {
      .message = {.type = MessageType::kError}};

  // Verify it can handle various results.
  ProcessBlockResult null_result;
  null_result.result = nullptr;
  result_processor->ProcessResult(std::move(null_result));

  ProcessBlockResult empty_result;
  empty_result.result = std::make_unique<CaptureResult>();
  result_processor->ProcessResult(std::move(empty_result));

  // Verify it can handle messages.
  result_processor->Notify(shutter_message);
  result_processor->Notify(error_message);
}

TEST(ResultProcessorTest, AddPendingRequests) {
  ProcessCaptureResultFunc process_capture_result =
      ProcessCaptureResultFunc([](std::unique_ptr<CaptureResult> /*result*/) {});
  NotifyFunc notify = NotifyFunc([](const NotifyMessage& /*message*/) {});

  for (auto& create_func : g_result_processor_create_funcs) {
    auto result_processor = create_func();
    ASSERT_NE(result_processor, nullptr)
        << "Creating a result processor failed";

    std::vector<ProcessBlockRequest> requests(1);
    requests[0].request.output_buffers = {StreamBuffer{}};

    EXPECT_EQ(
        result_processor->AddPendingRequests(requests, requests[0].request), OK);
  }
}

TEST(ResultProcessorTest, ProcessResultAndNotify) {
  ProcessCaptureResultFunc process_capture_result =
      ProcessCaptureResultFunc([](std::unique_ptr<CaptureResult> /*result*/) {});
  NotifyFunc notify = NotifyFunc([](const NotifyMessage& /*message*/) {});

  for (auto& create_func : g_result_processor_create_funcs) {
    auto result_processor = create_func();
    ASSERT_NE(result_processor, nullptr)
        << "Creating a result processor failed";

    // Test sending results and messages before setting result callback.
    SendResultsAndMessages(result_processor.get());

    // Test again after setting result callback.
    result_processor->SetResultCallback(process_capture_result, notify);
    SendResultsAndMessages(result_processor.get());
  }
}

TEST(ResultProcessorTest, BasicResultProcessorResultAndNotify) {
  auto result_processor = CreateBasicResultProcessor();
  bool result_received = false;
  bool message_received = false;

  ProcessCaptureResultFunc process_capture_result =
      ProcessCaptureResultFunc([&](std::unique_ptr<CaptureResult> result) {
        EXPECT_NE(result, nullptr);
        result_received = true;
      });

  NotifyFunc notify = NotifyFunc(
      [&](const NotifyMessage& /*message*/) { message_received = true; });

  result_processor->SetResultCallback(process_capture_result, notify);

  ProcessBlockResult null_result;
  result_processor->ProcessResult(std::move(null_result));
  EXPECT_EQ(result_received, false);
  EXPECT_EQ(message_received, false);

  result_received = false;
  message_received = false;
  ProcessBlockResult empty_result = {.result =
                                         std::make_unique<CaptureResult>()};
  result_processor->ProcessResult(std::move(empty_result));
  EXPECT_EQ(result_received, true);
  EXPECT_EQ(message_received, false);

  result_received = false;
  message_received = false;
  result_processor->Notify(ProcessBlockNotifyMessage{});
  EXPECT_EQ(result_received, false);
  EXPECT_EQ(message_received, true);
}

TEST(ResultProcessorTest, BasicResultProcessorAddPendingRequest) {
  auto result_processor = CreateBasicResultProcessor();

  ProcessCaptureResultFunc process_capture_result = ProcessCaptureResultFunc(
      [&](std::unique_ptr<CaptureResult> /*result*/) {});

  NotifyFunc notify = NotifyFunc([&](const NotifyMessage& /*message*/) {});

  result_processor->SetResultCallback(process_capture_result, notify);

  std::vector<ProcessBlockRequest> requests(1);
  requests[0].request.output_buffers = {StreamBuffer{}};

  CaptureRequest remaining_request;
  remaining_request.output_buffers.push_back({.buffer = kDummyBufferHandle});
  EXPECT_NE(result_processor->AddPendingRequests(requests, remaining_request), OK)
      << "Adding a pending request with a remaining output buffer that's not"
      << "included in the request should fail.";
}

}  // namespace google_camera_hal
}  // namespace android
