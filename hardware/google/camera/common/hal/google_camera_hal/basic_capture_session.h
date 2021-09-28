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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_BASIC_CAPTURE_SESSION_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_BASIC_CAPTURE_SESSION_H_

#include "camera_buffer_allocator_hwl.h"
#include "camera_device_session_hwl.h"
#include "capture_session.h"
#include "hwl_types.h"
#include "request_processor.h"
#include "result_processor.h"

namespace android {
namespace google_camera_hal {

// BasicCaptureSession implements a CaptureSession that contains a single
// process chain that consists of
//
//   BasicRequestProcessor -> RealtimeProcessBlock -> BasicResultProcessor
//
// It only supports a single physical camera device session.
class BasicCaptureSession : public CaptureSession {
 public:
  // Return if the device session HWL and stream configuration are supported.
  static bool IsStreamConfigurationSupported(
      CameraDeviceSessionHwl* device_session_hwl,
      const StreamConfiguration& stream_config);

  // Create a BasicCaptureSession.
  //
  // device_session_hwl is owned by the caller and must be valid during the
  // lifetime of BasicCaptureSession.
  // stream_config is the stream configuration.
  // process_capture_result is the callback function to notify results.
  // notify is the callback function to notify messages.
  // hal_configured_streams will be filled with HAL configured streams.
  // camera_allocator_hwl is owned by the caller and must be valid during the
  // lifetime of BasicCaptureSession
  static std::unique_ptr<CaptureSession> Create(
      CameraDeviceSessionHwl* device_session_hwl,
      const StreamConfiguration& stream_config,
      ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
      HwlRequestBuffersFunc request_stream_buffers,
      std::vector<HalStream>* hal_configured_streams,
      CameraBufferAllocatorHwl* camera_allocator_hwl = nullptr);

  virtual ~BasicCaptureSession();

  // Override functions in CaptureSession start.
  status_t ProcessRequest(const CaptureRequest& request) override;

  status_t Flush() override;
  // Override functions in CaptureSession end.

 protected:
  BasicCaptureSession() = default;

 private:
  status_t Initialize(CameraDeviceSessionHwl* device_session_hwl,
                      const StreamConfiguration& stream_config,
                      ProcessCaptureResultFunc process_capture_result,
                      NotifyFunc notify,
                      std::vector<HalStream>* hal_configured_streams);

  // Configure streams for request processor and process block.
  status_t ConfigureStreams(const StreamConfiguration& stream_config,
                            RequestProcessor* request_processor,
                            ProcessBlock* process_block);

  // Build pipelines and return HAL configured streams.
  status_t BuildPipelines(ProcessBlock* process_block,
                          std::vector<HalStream>* hal_configured_streams);

  // Connect the process chain.
  status_t ConnectProcessChain(RequestProcessor* request_processor,
                               std::unique_ptr<ProcessBlock> process_block,
                               std::unique_ptr<ResultProcessor> result_processor);

  std::unique_ptr<RequestProcessor> request_processor_;

  // device_session_hwl_ is owned by the client.
  CameraDeviceSessionHwl* device_session_hwl_ = nullptr;
  std::unique_ptr<InternalStreamManager> internal_stream_manager_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_BASIC_CAPTURE_SESSION_H_
