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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_ZOOM_RATIO_MAPPER_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_ZOOM_RATIO_MAPPER_H_

#include "hal_camera_metadata.h"
#include "hal_types.h"

namespace android {
namespace google_camera_hal {

class ZoomRatioMapperHwl {
 public:
  virtual ~ZoomRatioMapperHwl() = default;

  // Limit zoom ratio if concurrent mode is on
  virtual void LimitZoomRatioIfConcurrent(float* zoom_ratio) const = 0;

  // Apply zoom ratio to capture request
  virtual void UpdateCaptureRequest(CaptureRequest* request) = 0;

  // Apply zoom ratio to capture result
  virtual void UpdateCaptureResult(CaptureResult* result) = 0;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_ZOOM_RATIO_MAPPER_H_
