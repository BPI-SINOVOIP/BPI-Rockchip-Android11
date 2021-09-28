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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_MULTICAM_DUALFOV_ROI_TRANSLATOR_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_MULTICAM_DUALFOV_ROI_TRANSLATOR_H_

#include <utils/Errors.h>
#include <memory>
#include "hal_types.h"
#include "multicam_coordinator.h"

namespace android {
namespace google_camera_hal {

struct CameraModel {
  Dimension sensor_dimension;
  Rect active_array_size;
  float focal_length;
  float pixel_size;
  float fov;
};

class IFovRoiTranslator {
 public:
  virtual void TranslateRect(const Rect& src_rect, Rect* dst_rect,
                             uint32_t translation_type) = 0;
  virtual void TranslatePoint(const Point& src_point, Point* dst_point,
                              uint32_t translation_type) = 0;
  virtual ~IFovRoiTranslator() {
  }
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_MULTICAM_DUALFOV_ROI_TRANSLATOR_H_
