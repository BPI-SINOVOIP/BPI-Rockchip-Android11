/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_UTILS_ZOOM_RATIO_MAPPER_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_UTILS_ZOOM_RATIO_MAPPER_H_

#include "hal_types.h"
#include "zoom_ratio_mapper_hwl.h"

namespace android {
namespace google_camera_hal {

class ZoomRatioMapper {
 public:
  struct InitParams {
    Dimension active_array_dimension;
    std::unordered_map<uint32_t, Dimension> physical_cam_active_array_dimension;
    ZoomRatioRange zoom_ratio_range;
    std::unique_ptr<ZoomRatioMapperHwl> zoom_ratio_mapper_hwl;
  };

  void Initialize(InitParams* params);

  // Apply zoom ratio to capture request
  void UpdateCaptureRequest(CaptureRequest* request);

  // Apply zoom ratio to capture result
  void UpdateCaptureResult(CaptureResult* result);

 private:
  // Apply zoom ratio to the capture request or result.
  void ApplyZoomRatio(const Dimension& active_array_dimension,
                      const bool is_request, HalCameraMetadata* metadata);

  // Update rect region with respect to zoom ratio and active array
  // dimension.
  void UpdateRects(float zoom_ratio, const uint32_t tag_id,
                   const Dimension& active_array_dimension,
                   const bool is_request, HalCameraMetadata* metadata);

  // Update weighted rect regions with respect to zoom ratio and active array
  // dimension.
  void UpdateWeightedRects(float zoom_ratio, const uint32_t tag_id,
                           const Dimension& active_array_dimension,
                           const bool is_request, HalCameraMetadata* metadata);

  // Update point position with respect to zoom ratio and active array
  // dimension.
  void UpdatePoints(float zoom_ratio, const uint32_t tag_id,
                    const Dimension& active_array_dimension,
                    HalCameraMetadata* metadata);

  // Active array dimension of logical camera.
  Dimension active_array_dimension_;

  // Active array dimension of physical camera.
  std::unordered_map<uint32_t, Dimension> physical_cam_active_array_dimension_;

  // Zoom ratio range.
  ZoomRatioRange zoom_ratio_range_;

  // Indicate whether zoom ratio is supported.
  bool is_zoom_ratio_supported_ = false;

  std::unique_ptr<ZoomRatioMapperHwl> zoom_ratio_mapper_hwl_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_UTILS_ZOOM_RATIO_MAPPER_H_
