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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_CAMERA_DEVICE_SESSION_HWL_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_CAMERA_DEVICE_SESSION_HWL_H_

#include <utils/Errors.h>

#include "hal_camera_metadata.h"
#include "hwl_types.h"
#include "multicam_coordinator_hwl.h"
#include "session_data_defs.h"
#include "zoom_ratio_mapper_hwl.h"

namespace android {
namespace google_camera_hal {

// CameraDeviceSessionHwl provides methods to return default settings,
// create pipelines, submit capture requests, and flush the session.
class CameraDeviceSessionHwl {
 public:
  virtual ~CameraDeviceSessionHwl() = default;

  // Construct the default request settings for a request template type.
  virtual status_t ConstructDefaultRequestSettings(
      RequestTemplate type,
      std::unique_ptr<HalCameraMetadata>* default_settings) = 0;

  virtual status_t PrepareConfigureStreams(
      const StreamConfiguration& request_config) = 0;

  // To create pipelines for a capture session, the client will call
  // ConfigurePipeline() to configure each pipeline, and call BuildPipelines()
  // to build all successfully configured pipelines. If a ConfigurePipeline()
  // returns an error, BuildPipelines() will not build that failed pipeline
  // configuration. If ConfigurePipeline() is called when previously built
  // pipelines have not been destroyed, it will return ALREADY_EXISTS. If
  // DestroyPipelines() is call after ConfigurePipeline(), it will reset
  // and discard the configured pipelines.
  //
  // camera ID specifies which camera this pipeline captures requests from. It
  // will be one of the camera IDs returned from GetCameraId() and
  // GetPhysicalCameraIds().
  // hwl_pipeline_callback contains callback functions to notify results and
  // messages.
  // request_config is the requested stream configuration for one pipeline.
  // overall_config is the complete requested stream configuration from
  // frameworks.
  // pipeline_id is an unique pipeline ID filled by this method. It can be used
  // to submit requests to a specific pipeline in SubmitRequests().
  virtual status_t ConfigurePipeline(uint32_t camera_id,
                                     HwlPipelineCallback hwl_pipeline_callback,
                                     const StreamConfiguration& request_config,
                                     const StreamConfiguration& overall_config,
                                     uint32_t* pipeline_id) = 0;

  // Build the successfully configured pipelines from ConfigurePipeline(). If
  // there is no successfully configured pipeline, this method will return
  // NO_INIT.
  virtual status_t BuildPipelines() = 0;

  // Warm up pipeline to ready for taking request, this can be a NoOp for
  // implementation which doesn't support to put pipeline in standby mode
  // This call is optional for capture session before sending request. only
  // needed when capture session want to confirm if the pipeline is ready before
  // sending request, otherwise HWL session should implicitly get back to ready
  // state after receiving a request.Multiple pipelines in the same session can
  // be prepared in parallel by calling this function.
  // pipeline_id is the id returned from ConfigurePipeline
  // frame_number is the request frame number when call this interface
  virtual status_t PreparePipeline(uint32_t pipeline_id,
                                   uint32_t frame_number) = 0;

  // Fills required input streams for a certain offline pipeline. Returns an
  // error if the pipeline being queried is not an offline pipeline.
  // overall_config is the requested configuration from framework.
  // pipeline_role is the role of the offline pipeline to query for.
  // streams is a vector of required input streams to return.
  virtual status_t GetRequiredIntputStreams(
      const StreamConfiguration& overall_config,
      HwlOfflinePipelineRole pipeline_role, std::vector<Stream>* streams) = 0;

  // Get the configured HAL streams for a pipeline. If no pipeline was built,
  // this method will return NO_INIT. If pipeline_id was not built, this method
  // will return NAME_NOT_FOUND.
  virtual status_t GetConfiguredHalStream(
      uint32_t pipeline_id, std::vector<HalStream>* hal_streams) const = 0;

  // Destroy built pipelines or discard configured pipelines.
  virtual void DestroyPipelines() = 0;

  // frame_number is the frame number of the requests.
  // requests contain requests from all different pipelines. If requests contain
  // more than one request from a certain pipeline, this method will return an
  // error. All requests captured from camera sensors must be captured
  // synchronously.
  virtual status_t SubmitRequests(
      uint32_t frame_number,
      const std::vector<HwlPipelineRequest>& requests) = 0;

  // Flush all pending requests.
  virtual status_t Flush() = 0;

  // Return the camera ID that this camera device session is associated with.
  virtual uint32_t GetCameraId() const = 0;

  // Return the physical camera ID that this camera device session is associated
  // with. If the camera device does not have multiple physical camera devices,
  // this method should return an empty std::vector.
  virtual std::vector<uint32_t> GetPhysicalCameraIds() const = 0;

  // Return the characteristics that this camera device session is associated with.
  virtual status_t GetCameraCharacteristics(
      std::unique_ptr<HalCameraMetadata>* characteristics) const = 0;

  // Return the characteristics of a physical camera belonging to this device session.
  virtual status_t GetPhysicalCameraCharacteristics(
      uint32_t physical_camera_id,
      std::unique_ptr<HalCameraMetadata>* characteristics) const = 0;

  // See common/session_data_def.h for more info on Session Data API
  // Set a key/value pair for this session
  virtual status_t SetSessionData(SessionDataKey key, void* value) = 0;

  // Get the value corresponding to the give key in the session
  virtual status_t GetSessionData(SessionDataKey key, void** value) const = 0;

  // Set the session callback.
  virtual void SetSessionCallback(
      const HwlSessionCallback& hwl_session_callback) = 0;

  // Filter out the result metadata to remove any private metadata that is meant
  // to be internal to the HWL and should not be delivered to the upper layer.
  // Unless the request specified intermediate processing via
  // VendorTagIds::kProcessingMode, the HWL impelmentation should by default
  // remove any private data from the result metadata.
  virtual status_t FilterResultMetadata(HalCameraMetadata* metadata) const = 0;

  // Return the corresponding HWL coordinator utility interface
  virtual std::unique_ptr<IMulticamCoordinatorHwl>
  CreateMulticamCoordinatorHwl() = 0;

  // Check reconfiguration is required or not
  // old_session is old session parameter
  // new_session is new session parameter
  // If reconfiguration is required, set reconfiguration_required to true
  // If reconfiguration is not required, set reconfiguration_required to false
  virtual status_t IsReconfigurationRequired(
      const HalCameraMetadata* old_session, const HalCameraMetadata* new_session,
      bool* reconfiguration_required) const = 0;

  // Get zoom ratio mapper from HWL.
  virtual std::unique_ptr<ZoomRatioMapperHwl> GetZoomRatioMapperHwl() = 0;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_CAMERA_DEVICE_SESSION_HWL_H_
