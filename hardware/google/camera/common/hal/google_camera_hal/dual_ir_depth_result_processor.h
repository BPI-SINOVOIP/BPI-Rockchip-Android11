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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DUAL_IR_DEPTH_RESULT_PROCESSOR_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DUAL_IR_DEPTH_RESULT_PROCESSOR_H_

#include "internal_stream_manager.h"
#include "result_processor.h"

namespace android {
namespace google_camera_hal {

class DualIrDepthResultProcessor : public ResultProcessor {
 public:
  static std::unique_ptr<DualIrDepthResultProcessor> Create(
      InternalStreamManager* internal_stream_manager);

  virtual ~DualIrDepthResultProcessor() = default;

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
  DualIrDepthResultProcessor(InternalStreamManager* internal_stream_manager);

 private:
  InternalStreamManager* internal_stream_manager_ = nullptr;

  std::mutex callback_lock_;

  // The following callbacks must be protected by callback_lock_.
  ProcessCaptureResultFunc process_capture_result_;
  NotifyFunc notify_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DUAL_IR_DEPTH_RESULT_PROCESSOR_H_