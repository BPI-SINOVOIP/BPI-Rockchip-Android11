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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_REQUEST_PROCESSOR_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_REQUEST_PROCESSOR_H_

#include <utils/Errors.h>

#include "hal_types.h"
#include "internal_stream_manager.h"
#include "process_block.h"

namespace android {
namespace google_camera_hal {

// RequestProcessor defines the interface of a request processor. A request
// processor may modify the requests before sending the requests to its
// ProcessBlock. For example, if the original request contains a depth output
// stream, the request processor may request two output streams from dual
// cameras (one from each camera) in order to generate the depth stream in
// a downstream ProcessBlock.
class RequestProcessor {
 public:
  virtual ~RequestProcessor() = default;

  // Configure streams that will be supported by this RequestProcessor.
  //
  // internal_stream_manager is owned by the client and can be used by
  // RequestProcessor to register new internal stream and get buffers for those
  // internal streams.
  // stream_config is the desired stream configuration by the client.
  // process_block_stream_config will be filled with streams that are supported
  // by this RequestProcessor and should be used to configure the ProcessBlock
  // it's going to be connected to.
  // process_block_stream_config may contain additional streams
  // that are not present in stream_config. Those additional streams are
  // internal streams that may be produced by this RequestProcessor via
  // ProcessRequest().
  virtual status_t ConfigureStreams(
      InternalStreamManager* internal_stream_manager,
      const StreamConfiguration& stream_config,
      StreamConfiguration* process_block_stream_config) = 0;

  // Set the process block to send requests to. This must be called exactly
  // once before calling ProcessRequest. SetProcessBlock will return
  // ALREADY_EXISTS if it's called more than once.
  virtual status_t SetProcessBlock(
      std::unique_ptr<ProcessBlock> process_block) = 0;

  // Process a capture request. The request processor will generate requests
  // for the process block based on the original request.
  virtual status_t ProcessRequest(const CaptureRequest& request) = 0;

  // Flush all pending requests.
  virtual status_t Flush() = 0;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_REQUEST_PROCESSOR_H_