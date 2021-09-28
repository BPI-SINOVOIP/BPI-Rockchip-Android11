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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_MULTICAM_COORDINATOR_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_MULTICAM_COORDINATOR_H_

#include <utils/Errors.h>
#include <bitset>
#include <memory>
#include <vector>
#include "hal_camera_metadata.h"

namespace android {
namespace google_camera_hal {

struct PhysicalRequestPrepareParams {
  uint32_t frame_number;
  std::vector<HalCameraMetadata*> metadata;  // list of physical metadata
};

struct LogicalProcessingInputParams {
  uint32_t frame_number;
  std::vector<HalCameraMetadata*> metadata;
};

struct LogicalResultParams {
  uint32_t frame_number;
};

struct CoordinatorResult {
  uint32_t frame_num;
};

// This is that a multicamera coordinator needs to support to
// allow implementing a logical camera using physical pipelines.
class IMulticamCoordinator {
 public:
  // Prepares the physical camera requests' metadata based on
  // speicifc implementation (usecase).
  virtual status_t PreparePhysicalRequest(
      const PhysicalRequestPrepareParams& params) = 0;

  // Prepare logical (offline) processing by taking the results
  // of the physical pipelines and producing the input for the
  // logical pipeline.
  virtual status_t PrepareLogicalProcessing(
      const LogicalProcessingInputParams& params,
      std::unique_ptr<HalCameraMetadata>* logical_metadata) = 0;

  // Processes the results of the physical pipelines
  // and also does the required metadata translations if needed.
  virtual status_t ProcessPhysicalResult(HalCameraMetadata* result_metadata) = 0;

  // Updates the state of the coordinator based on the results of the logical
  // pipeline. It also does any necessary translation on the result metadata.
  virtual status_t ProcessLogicalResult(HalCameraMetadata* result_metadata,
                                        const LogicalResultParams& params) = 0;

  virtual status_t GetResult(uint32_t frame_num, CoordinatorResult* result) = 0;

  // Prepare the framework request for the coordinator to make transition decision.
  virtual status_t PrepareRequest(uint32_t frame_num,
                                  HalCameraMetadata* request_metadata) = 0;

  virtual ~IMulticamCoordinator() {
  }
};
}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_MULTICAM_COORDINATOR_H_
