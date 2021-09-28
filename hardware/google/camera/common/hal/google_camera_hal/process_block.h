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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_PROCESS_BLOCK_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_PROCESS_BLOCK_H_

#include <utils/Errors.h>

#include "camera_device_session_hwl.h"
#include "hal_types.h"

namespace android {
namespace google_camera_hal {

class ResultProcessor;

// Define a process block request.
struct ProcessBlockRequest {
  uint32_t request_id = 0;  // A unique ID of this process block request.
  CaptureRequest request;
};

// Define a process block result.
struct ProcessBlockResult {
  // ID of the ProcessBlockRequest that this result belongs to.
  uint32_t request_id = 0;
  std::unique_ptr<CaptureResult> result;
};

// Define a process block notify message.
struct ProcessBlockNotifyMessage {
  // ID of the ProcessBlockRequest that this message belongs to.
  uint32_t request_id = 0;
  NotifyMessage message;
};

// ProcessBlock defines the interface of a process block. A process block can
// process capture requests and sends results to a result processor. A process
// block can process capture requests using SW, ISP, GPU, or other HW components.
class ProcessBlock {
 public:
  virtual ~ProcessBlock() = default;

  // Configure streams. It must be called exactly once before any calls to
  // ProcessRequest. It will return an error if it's called more than once.
  // stream_config contains the streams that may be included in a capture
  // request.
  // overall_config contains the whole streams received from frameworks.
  virtual status_t ConfigureStreams(
      const StreamConfiguration& stream_config,
      const StreamConfiguration& overall_config) = 0;

  // Set the result processor to send capture results to.
  virtual status_t SetResultProcessor(
      std::unique_ptr<ResultProcessor> result_processor) = 0;

  // Get HAL streams configured in this process block.
  virtual status_t GetConfiguredHalStreams(
      std::vector<HalStream>* hal_streams) const = 0;

  // Process a capture request.
  // When this method is called, process block should forward
  // process_block_requests and remaining_session_request to the result
  // processor using ResultProcessor::AddPendingRequests() so the result process
  // knows what results to expect.
  //
  // process_block_requests are the requests for this process block. This method
  // is asynchronous so returning from this call doesn't mean the requests are
  // completed. If the process block captures from camera sensors, capturing
  // from camera sensors must be synchronized for all requests in this call.
  //
  // remaining_session_request is the remaining request that was sent to the
  // capture session. It contains all remaining output buffers that have not
  // been completed by the process chain yet. For the last result process in a
  // process chain, remaining_session_request should contain only the output
  // buffers that are present in process_block_requests.
  // remaining_session_request doesn't contain any internal buffers.
  virtual status_t ProcessRequests(
      const std::vector<ProcessBlockRequest>& process_block_requests,
      const CaptureRequest& remaining_session_request) = 0;

  // Flush pending requests.
  virtual status_t Flush() = 0;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_PROCESS_BLOCK_H_