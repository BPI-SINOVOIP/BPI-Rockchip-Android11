/*
 _* Copyright (C) 2013-2019 The Android Open Source Project
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

#define LOG_TAG "HWLUtils"
#include "HWLUtils.h"

#include <log/log.h>

#include <map>

namespace android {

bool HasCapability(const HalCameraMetadata* metadata, uint8_t capability) {
  if (metadata == nullptr) {
    return false;
  }

  camera_metadata_ro_entry_t entry;
  auto ret = metadata->Get(ANDROID_REQUEST_AVAILABLE_CAPABILITIES, &entry);
  if (ret != OK) {
    return false;
  }
  for (size_t i = 0; i < entry.count; i++) {
    if (entry.data.u8[i] == capability) {
      return true;
    }
  }

  return false;
}

status_t GetSensorCharacteristics(const HalCameraMetadata* metadata,
                                  SensorCharacteristics* sensor_chars /*out*/) {
  if ((metadata == nullptr) || (sensor_chars == nullptr)) {
    return BAD_VALUE;
  }

  status_t ret = OK;
  camera_metadata_ro_entry_t entry;
  ret = metadata->Get(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE, &entry);
  if ((ret != OK) || (entry.count != 2)) {
    ALOGE("%s: Invalid ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE!", __FUNCTION__);
    return BAD_VALUE;
  }
  sensor_chars->width = entry.data.i32[0];
  sensor_chars->height = entry.data.i32[1];

  ret = metadata->Get(ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS, &entry);
  if ((ret != OK) || (entry.count != 3)) {
    ALOGE("%s: Invalid ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS!", __FUNCTION__);
    return BAD_VALUE;
  }

  sensor_chars->max_raw_streams = entry.data.i32[0];
  sensor_chars->max_processed_streams = entry.data.i32[1];
  sensor_chars->max_stalling_streams = entry.data.i32[2];

  if (HasCapability(metadata,
                    ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR)) {
    ret = metadata->Get(ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE, &entry);
    if ((ret != OK) ||
        (entry.count != ARRAY_SIZE(sensor_chars->exposure_time_range))) {
      ALOGE("%s: Invalid ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE!",
            __FUNCTION__);
      return BAD_VALUE;
    }
    memcpy(sensor_chars->exposure_time_range, entry.data.i64,
           sizeof(sensor_chars->exposure_time_range));

    ret = metadata->Get(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION, &entry);
    if ((ret != OK) || (entry.count != 1)) {
      ALOGE("%s: Invalid ANDROID_SENSOR_INFO_MAX_FRAME_DURATION!", __FUNCTION__);
      return BAD_VALUE;
    }
    sensor_chars->frame_duration_range[1] = entry.data.i64[0];
    sensor_chars->frame_duration_range[0] =
        EmulatedSensor::kSupportedFrameDurationRange[0];

    ret = metadata->Get(ANDROID_SENSOR_INFO_SENSITIVITY_RANGE, &entry);
    if ((ret != OK) ||
        (entry.count != ARRAY_SIZE(sensor_chars->sensitivity_range))) {
      ALOGE("%s: Invalid ANDROID_SENSOR_INFO_SENSITIVITY_RANGE!", __FUNCTION__);
      return BAD_VALUE;
    }
    memcpy(sensor_chars->sensitivity_range, entry.data.i64,
           sizeof(sensor_chars->sensitivity_range));
  } else {
    memcpy(sensor_chars->exposure_time_range,
           EmulatedSensor::kSupportedExposureTimeRange,
           sizeof(sensor_chars->exposure_time_range));
    memcpy(sensor_chars->frame_duration_range,
           EmulatedSensor::kSupportedFrameDurationRange,
           sizeof(sensor_chars->frame_duration_range));
    memcpy(sensor_chars->sensitivity_range,
           EmulatedSensor::kSupportedSensitivityRange,
           sizeof(sensor_chars->sensitivity_range));
  }

  if (HasCapability(metadata, ANDROID_REQUEST_AVAILABLE_CAPABILITIES_RAW)) {
    ret = metadata->Get(ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT, &entry);
    if ((ret != OK) || (entry.count != 1)) {
      ALOGE("%s: Invalid ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT!",
            __FUNCTION__);
      return BAD_VALUE;
    }

    sensor_chars->color_arangement = static_cast<
        camera_metadata_enum_android_sensor_info_color_filter_arrangement>(
        entry.data.u8[0]);

    ret = metadata->Get(ANDROID_SENSOR_INFO_WHITE_LEVEL, &entry);
    if ((ret != OK) || (entry.count != 1)) {
      ALOGE("%s: Invalid ANDROID_SENSOR_INFO_WHITE_LEVEL!", __FUNCTION__);
      return BAD_VALUE;
    }
    sensor_chars->max_raw_value = entry.data.i32[0];

    ret = metadata->Get(ANDROID_SENSOR_BLACK_LEVEL_PATTERN, &entry);
    if ((ret != OK) ||
        (entry.count != ARRAY_SIZE(sensor_chars->black_level_pattern))) {
      ALOGE("%s: Invalid ANDROID_SENSOR_BLACK_LEVEL_PATTERN!", __FUNCTION__);
      return BAD_VALUE;
    }

    memcpy(sensor_chars->black_level_pattern, entry.data.i32,
           sizeof(sensor_chars->black_level_pattern));

    ret = metadata->Get(ANDROID_LENS_INFO_SHADING_MAP_SIZE, &entry);
    if ((ret == OK) && (entry.count == 2)) {
      sensor_chars->lens_shading_map_size[0] = entry.data.i32[0];
      sensor_chars->lens_shading_map_size[1] = entry.data.i32[1];
    } else {
      ALOGE("%s: No available shading map size!", __FUNCTION__);
      return BAD_VALUE;
    }

    ret = metadata->Get(ANDROID_SENSOR_COLOR_TRANSFORM1, &entry);
    if ((ret != OK) || (entry.count != (3 * 3))) {  // 3x3 rational matrix
      ALOGE("%s: Invalid ANDROID_SENSOR_COLOR_TRANSFORM1!", __FUNCTION__);
      return BAD_VALUE;
    }

    sensor_chars->color_filter.rX = RAT_TO_FLOAT(entry.data.r[0]);
    sensor_chars->color_filter.rY = RAT_TO_FLOAT(entry.data.r[1]);
    sensor_chars->color_filter.rZ = RAT_TO_FLOAT(entry.data.r[2]);
    sensor_chars->color_filter.grX = RAT_TO_FLOAT(entry.data.r[3]);
    sensor_chars->color_filter.grY = RAT_TO_FLOAT(entry.data.r[4]);
    sensor_chars->color_filter.grZ = RAT_TO_FLOAT(entry.data.r[5]);
    sensor_chars->color_filter.gbX = RAT_TO_FLOAT(entry.data.r[3]);
    sensor_chars->color_filter.gbY = RAT_TO_FLOAT(entry.data.r[4]);
    sensor_chars->color_filter.gbZ = RAT_TO_FLOAT(entry.data.r[5]);
    sensor_chars->color_filter.bX = RAT_TO_FLOAT(entry.data.r[6]);
    sensor_chars->color_filter.bY = RAT_TO_FLOAT(entry.data.r[7]);
    sensor_chars->color_filter.bZ = RAT_TO_FLOAT(entry.data.r[8]);
  } else {
    sensor_chars->color_arangement = static_cast<
        camera_metadata_enum_android_sensor_info_color_filter_arrangement>(
        EmulatedSensor::kSupportedColorFilterArrangement);
    sensor_chars->max_raw_value = EmulatedSensor::kDefaultMaxRawValue;
    memcpy(sensor_chars->black_level_pattern,
           EmulatedSensor::kDefaultBlackLevelPattern,
           sizeof(sensor_chars->black_level_pattern));
  }

  if (HasCapability(
          metadata,
          ANDROID_REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING) ||
      HasCapability(metadata,
                    ANDROID_REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING)) {
    ret = metadata->Get(ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS, &entry);
    if ((ret != OK) || (entry.count != 1)) {
      ALOGE("%s: Invalid ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS!", __FUNCTION__);
      return BAD_VALUE;
    }

    sensor_chars->max_input_streams = entry.data.i32[0];
  }

  ret = metadata->Get(ANDROID_REQUEST_PIPELINE_MAX_DEPTH, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    if (entry.data.u8[0] == 0) {
      ALOGE("%s: Maximum request pipeline must have a non zero value!",
            __FUNCTION__);
      return BAD_VALUE;
    }
    sensor_chars->max_pipeline_depth = entry.data.u8[0];
  } else {
    ALOGE("%s: Maximum request pipeline depth absent!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = metadata->Get(ANDROID_SENSOR_ORIENTATION, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    sensor_chars->orientation = entry.data.i32[0];
  } else {
    ALOGE("%s: Sensor orientation absent!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = metadata->Get(ANDROID_LENS_FACING, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    sensor_chars->is_front_facing = false;
    if (ANDROID_LENS_FACING_FRONT == entry.data.u8[0]) {
      sensor_chars->is_front_facing = true;
    }
  } else {
    ALOGE("%s: Lens facing absent!", __FUNCTION__);
    return BAD_VALUE;
  }
  return ret;
}

PhysicalDeviceMapPtr ClonePhysicalDeviceMap(const PhysicalDeviceMapPtr& src) {
  auto ret = std::make_unique<PhysicalDeviceMap>();
  for (const auto& it : *src) {
    ret->emplace(it.first, std::make_pair(it.second.first,
        HalCameraMetadata::Clone(it.second.second.get())));
  }
  return ret;
}

}  // namespace android
