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

#define LOG_TAG "StreamConfigurationMap"
#include "StreamConfigurationMap.h"

#include <log/log.h>

namespace android {
void StreamConfigurationMap::AppendAvailableStreamConfigurations(
    const camera_metadata_ro_entry& entry) {
  for (size_t i = 0; i < entry.count; i += kStreamConfigurationSize) {
    int32_t width = entry.data.i32[i + kStreamWidthOffset];
    int32_t height = entry.data.i32[i + kStreamHeightOffset];
    auto format = static_cast<android_pixel_format_t>(
        entry.data.i32[i + kStreamFormatOffset]);
    int32_t isInput = entry.data.i32[i + kStreamIsInputOffset];
    if (!isInput) {
      stream_output_formats_.insert(format);
      stream_output_size_map_[format].insert(std::make_pair(width, height));
    }
  }
}

void StreamConfigurationMap::AppendAvailableStreamMinDurations(
    const camera_metadata_ro_entry_t& entry) {
  for (size_t i = 0; i < entry.count; i += kStreamConfigurationSize) {
    auto format = static_cast<android_pixel_format_t>(
        entry.data.i64[i + kStreamFormatOffset]);
    uint32_t width = entry.data.i64[i + kStreamWidthOffset];
    uint32_t height = entry.data.i64[i + kStreamHeightOffset];
    nsecs_t duration = entry.data.i64[i + kStreamMinDurationOffset];
    auto streamConfiguration =
        std::make_pair(format, std::make_pair(width, height));
    stream_min_duration_map_[streamConfiguration] = duration;
  }
}

void StreamConfigurationMap::AppendAvailableStreamStallDurations(
    const camera_metadata_ro_entry& entry) {
  for (size_t i = 0; i < entry.count; i += kStreamConfigurationSize) {
    auto format = static_cast<android_pixel_format_t>(
        entry.data.i64[i + kStreamFormatOffset]);
    uint32_t width = entry.data.i64[i + kStreamWidthOffset];
    uint32_t height = entry.data.i64[i + kStreamHeightOffset];
    nsecs_t duration = entry.data.i64[i + kStreamStallDurationOffset];
    auto streamConfiguration =
        std::make_pair(format, std::make_pair(width, height));
    stream_stall_map_[streamConfiguration] = duration;
  }
}

StreamConfigurationMap::StreamConfigurationMap(const HalCameraMetadata& chars) {
  camera_metadata_ro_entry_t entry;
  auto ret = chars.Get(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry);
  if (ret != OK) {
    ALOGW("%s: ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS missing!",
          __FUNCTION__);
    entry.count = 0;
  }
  AppendAvailableStreamConfigurations(entry);

  ret = chars.Get(ANDROID_DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS, &entry);
  if (ret == OK) {
    AppendAvailableStreamConfigurations(entry);
  }

  ret = chars.Get(ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS, &entry);
  if (ret != OK) {
    ALOGW("%s: ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS missing!",
          __FUNCTION__);
    entry.count = 0;
  }
  AppendAvailableStreamMinDurations(entry);

  ret = chars.Get(ANDROID_DEPTH_AVAILABLE_DEPTH_MIN_FRAME_DURATIONS, &entry);
  if (ret == OK) {
    AppendAvailableStreamMinDurations(entry);
  }

  ret = chars.Get(ANDROID_SCALER_AVAILABLE_STALL_DURATIONS, &entry);
  if (ret != OK) {
    ALOGW("%s: ANDROID_SCALER_AVAILABLE_STALL_DURATIONS missing!", __FUNCTION__);
    entry.count = 0;
  }
  AppendAvailableStreamStallDurations(entry);

  ret = chars.Get(ANDROID_DEPTH_AVAILABLE_DEPTH_STALL_DURATIONS, &entry);
  if (ret == OK) {
    AppendAvailableStreamStallDurations(entry);
  }

  ret = chars.Get(ANDROID_SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP, &entry);
  if (ret == OK) {
    size_t i = 0;
    while (i < entry.count) {
      auto input_format =
          static_cast<android_pixel_format_t>(entry.data.i32[i++]);
      auto output_format_count = entry.data.i32[i++];
      if (output_format_count <= 0 ||
          ((output_format_count + i) > entry.count)) {
        ALOGE("%s: Invalid output format count: %d!", __func__,
              output_format_count);
        break;
      }
      size_t output_formats_end = output_format_count + i;
      for (; i < output_formats_end; i++) {
        stream_input_output_map_[input_format].insert(
            static_cast<android_pixel_format_t>(entry.data.i32[i]));
      }
      stream_input_formats_.insert(input_format);
    }
  }
}

}  // namespace android
