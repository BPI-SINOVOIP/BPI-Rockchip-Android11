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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RESULT_PROCESSOR_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RESULT_PROCESSOR_H_

#include <utils/Errors.h>

#include "hal_types.h"
#include "process_block.h"

namespace android {
namespace google_camera_hal {

// ResultProcessor defines the interface of a result processor. A result
// processor receives results from a ProcessBlock. It can return the finished
// results to the specified callback functions. If a class inherits both
// ResultProcessor and RequestProcessor interfaces, it can convert the results
// to requests to send to the next ProcessBlock.
class ResultProcessor {
 public:
  virtual ~ResultProcessor() = default;

  // Set the callbacks to send the finished results. Must be called before
  // calling ProcessResult.
  virtual void SetResultCallback(ProcessCaptureResultFunc process_capture_result,
                                 NotifyFunc notify) = 0;

  // Add pending requests to the result processor.
  //
  // process_block_requests are the requests that will be completed by the
  // preceding process block.
  //
  // remaining_session_request is the remaining request that was sent to the
  // capture session. It contains all remaining output buffers that have not
  // been completed by the process chain yet. For the last result process in a
  // process chain, remaining_session_request should contain only the output
  // buffers that are present in process_block_requests.
  // remaining_session_request doesn't contain any internal buffers.
  virtual status_t AddPendingRequests(
      const std::vector<ProcessBlockRequest>& process_block_requests,
      const CaptureRequest& remaining_session_request) = 0;

  // Called by a ProcessBlock to send the capture results.
  virtual void ProcessResult(ProcessBlockResult block_result) = 0;

  // Called by a ProcessBlock to notify a message.
  virtual void Notify(const ProcessBlockNotifyMessage& block_message) = 0;

  // Flush all pending workload.
  virtual status_t FlushPendingRequests() = 0;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RESULT_PROCESSOR_H_