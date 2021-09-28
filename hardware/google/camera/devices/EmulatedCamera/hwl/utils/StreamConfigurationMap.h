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

#ifndef EMULATOR_STREAM_CONFIGURATION_MAP_H_
#define EMULATOR_STREAM_CONFIGURATION_MAP_H_

#include <set>
#include <unordered_map>

#include "hwl_types.h"
#include "system/camera_metadata.h"
#include "utils/Timers.h"

namespace android {

using google_camera_hal::HalCameraMetadata;

typedef std::pair<uint32_t, uint32_t> StreamSize;
typedef std::pair<android_pixel_format_t, StreamSize> StreamConfig;

inline bool operator==(const StreamConfig& lhs, const StreamConfig& rhs) {
  return (std::get<0>(lhs) == std::get<0>(rhs)) &&
         (std::get<1>(lhs).first == std::get<1>(rhs).first) &&
         (std::get<1>(lhs).second == std::get<1>(rhs).second);
}

struct StreamConfigurationHash {
  inline std::size_t operator()(const StreamConfig& entry) const {
    size_t result = 1;
    size_t hashValue = 31;
    result = hashValue * result +
             std::hash<android_pixel_format_t>{}(std::get<0>(entry));
    result =
        hashValue * result + std::hash<uint32_t>{}(std::get<1>(entry).first);
    result =
        hashValue * result + std::hash<uint32_t>{}(std::get<1>(entry).second);
    return result;
  }
};

class StreamConfigurationMap {
 public:
  StreamConfigurationMap(const HalCameraMetadata& chars);

  const std::set<android_pixel_format_t>& GetOutputFormats() const {
    return stream_output_formats_;
  }

  const std::set<StreamSize>& GetOutputSizes(android_pixel_format_t format) {
    return stream_output_size_map_[format];
  }

  nsecs_t GetOutputMinFrameDuration(StreamConfig configuration) const {
    auto ret = stream_min_duration_map_.find(configuration);
    return (ret == stream_min_duration_map_.end()) ? 0 : ret->second;
  }

  nsecs_t GetOutputStallDuration(StreamConfig configuration) const {
    auto ret = stream_stall_map_.find(configuration);
    return (ret == stream_stall_map_.end()) ? 0 : ret->second;
  }

  bool SupportsReprocessing() const {
    return !stream_input_output_map_.empty();
  }

  const std::set<android_pixel_format_t>& GetValidOutputFormatsForInput(
      android_pixel_format_t format) {
    return stream_input_output_map_[format];
  }

  const std::set<android_pixel_format_t>& GetInputFormats() {
    return stream_input_formats_;
  }

 private:
  void AppendAvailableStreamConfigurations(const camera_metadata_ro_entry& entry);
  void AppendAvailableStreamMinDurations(const camera_metadata_ro_entry_t& entry);
  void AppendAvailableStreamStallDurations(const camera_metadata_ro_entry& entry);

  const size_t kStreamFormatOffset = 0;
  const size_t kStreamWidthOffset = 1;
  const size_t kStreamHeightOffset = 2;
  const size_t kStreamIsInputOffset = 3;
  const size_t kStreamMinDurationOffset = 3;
  const size_t kStreamStallDurationOffset = 3;
  const size_t kStreamConfigurationSize = 4;

  std::set<android_pixel_format_t> stream_output_formats_;
  std::unordered_map<android_pixel_format_t, std::set<StreamSize>>
      stream_output_size_map_;
  std::unordered_map<StreamConfig, nsecs_t, StreamConfigurationHash>
      stream_stall_map_;
  std::unordered_map<StreamConfig, nsecs_t, StreamConfigurationHash>
      stream_min_duration_map_;
  std::set<android_pixel_format_t> stream_input_formats_;
  std::unordered_map<android_pixel_format_t, std::set<android_pixel_format_t>>
      stream_input_output_map_;
};

}  // namespace android

#endif  // EMULATOR_STREAM_CONFIGURATION_MAP_H_
