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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DUAL_IR_RESULT_PROCESSOR_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DUAL_IR_RESULT_PROCESSOR_H_

#include <map>

#include "request_processor.h"
#include "result_processor.h"

namespace android {
namespace google_camera_hal {

// DualIrResultRequestProcessor implements a ResultProcessor for a logical
// camera that consists of two IR cameras. It also implements a RequestProcessor
// for the logical camera to generate depth.
class DualIrResultRequestProcessor : public ResultProcessor,
                                     public RequestProcessor {
 public:
  // Create a DualIrResultRequestProcessor.
  // device_session_hwl is owned by the client and must be valid during the life
  // cycle of this DualIrResultRequestProcessor.
  // stream_config is the stream configuration set by the framework. It's not
  // the process block's stream configuration.
  // lead_camera_id is the ID of the lead IR camera.
  static std::unique_ptr<DualIrResultRequestProcessor> Create(
      CameraDeviceSessionHwl* device_session_hwl,
      const StreamConfiguration& stream_config, uint32_t lead_camera_id);

  virtual ~DualIrResultRequestProcessor() = default;

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

  // Override functions of RequestProcessor start.
  status_t ConfigureStreams(
      InternalStreamManager* internal_stream_manager,
      const StreamConfiguration& stream_config,
      StreamConfiguration* process_block_stream_config) override;

  status_t SetProcessBlock(std::unique_ptr<ProcessBlock> process_block) override;

  status_t ProcessRequest(const CaptureRequest& request) override;

  status_t Flush() override;
  // Override functions of RequestProcessor end.

 protected:
  DualIrResultRequestProcessor(const StreamConfiguration& stream_config,
                               uint32_t logical_camera_id,
                               uint32_t lead_camera_id);

 private:
  const uint32_t kLogicalCameraId;
  const uint32_t kLeadCameraId;

  // Define a pending result metadata
  struct PendingResultMetadata {
    // Result metadata for the logical camera.
    std::unique_ptr<HalCameraMetadata> metadata;
    // Map from a physical camera ID to the physical camera's result metadata.
    std::map<uint32_t, std::unique_ptr<HalCameraMetadata>> physical_metadata;
  };

  // If a stream is a physical stream configured by the framework.
  // stream_id is the ID of the stream.
  // physical_camera_id will be filled with the physical camera ID if this
  // method return true.
  bool IsFrameworkPhyiscalStream(int32_t stream_id,
                                 uint32_t* physical_camera_id) const;

  // Add pending physical camera's result metadata to the map.
  // block_request is a block request used figure out pending results.
  // physical_metadata is the map to add the pending physical camera's result
  // metadata to.
  status_t AddPendingPhysicalCameraMetadata(
      const ProcessBlockRequest& block_request,
      std::map<uint32_t, std::unique_ptr<HalCameraMetadata>>* physical_metadata);

  // Try to send result metadata for a frame number if all of it's result
  // metadata are ready. Must have pending_result_metadata_mutex_ locked.
  void TrySendingResultMetadataLocked(uint32_t frame_number);

  // Process a result metadata and update the pending result metadata map.
  status_t ProcessResultMetadata(
      uint32_t frame_number, uint32_t physical_camera_id,
      std::unique_ptr<HalCameraMetadata> result_metadata);

  // Map from a stream ID to a camera ID based on framework stream configuration.
  std::map<int32_t, uint32_t> stream_camera_ids_;

  std::mutex pending_result_metadata_mutex_;

  // Map from a frame number to the pending result metadata. Must be protected
  // by pending_result_metadata_mutex_.
  std::map<uint32_t, PendingResultMetadata> pending_result_metadata_;

  std::mutex callback_lock_;

  // The following callbacks must be protected by callback_lock_.
  ProcessCaptureResultFunc process_capture_result_;
  NotifyFunc notify_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DUAL_IR_RESULT_PROCESSOR_H_
