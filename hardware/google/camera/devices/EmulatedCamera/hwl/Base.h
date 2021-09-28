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

#ifndef HW_EMULATOR_CAMERA_BASE_H
#define HW_EMULATOR_CAMERA_BASE_H

#include <log/log.h>

#include <memory>

#include "HandleImporter.h"
#include "hwl_types.h"

namespace android {

using android::hardware::camera::common::V1_0::helper::HandleImporter;
using google_camera_hal::HwlPipelineCallback;
using google_camera_hal::StreamBuffer;

struct YCbCrPlanes {
  uint8_t* img_y = nullptr;
  uint8_t* img_cb = nullptr;
  uint8_t* img_cr = nullptr;
  uint32_t y_stride = 0;
  uint32_t cbcr_stride = 0;
  uint32_t cbcr_step = 0;
};

struct SinglePlane {
  uint8_t* img = nullptr;
  uint32_t stride = 0;
  uint32_t buffer_size = 0;
};

struct SensorBuffer {
  uint32_t width, height;
  uint32_t frame_number;
  uint32_t pipeline_id;
  uint32_t camera_id;
  android_pixel_format_t format;
  android_dataspace_t dataSpace;
  StreamBuffer stream_buffer;
  HandleImporter importer;
  HwlPipelineCallback callback;
  int acquire_fence_fd;
  bool is_input;
  bool is_failed_request;

  union Plane {
    SinglePlane img;
    YCbCrPlanes img_y_crcb;
  } plane;

  SensorBuffer()
      : width(0),
        height(0),
        frame_number(0),
        pipeline_id(0),
        camera_id(0),
        format(HAL_PIXEL_FORMAT_RGBA_8888),
        dataSpace(HAL_DATASPACE_UNKNOWN),
        acquire_fence_fd(-1),
        is_input(false),
        is_failed_request(false),
        plane{} {
  }

  SensorBuffer(const SensorBuffer&) = delete;
  SensorBuffer& operator=(const SensorBuffer&) = delete;
};

typedef std::vector<std::unique_ptr<SensorBuffer>> Buffers;

}  // namespace android

using android::google_camera_hal::BufferStatus;
using android::google_camera_hal::ErrorCode;
using android::google_camera_hal::HwlPipelineResult;
using android::google_camera_hal::MessageType;
using android::google_camera_hal::NotifyMessage;

template <>
struct std::default_delete<android::SensorBuffer> {
  inline void operator()(android::SensorBuffer* buffer) const {
    if (buffer != nullptr) {
      if (buffer->stream_buffer.buffer != nullptr) {
        buffer->importer.unlock(buffer->stream_buffer.buffer);
        buffer->importer.freeBuffer(buffer->stream_buffer.buffer);
      }

      if (buffer->acquire_fence_fd >= 0) {
        buffer->importer.closeFence(buffer->acquire_fence_fd);
      }

      if ((buffer->stream_buffer.status != BufferStatus::kOk) &&
          (buffer->callback.notify != nullptr) && (!buffer->is_failed_request)) {
        NotifyMessage msg = {
            .type = MessageType::kError,
            .message.error = {.frame_number = buffer->frame_number,
                              .error_stream_id = buffer->stream_buffer.stream_id,
                              .error_code = ErrorCode::kErrorBuffer}};
        buffer->callback.notify(buffer->pipeline_id, msg);
      }

      if (buffer->callback.process_pipeline_result != nullptr) {
        auto result = std::make_unique<HwlPipelineResult>();
        result->camera_id = buffer->camera_id;
        result->pipeline_id = buffer->pipeline_id;
        result->frame_number = buffer->frame_number;
        result->partial_result = 0;

        buffer->stream_buffer.acquire_fence =
            buffer->stream_buffer.release_fence = nullptr;
        if (buffer->is_input) {
          result->input_buffers.push_back(buffer->stream_buffer);
        } else {
          result->output_buffers.push_back(buffer->stream_buffer);
        }
        buffer->callback.process_pipeline_result(std::move(result));
      }
      delete buffer;
    }
  }
};

#endif
