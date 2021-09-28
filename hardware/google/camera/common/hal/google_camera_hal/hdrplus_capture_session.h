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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_HDRPLUS_CAPTURE_SESSION_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_HDRPLUS_CAPTURE_SESSION_H_

#include "camera_buffer_allocator_hwl.h"
#include "camera_device_session_hwl.h"
#include "capture_session.h"
#include "hwl_types.h"
#include "request_processor.h"
#include "result_dispatcher.h"
#include "result_processor.h"
#include "vendor_tag_types.h"

namespace android {
namespace google_camera_hal {

// HdrplusCaptureSession implements a CaptureSession that contains two
// process chains (realtime and HDR+)
//
// 1. RealtimeZslRequestProcessor -> RealtimeProcessBlock ->
//    RealtimeZslResultProcessor
// 2. HdrplusRequestProcessor -> HdrplusProcessBlock -> HdrplusResultProcessor
//
// It only supports a single physical camera device session.
class HdrplusCaptureSession : public CaptureSession {
 public:
  // Return if the device session HWL and stream configuration are supported.
  static bool IsStreamConfigurationSupported(
      CameraDeviceSessionHwl* device_session_hwl,
      const StreamConfiguration& stream_config);

  // Create a HdrplusCaptureSession.
  //
  // device_session_hwl is owned by the caller and must be valid during the
  // lifetime of HdrplusCaptureSession.
  // stream_config is the stream configuration.
  // process_capture_result is the callback function to notify results.
  // notify is the callback function to notify messages.
  // hal_configured_streams will be filled with HAL configured streams.
  // camera_allocator_hwl is owned by the caller and must be valid during the
  // lifetime of HdrplusCaptureSession
  static std::unique_ptr<HdrplusCaptureSession> Create(
      CameraDeviceSessionHwl* device_session_hwl,
      const StreamConfiguration& stream_config,
      ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
      HwlRequestBuffersFunc request_stream_buffers,
      std::vector<HalStream>* hal_configured_streams,
      CameraBufferAllocatorHwl* camera_allocator_hwl = nullptr);

  virtual ~HdrplusCaptureSession();

  // Override functions in CaptureSession start.
  status_t ProcessRequest(const CaptureRequest& request) override;

  status_t Flush() override;
  // Override functions in CaptureSession end.

 protected:
  HdrplusCaptureSession() = default;

 private:
  static const uint32_t kRawBufferCount = 16;
  static const uint32_t kRawMinBufferCount = 12;
  static constexpr uint32_t kPartialResult = 1;
  static const android_pixel_format_t kHdrplusRawFormat = HAL_PIXEL_FORMAT_RAW10;
  status_t Initialize(CameraDeviceSessionHwl* device_session_hwl,
                      const StreamConfiguration& stream_config,
                      ProcessCaptureResultFunc process_capture_result,
                      NotifyFunc notify,
                      std::vector<HalStream>* hal_configured_streams);

  // Setup realtime process chain
  status_t SetupRealtimeProcessChain(
      const StreamConfiguration& stream_config,
      ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
      std::unique_ptr<ProcessBlock>* realtime_process_block,
      std::unique_ptr<ResultProcessor>* realtime_result_processor,
      int32_t* raw_stream_id);

  // Setup hdrplus process chain
  status_t SetupHdrplusProcessChain(
      const StreamConfiguration& stream_config,
      ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
      std::unique_ptr<ProcessBlock>* hdrplus_process_block,
      std::unique_ptr<ResultProcessor>* hdrplus_result_processor,
      int32_t raw_stream_id);

  // Configure streams for request processor and process block.
  status_t ConfigureStreams(const StreamConfiguration& stream_config,
                            RequestProcessor* request_processor,
                            ProcessBlock* process_block, int32_t* raw_stream_id);

  // Configure hdrplus streams for request processor and process block.
  status_t ConfigureHdrplusStreams(const StreamConfiguration& stream_config,
                                   RequestProcessor* hdrplus_request_processor,
                                   ProcessBlock* hdrplus_process_block);

  // Build pipelines and return HAL configured streams.
  // Allocate internal raw buffer
  status_t BuildPipelines(ProcessBlock* process_block,
                          ProcessBlock* hdrplus_process_block,
                          std::vector<HalStream>* hal_configured_streams);

  // Connect the process chain.
  status_t ConnectProcessChain(RequestProcessor* request_processor,
                               std::unique_ptr<ProcessBlock> process_block,
                               std::unique_ptr<ResultProcessor> result_processor);

  // Purge the hal_configured_streams such that only framework streams are left
  status_t PurgeHalConfiguredStream(
      const StreamConfiguration& stream_config,
      std::vector<HalStream>* hal_configured_streams);

  // Invoked when receiving a result from result processor.
  void ProcessCaptureResult(std::unique_ptr<CaptureResult> result);

  // Invoked when reciving a message from result processor.
  void NotifyHalMessage(const NotifyMessage& message);

  std::unique_ptr<RequestProcessor> request_processor_;

  std::unique_ptr<RequestProcessor> hdrplus_request_processor_;
  // device_session_hwl_ is owned by the client.
  CameraDeviceSessionHwl* device_session_hwl_ = nullptr;

  std::unique_ptr<InternalStreamManager> internal_stream_manager_;

  std::unique_ptr<ResultDispatcher> result_dispatcher_;

  std::mutex callback_lock_;
  // The following callbacks must be protected by callback_lock_.
  ProcessCaptureResultFunc process_capture_result_;
  NotifyFunc notify_;
  // For error notify to framework directly
  NotifyFunc device_session_notify_;
  // Use this stream id to check the request is HDR+ compatible
  int32_t hal_preview_stream_id_ = -1;

  HdrMode hdr_mode_ = HdrMode::kHdrplusMode;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_HDRPLUS_CAPTURE_SESSION_H_
