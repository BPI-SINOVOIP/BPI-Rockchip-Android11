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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_HDRPLUS_REQUEST_PROCESSOR_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_HDRPLUS_REQUEST_PROCESSOR_H_

#include "process_block.h"
#include "request_processor.h"

namespace android {
namespace google_camera_hal {

// HdrplusRequestProcessor implements a RequestProcessor that adds
// internal raw stream as input stream to request and forwards the request to
// its ProcessBlock.
class HdrplusRequestProcessor : public RequestProcessor {
 public:
  // device_session_hwl is owned by the caller and must be valid during the
  // lifetime of this HdrplusRequestProcessor.
  static std::unique_ptr<HdrplusRequestProcessor> Create(
      CameraDeviceSessionHwl* device_session_hwl, int32_t raw_stream_id,
      uint32_t physical_camera_id);

  virtual ~HdrplusRequestProcessor() = default;

  // Override functions of RequestProcessor start.
  status_t ConfigureStreams(
      InternalStreamManager* internal_stream_manager,
      const StreamConfiguration& stream_config,
      StreamConfiguration* process_block_stream_config) override;

  status_t SetProcessBlock(std::unique_ptr<ProcessBlock> process_block) override;

  // Adds internal raw stream as input stream to request and forwards the
  // request to its ProcessBlock.
  status_t ProcessRequest(const CaptureRequest& request) override;

  status_t Flush() override;
  // Override functions of RequestProcessor end.

 protected:
  HdrplusRequestProcessor(uint32_t physical_camera_id)
      : kCameraId(physical_camera_id){};

 private:
  // Physical camera ID of request processor.
  const uint32_t kCameraId;

  status_t Initialize(CameraDeviceSessionHwl* device_session_hwl,
                      int32_t raw_stream_id);
  bool IsReadyForNextRequest();
  // For CTS (android.hardware.camera2.cts.StillCaptureTest#testJpegExif)
  // Remove JPEG metadata (THUMBNAIL_SIZE, ORIENTATION...) from internal raw
  // buffer in order to get these metadata from HDR+ capture request directly
  void RemoveJpegMetadata(
      std::vector<std::unique_ptr<HalCameraMetadata>>* metadata);
  std::mutex process_block_lock_;

  // Protected by process_block_lock_.
  std::unique_ptr<ProcessBlock> process_block_;

  InternalStreamManager* internal_stream_manager_;
  int32_t raw_stream_id_ = -1;
  uint32_t active_array_width_ = 0;
  uint32_t active_array_height_ = 0;
  // The number of HDR+ input buffers
  uint32_t payload_frames_ = 0;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_HDRPLUS_REQUEST_PROCESSOR_H_
