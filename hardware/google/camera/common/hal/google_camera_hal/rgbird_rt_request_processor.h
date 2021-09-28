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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RGBIRD_RT_REQUEST_PROCESSOR_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RGBIRD_RT_REQUEST_PROCESSOR_H_

#include <limits>

#include "process_block.h"
#include "request_processor.h"

namespace android {
namespace google_camera_hal {

// RgbirdRtRequestProcessor implements a RequestProcessor handling realtime
// requests for a logical camera consisting of one RGB camera sensor and two IR
// camera sensors.
class RgbirdRtRequestProcessor : public RequestProcessor {
 public:
  // device_session_hwl is owned by the caller and must be valid during the
  // lifetime of this RgbirdRtRequestProcessor.
  static std::unique_ptr<RgbirdRtRequestProcessor> Create(
      CameraDeviceSessionHwl* device_session_hwl, bool is_hdrplus_supported);

  virtual ~RgbirdRtRequestProcessor() = default;

  // Override functions of RequestProcessor start.
  status_t ConfigureStreams(
      InternalStreamManager* internal_stream_manager,
      const StreamConfiguration& stream_config,
      StreamConfiguration* process_block_stream_config) override;

  status_t SetProcessBlock(std::unique_ptr<ProcessBlock> process_block) override;

  status_t ProcessRequest(const CaptureRequest& request) override;

  status_t Flush() override;
  // Override functions of RequestProcessor end.

  // Whether the current session is a session in which auto cal should happen.
  bool IsAutocalSession() const;

 protected:
  RgbirdRtRequestProcessor(uint32_t rgb_camera_id, uint32_t ir1_camera_id,
                           uint32_t ir2_camera_id, uint32_t active_array_width,
                           uint32_t active_array_height,
                           bool is_hdrplus_supported,
                           CameraDeviceSessionHwl* device_session_hwl);

 private:
  static const int32_t kStreamIdInvalid = -1;
  static constexpr uint32_t kAutocalFrameNumber = 5;
  const uint32_t kDefaultYuvStreamWidth = 640;
  const uint32_t kDefaultYuvStreamHeight = 480;
  const uint32_t kRgbCameraId;
  const uint32_t kIr1CameraId;
  const uint32_t kIr2CameraId;

  status_t CreateDepthInternalStreams(
      InternalStreamManager* internal_stream_manager,
      StreamConfiguration* process_block_stream_config);

  status_t RegisterHdrplusInternalRaw(
      StreamConfiguration* process_block_stream_config);
  status_t TryAddHdrplusRawOutputLocked(CaptureRequest* block_request,
                                        const CaptureRequest& request);
  // Try to add RGB internal YUV buffer if there is no request on any stream
  // from the RGB sensor.
  // Must lock process_block_lock_ before calling this function.
  status_t TryAddDepthInternalYuvOutputLocked(CaptureRequest* block_request);
  status_t AddIrRawProcessBlockRequestLocked(
      std::vector<ProcessBlockRequest>* block_requests,
      const CaptureRequest& request, uint32_t camera_id);

  status_t TryAddRgbProcessBlockRequestLocked(
      std::vector<ProcessBlockRequest>* block_requests,
      const CaptureRequest& request);

  // Find a resolution from the available stream configuration that has the same
  // aspect ratio with one of the non-raw and non-depth stream in the framework
  // stream config.
  // If there is no non-raw and non-depth stream from framework, use the
  // resolution with the smallest area in the available stream config.
  status_t FindSmallestResolutionForInternalYuvStream(
      const StreamConfiguration& process_block_stream_config,
      uint32_t* yuv_w_adjusted, uint32_t* yuv_h_adjusted);

  /// Find smallest non-warped YUV stream resolution supported by HWL
  status_t FindSmallestNonWarpedYuvStreamResolution(uint32_t* yuv_w_adjusted,
                                                    uint32_t* yuv_h_adjusted);

  // Set the stream id of the yuv stream that does not need warping in
  // the session parameter of the process block stream configuration.
  status_t SetNonWarpedYuvStreamId(
      int32_t non_warped_yuv_stream_id,
      StreamConfiguration* process_block_stream_config);

  // Whether the internal YUV stream result should be used for auto cal.
  bool IsAutocalRequest(uint32_t frame_number);

  std::mutex process_block_lock_;

  // Protected by process_block_lock_.
  std::unique_ptr<ProcessBlock> process_block_;
  // [0]: IR1 stream; [1]: IR2 stream
  int32_t ir_raw_stream_id_[2] = {kStreamIdInvalid, kStreamIdInvalid};
  int32_t rgb_yuv_stream_id_ = kStreamIdInvalid;

  bool preview_intent_seen_ = false;
  // rgb_raw_stream_id_ is the stream ID of internal raw from RGB camera for HDR+
  int32_t rgb_raw_stream_id_ = -1;
  uint32_t rgb_active_array_width_ = 0;
  uint32_t rgb_active_array_height_ = 0;
  bool is_hdrplus_supported_ = false;
  bool is_hdrplus_zsl_enabled_ = false;

  // TODO(b/128633958): remove this after FLL syncing is verified
  bool force_internal_stream_ = false;
  int32_t depth_stream_id_ = kStreamIdInvalid;
  InternalStreamManager* internal_stream_manager_ = nullptr;
  CameraDeviceSessionHwl* device_session_hwl_ = nullptr;

  // Whether RGB-IR auto cal is needed
  bool rgb_ir_auto_cal_enabled_ = false;
  // Indicates whether a session needs auto cal(not every session needs even if
  // rgb_ir_auto_cal_enabled_ is true).
  bool is_auto_cal_session_ = false;
  bool auto_cal_triggered_ = false;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RGBIRD_RT_REQUEST_PROCESSOR_H_
