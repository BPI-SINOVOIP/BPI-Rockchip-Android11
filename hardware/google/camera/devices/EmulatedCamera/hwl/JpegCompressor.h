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

#ifndef HW_EMULATOR_CAMERA_JPEG_H
#define HW_EMULATOR_CAMERA_JPEG_H

#include <hwl_types.h>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "Base.h"
#include "HandleImporter.h"

extern "C" {
#include <jpeglib.h>
}

#include "utils/ExifUtils.h"

namespace android {

using android::hardware::camera::common::V1_0::helper::HandleImporter;
using google_camera_hal::BufferStatus;
using google_camera_hal::HwlPipelineCallback;
using google_camera_hal::HwlPipelineResult;

struct JpegYUV420Input {
  uint32_t width, height;
  bool buffer_owner;
  YCbCrPlanes yuv_planes;

  JpegYUV420Input() : width(0), height(0), buffer_owner(false) {
  }
  ~JpegYUV420Input() {
    if ((yuv_planes.img_y != nullptr) && buffer_owner) {
      delete[] yuv_planes.img_y;
      yuv_planes = {};
    }
  }

  JpegYUV420Input(const JpegYUV420Input&) = delete;
  JpegYUV420Input& operator=(const JpegYUV420Input&) = delete;
};

struct JpegYUV420Job {
  std::unique_ptr<JpegYUV420Input> input;
  std::unique_ptr<SensorBuffer> output;
  std::unique_ptr<HalCameraMetadata> result_metadata;
  std::unique_ptr<ExifUtils> exif_utils;
};

class JpegCompressor {
 public:
  JpegCompressor();
  virtual ~JpegCompressor();

  status_t QueueYUV420(std::unique_ptr<JpegYUV420Job> job);

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  std::atomic_bool jpeg_done_ = false;
  std::thread jpeg_processing_thread_;
  std::queue<std::unique_ptr<JpegYUV420Job>> pending_yuv_jobs_;
  std::string exif_make_, exif_model_;

  j_common_ptr jpeg_error_info_;
  bool CheckError(const char* msg);
  void CompressYUV420(std::unique_ptr<JpegYUV420Job> job);
  struct YUV420Frame {
    uint8_t* output_buffer;
    size_t output_buffer_size;
    YCbCrPlanes yuv_planes;
    size_t width;
    size_t height;
    const uint8_t* app1_buffer;
    size_t app1_buffer_size;
  };
  size_t CompressYUV420Frame(YUV420Frame frame);
  void ThreadLoop();

  JpegCompressor(const JpegCompressor&) = delete;
  JpegCompressor& operator=(const JpegCompressor&) = delete;
};

}  // namespace android

template <>
struct std::default_delete<jpeg_compress_struct> {
  inline void operator()(jpeg_compress_struct* cinfo) const {
    if (cinfo != nullptr) {
      jpeg_destroy_compress(cinfo);
      delete cinfo;
    }
  }
};

#endif
