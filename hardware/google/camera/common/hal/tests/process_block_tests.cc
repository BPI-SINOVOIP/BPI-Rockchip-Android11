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

#define LOG_TAG "ProcessBlockTest"
#include <log/log.h>

#include <gtest/gtest.h>
#include <memory>

#include "mock_device_session_hwl.h"
#include "mock_result_processor.h"
#include "multicam_realtime_process_block.h"
#include "realtime_process_block.h"
#include "test_utils.h"

using ::testing::_;

namespace android {
namespace google_camera_hal {

// Setup for testing a process block.
struct ProcessBlockTestSetup {
  // Function to create a process block.
  std::function<std::unique_ptr<ProcessBlock>()> process_block_create_func;
  uint32_t camera_id;                         // Camera ID to test
  std::vector<uint32_t> physical_camera_ids;  // Physical camera IDs to test.
};

class ProcessBlockTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize all process block test setups.
    realtime_process_block_setup_ = {
        .process_block_create_func =
            [&]() { return RealtimeProcessBlock::Create(session_hwl_.get()); },
        .camera_id = 3,
    };

    multi_camera_process_block_setup_ = {
        .process_block_create_func =
            [&]() {
              return MultiCameraRtProcessBlock::Create(session_hwl_.get());
            },
        .camera_id = 3,
        .physical_camera_ids = {1, 5},
    };

    process_block_test_setups_ = {
        realtime_process_block_setup_,
        multi_camera_process_block_setup_,
    };
  }

  // Create a mock device session HWL and stream configuration based on
  // the test setup.
  void InitializeProcessBlockTest(const ProcessBlockTestSetup& setup) {
    // Create a mock session HWL.
    session_hwl_ = std::make_unique<MockDeviceSessionHwl>(
        setup.camera_id, setup.physical_camera_ids);
    ASSERT_NE(session_hwl_, nullptr);

    session_hwl_->DelegateCallsToFakeSession();

    // Create a stream configuration for testing.
    if (setup.physical_camera_ids.empty()) {
      test_utils::GetPreviewOnlyStreamConfiguration(&test_config_);
    } else {
      test_utils::GetPhysicalPreviewStreamConfiguration(
          &test_config_, setup.physical_camera_ids);
    }
  }

  std::unique_ptr<MockDeviceSessionHwl> session_hwl_;
  std::vector<ProcessBlockTestSetup> process_block_test_setups_;
  ProcessBlockTestSetup realtime_process_block_setup_;
  ProcessBlockTestSetup multi_camera_process_block_setup_;
  StreamConfiguration test_config_;  // Configuration used in tests.
};

TEST_F(ProcessBlockTest, Create) {
  for (auto& setup : process_block_test_setups_) {
    InitializeProcessBlockTest(setup);

    auto block = setup.process_block_create_func();
    ASSERT_NE(block, nullptr) << "Creating a process block failed";
  }
}

TEST_F(ProcessBlockTest, StreamConfiguration) {
  for (auto& setup : process_block_test_setups_) {
    InitializeProcessBlockTest(setup);

    auto block = setup.process_block_create_func();
    ASSERT_NE(block, nullptr) << "Creating a process block failed";

    ASSERT_EQ(block->ConfigureStreams(test_config_, test_config_), OK);

    // Configuring stream again should return ALREADY_EXISTS
    ASSERT_EQ(block->ConfigureStreams(test_config_, test_config_),
              ALREADY_EXISTS);
  }
}

TEST_F(ProcessBlockTest, SetResultProcessor) {
  for (auto& setup : process_block_test_setups_) {
    InitializeProcessBlockTest(setup);

    auto block = setup.process_block_create_func();
    ASSERT_NE(block, nullptr) << "Creating a process block failed";

    ASSERT_EQ(block->SetResultProcessor(nullptr), BAD_VALUE)
        << "Setting a nullptr result processor should return BAD_VALUE";

    ASSERT_EQ(
        block->SetResultProcessor(std::make_unique<MockResultProcessor>()), OK);

    ASSERT_EQ(block->SetResultProcessor(std::make_unique<MockResultProcessor>()),
              ALREADY_EXISTS)
        << "Setting result processor again should return ALREADY_EXISTS";
  }
}

TEST_F(ProcessBlockTest, GetConfiguredHalStreams) {
  for (auto& setup : process_block_test_setups_) {
    InitializeProcessBlockTest(setup);

    auto block = setup.process_block_create_func();
    ASSERT_NE(block, nullptr) << "Creating a process block failed";

    ASSERT_EQ(block->GetConfiguredHalStreams(nullptr), BAD_VALUE)
        << "Passing nullptr should return BAD_VALUE";

    std::vector<HalStream> hal_streams;
    ASSERT_EQ(block->GetConfiguredHalStreams(&hal_streams), NO_INIT)
        << "Should return NO_INIT without ConfigureStreams()";

    ASSERT_EQ(block->ConfigureStreams(test_config_, test_config_), OK);
    ASSERT_EQ(session_hwl_->BuildPipelines(), OK);
    ASSERT_EQ(block->GetConfiguredHalStreams(&hal_streams), OK);
    ASSERT_EQ(hal_streams.size(), test_config_.streams.size());
  }
}

TEST_F(ProcessBlockTest, Flush) {
  for (auto& setup : process_block_test_setups_) {
    InitializeProcessBlockTest(setup);

    auto block = setup.process_block_create_func();
    ASSERT_NE(block, nullptr) << "Creating a process block failed";

    ASSERT_EQ(block->Flush(), OK);
    ASSERT_EQ(block->ConfigureStreams(test_config_, test_config_), OK);
    ASSERT_EQ(block->Flush(), OK);
  }
}

TEST_F(ProcessBlockTest, RealtimeProcessBlockRequest) {
  ProcessBlockTestSetup& setup = realtime_process_block_setup_;
  InitializeProcessBlockTest(setup);

  // Verify process block calls HWL session.
  EXPECT_CALL(*session_hwl_, ConfigurePipeline(_, _, _, _, _)).Times(1);
  EXPECT_CALL(*session_hwl_, GetConfiguredHalStream(_, _))
      .Times(test_config_.streams.size());
  EXPECT_CALL(*session_hwl_, SubmitRequests(_, _)).Times(1);

  auto result_processor = std::make_unique<MockResultProcessor>();
  ASSERT_NE(result_processor, nullptr) << "Cannot create a MockResultProcessor";

  // Verify process block calls result processor
  EXPECT_CALL(*result_processor, AddPendingRequests(_, _)).Times(1);
  EXPECT_CALL(*result_processor, ProcessResult(_)).Times(1);
  EXPECT_CALL(*result_processor, Notify(_)).Times(1);

  auto block = setup.process_block_create_func();
  ASSERT_NE(block, nullptr) << "Creating RealtimeProcessBlock failed";

  ASSERT_EQ(block->ConfigureStreams(test_config_, test_config_), OK);

  ASSERT_EQ(session_hwl_->BuildPipelines(), OK);

  std::vector<HalStream> hal_streams;
  ASSERT_EQ(block->GetConfiguredHalStreams(&hal_streams), OK);
  EXPECT_EQ(hal_streams.size(), test_config_.streams.size());

  ASSERT_EQ(block->SetResultProcessor(std::move(result_processor)), OK);

  // Testing RealtimeProcessBlock with a dummy request.
  std::vector<ProcessBlockRequest> block_requests(1);
  ASSERT_EQ(block->ProcessRequests(block_requests, block_requests[0].request),
            OK);
}

TEST_F(ProcessBlockTest, MultiCameraRtProcessBlockRequest) {
  ProcessBlockTestSetup& setup = multi_camera_process_block_setup_;
  InitializeProcessBlockTest(setup);

  size_t num_pipelines = setup.physical_camera_ids.size();
  size_t num_streams = test_config_.streams.size();

  // Verify process block calls HWL session.
  EXPECT_CALL(*session_hwl_, ConfigurePipeline(_, _, _, _, _))
      .Times(num_pipelines);
  EXPECT_CALL(*session_hwl_, GetConfiguredHalStream(_, _)).Times(num_streams);
  EXPECT_CALL(*session_hwl_, SubmitRequests(_, _)).Times(1);

  auto result_processor = std::make_unique<MockResultProcessor>();
  ASSERT_NE(result_processor, nullptr) << "Cannot create a MockResultProcessor";

  // Verify process block calls result processor
  EXPECT_CALL(*result_processor, AddPendingRequests(_, _)).Times(1);
  EXPECT_CALL(*result_processor, ProcessResult(_)).Times(num_pipelines);
  EXPECT_CALL(*result_processor, Notify(_)).Times(num_pipelines);

  auto block = setup.process_block_create_func();
  ASSERT_NE(block, nullptr) << "Creating MultiCameraRtProcessBlock failed";

  ASSERT_EQ(block->ConfigureStreams(test_config_, test_config_), OK);

  ASSERT_EQ(session_hwl_->BuildPipelines(), OK);

  std::vector<HalStream> hal_streams;
  ASSERT_EQ(block->GetConfiguredHalStreams(&hal_streams), OK);
  EXPECT_EQ(hal_streams.size(), test_config_.streams.size());

  ASSERT_EQ(block->SetResultProcessor(std::move(result_processor)), OK);

  // Testing RealtimeProcessBlock with dummy requests.
  std::vector<ProcessBlockRequest> block_requests;
  CaptureRequest remaining_session_requests;

  for (auto& stream : test_config_.streams) {
    StreamBuffer buffer;
    buffer.stream_id = stream.id;

    ProcessBlockRequest block_request;
    block_request.request.output_buffers.push_back(buffer);

    block_requests.push_back(std::move(block_request));
    remaining_session_requests.output_buffers.push_back(buffer);
  }

  ASSERT_EQ(block->ProcessRequests(block_requests, remaining_session_requests),
            OK);
}

}  // namespace google_camera_hal
}  // namespace android
