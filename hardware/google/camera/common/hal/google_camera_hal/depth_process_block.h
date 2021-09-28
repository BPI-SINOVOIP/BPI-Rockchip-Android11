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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DEPTH_PROCESS_BLOCK_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DEPTH_PROCESS_BLOCK_H_

#include <map>

#include "depth_generator.h"
#include "hwl_types.h"
#include "process_block.h"

using android::depth_generator::DepthGenerator;
using android::depth_generator::DepthRequestInfo;
using android::depth_generator::DepthResultStatus;

namespace android {
namespace google_camera_hal {

// DepthProcessBlock implements a ProcessBlock to generate a depth stream
// for a logical camera consisting of one RGB and two IR camera sensors.
class DepthProcessBlock : public ProcessBlock {
 public:
  struct DepthProcessBlockCreateData {
    // stream id of the internal yuv stream from RGB sensor
    int32_t rgb_internal_yuv_stream_id = -1;
    // stream id of the internal raw stream from IR 1
    int32_t ir1_internal_raw_stream_id = -1;
    // stream id of the internal raw stream from IR 2
    int32_t ir2_internal_raw_stream_id = -1;
  };
  // Create a DepthProcessBlock.
  static std::unique_ptr<DepthProcessBlock> Create(
      CameraDeviceSessionHwl* device_session_hwl,
      HwlRequestBuffersFunc request_stream_buffers,
      const DepthProcessBlockCreateData& create_data);

  virtual ~DepthProcessBlock();

  // Override functions of ProcessBlock start.
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
  DepthProcessBlock(HwlRequestBuffersFunc request_stream_buffers_,
                    const DepthProcessBlockCreateData& create_data);

 private:
  struct PendingDepthRequestInfo {
    CaptureRequest request;
    DepthRequestInfo depth_request;
  };

  static constexpr int32_t kInvalidStreamId = -1;
  const uint32_t kDepthStreamMaxBuffers = 8;

  // Callback function to request stream buffer from camera device session
  const HwlRequestBuffersFunc request_stream_buffers_;

  // Load the depth generator dynamically
  status_t LoadDepthGenerator(std::unique_ptr<DepthGenerator>* depth_generator);

  // Map the input and output buffers from buffer_handle_t to UMD virtual addr
  status_t MapBuffersForDepthGenerator(const StreamBuffer& stream_buffer,
                                       depth_generator::Buffer* depth_buffer);

  // Get the gralloc buffer size of a stream
  status_t GetStreamBufferSize(const Stream& stream, int32_t* buffer_size);

  // Ummap the input and output buffers
  status_t UnmapBuffersForDepthGenerator(const StreamBuffer& stream_buffer,
                                         uint8_t* addr);

  // Prepare a depth request info for the depth generator
  status_t PrepareDepthRequestInfo(const CaptureRequest& request,
                                   DepthRequestInfo* depth_request_info,
                                   HalCameraMetadata* metadata,
                                   const HalCameraMetadata* color_metadata);

  // Clean up a depth request info by unmapping the buffers
  status_t UnmapDepthRequestBuffers(uint32_t frame_number);

  // Caclculate the ratio of logical camera active array size comparing to the
  // IR camera active array size
  status_t CalculateActiveArraySizeRatio(
      CameraDeviceSessionHwl* device_session_hwl);

  // Calculate the crop region info from the RGB sensor framework to the IR
  // sensor framework. Update the depth_request_info with the updated result.
  status_t UpdateCropRegion(const CaptureRequest& request,
                            DepthRequestInfo* depth_request_info,
                            HalCameraMetadata* metadata);

  // Request the stream buffer for depth stream. incomplete_buffer is the
  // StreamBuffer that does not have a valid buffer handle and needs to be
  // replaced by the newly requested buffer.
  status_t RequestDepthStreamBuffer(StreamBuffer* incomplete_buffer,
                                    uint32_t frame_number);

  // Initialize the HAL Buffer Management status.
  status_t InitializeBufferManagementStatus(
      CameraDeviceSessionHwl* device_session_hwl);

  // Submit a depth request through the blocking depth generator API
  status_t SubmitBlockingDepthRequest(const DepthRequestInfo& request_info);

  // Submit a detph request through the asynchronized depth generator API
  status_t SubmitAsyncDepthRequest(const DepthRequestInfo& request_info);

  // Process the depth result of frame frame_number
  status_t ProcessDepthResult(DepthResultStatus result_status,
                              uint32_t frame_number);

  // Map all buffers needed by a depth request from request
  status_t MapDepthRequestBuffers(const CaptureRequest& request,
                                  DepthRequestInfo* depth_request_info);

  mutable std::mutex configure_lock_;

  // If streams are configured. Must be protected by configure_lock_.
  bool is_configured_ = false;

  std::mutex result_processor_lock_;

  // Result processor. Must be protected by result_processor_lock_.
  std::unique_ptr<ResultProcessor> result_processor_ = nullptr;

  // Depth stream configured in the depth process block
  HalStream depth_stream_;

  // TODO(b/128633958): remove this after FLL syncing is verified
  bool force_internal_stream_ = false;

  // Provider library handle.
  void* depth_generator_lib_handle_ = nullptr;

  // Depth Generator
  std::unique_ptr<DepthGenerator> depth_generator_ = nullptr;

  // Map from stream id to their buffer size
  std::map<int32_t, uint32_t> stream_buffer_sizes_;

  // Map from stream id to the stream
  std::map<int32_t, Stream> depth_io_streams_;

  // Ratio of logical camera active array size comparing to IR camera active
  // array size.
  float logical_to_ir_ratio_ = 1.0;

  // IR sensor active array sizes
  int32_t ir_active_array_width_ = 640;
  int32_t ir_active_array_height_ = 480;

  // Whether the HAL Buffer Management is supported
  bool buffer_management_supported_ = false;

  // Whether the pipelined depth engine is enabled
  bool pipelined_depth_engine_enabled_ = false;

  std::mutex pending_requests_mutex_;
  // Pending depth request indexed by the frame_number
  // Must be protected by pending_requests_mutex_
  std::unordered_map<uint32_t, PendingDepthRequestInfo> pending_depth_requests_;

  // Whether RGB-IR auto-calibration is enabled. This affects how the internal
  // YUV stream results are handled.
  bool rgb_ir_auto_cal_enabled_ = false;

  // stream id of the internal yuv stream from RGB sensor
  int32_t rgb_internal_yuv_stream_id_ = kInvalidStreamId;
  // stream id of the internal raw stream from IR 1
  int32_t ir1_internal_raw_stream_id_ = kInvalidStreamId;
  // stream id of the internal raw stream from IR 2
  int32_t ir2_internal_raw_stream_id_ = kInvalidStreamId;

  // Guarding async depth generator API calls and the result processing calls
  std::mutex depth_generator_api_lock_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_DEPTH_PROCESS_BLOCK_H_
