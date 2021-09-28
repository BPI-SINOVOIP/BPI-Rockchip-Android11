/*
 * Copyright (C) 2020 The Android Open Source Project
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

#define LOG_TAG "GCH_ZoomRatioMapper"

#include <log/log.h>
#include <cmath>

#include "utils.h"
#include "zoom_ratio_mapper.h"

namespace android {
namespace google_camera_hal {

int32_t kWeightedRectToConvert[] = {ANDROID_CONTROL_AE_REGIONS,
                                    ANDROID_CONTROL_AF_REGIONS,
                                    ANDROID_CONTROL_AWB_REGIONS};

int32_t kRectToConvert[] = {ANDROID_SCALER_CROP_REGION};

int32_t kResultPointsToConvert[] = {ANDROID_STATISTICS_FACE_LANDMARKS,
                                    ANDROID_STATISTICS_FACE_RECTANGLES};

void ZoomRatioMapper::Initialize(InitParams* params) {
  if (params == nullptr) {
    ALOGE("%s: invalid param", __FUNCTION__);
    return;
  }
  memcpy(&active_array_dimension_, &params->active_array_dimension,
         sizeof(active_array_dimension_));
  physical_cam_active_array_dimension_ =
      params->physical_cam_active_array_dimension;
  memcpy(&zoom_ratio_range_, &params->zoom_ratio_range,
         sizeof(zoom_ratio_range_));
  zoom_ratio_mapper_hwl_ = std::move(params->zoom_ratio_mapper_hwl);
  is_zoom_ratio_supported_ = true;
}

void ZoomRatioMapper::UpdateCaptureRequest(CaptureRequest* request) {
  if (request == nullptr) {
    ALOGE("%s: request is nullptr", __FUNCTION__);
    return;
  }

  if (!is_zoom_ratio_supported_) {
    ALOGV("%s: zoom ratio is not supported", __FUNCTION__);
    return;
  }

  if (request->settings != nullptr) {
    ApplyZoomRatio(active_array_dimension_, true, request->settings.get());
  }

  for (auto& [camera_id, metadata] : request->physical_camera_settings) {
    if (metadata != nullptr) {
      auto physical_cam_iter =
          physical_cam_active_array_dimension_.find(camera_id);
      if (physical_cam_iter == physical_cam_active_array_dimension_.end()) {
        ALOGE("%s: Physical camera id %d is not found!", __FUNCTION__,
              camera_id);
        continue;
      }
      Dimension physical_active_array_dimension = physical_cam_iter->second;
      ApplyZoomRatio(physical_active_array_dimension, true, metadata.get());
    }
  }
  if (zoom_ratio_mapper_hwl_) {
    zoom_ratio_mapper_hwl_->UpdateCaptureRequest(request);
  }
}

void ZoomRatioMapper::UpdateCaptureResult(CaptureResult* result) {
  if (result == nullptr) {
    ALOGE("%s: result is nullptr", __FUNCTION__);
    return;
  }

  if (!is_zoom_ratio_supported_) {
    ALOGV("%s: zoom ratio is not supported", __FUNCTION__);
    return;
  }

  if (result->result_metadata != nullptr) {
    ApplyZoomRatio(active_array_dimension_, false,
                   result->result_metadata.get());
  }

  for (auto& [camera_id, metadata] : result->physical_metadata) {
    if (metadata != nullptr) {
      auto physical_cam_iter =
          physical_cam_active_array_dimension_.find(camera_id);
      if (physical_cam_iter == physical_cam_active_array_dimension_.end()) {
        ALOGE("%s: Physical camera id %d is not found!", __FUNCTION__,
              camera_id);
        continue;
      }
      Dimension physical_active_array_dimension = physical_cam_iter->second;
      ApplyZoomRatio(physical_active_array_dimension, false, metadata.get());
    }
  }
  if (zoom_ratio_mapper_hwl_) {
    zoom_ratio_mapper_hwl_->UpdateCaptureResult(result);
  }
}

void ZoomRatioMapper::ApplyZoomRatio(const Dimension& active_array_dimension,
                                     const bool is_request,
                                     HalCameraMetadata* metadata) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr", __FUNCTION__);
    return;
  }

  camera_metadata_ro_entry entry = {};
  status_t res = metadata->Get(ANDROID_CONTROL_ZOOM_RATIO, &entry);
  if (res != OK) {
    ALOGE("%s: Failed to get the zoom ratio", __FUNCTION__);
    return;
  }
  float zoom_ratio = entry.data.f[0];

  if (zoom_ratio < zoom_ratio_range_.min) {
    ALOGE("%s, zoom_ratio(%f) is smaller than lower bound(%f)", __FUNCTION__,
          zoom_ratio, zoom_ratio_range_.min);
    zoom_ratio = zoom_ratio_range_.min;
  } else if (zoom_ratio > zoom_ratio_range_.max) {
    ALOGE("%s, zoom_ratio(%f) is larger than upper bound(%f)", __FUNCTION__,
          zoom_ratio, zoom_ratio_range_.max);
    zoom_ratio = zoom_ratio_range_.max;
  }

  if (zoom_ratio_mapper_hwl_ != nullptr && is_request) {
    zoom_ratio_mapper_hwl_->LimitZoomRatioIfConcurrent(&zoom_ratio);
  }

  if (fabs(zoom_ratio - entry.data.f[0]) > 1e-9) {
    metadata->Set(ANDROID_CONTROL_ZOOM_RATIO, &zoom_ratio, entry.count);
  }

  for (auto tag_id : kRectToConvert) {
    UpdateRects(zoom_ratio, tag_id, active_array_dimension, is_request,
                metadata);
  }

  for (auto tag_id : kWeightedRectToConvert) {
    UpdateWeightedRects(zoom_ratio, tag_id, active_array_dimension, is_request,
                        metadata);
  }

  if (!is_request) {
    for (auto tag_id : kResultPointsToConvert) {
      UpdatePoints(zoom_ratio, tag_id, active_array_dimension, metadata);
    }
  }
}

void ZoomRatioMapper::UpdateRects(const float zoom_ratio, const uint32_t tag_id,
                                  const Dimension& active_array_dimension,
                                  const bool is_request,
                                  HalCameraMetadata* metadata) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr", __FUNCTION__);
    return;
  }
  camera_metadata_ro_entry entry = {};
  status_t res = metadata->Get(tag_id, &entry);
  if (res != OK || entry.count == 0) {
    ALOGE("%s: Failed to get the region: %d, res: %d", __FUNCTION__, tag_id,
          res);
    return;
  }
  int32_t left = entry.data.i32[0];
  int32_t top = entry.data.i32[1];
  int32_t width = entry.data.i32[2];
  int32_t height = entry.data.i32[3];

  if (is_request) {
    utils::ConvertZoomRatio(zoom_ratio, active_array_dimension, &left, &top,
                            &width, &height);
  } else {
    utils::RevertZoomRatio(zoom_ratio, active_array_dimension, true, &left,
                           &top, &width, &height);
  }
  int32_t rect[4] = {left, top, width, height};

  ALOGV(
      "%s: is request: %d, zoom ratio: %f, set rect: [%d, %d, %d, %d] -> [%d, "
      "%d, %d, %d]",
      __FUNCTION__, is_request, zoom_ratio, entry.data.i32[0], entry.data.i32[1],
      entry.data.i32[2], entry.data.i32[3], rect[0], rect[1], rect[2], rect[3]);

  res = metadata->Set(tag_id, rect, sizeof(rect) / sizeof(int32_t));
  if (res != OK) {
    ALOGE("%s: Updating region: %u failed: %s (%d)", __FUNCTION__, tag_id,
          strerror(-res), res);
  }
}

void ZoomRatioMapper::UpdateWeightedRects(
    const float zoom_ratio, const uint32_t tag_id,
    const Dimension& active_array_dimension, const bool is_request,
    HalCameraMetadata* metadata) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr", __FUNCTION__);
    return;
  }
  camera_metadata_ro_entry entry = {};
  status_t res = metadata->Get(tag_id, &entry);
  if (res != OK || entry.count == 0) {
    ALOGV("%s: Failed to get the region: %d, res: %d", __FUNCTION__, tag_id,
          res);
    return;
  }
  const WeightedRect* regions =
      reinterpret_cast<const WeightedRect*>(entry.data.i32);
  const size_t kNumElementsInTuple = sizeof(WeightedRect) / sizeof(int32_t);
  std::vector<WeightedRect> updated_regions(entry.count / kNumElementsInTuple);

  for (size_t i = 0; i < updated_regions.size(); i++) {
    int32_t left = regions[i].left;
    int32_t top = regions[i].top;
    int32_t width = regions[i].right - regions[i].left + 1;
    int32_t height = regions[i].bottom - regions[i].top + 1;

    if (is_request) {
      utils::ConvertZoomRatio(zoom_ratio, active_array_dimension, &left, &top,
                              &width, &height);
    } else {
      utils::RevertZoomRatio(zoom_ratio, active_array_dimension, true, &left,
                             &top, &width, &height);
    }

    updated_regions[i].left = left;
    updated_regions[i].top = top;
    updated_regions[i].right = left + width - 1;
    updated_regions[i].bottom = top + height - 1;
    updated_regions[i].weight = regions[i].weight;

    ALOGV("%s: set region(%d): [%d, %d, %d, %d, %d]", __FUNCTION__, tag_id,
          updated_regions[i].left, updated_regions[i].top,
          updated_regions[i].right, updated_regions[i].bottom,
          updated_regions[i].weight);
  }
  res = metadata->Set(tag_id, reinterpret_cast<int32_t*>(updated_regions.data()),
                      updated_regions.size() * kNumElementsInTuple);
  if (res != OK) {
    ALOGE("%s: Updating region(%d) failed: %s (%d)", __FUNCTION__, tag_id,
          strerror(-res), res);
  }
}

void ZoomRatioMapper::UpdatePoints(const float zoom_ratio, const uint32_t tag_id,
                                   const Dimension& active_array_dimension,
                                   HalCameraMetadata* metadata) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr", __FUNCTION__);
    return;
  }
  camera_metadata_ro_entry entry = {};
  if (metadata->Get(tag_id, &entry) != OK) {
    ALOGV("%s: tag: %u not published.", __FUNCTION__, tag_id);
    return;
  }

  if (entry.count <= 0) {
    ALOGV("%s: No data found, tag: %u", __FUNCTION__, tag_id);
    return;
  }
  // x, y
  const uint32_t kDataSizePerPoint = 2;
  const uint32_t point_num = entry.count / kDataSizePerPoint;
  std::vector<PointI> points(point_num);
  uint32_t data_index = 0;

  for (uint32_t i = 0; i < point_num; i++) {
    data_index = i * kDataSizePerPoint;
    PointI* transformed = &points.at(i);
    transformed->x = entry.data.i32[data_index];
    transformed->y = entry.data.i32[data_index + 1];
    utils::RevertZoomRatio(zoom_ratio, active_array_dimension, true,
                           &transformed->x, &transformed->y);
  }

  status_t res = metadata->Set(
      tag_id, reinterpret_cast<int32_t*>(points.data()), entry.count);

  if (res != OK) {
    ALOGE("%s: Updating tag: %u failed: %s (%d)", __FUNCTION__, tag_id,
          strerror(-res), res);
  }
}

}  // namespace google_camera_hal
}  // namespace android
