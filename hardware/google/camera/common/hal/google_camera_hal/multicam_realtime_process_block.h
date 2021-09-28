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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_MULTICAM_REALTIME_PROCESS_BLOCK_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_MULTICAM_REALTIME_PROCESS_BLOCK_H_

#include <map>
#include <shared_mutex>

#include "pipeline_request_id_manager.h"
#include "process_block.h"
#include "result_processor.h"

namespace android {
namespace google_camera_hal {

// MultiCameraRtProcessBlock implements a real-time ProcessBlock that can
// process real-time capture requests for multiple physical cameras.
// MultiCameraRtProcessBlock only supports a logical camera with multiple
// physical cameras. It also only supports physical output streams.
class MultiCameraRtProcessBlock : public ProcessBlock {
 public:
  // Create a MultiCameraRtProcessBlock.
  // device_session_hwl is owned by the caller and must be valid during the
  // lifetime of this MultiCameraRtProcessBlock.
  static std::unique_ptr<MultiCameraRtProcessBlock> Create(
      CameraDeviceSessionHwl* device_session_hwl);

  virtual ~MultiCameraRtProcessBlock() = default;

  // Override functions of ProcessBlock start.
  // Only physical output streams are supported.
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

  // Prepare pipeline by camera id
  status_t PrepareBlockByCameraId(uint32_t camera_id, uint32_t frame_number);

 protected:
  MultiCameraRtProcessBlock(CameraDeviceSessionHwl* device_session_hwl);

 private:
  // Camera ID of this process block.
  const uint32_t kCameraId;

  // Define a configured stream.
  struct ConfiguredStream {
    uint32_t pipeline_id = 0;
    Stream stream;
  };

  // Map from a camera ID to the camera's stream configuration.
  using CameraStreamConfigurationMap = std::map<uint32_t, StreamConfiguration>;

  // If the real-time process block supports the device session.
  static bool IsSupported(CameraDeviceSessionHwl* device_session_hwl);

  // Invoked when the HWL pipeline sends a result.
  void NotifyHwlPipelineResult(std::unique_ptr<HwlPipelineResult> hwl_result);

  // Invoked when the HWL pipeline sends a message.
  void NotifyHwlPipelineMessage(uint32_t pipeline_id,
                                const NotifyMessage& message);

  // Get a map from a camera ID to the camera's stream configuration.
  // Each camera will have its own stream configuration.
  status_t GetCameraStreamConfigurationMap(
      const StreamConfiguration& stream_config,
      CameraStreamConfigurationMap* camera_stream_config_map) const;

  // Get the camera ID that a buffer will be captured from. Must be called with
  // configure_shared_mutex_ locked.
  status_t GetBufferPhysicalCameraIdLocked(const StreamBuffer& buffer,
                                           uint32_t* camera_id) const;

  // Get the pipeline ID that a buffer will be captured from. Must be called
  // with configure_shared_mutex_ locked.
  status_t GetOutputBufferPipelineIdLocked(const StreamBuffer& buffer,
                                           uint32_t* pipeline_id) const;

  // Return if requests are valid. Must be called with configure_shared_mutex_ locked.
  bool AreRequestsValidLocked(
      const std::vector<ProcessBlockRequest>& requests) const;

  // Forward the pending requests to result processor.
  status_t ForwardPendingRequests(
      const std::vector<ProcessBlockRequest>& process_block_requests,
      const CaptureRequest& remaining_session_request);

  HwlPipelineCallback hwl_pipeline_callback_;
  CameraDeviceSessionHwl* device_session_hwl_ = nullptr;

  mutable std::shared_mutex configure_shared_mutex_;

  bool is_configured_ = false;  // Must be protected by configure_shared_mutex_.

  // Map from physical camera ID to HWL pipeline ID. Must be protected by
  // configure_shared_mutex_.
  std::unordered_map<uint32_t, uint32_t> camera_pipeline_ids_;

  // Map from a stream ID to the configured streams. Must be protected by
  // configure_shared_mutex_.
  std::unordered_map<uint32_t, ConfiguredStream> configured_streams_;

  std::mutex result_processor_mutex_;

  // Result processor. Must be protected by result_processor_mutex_.
  std::unique_ptr<ResultProcessor> result_processor_ = nullptr;

  // Pipeline request id manager
  std::unique_ptr<PipelineRequestIdManager> request_id_manager_ = nullptr;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_MULTICAM_REALTIME_PROCESS_BLOCK_H_
