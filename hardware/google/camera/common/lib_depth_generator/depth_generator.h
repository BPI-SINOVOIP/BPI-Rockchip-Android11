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

#ifndef HARDWARE_GOOGLE_CAMERA_LIB_DEPTH_GENERATOR_H_
#define HARDWARE_GOOGLE_CAMERA_LIB_DEPTH_GENERATOR_H_

#include <utils/Errors.h>
#include "depth_types.h"

namespace android {
namespace depth_generator {

// DepthGenerator is the basic interface for any provider that can generate
// depth buffer from several NIR and YUV buffers.
class DepthGenerator {
 public:
  virtual ~DepthGenerator() = default;

  // Enqueue a depth buffer request for asynchronous processing
  virtual status_t EnqueueProcessRequest(const DepthRequestInfo&) = 0;

  // Blocking call to execute the process request right way.
  virtual status_t ExecuteProcessRequest(const DepthRequestInfo&) = 0;

  // Set a callback function to allow the depth generator to asynchronously
  // return the depth buffer.
  virtual void SetResultCallback(DepthResultCallbackFunction) = 0;
};
typedef DepthGenerator* (*CreateDepthGenerator_t)();
}  // namespace depth_generator
}  // namespace android
#endif  // HARDWARE_GOOGLE_CAMERA_LIB_DEPTH_GENERATOR_H_