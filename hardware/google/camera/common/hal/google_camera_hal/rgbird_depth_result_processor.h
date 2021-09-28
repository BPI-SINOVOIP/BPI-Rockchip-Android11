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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RGBIRD_DEPTH_RESULT_PROCESSOR_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RGBIRD_DEPTH_RESULT_PROCESSOR_H_

#include "internal_stream_manager.h"
#include "result_processor.h"

namespace android {
namespace google_camera_hal {

// RgbirdDepthResultProcessor implements a ResultProcessor that returns depth
// stream to the framework, and the internal NIR raw streams(and optionally
// internal YUV stream) to the capture session internal stream manager.
// It is assumed that the result metadata and shutter has been reported to the
// framework by the request_result_processor before depth process block. So
// RgbirdDepthResultProcessor is not responsible for metadata or shutter
// notification. It only needs to return/recycle buffers unless there is an
// error returned from the depth process block.
class RgbirdDepthResultProcessor : public ResultProcessor {
 public:
  static std::unique_ptr<RgbirdDepthResultProcessor> Create(
      InternalStreamManager* internal_stream_manager);

  virtual ~RgbirdDepthResultProcessor() = default;

  // Override functions of ResultProcessor start.
  void SetResultCallback(ProcessCaptureResultFunc process_capture_result,
                         NotifyFunc notify) override;

  status_t AddPendingRequests(
      const std::vector<ProcessBlockRequest>& process_block_requests,
      const CaptureRequest& remaining_session_request) override;

  void ProcessResult(ProcessBlockResult block_result) override;

  void Notify(const ProcessBlockNotifyMessage& block_message) override;

  status_t FlushPendingRequests() override;
  // Override functions of ResultProcessor end.

 protected:
  RgbirdDepthResultProcessor(InternalStreamManager* internal_stream_manager);

 private:
  static constexpr int32_t kInvalidStreamId = -1;
  InternalStreamManager* internal_stream_manager_ = nullptr;

  std::mutex callback_lock_;

  // The following callbacks must be protected by callback_lock_.
  ProcessCaptureResultFunc process_capture_result_;
  NotifyFunc notify_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RGBIRD_DEPTH_RESULT_PROCESSOR_H_