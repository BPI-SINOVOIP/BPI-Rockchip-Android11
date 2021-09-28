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

#ifndef HARDWARE_GOOGLE_CAMERA_LIB_DEPTH_TYPES_H_
#define HARDWARE_GOOGLE_CAMERA_LIB_DEPTH_TYPES_H_

#include <cutils/native_handle.h>
#include <system/camera_metadata.h>
#include <system/graphics-base-v1.0.h>
#include <functional>
#include <vector>

namespace android {
namespace depth_generator {

enum class DepthResultStatus : uint32_t {
  // Depth generator is able to successfully process the request
  kOk = 0,
  // Depth generator failed to process the request
  kError,
};

struct StreamBuffer {
  // TODO(b/126379504): Add handle to Framework/HAL stream if needed.
  // The client owns the buffer and should guarantee that they are valid during
  // the entire life cycle that this buffer is passed into the depth_generator.
  buffer_handle_t* buffer = nullptr;
};

struct BufferPlane {
  // The virtual address mapped to the UMD of the client process. The client
  // should guarantee that this is valid and not unmapped during the entire life
  // cycle that this buffer is passed into the depth_generator.
  uint8_t* addr = nullptr;
  // In bytes
  uint32_t stride = 0;
  // Number of lines actually allocated
  uint32_t scanline = 0;
};

struct Buffer {
  // Format of the image buffer
  android_pixel_format_t format = HAL_PIXEL_FORMAT_RGBA_8888;
  // Image planes mapped to UMD
  std::vector<BufferPlane> planes;
  // Dimension of this image buffer
  uint32_t width = 0;
  uint32_t height = 0;
  // Information of the framework buffer
  StreamBuffer framework_buffer;
};

struct DepthRequestInfo {
  // Frame number used by the caller to identify this request
  uint32_t frame_number = 0;
  // Input buffers
  // Sequence of buffers from color sensor
  std::vector<Buffer> color_buffer;
  // Sequence of buffers from multiple NIR sensors(e.g. {{d0, f0},{d1, f1}})
  std::vector<std::vector<Buffer>> ir_buffer;
  // Output buffer
  Buffer depth_buffer;
  // Place holder for input metadata(e.g. crop_region). The client should
  // guarantee that the metadata is valid during the entire life cycle that this
  // metadata is passed into the depth_generator.
  const camera_metadata_t* settings = nullptr;
  // input buffer metadata for the color_buffer. This metadata contains info on
  // how the color_buffer is generated(e.g. crop info, FD result, etc.). The
  // caller owns the data and guarantee that the data is valid during the func
  // call. The callee should copy this if it still needs this after the call is
  // returned.
  const camera_metadata_t* color_buffer_metadata = nullptr;
};

// Callback function invoked to notify depth buffer readiness. This method must
// be invoked by a thread different from the thread that enqueues the request to
// avoid deadlock.
using DepthResultCallbackFunction =
    std::function<void(DepthResultStatus result, uint32_t frame_number)>;

}  // namespace depth_generator
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_LIB_DEPTH_TYPES_H_