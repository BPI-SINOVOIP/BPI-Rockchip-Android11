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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_HAL_UTILS_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_HAL_UTILS_H_

#include "hal_types.h"
#include "hwl_types.h"
#include "process_block.h"
#include "utils.h"

namespace android {
namespace google_camera_hal {
namespace hal_utils {

// Create a HWL pipeline request for a pipeline based on a capture request.
status_t CreateHwlPipelineRequest(HwlPipelineRequest* hwl_request,
                                  uint32_t pipeline_id,
                                  const CaptureRequest& request);

// Create a vector of sychrounous HWL pipeline requests for pipelines
// based on capture requests.
// pipeline_ids and requests must have the same size.
// One HWL request will be created for each pair of a pipeline ID and a request.
status_t CreateHwlPipelineRequests(
    std::vector<HwlPipelineRequest>* hwl_requests,
    const std::vector<uint32_t>& pipeline_ids,
    const std::vector<ProcessBlockRequest>& requests);

// Convert a HWL result to a capture result.
std::unique_ptr<CaptureResult> ConvertToCaptureResult(
    std::unique_ptr<HwlPipelineResult> hwl_result);

// Return if the request contains an output buffer.
bool ContainsOutputBuffer(const CaptureRequest& request,
                          const buffer_handle_t& buffer);

// Return if all output buffers in remaining_session_request are included in
// process_block_requests.
bool AreAllRemainingBuffersRequested(
    const std::vector<ProcessBlockRequest>& process_block_requests,
    const CaptureRequest& remaining_session_request);

// Return if this is an IR camera.
bool IsIrCamera(const HalCameraMetadata* characteristics);

// Return if this is an MONO camera.
bool IsMonoCamera(const HalCameraMetadata* characteristics);

// Return if this is a bayer camera.
bool IsBayerCamera(const HalCameraMetadata* characteristics);

// Return if this is a HDR+ request
bool IsRequestHdrplusCompatible(const CaptureRequest& request,
                                int32_t preview_stream_id);

// Return true if this is a fixed-focus camera.
bool IsFixedFocusCamera(const HalCameraMetadata* characteristics);

// Return if HDR+ stream is supported
bool IsStreamHdrplusCompatible(const StreamConfiguration& stream_config,
                               const HalCameraMetadata* characteristics);

// Set ANDROID_CONTROL_ENABLE_ZSL metadata
status_t SetEnableZslMetadata(HalCameraMetadata* metadata, bool enable);

// Set hybrid AE vendor tag
status_t SetHybridAeMetadata(HalCameraMetadata* metadata, bool enable);

// Modify the request of realtime pipeline for HDR+
status_t ModifyRealtimeRequestForHdrplus(HalCameraMetadata* metadata,
                                         const bool hybrid_ae_enable = true);

// Get ANDROID_STATISTICS_FACE_DETECT_MODE
status_t GetFdMode(const CaptureRequest& request, uint8_t* face_detect_mode);

// Remove face detect information
status_t RemoveFdInfoFromResult(HalCameraMetadata* metadata);

// Force lens shading map mode on
status_t ForceLensShadingMapModeOn(HalCameraMetadata* metadata);

// Get lens shading map mode
status_t GetLensShadingMapMode(const CaptureRequest& request,
                               uint8_t* lens_shading_mode);

// Remove lens shading information
status_t RemoveLsInfoFromResult(HalCameraMetadata* metadata);

// Dump the information in the stream configuration
void DumpStreamConfiguration(const StreamConfiguration& stream_configuration,
                             std::string title);

// Dump the information in the HAL configured streams
void DumpHalConfiguredStreams(
    const std::vector<HalStream>& hal_configured_streams, std::string title);

// Dump the information in a capture request
void DumpCaptureRequest(const CaptureRequest& request, std::string title);

// Dump the information in a capture result
void DumpCaptureResult(const ProcessBlockResult& result, std::string title);

// Dump the information in a capture result
void DumpCaptureResult(const CaptureResult& result, std::string title);

// Dump the information in a notification
void DumpNotify(const NotifyMessage& message, std::string title);

// Dump Stream
void DumpStream(const Stream& stream, std::string title);

// Dump HalStream
void DumpHalStream(const HalStream& hal_stream, std::string title);

// Dump the information in a buffer return
void DumpBufferReturn(const std::vector<StreamBuffer>& stream_buffers,
                      std::string title);

// Dump the information in a buffer request
void DumpBufferRequest(const std::vector<BufferRequest>& hal_buffer_requests,
                       const std::vector<BufferReturn>* hal_buffer_returns,
                       std::string title);

}  // namespace hal_utils
}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_HAL_UTILS_H_