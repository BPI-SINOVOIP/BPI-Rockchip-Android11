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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_PIPELINE_REQUEST_ID_MANAGER_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_PIPELINE_REQUEST_ID_MANAGER_H_

#include <utils/Errors.h>
#include <utility>

#include "hal_types.h"

namespace android {
namespace google_camera_hal {

// PipelineRequestIdManager manage mapping from frame number to request id for
// each pipeline.
class PipelineRequestIdManager {
 public:
  // Creates PipelineRequestIdManager
  static std::unique_ptr<PipelineRequestIdManager> Create(
      size_t max_pending_request = kDefaultMaxPendingRequest);

  // Set mapping between from frame number to request id.
  status_t SetPipelineRequestId(uint32_t request_id, uint32_t frame_number,
                                uint32_t pipeline_id);

  // Get request id from pipeline id and frame number.
  status_t GetPipelineRequestId(uint32_t pipeline_id, uint32_t frame_number,
                                uint32_t* request_id);

 protected:
  PipelineRequestIdManager(size_t max_pending_request);

 private:
  // Define a request id pack that bind request_id and frame_number.
  struct RequestIdInfo {
    // The request id set by client.
    uint32_t request_id = 0;
    // Frame number used to detect overflow of ring buffer.
    uint32_t frame_number = 0;
  };

  // Default max pending request if max_pending_request isn't provided while
  // creating class. 32 should cover all the case.
  static const size_t kDefaultMaxPendingRequest = 32;

  // Max pending request support in pipeline_request_ids_.
  const size_t kMaxPendingRequest = 0;

  std::mutex pipeline_request_ids_mutex_;

  // Map from a HWL pipeline ID to a RequestIdInfo vector.
  // Must be protected by pipeline_request_ids_mutex_.
  std::unordered_map<uint32_t, std::vector<RequestIdInfo>> pipeline_request_ids_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_PIPELINE_REQUEST_ID_MANAGER_H_
