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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_HDRPLUS_PROCESS_BLOCK_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_HDRPLUS_PROCESS_BLOCK_H_

#include "process_block.h"

namespace android {
namespace google_camera_hal {

// HdrplusProcessBlock implements a offline ProcessBlock.
// It can process offline capture requests for a single physical camera.
class HdrplusProcessBlock : public ProcessBlock {
 public:
  // Create a HdrplusProcessBlock.
  // device_session_hwl is owned by the caller and must be valid during the
  // lifetime of this HdrplusProcessBlock.
  static std::unique_ptr<HdrplusProcessBlock> Create(
      CameraDeviceSessionHwl* device_session_hwl, uint32_t cameraId);

  virtual ~HdrplusProcessBlock() = default;

  // Override functions of ProcessBlock start.
  // All output streams must be physical streams. HdrplusProcessBlock does not
  // support logical output streams.
  status_t ConfigureStreams(const StreamConfiguration& stream_config,
                            const StreamConfiguration& overall_config) override;

  status_t SetResultProcessor(
      std::unique_ptr<ResultProcessor> result_processor) override;

  status_t GetConfiguredHalStreams(
      std::vector<HalStream>* hal_streams) const override;

  status_t ProcessRequests(
      const std::vector<ProcessBlockRequest>& process_block_requests,
      const CaptureRequest& remaining_session_request) override;

  status_t Flush() override;
  // Override functions of ProcessBlock end.

 protected:
  HdrplusProcessBlock(uint32_t cameraId,
                      CameraDeviceSessionHwl* device_session_hwl);

 private:
  // Camera ID of this process block.
  const uint32_t kCameraId;

  // If the process block supports the device session.
  static bool IsSupported(CameraDeviceSessionHwl* device_session_hwl);

  // Invoked when the HWL pipeline sends a result.
  void NotifyHwlPipelineResult(std::unique_ptr<HwlPipelineResult> hwl_result);

  // Invoked when the HWL pipeline sends a message.
  void NotifyHwlPipelineMessage(uint32_t pipeline_id,
                                const NotifyMessage& message);

  HwlPipelineCallback hwl_pipeline_callback_;
  CameraDeviceSessionHwl* device_session_hwl_ = nullptr;

  mutable std::mutex configure_lock_;

  // If streams are configured. Must be protected by configure_lock_.
  bool is_configured_ = false;

  // HWL pipeline ID. Must be protected by configure_lock_.
  uint32_t pipeline_id_ = 0;

  std::mutex result_processor_lock_;

  // Result processor. Must be protected by result_processor_lock_.
  std::unique_ptr<ResultProcessor> result_processor_ = nullptr;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_HDRPLUS_PROCESS_BLOCK_H_
