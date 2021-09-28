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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DUAL_IR_REQUEST_PROCESSOR_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DUAL_IR_REQUEST_PROCESSOR_H_

#include "process_block.h"
#include "request_processor.h"

namespace android {
namespace google_camera_hal {

// DualIrRequestProcessor implements a RequestProcessor handling realtime
// requests for a logical camera consisting of two IR camera sensors.
class DualIrRequestProcessor : public RequestProcessor {
 public:
  // device_session_hwl is owned by the caller and must be valid during the
  // lifetime of this DualIrRequestProcessor.
  // lead_camera_id is the lead IR camera ID. Logical streams will be
  // assigned to the lead IR camera.
  static std::unique_ptr<DualIrRequestProcessor> Create(
      CameraDeviceSessionHwl* device_session_hwl, uint32_t lead_camera_id);

  virtual ~DualIrRequestProcessor() = default;

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
  DualIrRequestProcessor(uint32_t lead_camera_id);

 private:
  // ID of the lead IR camera. All logical streams will be assigned to the
  // lead camera.
  const uint32_t kLeadCameraId;

  std::mutex process_block_lock_;

  // Protected by process_block_lock_.
  std::unique_ptr<ProcessBlock> process_block_;

  // Map from a stream ID to the physical camera ID the stream belongs to.
  // Protected by process_block_lock_.
  std::map<int32_t, uint32_t> stream_physical_camera_ids_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DUAL_IR_REQUEST_PROCESSOR_H_
