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

#define LOG_TAG "TestUtils"
#include <log/log.h>

#include <gtest/gtest.h>
#include <hardware/gralloc.h>

#include "test_utils.h"

namespace android {
namespace google_camera_hal {
namespace test_utils {

void GetDummyPreviewStream(Stream* stream, uint32_t width, uint32_t height,
                           bool is_physical_camera_stream = false,
                           uint32_t physical_camera_id = 0,
                           uint32_t stream_id = 0) {
  ASSERT_NE(stream, nullptr);

  *stream = {};
  stream->id = stream_id;
  stream->stream_type = StreamType::kOutput;
  stream->width = width;
  stream->height = height;
  stream->format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
  stream->usage = GRALLOC_USAGE_HW_TEXTURE;
  stream->data_space = HAL_DATASPACE_ARBITRARY;
  stream->rotation = StreamRotation::kRotation0;
  stream->is_physical_camera_stream = is_physical_camera_stream;
  stream->physical_camera_id = physical_camera_id;
}

void GetPreviewOnlyStreamConfiguration(StreamConfiguration* config,
                                       uint32_t width, uint32_t height) {
  ASSERT_NE(config, nullptr);

  Stream preview_stream = {};
  GetDummyPreviewStream(&preview_stream, width, height);

  *config = {};
  config->streams.push_back(preview_stream);
  config->operation_mode = StreamConfigurationMode::kNormal;
}

void GetPhysicalPreviewStreamConfiguration(
    StreamConfiguration* config,
    const std::vector<uint32_t>& physical_camera_ids, uint32_t width,
    uint32_t height) {
  ASSERT_NE(config, nullptr);

  *config = {};
  config->operation_mode = StreamConfigurationMode::kNormal;

  int32_t stream_id = 0;
  for (auto& camera_id : physical_camera_ids) {
    Stream preview_stream;
    GetDummyPreviewStream(&preview_stream, width, height,
                          /*is_physical_camera_stream=*/true, camera_id,
                          stream_id++);
    config->streams.push_back(preview_stream);
  }
}

bool IsLogicalCamera(CameraDeviceSessionHwl* session_hwl) {
  return session_hwl->GetPhysicalCameraIds().size() > 1;
}

}  // namespace test_utils
}  // namespace google_camera_hal
}  // namespace android
