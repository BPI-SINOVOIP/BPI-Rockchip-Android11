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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RGBIRD_CAPTURE_SESSION_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RGBIRD_CAPTURE_SESSION_H_

#include "camera_buffer_allocator_hwl.h"
#include "camera_device_session_hwl.h"
#include "capture_session.h"
#include "depth_process_block.h"
#include "hwl_types.h"
#include "result_dispatcher.h"
#include "result_processor.h"
#include "rgbird_depth_result_processor.h"
#include "rgbird_result_request_processor.h"
#include "rgbird_rt_request_processor.h"

namespace android {
namespace google_camera_hal {

// RgbirdCaptureSession implements a CaptureSession that contains a single
// process chain that consists of
//
//   RgbirdRtRequestProcessor -> MultiCameraRtProcessBlock ->
//     RgbirdResultRequestProcessor -> DepthProcessBlock ->
//     BasicResultProcessor
//
// It only supports a camera device session that consists of one RGB and two
// IR cameras.
class RgbirdCaptureSession : public CaptureSession {
 public:
  // Return if the device session HWL and stream configuration are supported.
  static bool IsStreamConfigurationSupported(
      CameraDeviceSessionHwl* device_session_hwl,
      const StreamConfiguration& stream_config);

  // Create a RgbirdCaptureSession.
  //
  // device_session_hwl is owned by the caller and must be valid during the
  // lifetime of RgbirdCaptureSession.
  // stream_config is the stream configuration.
  // process_capture_result is the callback function to notify results.
  // notify is the callback function to notify messages.
  // hal_configured_streams will be filled with HAL configured streams.
  // camera_allocator_hwl is owned by the caller and must be valid during the
  // lifetime of RgbirdCaptureSession
  static std::unique_ptr<CaptureSession> Create(
      CameraDeviceSessionHwl* device_session_hwl,
      const StreamConfiguration& stream_config,
      ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
      HwlRequestBuffersFunc request_stream_buffers,
      std::vector<HalStream>* hal_configured_streams,
      CameraBufferAllocatorHwl* camera_allocator_hwl = nullptr);

  virtual ~RgbirdCaptureSession();

  // Override functions in CaptureSession start.
  status_t ProcessRequest(const CaptureRequest& request) override;

  status_t Flush() override;
  // Override functions in CaptureSession end.

 protected:
  RgbirdCaptureSession() = default;

 private:
  static constexpr int32_t kInvalidStreamId = -1;
  static const uint32_t kRgbRawBufferCount = 16;
  // Min required buffer count of internal raw stream.
  static const uint32_t kRgbMinRawBufferCount = 12;
  static constexpr uint32_t kPartialResult = 1;
  static const android_pixel_format_t kHdrplusRawFormat = HAL_PIXEL_FORMAT_RAW10;
  static const uint32_t kDefaultInternalBufferCount = 8;

  status_t Initialize(CameraDeviceSessionHwl* device_session_hwl,
                      const StreamConfiguration& stream_config,
                      ProcessCaptureResultFunc process_capture_result,
                      NotifyFunc notify,
                      HwlRequestBuffersFunc request_stream_buffers,
                      std::vector<HalStream>* hal_configured_streams);

  // Create a process chain that contains a realtime process block and a
  // depth process block.
  status_t CreateProcessChain(const StreamConfiguration& stream_config,
                              ProcessCaptureResultFunc process_capture_result,
                              NotifyFunc notify,
                              std::vector<HalStream>* hal_configured_streams);

  // Check if all streams in stream_config are also in
  // process_block_stream_config.
  bool AreAllStreamsConfigured(
      const StreamConfiguration& stream_config,
      const StreamConfiguration& process_block_stream_config) const;

  // Setup realtime process chain
  status_t SetupRealtimeProcessChain(
      const StreamConfiguration& stream_config,
      ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
      std::unique_ptr<ProcessBlock>* realtime_process_block,
      std::unique_ptr<RgbirdResultRequestProcessor>* realtime_result_processor,
      std::unique_ptr<ProcessBlock>* depth_process_block,
      std::unique_ptr<ResultProcessor>* depth_result_processor);

  // Setup hdrplus process chain
  status_t SetupHdrplusProcessChain(
      const StreamConfiguration& stream_config,
      ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
      std::unique_ptr<ProcessBlock>* hdrplus_process_block,
      std::unique_ptr<ResultProcessor>* hdrplus_result_processor);

  // Configure streams for the process chain.
  status_t ConfigureStreams(const StreamConfiguration& stream_config,
                            RequestProcessor* request_processor,
                            ProcessBlock* process_block,
                            StreamConfiguration* process_block_stream_config);

  // Build pipelines and return HAL configured streams.
  // Allocate internal raw buffer
  status_t BuildPipelines(const StreamConfiguration& stream_config,
                          ProcessBlock* realtime_process_block,
                          ProcessBlock* depth_process_block,
                          ProcessBlock* hdrplus_process_block,
                          std::vector<HalStream>* hal_configured_streams);

  // Connect the process chain.
  status_t ConnectProcessChain(RequestProcessor* request_processor,
                               std::unique_ptr<ProcessBlock> process_block,
                               std::unique_ptr<ResultProcessor> result_processor);

  // Invoked when receiving a result from result processor.
  void ProcessCaptureResult(std::unique_ptr<CaptureResult> result);

  // Invoked when reciving a message from result processor.
  void NotifyHalMessage(const NotifyMessage& message);

  // Get internal rgb raw stream id from request processor.
  status_t ConfigureHdrplusRawStreamId(
      const StreamConfiguration& process_block_stream_config);

  // Get internal RGB YUV stream id and IR RAW streams from request processor in
  // case depth is configured.
  status_t SetDepthInternalStreamId(
      const StreamConfiguration& process_block_stream_config,
      const StreamConfiguration& stream_config);

  // Combine usage of realtime and HDR+ hal stream
  // And allocate internal rgb raw stream buffers
  status_t ConfigureHdrplusUsageAndBuffers(
      std::vector<HalStream>* hal_configured_streams,
      std::vector<HalStream>* hdrplus_hal_configured_streams);

  // Allocate buffers for internal stream buffer managers
  status_t AllocateInternalBuffers(
      const StreamConfiguration& framework_stream_config,
      std::vector<HalStream>* hal_configured_streams,
      ProcessBlock* hdrplus_process_block);

  // Initialize physical camera ids from the camera characteristics
  status_t InitializeCameraIds(CameraDeviceSessionHwl* device_session_hwl);

  // Remove internal streams from the hal configured stream list
  status_t PurgeHalConfiguredStream(
      const StreamConfiguration& stream_config,
      std::vector<HalStream>* hal_configured_streams);

  // Determine if a depth process block is needed the capture session
  bool NeedDepthProcessBlock() const;

  // Create stream config for the Depth process chain segment
  // Keep all output stream from stream_config, change rt internal streams added
  // for depth processing as input streams.
  status_t MakeDepthStreamConfig(
      const StreamConfiguration& rt_process_block_stream_config,
      const StreamConfiguration& stream_config,
      StreamConfiguration* depth_stream_config);

  // Create the segment of chain that contains a depth process block
  status_t CreateDepthChainSegment(
      std::unique_ptr<DepthProcessBlock>* depth_process_block,
      std::unique_ptr<RgbirdDepthResultProcessor>* depth_result_processor,
      RgbirdResultRequestProcessor* rt_result_processor,
      const StreamConfiguration& overall_config,
      const StreamConfiguration& stream_config,
      StreamConfiguration* depth_block_stream_config);

  // Setup the offline segment connecting to the realtime process chain
  status_t SetupDepthChainSegment(
      const StreamConfiguration& stream_config,
      RgbirdResultRequestProcessor* realtime_result_processor,
      std::unique_ptr<ProcessBlock>* depth_process_block,
      std::unique_ptr<ResultProcessor>* depth_result_processor,
      StreamConfiguration* rt_process_block_stream_config);

  // device_session_hwl_ is owned by the client.
  CameraDeviceSessionHwl* device_session_hwl_ = nullptr;
  std::unique_ptr<InternalStreamManager> internal_stream_manager_;

  std::unique_ptr<RgbirdRtRequestProcessor> rt_request_processor_;

  std::unique_ptr<RequestProcessor> hdrplus_request_processor_;

  std::unique_ptr<ResultDispatcher> result_dispatcher_;

  std::mutex callback_lock_;
  // The following callbacks must be protected by callback_lock_.
  ProcessCaptureResultFunc process_capture_result_;
  NotifyFunc notify_;
  HwlRequestBuffersFunc request_stream_buffers_;

  // For error notify to framework directly
  NotifyFunc device_session_notify_;
  int32_t rgb_raw_stream_id_ = kInvalidStreamId;
  bool is_hdrplus_supported_ = false;

  // Whether the stream configuration has depth stream
  bool has_depth_stream_ = false;
  // Internal YUV stream id if there is a depth stream configured
  int32_t rgb_internal_yuv_stream_id_ = kInvalidStreamId;
  // Internal IR source stream id
  int32_t ir1_internal_raw_stream_id_ = kInvalidStreamId;
  // Internal IR target stream id
  int32_t ir2_internal_raw_stream_id_ = kInvalidStreamId;

  // Camera ids parsed from the characteristics
  uint32_t rgb_camera_id_ = 0;
  // Ir1 generates the src buffer for depth
  uint32_t ir1_camera_id_ = 0;
  // Ir2 generates the tar buffer for depth
  uint32_t ir2_camera_id_ = 0;

  // TODO(b/128633958): remove this after FLL syncing is verified
  bool force_internal_stream_ = false;
  // Use this stream id to check the request is HDR+ compatible
  int32_t hal_preview_stream_id_ = -1;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RGBIRD_CAPTURE_SESSION_H_
