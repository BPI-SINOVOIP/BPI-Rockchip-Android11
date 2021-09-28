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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_REALTIME_ZSL_RESULT_PROCESSOR_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_REALTIME_ZSL_RESULT_PROCESSOR_H_

#include "internal_stream_manager.h"
#include "result_processor.h"

namespace android {
namespace google_camera_hal {

// RealtimeZslResultProcessor implements a ResultProcessor that return filled
// raw buffer and matedata to internal stream manager and forwards the results
// without raw buffer to its callback functions.
class RealtimeZslResultProcessor : public ResultProcessor {
 public:
  static std::unique_ptr<RealtimeZslResultProcessor> Create(
      InternalStreamManager* internal_stream_manager, int32_t raw_stream_id);

  virtual ~RealtimeZslResultProcessor() = default;

  // Override functions of ResultProcessor start.
  void SetResultCallback(ProcessCaptureResultFunc process_capture_result,
                         NotifyFunc notify) override;

  status_t AddPendingRequests(
      const std::vector<ProcessBlockRequest>& process_block_requests,
      const CaptureRequest& remaining_session_request) override;

  // Return filled raw buffer and matedata to internal stream manager
  // and forwards the results without raw buffer to its callback functions.
  void ProcessResult(ProcessBlockResult block_result) override;

  void Notify(const ProcessBlockNotifyMessage& block_message) override;

  status_t FlushPendingRequests() override;
  // Override functions of ResultProcessor end.

 protected:
  RealtimeZslResultProcessor(InternalStreamManager* internal_stream_manager,
                             int32_t raw_stream_id);

 private:
  // Save face detect mode for HDR+
  void SaveFdForHdrplus(const CaptureRequest& request);
  // Handle face detect metadata from result for HDR+
  status_t HandleFdResultForHdrplus(uint32_t frameNumber,
                                    HalCameraMetadata* metadata);
  // Save lens shading map mode for HDR+
  void SaveLsForHdrplus(const CaptureRequest& request);
  // Handle Lens shading metadata from result for HDR+
  status_t HandleLsResultForHdrplus(uint32_t frameNumber,
                                    HalCameraMetadata* metadata);
  std::mutex callback_lock_;

  // The following callbacks must be protected by callback_lock_.
  ProcessCaptureResultFunc process_capture_result_;
  NotifyFunc notify_;

  InternalStreamManager* internal_stream_manager_;
  int32_t raw_stream_id_ = -1;

  // Current face detect mode set by framework.
  uint8_t current_face_detect_mode_ = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;

  std::mutex face_detect_lock_;
  // Map from frame number to face detect mode requested for that frame by
  // framework. And requested_face_detect_modes_ is protected by
  // face_detect_lock_
  std::unordered_map<uint32_t, uint8_t> requested_face_detect_modes_;

  // Current lens shading map mode set by framework.
  uint8_t current_lens_shading_map_mode_ =
      ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF;

  std::mutex lens_shading_lock_;
  // Map from frame number to lens shading map mode requested for that frame
  // by framework. And requested_lens_shading_map_modes_ is protected by
  // lens_shading_lock_
  std::unordered_map<uint32_t, uint8_t> requested_lens_shading_map_modes_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_REALTIME_ZSL_RESULT_PROCESSOR_H_
