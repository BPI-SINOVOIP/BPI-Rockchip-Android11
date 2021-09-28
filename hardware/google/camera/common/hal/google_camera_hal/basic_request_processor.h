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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_BASIC_REQUEST_PROCESSOR_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_BASIC_REQUEST_PROCESSOR_H_

#include <shared_mutex>

#include "process_block.h"
#include "request_processor.h"

namespace android {
namespace google_camera_hal {

// BasicRequestProcessor implements a basic RequestProcessor that simply
// forwards the request to its ProcessBlock without any modification.
class BasicRequestProcessor : public RequestProcessor {
 public:
  // device_session_hwl is owned by the caller and must be valid during the
  // lifetime of this BasicRequestProcessor.
  static std::unique_ptr<BasicRequestProcessor> Create(
      CameraDeviceSessionHwl* device_session_hwl);

  virtual ~BasicRequestProcessor() = default;

  // Override functions of RequestProcessor start.
  // BasicRequestProcessor will configure all streams in stream_config.
  status_t ConfigureStreams(
      InternalStreamManager* internal_stream_manager,
      const StreamConfiguration& stream_config,
      StreamConfiguration* process_block_stream_config) override;

  status_t SetProcessBlock(std::unique_ptr<ProcessBlock> process_block) override;

  status_t ProcessRequest(const CaptureRequest& request) override;

  status_t Flush() override;
  // Override functions of RequestProcessor end.

 protected:
  BasicRequestProcessor() = default;

 private:
  std::shared_mutex process_block_shared_lock_;

  // Protected by process_block_shared_lock_.
  std::unique_ptr<ProcessBlock> process_block_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_BASIC_REQUEST_PROCESSOR_H_
