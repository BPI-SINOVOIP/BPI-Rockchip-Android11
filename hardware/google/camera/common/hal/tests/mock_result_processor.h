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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_RESULT_PROCESSOR_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_RESULT_PROCESSOR_H_

#include <gmock/gmock.h>
#include <result_processor.h>

namespace android {
namespace google_camera_hal {

// Defines a ResultProcessor mock using gmock.
class MockResultProcessor : public ResultProcessor {
 public:
  MOCK_METHOD2(SetResultCallback,
               void(ProcessCaptureResultFunc process_capture_result,
                    NotifyFunc notify));

  MOCK_METHOD2(
      AddPendingRequests,
      status_t(const std::vector<ProcessBlockRequest>& process_block_requests,
               const CaptureRequest& remaining_session_request));

  MOCK_METHOD1(ProcessResult, void(ProcessBlockResult result));

  MOCK_METHOD1(Notify, void(const ProcessBlockNotifyMessage& message));

  MOCK_METHOD0(FlushPendingRequests, status_t());
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_RESULT_PROCESSOR_H_