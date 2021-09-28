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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_VENDOR_TAG_TYPES_H
#define HARDWARE_GOOGLE_CAMERA_HAL_VENDOR_TAG_TYPES_H

#include "hal_types.h"

namespace android {
namespace google_camera_hal {

// Payload for com.google.internal.OutputIntent to indicate output intent of
// the request. HWL can remap these to its usecases.
enum class OutputIntent : uint8_t {
  kPreview,        // For requesting preview output.
  kSnapshot,       // For requesting jpeg output.
  kVideo,          // For requesting video output, i.e. only in recording.
  kZsl,            // For requesting zsl output.
  kVideoSnapshot,  // For requesting jpeg output during recording.
};

// Payload for com.google.internal.ProcessingMode to indicate processing mode
// of the request
enum class ProcessingMode : uint8_t {
  // The request's output is indended to be the final output that will be
  // delivered to the camera framework
  kFinalProcessing,
  // The request's output is indended to be an intermediate output, and further
  // requests will be made to deliver the result to the camera framework
  kIntermediateProcessing
};

// Payload for com.google.internal.hdr.UsageMode to indicate usage mode
// of project
enum class HdrMode : uint8_t {
  // Mode of default hdrplus engine flow
  kHdrplusMode,
  // Mode of non-hdrplus flow
  kNonHdrplusMode,
  // Mode of non-hdrplus + Hdrnet flow
  kHdrnetMode
};

// Byte-pack any structures that are used as the payload of the vendor tags
#pragma pack(push, 1)
// Placeholder to define any structures used as payload for HAL vendor tags
#pragma pack(pop)

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_VENDOR_TAG_TYPES_H
