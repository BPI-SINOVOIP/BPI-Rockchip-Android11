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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_PROFILER_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_PROFILER_H_

#include <memory>

#include "profiler.h"

namespace android {
namespace hardware {
namespace camera {
namespace implementation {
namespace hidl_profiler {

class HidlProfilerItem {
 public:
  HidlProfilerItem(
      std::shared_ptr<google::camera_common::Profiler> profiler,
      const std::string target, std::function<void()> on_end,
      int request_id = google::camera_common::Profiler::kInvalidRequestId);

  ~HidlProfilerItem();

 private:
  std::shared_ptr<google::camera_common::Profiler> profiler_;
  const std::string target_;
  int request_id_;
  std::function<void()> on_end_;
};

// Start timer for open camera. The timer will stop when the returned
// ProfilerItem is destroyed.
std::unique_ptr<HidlProfilerItem> OnCameraOpen();

// Start timer for flush camera. The timer will stop when the returned
// ProfilerItem is destroyed.
std::unique_ptr<HidlProfilerItem> OnCameraFlush();

// Start timer for close camera. The timer will stop when the returned
// ProfilerItem is destroyed.
std::unique_ptr<HidlProfilerItem> OnCameraClose();

// Start timer for configure streams. The timer will stop when the returned
// ProfilerItem is destroyed.
std::unique_ptr<HidlProfilerItem> OnCameraStreamConfigure();

// Call when first frame is requested.
void OnFirstFrameRequest();

// Call when all bufer in first frame is received.
void OnFirstFrameResult();

}  // namespace hidl_profiler
}  // namespace implementation
}  // namespace camera
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_PROFILER_H_
