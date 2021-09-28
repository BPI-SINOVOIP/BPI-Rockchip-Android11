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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RGBIRD_RESULT_REQUEST_PROCESSOR_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RGBIRD_RESULT_REQUEST_PROCESSOR_H_

#include <set>

#include "request_processor.h"
#include "result_processor.h"
#include "vendor_tag_defs.h"

namespace android {
namespace google_camera_hal {

// RgbirdResultRequestProcessor implements a ResultProcessor handling realtime
// capture results for a logical camera consisting of one RGB and two IR camera
// sensors.
class RgbirdResultRequestProcessor : public ResultProcessor,
                                     public RequestProcessor {
 public:
  struct RgbirdResultRequestProcessorCreateData {
    // camera id of the color sensor
    uint32_t rgb_camera_id = 0;
    // camera id of the NIR sensor used as source
    uint32_t ir1_camera_id = 0;
    // camera id of the NIR sensor used as target
    uint32_t ir2_camera_id = 0;
    // stream id of the internal raw stream for hdr+
    int32_t rgb_raw_stream_id = -1;
    // whether hdr+ is supported
    bool is_hdrplus_supported = false;
    // stream id of the internal yuv stream in case depth is configured
    int32_t rgb_internal_yuv_stream_id = -1;
  };

  static std::unique_ptr<RgbirdResultRequestProcessor> Create(
      const RgbirdResultRequestProcessorCreateData& create_data);

  virtual ~RgbirdResultRequestProcessor() = default;

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
  RgbirdResultRequestProcessor(
      const RgbirdResultRequestProcessorCreateData& create_data);

 private:
  static constexpr int32_t kInvalidStreamId = -1;
  static constexpr uint32_t kAutocalFrameNumber = 5;
  static constexpr uint32_t kNumOfAutoCalInputBuffers = /*YUV+IR+IR*/ 3;
  const uint32_t kRgbCameraId;
  const uint32_t kIr1CameraId;
  const uint32_t kIr2CameraId;
  const int32_t kSyncWaitTime = 5000;  // milliseconds

  void ProcessResultForHdrplus(CaptureResult* result, bool* rgb_raw_output);
  // Return the RGB internal YUV stream buffer if there is any and depth is
  // configured
  void TryReturnInternalBufferForDepth(CaptureResult* result,
                                       bool* has_internal);

  // Save face detect mode for HDR+
  void SaveFdForHdrplus(const CaptureRequest& request);
  // Handle face detect metadata from result for HDR+
  status_t HandleFdResultForHdrplus(uint32_t frameNumber,
                                    HalCameraMetadata* metadata);
  // Save lens shading map mode for HDR+
  void SaveLsForHdrplus(const CaptureRequest& request);
  // Handle Lens shading metadata from result for HDR+
  status_t HandleLsResultForHdrplus(uint32_t frameNumber,
                                    HalCameraMetadata* metadata);
  // TODO(b/127322570): update the following function after FLL sync verified
  // Remove internal streams for depth lock
  status_t ReturnInternalStreams(CaptureResult* result);

  // Check fence status if need
  status_t CheckFenceStatus(CaptureRequest* request);

  // Check all metadata exist for Autocal
  // Protected by depth_requests_mutex_
  bool IsAutocalMetadataReadyLocked(const HalCameraMetadata& metadata);

  // Prepare Depth Process Block request and try to submit that
  status_t TrySubmitDepthProcessBlockRequest(
      const ProcessBlockResult& block_result);

  // Whether the internal yuv stream buffer needs to be passed to the depth
  // process block.
  bool IsAutocalRequest(uint32_t frame_number) const;

  // Verify if all information is ready for a depth request for frame_number and
  // submit the request to the process block if so.
  status_t VerifyAndSubmitDepthRequest(uint32_t frame_number);

  std::mutex callback_lock_;

  // The following callbacks must be protected by callback_lock_.
  ProcessCaptureResultFunc process_capture_result_;
  NotifyFunc notify_;

  std::mutex depth_process_block_lock_;
  // Protected by depth_process_block_lock_.
  std::unique_ptr<ProcessBlock> depth_process_block_;

  // rgb_raw_stream_id_ is the stream ID of internal raw from RGB camera for HDR+
  int32_t rgb_raw_stream_id_ = -1;
  bool is_hdrplus_supported_ = false;

  // Current face detect mode set by framework.
  uint8_t current_face_detect_mode_ = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;

  std::mutex face_detect_lock_;
  // Map from frame number to face detect mode requested for that frame by
  // framework. And requested_face_detect_modes_ is protected by
  // face_detect_lock_
  std::unordered_map<uint32_t, uint8_t> requested_face_detect_modes_;

  // Current lens shading map mode set by framework.
  uint8_t current_lens_shading_map_mode_ =
      ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF;

  std::mutex lens_shading_lock_;
  // Map from frame number to lens shading map mode requested for that frame
  // by framework. And requested_lens_shading_map_modes_ is protected by
  // lens_shading_lock_
  std::unordered_map<uint32_t, uint8_t> requested_lens_shading_map_modes_;

  // Internal stream manager
  InternalStreamManager* internal_stream_manager_ = nullptr;

  // TODO(b/128633958): remove this after FLL syncing is verified
  bool force_internal_stream_ = false;

  // Set of framework stream id
  std::set<int32_t> framework_stream_id_set_;

  std::mutex depth_requests_mutex_;

  // Map from framework number to capture request for depth process block. If a
  // request does not contain any depth buffer, it is not recorded in the map.
  // Protected by depth_requests_mutex_
  std::unordered_map<uint32_t, std::unique_ptr<CaptureRequest>> depth_requests_;

  // Depth stream id if it is configured for the current session
  int32_t depth_stream_id_ = -1;

  // If a depth stream is configured, always configure an extra internal YUV
  // stream to cover the case when there is no request for any stream from the
  // RGB sensor.
  int32_t rgb_internal_yuv_stream_id_ = -1;

  // Whether RGB-IR auto-calibration is enabled. This affects how the internal
  // YUV stream results are handled.
  bool rgb_ir_auto_cal_enabled_ = false;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_RGBIRD_RESULT_REQUEST_PROCESSOR_H_
