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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DUAL_IR_CAPTURE_SESSION_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DUAL_IR_CAPTURE_SESSION_H_

#include "camera_device_session_hwl.h"
#include "capture_session.h"
#include "depth_process_block.h"
#include "dual_ir_depth_result_processor.h"
#include "dual_ir_request_processor.h"
#include "dual_ir_result_request_processor.h"
#include "hwl_types.h"
#include "multicam_realtime_process_block.h"
#include "result_processor.h"

namespace android {
namespace google_camera_hal {

// DualIrCaptureSession implements a CaptureSession that contains a single
// process chain that consists of
//
//   DualIrRequestProcessor -> MultiCameraRtProcessBlock ->
//     DualIrResultRequestProcessor -> DepthProcessBlock ->
//     DualIrDepthResultProcessor
//
// It only supports a camera device session that consists of two IR cameras.
class DualIrCaptureSession : public CaptureSession {
 public:
  // Return if the device session HWL and stream configuration are supported.
  static bool IsStreamConfigurationSupported(
      CameraDeviceSessionHwl* device_session_hwl,
      const StreamConfiguration& stream_config);

  // Create a DualIrCaptureSession.
  //
  // device_session_hwl is owned by the caller and must be valid during the
  // lifetime of DualIrCaptureSession.
  // stream_config is the stream configuration.
  // process_capture_result is the callback function to notify results.
  // notify is the callback function to notify messages.
  // hal_configured_streams will be filled with HAL configured streams.
  // camera_allocator_hwl is owned by the caller and must be valid during the
  // lifetime of DualIrCaptureSession
  static std::unique_ptr<CaptureSession> Create(
      CameraDeviceSessionHwl* device_session_hwl,
      const StreamConfiguration& stream_config,
      ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
      HwlRequestBuffersFunc request_stream_buffers,
      std::vector<HalStream>* hal_configured_streams,
      CameraBufferAllocatorHwl* camera_allocator_hwl);

  virtual ~DualIrCaptureSession();

  // Override functions in CaptureSession start.
  status_t ProcessRequest(const CaptureRequest& request) override;

  status_t Flush() override;
  // Override functions in CaptureSession end.

 protected:
  DualIrCaptureSession(uint32_t lead_camera_id);

 private:
  const uint32_t kLeadCameraId;

  status_t Initialize(CameraDeviceSessionHwl* device_session_hwl,
                      const StreamConfiguration& stream_config,
                      ProcessCaptureResultFunc process_capture_result,
                      NotifyFunc notify,
                      std::vector<HalStream>* hal_configured_streams);

  status_t CreateProcessChain(const StreamConfiguration& stream_config,
                              ProcessCaptureResultFunc process_capture_result,
                              NotifyFunc notify,
                              std::vector<HalStream>* hal_configured_streams);

  // Connect ProcessBlock and Request/Result processors to form a process chain
  status_t ConnectProcessChain(RequestProcessor* request_processor,
                               std::unique_ptr<ProcessBlock> process_block,
                               std::unique_ptr<ResultProcessor> result_processor);

  // Check if all streams in stream_config are also in
  // process_block_stream_config.
  bool AreAllStreamsConfigured(
      const StreamConfiguration& stream_config,
      const StreamConfiguration& process_block_stream_config) const;

  // Configure streams for the process chain.
  status_t ConfigureStreams(RequestProcessor* request_processor,
                            ProcessBlock* process_block,
                            const StreamConfiguration& overall_config,
                            const StreamConfiguration& stream_config,
                            StreamConfiguration* process_block_stream_config);

  // Make a stream configuration for the depth chaing segment in case a depth
  // stream is configured by the framework.
  status_t MakeDepthChainSegmentStreamConfig(
      const StreamConfiguration& stream_config,
      StreamConfiguration* rt_process_block_stream_config,
      StreamConfiguration* depth_chain_segment_stream_config);

  // Purge the hal_configured_streams such that only framework streams are left
  status_t PurgeHalConfiguredStream(
      const StreamConfiguration& stream_config,
      std::vector<HalStream>* hal_configured_streams);

  // Setup the realtime segment of the DualIr process chain. This creates the
  // related process block and request/result processors. It also calls the
  // ConfigureStreams for the request processor and the process block.
  status_t SetupRealtimeSegment(
      const StreamConfiguration& stream_config,
      StreamConfiguration* process_block_stream_config,
      std::unique_ptr<MultiCameraRtProcessBlock>* rt_process_block,
      std::unique_ptr<DualIrResultRequestProcessor>* rt_result_request_processor);

  // Setup the depth segment of the DualIr process chain. This creates the
  // related process block and request/result processors. It also calls the
  // ConfigureStreams for the request processor and the process block.
  // It generates the stream_config for the depth chain segment internally.
  // This function should only be invoked when has_depth_stream_ is true
  status_t SetupDepthSegment(
      const StreamConfiguration& stream_config,
      StreamConfiguration* process_block_stream_config,
      DualIrResultRequestProcessor* rt_result_request_processor,
      std::unique_ptr<DepthProcessBlock>* depth_process_block,
      std::unique_ptr<DualIrDepthResultProcessor>* depth_result_processor);

  // This build pipelines for all process blocks. It also collects those
  // framework streams which are configured by the HAL(i.e. process blocks)
  status_t BuildPipelines(const StreamConfiguration& stream_config,
                          std::vector<HalStream>* hal_configured_streams,
                          MultiCameraRtProcessBlock* rt_process_block,
                          DepthProcessBlock* depth_process_block);

  // device_session_hwl_ is owned by the client.
  CameraDeviceSessionHwl* device_session_hwl_ = nullptr;

  std::unique_ptr<DualIrRequestProcessor> request_processor_;

  // Internal stream manager
  std::unique_ptr<InternalStreamManager> internal_stream_manager_;

  // Whether there is a depth stream configured in the current session
  bool has_depth_stream_ = false;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DUAL_IR_CAPTURE_SESSION_H_
