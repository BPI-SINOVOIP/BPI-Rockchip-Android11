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

#define LOG_TAG "RequestProcessorTest"
#include <log/log.h>

#include <gtest/gtest.h>
#include <memory>

#include "basic_request_processor.h"
#include "mock_device_session_hwl.h"
#include "mock_process_block.h"
#include "result_processor.h"
#include "test_utils.h"

using ::testing::_;

namespace android {
namespace google_camera_hal {

using RequestProcessorCreateFunc =
    std::function<std::unique_ptr<RequestProcessor>()>;

class RequestProcessorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a mock session HWL.
    session_hwl_ = std::make_unique<MockDeviceSessionHwl>();
    ASSERT_NE(session_hwl_, nullptr);

    session_hwl_->DelegateCallsToFakeSession();
  }

  std::unique_ptr<RequestProcessor> CreateBasicRequestrocessor() {
    return BasicRequestProcessor::Create(session_hwl_.get());
  }

  // All result processor implementation classes should be added to this
  // vector.
  std::vector<RequestProcessorCreateFunc> request_processor_create_funcs_ = {
      [&]() { return CreateBasicRequestrocessor(); },
  };

  std::unique_ptr<MockDeviceSessionHwl> session_hwl_;
};

TEST_F(RequestProcessorTest, Create) {
  for (auto& create_func : request_processor_create_funcs_) {
    auto request_processor = create_func();
    ASSERT_NE(request_processor, nullptr)
        << "Creating a request processor failed";
  }
}

TEST_F(RequestProcessorTest, StreamConfiguration) {
  auto stream_manager = InternalStreamManager::Create();
  StreamConfiguration preview_config;
  test_utils::GetPreviewOnlyStreamConfiguration(&preview_config);

  for (auto& create_func : request_processor_create_funcs_) {
    auto request_processor = create_func();
    ASSERT_NE(request_processor, nullptr)
        << "Creating a request processor failed";

    EXPECT_EQ(request_processor->ConfigureStreams(
                  stream_manager.get(), preview_config,
                  /*process_block_stream_config=*/nullptr),
              BAD_VALUE)
        << "Configuring streams with nullptr process_block_stream_config "
           "should fail.";

    StreamConfiguration process_block_stream_config;
    EXPECT_EQ(
        request_processor->ConfigureStreams(
            stream_manager.get(), preview_config, &process_block_stream_config),
        OK);

    // Verify that all streams in process_block_stream_config are physical
    // streams.
    if (test_utils::IsLogicalCamera(session_hwl_.get())) {
      for (auto& stream : process_block_stream_config.streams) {
        EXPECT_TRUE(stream.is_physical_camera_stream);
      }
    }
  }
}

TEST_F(RequestProcessorTest, SetProcessBlock) {
  for (auto& create_func : request_processor_create_funcs_) {
    auto request_processor = create_func();
    ASSERT_NE(request_processor, nullptr)
        << "Creating a request processor failed";

    EXPECT_EQ(request_processor->SetProcessBlock(nullptr), BAD_VALUE)
        << "Setting nullptr process block should fail.";

    EXPECT_EQ(
        request_processor->SetProcessBlock(std::make_unique<MockProcessBlock>()),
        OK);

    EXPECT_EQ(
        request_processor->SetProcessBlock(std::make_unique<MockProcessBlock>()),
        ALREADY_EXISTS)
        << "Setting process block again should fail.";
  }
}

TEST_F(RequestProcessorTest, Flush) {
  for (auto& create_func : request_processor_create_funcs_) {
    auto request_processor = create_func();
    ASSERT_NE(request_processor, nullptr)
        << "Creating a request processor failed";

    auto process_block = std::make_unique<MockProcessBlock>();
    ASSERT_NE(process_block, nullptr);

    // Expect request process to flush the process block.
    EXPECT_CALL(*process_block, Flush()).Times(1);

    EXPECT_EQ(request_processor->SetProcessBlock(std::move(process_block)), OK);
    EXPECT_EQ(request_processor->Flush(), OK);
  }
}

TEST_F(RequestProcessorTest, BasicRequestProcessorRequest) {
  auto request_processor = CreateBasicRequestrocessor();
  ASSERT_NE(request_processor, nullptr);

  auto process_block = std::make_unique<MockProcessBlock>();
  ASSERT_NE(process_block, nullptr);

  // Expect request process to send a request to the process block.
  EXPECT_CALL(*process_block, ProcessRequests(_, _)).Times(1);

  EXPECT_EQ(request_processor->SetProcessBlock(std::move(process_block)), OK);

  // Testing BasicRequestProcessorRequest with a dummy request.
  CaptureRequest request = {};
  ASSERT_EQ(request_processor->ProcessRequest(request), OK);
}

}  // namespace google_camera_hal
}  // namespace android
