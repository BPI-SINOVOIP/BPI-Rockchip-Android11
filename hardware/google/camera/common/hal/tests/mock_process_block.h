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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_PROCESS_BLOCK_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_PROCESS_BLOCK_H_

#include <gmock/gmock.h>
#include <process_block.h>

namespace android {
namespace google_camera_hal {

// Defines a ProcessBlock mock using gmock.
class MockProcessBlock : public ProcessBlock {
 public:
  MOCK_METHOD2(ConfigureStreams,
               status_t(const StreamConfiguration& stream_config,
                        const StreamConfiguration& overall_config));

  MOCK_METHOD1(SetResultProcessor,
               status_t(std::unique_ptr<ResultProcessor> result_processor));

  MOCK_CONST_METHOD1(GetConfiguredHalStreams,
                     status_t(std::vector<HalStream>* hal_streams));

  MOCK_METHOD2(
      ProcessRequests,
      status_t(const std::vector<ProcessBlockRequest>& process_block_requests,
               const CaptureRequest& remaining_session_request));

  MOCK_METHOD0(Flush, status_t());
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_PROCESS_BLOCK_H_