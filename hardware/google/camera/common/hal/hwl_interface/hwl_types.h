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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_HWL_TYPES_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_HWL_TYPES_H_

#include "hal_types.h"

namespace android {
namespace google_camera_hal {

// Enumerates pipeline roles that are used to communicate with HWL.
enum class HwlOfflinePipelineRole {
  kOfflineInvalidRole = 0,
  kOfflineSmoothTransitionRole,
  kOfflineHdrplusRole,
};

// Define a HWL pipeline request.
struct HwlPipelineRequest {
  // ID of the pipeline that this request should be submitted to.
  uint32_t pipeline_id = 0;

  std::unique_ptr<HalCameraMetadata> settings;

  // If empty, the output buffers are captured from the camera sensors. If
  // not empty, the output buffers are captured from the input buffers.
  std::vector<StreamBuffer> input_buffers;

  // The metadata of the input_buffers. This is used for multi-frame merging
  // like HDR+.
  std::vector<std::unique_ptr<HalCameraMetadata>> input_buffer_metadata;

  std::vector<StreamBuffer> output_buffers;
};

// Define a HWL pipeline result.
struct HwlPipelineResult {
  // camera_id, pipeline_id, frame_number should match those in the original
  // request.
  uint32_t camera_id = 0;
  uint32_t pipeline_id = 0;
  uint32_t frame_number = 0;

  // result_metadata, input_buffers, and output_buffers can be returned
  // separately.
  std::unique_ptr<HalCameraMetadata> result_metadata;
  std::vector<StreamBuffer> input_buffers;
  std::vector<StreamBuffer> output_buffers;

  // Maps from physical camera ID to physical camera results.
  // Only to be used for logical cameras that receive requests
  // with output buffers belonging to streams tied to physical devices.
  std::unordered_map<uint32_t, std::unique_ptr<HalCameraMetadata>>
      physical_camera_results;

  uint32_t partial_result = 0;
};

// Callback to invoke to send a result from HWL.
using HwlProcessPipelineResultFunc =
    std::function<void(std::unique_ptr<HwlPipelineResult> /*result*/)>;

// Callback to invoke to notify a message from HWL.
using NotifyHwlPipelineMessageFunc = std::function<void(
    uint32_t /*pipeline_id*/, const NotifyMessage& /*message*/)>;

// Defines callbacks to notify from a HWL pipeline.
struct HwlPipelineCallback {
  // Callback to notify when a HWL pipeline produces a capture result.
  HwlProcessPipelineResultFunc process_pipeline_result;

  // Callback to notify shutters or errors.
  NotifyHwlPipelineMessageFunc notify;
};

// Callback to invoke to request buffers from HAL. Only in case of HFR, there
// is a chance for the client to ask for more than one buffer each time
// (in batch).
// TODO(b/134959043): a more decoupled implementation of HAL Buffer Management
//                    allowos us to remove the frame_number from the arg list.
using HwlRequestBuffersFunc = std::function<status_t(
    uint32_t /*stream_id*/, uint32_t /*num_buffers*/,
    std::vector<StreamBuffer>* /*buffers*/, uint32_t /*frame_number*/)>;

// Callback to invoke to return buffers, acquired by HwlRequestBuffersFunc,
// to HAL.
using HwlReturnBuffersFunc =
    std::function<void(const std::vector<StreamBuffer>& /*buffers*/)>;

// Defines callbacks to invoke from a HWL session.
struct HwlSessionCallback {
  // Callback to request stream buffers.
  HwlRequestBuffersFunc request_stream_buffers;

  // Callback to return stream buffers.
  HwlReturnBuffersFunc return_stream_buffers;
};

// Callback defined from framework to indicate the state of camera device
// has changed.
using HwlCameraDeviceStatusChangeFunc =
    std::function<void(uint32_t /*camera_id*/, CameraDeviceStatus /*new_status*/)>;

// Callback defined from framework to indicate the state of physical camera
// device has changed.
using HwlPhysicalCameraDeviceStatusChangeFunc =
    std::function<void(uint32_t /*camera_id*/, uint32_t /*physical_camera_id*/,
                       CameraDeviceStatus /*new_status*/)>;

// Callback defined from framework to indicate the state of the torch mode
// has changed.
using HwlTorchModeStatusChangeFunc =
    std::function<void(uint32_t /*camera_id*/, TorchModeStatus /*new_status*/)>;

// Defines callbacks to notify when a status changed.
struct HwlCameraProviderCallback {
  // Callback to notify when a camera device's status changed.
  HwlCameraDeviceStatusChangeFunc camera_device_status_change;

  // Callback to notify when a physical camera device's status changed.
  HwlPhysicalCameraDeviceStatusChangeFunc physical_camera_device_status_change;

  // Callback to notify when a torch mode status changed.
  HwlTorchModeStatusChangeFunc torch_mode_status_change;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_HWL_TYPES_H_
