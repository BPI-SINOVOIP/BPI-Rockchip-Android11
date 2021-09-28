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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_UTILS_UTILS_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_UTILS_UTILS_H_

#include <log/log.h>
#include <utility>
#include "hal_types.h"

namespace android {
namespace google_camera_hal {
namespace utils {

bool IsPreviewStream(const Stream& stream);
bool IsJPEGSnapshotStream(const Stream& stream);
bool IsVideoStream(const Stream& stream);
bool IsRawStream(const Stream& stream);
bool IsInputRawStream(const Stream& stream);
bool IsArbitraryDataSpaceRawStream(const Stream& stream);
bool IsYUVSnapshotStream(const Stream& stream);
bool IsDepthStream(const Stream& stream);
bool IsOutputZslStream(const Stream& stream);

status_t GetSensorPhysicalSize(const HalCameraMetadata* characteristics,
                               float* width, float* height);

status_t GetSensorActiveArraySize(const HalCameraMetadata* characteristics,
                                  Rect* active_array);

status_t GetSensorPixelArraySize(const HalCameraMetadata* characteristics,
                                 Dimension* pixel_array);

status_t GetFocalLength(const HalCameraMetadata* characteristics,
                        float* focal_length);

status_t GetZoomRatioRange(const HalCameraMetadata* characteristics,
                           ZoomRatioRange* zoom_ratio_range);

// Return if LiveSnapshot is configured
bool IsLiveSnapshotConfigured(const StreamConfiguration& stream_config);

// Return true if max fps is the same at high speed mode
bool IsHighSpeedModeFpsCompatible(StreamConfigurationMode mode,
                                  const HalCameraMetadata* old_session,
                                  const HalCameraMetadata* new_session);

// This method is for IsReconfigurationRequired purpose
// Return true if meet any condition below
// 1. Any session entry count is zero
// 2. All metadata are the same between old and new session.
//    For ANDROID_CONTROL_AE_TARGET_FPS_RANGE, only compare max fps.
bool IsSessionParameterCompatible(const HalCameraMetadata* old_session,
                                  const HalCameraMetadata* new_session);

bool SupportRealtimeThread();
status_t SetRealtimeThread(pthread_t thread);
status_t UpdateThreadSched(pthread_t thread, int32_t policy,
                           struct sched_param* param);

// Map the rectangle to the coordination of HAL.
void ConvertZoomRatio(float zoom_ratio, const Dimension& active_array_dimension,
                      int32_t* left, int32_t* top, int32_t* width,
                      int32_t* height);

// Clamp the coordinate boundary within dimension.
template <typename T>
void ClampBoundary(const Dimension& active_array_dimension, T* x, T* y,
                   T* width = nullptr, T* height = nullptr) {
  if (x == nullptr || y == nullptr) {
    ALOGE("%s, invalid params", __FUNCTION__);
    return;
  }
  *x = std::clamp(*x, static_cast<T>(0),
                  static_cast<T>(active_array_dimension.width - 1));
  *y = std::clamp(*y, static_cast<T>(0),
                  static_cast<T>(active_array_dimension.height - 1));

  if (width) {
    *width = std::clamp(*width, static_cast<T>(1),
                        static_cast<T>(active_array_dimension.width - *x));
  }
  if (height) {
    *height = std::clamp(*height, static_cast<T>(1),
                         static_cast<T>(active_array_dimension.height - *y));
  }
}

// Map the position to the coordinate of framework.
template <typename T>
void RevertZoomRatio(const float zoom_ratio,
                     const Dimension& active_array_dimension,
                     const bool round_to_int, T* x, T* y, T* width = nullptr,
                     T* height = nullptr) {
  if (x == nullptr || y == nullptr) {
    ALOGE("%s, invalid params", __FUNCTION__);
    return;
  }
  float tmp_x = *x * zoom_ratio -
                0.5f * active_array_dimension.width * (zoom_ratio - 1.0f);
  float tmp_y = *y * zoom_ratio -
                0.5f * active_array_dimension.height * (zoom_ratio - 1.0f);

  if (round_to_int) {
    *x = std::round(tmp_x);
    *y = std::round(tmp_y);
  } else {
    *x = tmp_x;
    *y = tmp_y;
  }

  if (width) {
    *width = std::round(*width * zoom_ratio);
  }
  if (height) {
    *height = std::round(*height * zoom_ratio);
  }
  ClampBoundary(active_array_dimension, x, y, width, height);
}

}  // namespace utils
}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_UTILS_UTILS_H_
