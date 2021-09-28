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

#define LOG_TAG "PipelineRequestIdManagerTests"
#include <log/log.h>

#include <gtest/gtest.h>
#include <hal_camera_metadata.h>
#include <hal_types.h>
#include <pipeline_request_id_manager.h>

namespace android {
namespace google_camera_hal {

struct SampleRequest {
  uint32_t pipeline_id;
  uint32_t request_id;
};

const uint32_t kSampleFrameNumber = 10;
const size_t kMaxPendingRequest = 8;

const SampleRequest kSampleRequest[2] = {
    {
        .pipeline_id = 1,
        .request_id = 3,
    },
    {
        .pipeline_id = 2,
        .request_id = 4,
    },
};

TEST(PipelineRequestIdManagerTests, SetPipelineRequestId) {
  auto id_manager = PipelineRequestIdManager::Create(kMaxPendingRequest);
  ASSERT_NE(id_manager, nullptr) << "Creating PipelineRequestIdManager failed.";

  for (const auto& r : kSampleRequest) {
    EXPECT_EQ(id_manager->SetPipelineRequestId(r.request_id, kSampleFrameNumber,
                                               r.pipeline_id),
              OK)
        << "SetPipelineRequestId failed.";
  }

  // Set same frame number again should failed.
  EXPECT_NE(id_manager->SetPipelineRequestId(kSampleRequest[0].request_id + 1,
                                             kSampleFrameNumber,
                                             kSampleRequest[0].pipeline_id),
            OK)
      << "Set a frame number which already set to SetPipelineRequestId should "
         "failed.";
}

TEST(PipelineRequestIdManagerTests, GetPipelineRequestId) {
  auto id_manager = PipelineRequestIdManager::Create(kMaxPendingRequest);
  ASSERT_NE(id_manager, nullptr) << "Creating PipelineRequestIdManager failed.";

  for (const auto& r : kSampleRequest) {
    ASSERT_EQ(id_manager->SetPipelineRequestId(r.request_id, kSampleFrameNumber,
                                               r.pipeline_id),
              OK)
        << "SetPipelineRequestId failed.";
  }

  uint32_t returned_request_id;

  // Provide unset pipeline id should failed
  EXPECT_NE(id_manager->GetPipelineRequestId(99, kSampleFrameNumber,
                                             &returned_request_id),
            OK)
      << "Provide unset pipeline_id to GetPipelineRequestId should failed.";

  // Provide invalid frame number should failed
  EXPECT_NE(id_manager->GetPipelineRequestId(kSampleRequest[0].pipeline_id,
                                             999999, &returned_request_id),
            OK)
      << "Provide empty result to GetPipelineRequestId should failed.";

  for (const auto& request : kSampleRequest) {
    EXPECT_EQ(id_manager->GetPipelineRequestId(
                  request.pipeline_id, kSampleFrameNumber, &returned_request_id),
              OK)
        << "GetPipelineRequestId failed.";
    EXPECT_EQ(returned_request_id, request.request_id)
        << "The getting request_id is different from the setting.";
  }
}

TEST(PipelineRequestIdManagerTests, SetPipelineRequestIdWithOverflow) {
  auto id_manager = PipelineRequestIdManager::Create(kMaxPendingRequest);
  ASSERT_NE(id_manager, nullptr) << "Creating PipelineRequestIdManager failed.";

  ASSERT_EQ(id_manager->SetPipelineRequestId(kSampleRequest[0].request_id,
                                             kSampleFrameNumber,
                                             kSampleRequest[0].pipeline_id),
            OK)
      << "SetPipelineRequestId failed.";

  uint32_t returned_request_id;

  ASSERT_EQ(id_manager->GetPipelineRequestId(kSampleRequest[0].pipeline_id,
                                             kSampleFrameNumber,
                                             &returned_request_id),
            OK)
      << "GetPipelineRequestId failed.";
  ASSERT_EQ(returned_request_id, kSampleRequest[0].request_id)
      << "The getting request_id is different from the setting.";

  // Set frame number with same modulo value.
  // This should overwrite original frame number's data.
  uint32_t new_frame_number = kSampleFrameNumber + kMaxPendingRequest;
  ASSERT_EQ(id_manager->SetPipelineRequestId(kSampleRequest[0].request_id,
                                             new_frame_number,
                                             kSampleRequest[0].pipeline_id),
            OK)
      << "SetPipelineRequestId failed.";

  EXPECT_NE(id_manager->GetPipelineRequestId(kSampleRequest[0].pipeline_id,
                                             kSampleFrameNumber,
                                             &returned_request_id),
            OK)
      << "GetPipelineRequestId should failed after overwrite frame number.";
}

}  // namespace google_camera_hal
}  // namespace android
