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

//#define LOG_NDEBUG 0
#define LOG_TAG "GCH_Utils"

#include "utils.h"

#include <cutils/properties.h>
#include <hardware/gralloc.h>

#include "vendor_tag_defs.h"

namespace android {
namespace google_camera_hal {
namespace utils {

const std::string kRealtimeThreadSetProp = "persist.camera.realtimethread";

bool IsDepthStream(const Stream& stream) {
  if (stream.stream_type == StreamType::kOutput &&
      stream.data_space == HAL_DATASPACE_DEPTH &&
      stream.format == HAL_PIXEL_FORMAT_Y16) {
    return true;
  }

  return false;
}

bool IsPreviewStream(const Stream& stream) {
  if (stream.stream_type == StreamType::kOutput &&
      stream.format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED &&
      ((stream.usage & GRALLOC_USAGE_HW_COMPOSER) == GRALLOC_USAGE_HW_COMPOSER ||
       (stream.usage & GRALLOC_USAGE_HW_TEXTURE) == GRALLOC_USAGE_HW_TEXTURE)) {
    return true;
  }

  return false;
}

bool IsJPEGSnapshotStream(const Stream& stream) {
  if (stream.stream_type == StreamType::kOutput &&
      stream.format == HAL_PIXEL_FORMAT_BLOB &&
      (stream.data_space == HAL_DATASPACE_JFIF ||
       stream.data_space == HAL_DATASPACE_V0_JFIF)) {
    return true;
  }

  return false;
}

bool IsOutputZslStream(const Stream& stream) {
  if (stream.stream_type == StreamType::kOutput &&
      (stream.usage & GRALLOC_USAGE_HW_CAMERA_ZSL) ==
          GRALLOC_USAGE_HW_CAMERA_ZSL) {
    return true;
  }

  return false;
}

bool IsVideoStream(const Stream& stream) {
  if (stream.stream_type == StreamType::kOutput &&
      (stream.usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) != 0) {
    return true;
  }

  return false;
}

bool IsRawStream(const Stream& stream) {
  if (stream.stream_type == StreamType::kOutput &&
      (stream.format == HAL_PIXEL_FORMAT_RAW10 ||
       stream.format == HAL_PIXEL_FORMAT_RAW16 ||
       stream.format == HAL_PIXEL_FORMAT_RAW_OPAQUE)) {
    return true;
  }

  return false;
}

bool IsInputRawStream(const Stream& stream) {
  if (stream.stream_type == StreamType::kInput &&
      (stream.format == HAL_PIXEL_FORMAT_RAW10 ||
       stream.format == HAL_PIXEL_FORMAT_RAW16 ||
       stream.format == HAL_PIXEL_FORMAT_RAW_OPAQUE)) {
    return true;
  }

  return false;
}

bool IsArbitraryDataSpaceRawStream(const Stream& stream) {
  return IsRawStream(stream) && (stream.data_space == HAL_DATASPACE_ARBITRARY);
}

bool IsYUVSnapshotStream(const Stream& stream) {
  if (stream.stream_type == StreamType::kOutput &&
      stream.format == HAL_PIXEL_FORMAT_YCbCr_420_888 &&
      !IsVideoStream(stream) && !IsPreviewStream(stream)) {
    return true;
  }

  return false;
}

status_t GetSensorPhysicalSize(const HalCameraMetadata* characteristics,
                               float* width, float* height) {
  if (characteristics == nullptr || width == nullptr || height == nullptr) {
    ALOGE("%s: characteristics or width/height is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry entry;
  status_t res = characteristics->Get(ANDROID_SENSOR_INFO_PHYSICAL_SIZE, &entry);
  if (res != OK || entry.count != 2) {
    ALOGE(
        "%s: Getting ANDROID_SENSOR_INFO_PHYSICAL_SIZE failed: %s(%d) count: "
        "%zu",
        __FUNCTION__, strerror(-res), res, entry.count);
    return res;
  }

  *width = entry.data.f[0];
  *height = entry.data.f[1];
  return OK;
}

status_t GetSensorActiveArraySize(const HalCameraMetadata* characteristics,
                                  Rect* active_array) {
  if (characteristics == nullptr || active_array == nullptr) {
    ALOGE("%s: characteristics or active_array is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry entry;
  status_t res =
      characteristics->Get(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, &entry);
  if (res != OK || entry.count != 4) {
    ALOGE(
        "%s: Getting ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE failed: %s(%d) "
        "count: %zu",
        __FUNCTION__, strerror(-res), res, entry.count);
    return res;
  }

  active_array->left = entry.data.i32[0];
  active_array->top = entry.data.i32[1];
  active_array->right = entry.data.i32[0] + entry.data.i32[2] - 1;
  active_array->bottom = entry.data.i32[1] + entry.data.i32[3] - 1;

  return OK;
}

status_t GetZoomRatioRange(const HalCameraMetadata* characteristics,
                           ZoomRatioRange* zoom_ratio_range) {
  if (characteristics == nullptr || zoom_ratio_range == nullptr) {
    ALOGE("%s: characteristics or zoom_ratio_range is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry entry;
  status_t res = characteristics->Get(ANDROID_CONTROL_ZOOM_RATIO_RANGE, &entry);
  if (res != OK || entry.count != 2) {
    ALOGE(
        "%s: Getting ANDROID_CONTROL_ZOOM_RATIO_RANGE failed: %s(%d) "
        "count: %zu",
        __FUNCTION__, strerror(-res), res, entry.count);
    return res;
  }

  zoom_ratio_range->min = entry.data.f[0];
  zoom_ratio_range->max = entry.data.f[1];

  return OK;
}

status_t GetSensorPixelArraySize(const HalCameraMetadata* characteristics,
                                 Dimension* pixel_array) {
  if (characteristics == nullptr || pixel_array == nullptr) {
    ALOGE("%s: characteristics or pixel_array is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry entry;
  status_t res =
      characteristics->Get(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE, &entry);
  if (res != OK || entry.count != 2) {
    ALOGE(
        "%s: Getting ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE failed: %s(%d) "
        "count: %zu",
        __FUNCTION__, strerror(-res), res, entry.count);
    return res;
  }

  pixel_array->width = entry.data.i32[0];
  pixel_array->height = entry.data.i32[1];

  return OK;
}

status_t GetFocalLength(const HalCameraMetadata* characteristics,
                        float* focal_length) {
  if (characteristics == nullptr || focal_length == nullptr) {
    ALOGE("%s: characteristics or focal_length is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry entry;
  status_t res =
      characteristics->Get(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, &entry);
  if (res != OK || entry.count != 1) {
    ALOGE(
        "%s: Getting ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS failed: %s(%d) "
        "count: %zu",
        __FUNCTION__, strerror(-res), res, entry.count);
    return res;
  }

  *focal_length = entry.data.f[0];

  return OK;
}

bool IsLiveSnapshotConfigured(const StreamConfiguration& stream_config) {
  bool has_video_stream = false;
  bool has_jpeg_stream = false;
  for (auto stream : stream_config.streams) {
    if (utils::IsVideoStream(stream)) {
      has_video_stream = true;
    } else if (utils::IsJPEGSnapshotStream(stream)) {
      has_jpeg_stream = true;
    }
  }

  return (has_video_stream & has_jpeg_stream);
}

bool IsHighSpeedModeFpsCompatible(StreamConfigurationMode mode,
                                  const HalCameraMetadata* old_session,
                                  const HalCameraMetadata* new_session) {
  if (mode != StreamConfigurationMode::kConstrainedHighSpeed) {
    return false;
  }

  camera_metadata_ro_entry_t ae_target_fps_entry;
  int32_t old_max_fps = 0;
  int32_t new_max_fps = 0;

  if (old_session->Get(ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
                       &ae_target_fps_entry) == OK) {
    old_max_fps = ae_target_fps_entry.data.i32[1];
  }
  if (new_session->Get(ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
                       &ae_target_fps_entry) == OK) {
    new_max_fps = ae_target_fps_entry.data.i32[1];
  }

  ALOGI("%s: HFR: old max fps: %d, new max fps: %d", __FUNCTION__, old_max_fps,
        new_max_fps);

  if (new_max_fps == old_max_fps) {
    return true;
  }

  return false;
}

bool IsSessionParameterCompatible(const HalCameraMetadata* old_session,
                                  const HalCameraMetadata* new_session) {
  auto old_session_count = old_session->GetEntryCount();
  auto new_session_count = new_session->GetEntryCount();
  if (old_session_count == 0 || new_session_count == 0) {
    ALOGI("No session paramerter, old:%zu, new:%zu", old_session_count,
          new_session_count);
    if (new_session_count != 0) {
      camera_metadata_ro_entry_t entry;
      if (new_session->Get(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, &entry) == OK) {
        int32_t max_fps = entry.data.i32[1];
        if (max_fps > 30) {
          ALOGI("new session paramerter max fps:%d", max_fps);
          return false;
        }
      }
    }
    return true;
  }

  if (old_session_count != new_session_count) {
    ALOGI(
        "Entry count has changed from %zu "
        "to %zu",
        old_session_count, new_session_count);
    return false;
  }

  for (size_t entry_index = 0; entry_index < new_session_count; entry_index++) {
    camera_metadata_ro_entry_t new_entry;
    // Get the medata from new session first
    if (new_session->GetByIndex(&new_entry, entry_index) != OK) {
      ALOGW("Unable to get new session entry for index %zu", entry_index);
      return false;
    }

    // Get the same tag from old session
    camera_metadata_ro_entry_t old_entry;
    if (old_session->Get(new_entry.tag, &old_entry) != OK) {
      ALOGW("Unable to get old session tag 0x%x", new_entry.tag);
      return false;
    }

    if (new_entry.count != old_entry.count) {
      ALOGI(
          "New entry count %zu doesn't "
          "match old entry count %zu",
          new_entry.count, old_entry.count);
      return false;
    }

    if (new_entry.tag == ANDROID_CONTROL_AE_TARGET_FPS_RANGE) {
      // Stream reconfiguration is not needed in case the upper
      // framerate range remains unchanged. Any other modification
      // to the session parameters must trigger new stream
      // configuration.
      int32_t old_min_fps = old_entry.data.i32[0];
      int32_t old_max_fps = old_entry.data.i32[1];
      int32_t new_min_fps = new_entry.data.i32[0];
      int32_t new_max_fps = new_entry.data.i32[1];
      if (old_max_fps == new_max_fps) {
        ALOGI("%s: Ignore fps (%d, %d) to (%d, %d)", __FUNCTION__, old_min_fps,
              old_max_fps, new_min_fps, new_max_fps);
        continue;
      }

      return false;
    } else {
      // Same type and count, compare values
      size_t type_size = camera_metadata_type_size[old_entry.type];
      size_t entry_size = type_size * old_entry.count;
      int32_t cmp = memcmp(new_entry.data.u8, old_entry.data.u8, entry_size);
      if (cmp != 0) {
        ALOGI("Session parameter value has changed");
        return false;
      }
    }
  }

  return true;
}

void ConvertZoomRatio(const float zoom_ratio,
                      const Dimension& active_array_dimension, int32_t* left,
                      int32_t* top, int32_t* width, int32_t* height) {
  if (left == nullptr || top == nullptr || width == nullptr ||
      height == nullptr) {
    ALOGE("%s, invalid params", __FUNCTION__);
    return;
  }

  assert(zoom_ratio != 0);
  *left = std::round(*left / zoom_ratio + 0.5f * active_array_dimension.width *
                                              (1.0f - 1.0f / zoom_ratio));
  *top = std::round(*top / zoom_ratio + 0.5f * active_array_dimension.height *
                                            (1.0f - 1.0f / zoom_ratio));
  *width = std::round(*width / zoom_ratio);
  *height = std::round(*height / zoom_ratio);

  if (zoom_ratio >= 1.0f) {
    utils::ClampBoundary(active_array_dimension, left, top, width, height);
  }
}

bool SupportRealtimeThread() {
  static bool support_real_time = false;
  static bool first_time = false;
  if (first_time == false) {
    first_time = true;
    support_real_time = property_get_bool(kRealtimeThreadSetProp.c_str(), false);
  }

  return support_real_time;
}

status_t SetRealtimeThread(pthread_t thread) {
  struct sched_param param = {
      .sched_priority = 1,
  };
  int32_t res =
      pthread_setschedparam(thread, SCHED_FIFO | SCHED_RESET_ON_FORK, &param);
  if (res != 0) {
    ALOGE("%s: Couldn't set SCHED_FIFO", __FUNCTION__);
    return BAD_VALUE;
  }

  return OK;
}

status_t UpdateThreadSched(pthread_t thread, int32_t policy,
                           struct sched_param* param) {
  if (param == nullptr) {
    ALOGE("%s: sched_param is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }
  int32_t res = pthread_setschedparam(thread, policy, param);
  if (res != 0) {
    ALOGE("%s: Couldn't set schedparam", __FUNCTION__);
    return BAD_VALUE;
  }

  return OK;
}

}  // namespace utils
}  // namespace google_camera_hal
}  // namespace android
