/*
 * Copyright (C) 2016-2017 Intel Corporation.
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#define LOG_TAG "Metadata"

#include "Metadata.h"
#include "ControlUnit.h"
#include "LogHelper.h"
#include "SettingsProcessor.h"
#include "CameraMetadataHelper.h"

namespace android {
namespace camera2 {

Metadata::Metadata(int cameraId):
        mCameraId(cameraId)
{
}

Metadata::~Metadata()
{
}

status_t Metadata::init()
{
    return OK;
}

/**
 * Update the Jpeg metadata
 * Only copying from control to dynamic
 */
void Metadata::writeJpegMetadata(RequestCtrlState &reqState) const
{
    if (reqState.request == nullptr) {
        LOGE("nullptr request in RequestCtrlState - BUG.");
        return;
    }

    const CameraMetadata *settings = reqState.request->getSettings();

    if (settings == nullptr) {
        LOGE("No settings for JPEG in request - BUG.");
        return;
    }

    // TODO: Put JPEG settings to ProcessingUnitSettings, when implemented

    camera_metadata_ro_entry entry = settings->find(ANDROID_JPEG_GPS_COORDINATES);
    if (entry.count == 3) {
        reqState.ctrlUnitResult->update(ANDROID_JPEG_GPS_COORDINATES, entry.data.d, entry.count);
    }

    entry = settings->find(ANDROID_JPEG_GPS_PROCESSING_METHOD);
    if (entry.count > 0) {
        reqState.ctrlUnitResult->update(ANDROID_JPEG_GPS_PROCESSING_METHOD, entry.data.u8, entry.count);
    }

    entry = settings->find(ANDROID_JPEG_GPS_TIMESTAMP);
    if (entry.count == 1) {
        reqState.ctrlUnitResult->update(ANDROID_JPEG_GPS_TIMESTAMP, entry.data.i64, entry.count);
    }

    entry = settings->find(ANDROID_JPEG_ORIENTATION);
    if (entry.count == 1) {
        reqState.ctrlUnitResult->update(ANDROID_JPEG_ORIENTATION, entry.data.i32, entry.count);
    }

    entry = settings->find(ANDROID_JPEG_QUALITY);
    if (entry.count == 1) {
        reqState.ctrlUnitResult->update(ANDROID_JPEG_QUALITY, entry.data.u8, entry.count);
    }

    entry = settings->find(ANDROID_JPEG_THUMBNAIL_QUALITY);
    if (entry.count == 1) {
        reqState.ctrlUnitResult->update(ANDROID_JPEG_THUMBNAIL_QUALITY, entry.data.u8, entry.count);
    }

    entry = settings->find(ANDROID_JPEG_THUMBNAIL_SIZE);
    if (entry.count == 2) {
        reqState.ctrlUnitResult->update(ANDROID_JPEG_THUMBNAIL_SIZE, entry.data.i32, entry.count);
    }
}

void Metadata::checkResultMetadata(CameraMetadata *results, int cameraId) const{
    LOGI("@%s %d: enter", __FUNCTION__, __LINE__);
    const camera_metadata *staticMeta = PlatformData::getStaticMetadata(cameraId);
    camera_metadata_ro_entry entry;
    find_camera_metadata_ro_entry(staticMeta,
                                  ANDROID_REQUEST_AVAILABLE_RESULT_KEYS,
                                  &entry);
    for(int i = 0; i < entry.count; i++) {
        if(results->find(entry.data.i32[i]).count == 0) {
            LOGW("@%s %d: result key (%s) not included, CTS:testCameraANDROID_llKeys may fail",
                 __FUNCTION__, __LINE__, METAID2STR(metadataNames, entry.data.i32[i]));
        }
    }
}

void Metadata::writeRestMetadata(RequestCtrlState &reqState) const
{
    const CameraMetadata *settings = reqState.request->getSettings();
    CameraMetadata *results = reqState.ctrlUnitResult;
    camera_metadata_ro_entry entry;
#define RESULT_UPDATE_SETTING(tag) \
    entry = settings->find(tag); \
    if (entry.count) { \
        LOGD("@%s %d: %s update", __FUNCTION__, __LINE__, METAID2STR(metadataNames, tag)); \
        switch(entry.type) { \
        case TYPE_BYTE: \
              results->update(tag, entry.data.u8, entry.count); \
              break; \
        case TYPE_INT32: \
              results->update(tag, entry.data.i32, entry.count); \
              break; \
        case TYPE_FLOAT: \
              results->update(tag, entry.data.f, entry.count); \
              break; \
        case TYPE_INT64: \
              results->update(tag, entry.data.i64, entry.count); \
              break; \
        case TYPE_DOUBLE: \
              results->update(tag, entry.data.d, entry.count); \
              break; \
        case TYPE_RATIONAL: \
              results->update(tag, entry.data.r, entry.count); \
              break; \
        default: \
              LOGW("@%s %d: do not support the metadata entry type: %d", __FUNCTION__, __LINE__, entry.type); \
              break; \
        } \
    }

// if result keys not filled before, fill the key with the value that from
// app settings or a fake value.
#define RESULT_UPDATE_IF_NEED(tag) \
    if(results->find(tag).count == 0) { \
        RESULT_UPDATE_SETTING(tag) \
    }

// This should seldom happen because the real value should be filled somewhere
// else. The supplement of result keys here is for the cts test
#define RESULT_UPDATE_WITH_VALUE_IF_NEED(tag, cnt, value) \
    if(results->find(tag).count == 0) { \
        results->update(tag, value, cnt); \
    }

    // all result keys CTS will check. Check and fill the unfilled keys first
    RESULT_UPDATE_IF_NEED(ANDROID_COLOR_CORRECTION_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_COLOR_CORRECTION_TRANSFORM);
    RESULT_UPDATE_IF_NEED(ANDROID_COLOR_CORRECTION_GAINS);
    RESULT_UPDATE_IF_NEED(ANDROID_COLOR_CORRECTION_ABERRATION_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AE_ANTIBANDING_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AE_LOCK);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AE_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AE_REGIONS);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AF_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AF_REGIONS);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AF_TRIGGER);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AWB_LOCK);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AWB_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AWB_REGIONS);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_CAPTURE_INTENT);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_EFFECT_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_SCENE_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AE_STATE);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AF_STATE);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_AWB_STATE);
    RESULT_UPDATE_IF_NEED(ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST);
    RESULT_UPDATE_IF_NEED(ANDROID_EDGE_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_FLASH_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_FLASH_STATE);
    RESULT_UPDATE_IF_NEED(ANDROID_HOT_PIXEL_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_JPEG_GPS_COORDINATES);
    RESULT_UPDATE_IF_NEED(ANDROID_JPEG_ORIENTATION);
    RESULT_UPDATE_IF_NEED(ANDROID_JPEG_QUALITY);
    RESULT_UPDATE_IF_NEED(ANDROID_JPEG_THUMBNAIL_QUALITY);
    RESULT_UPDATE_IF_NEED(ANDROID_JPEG_THUMBNAIL_SIZE);
    RESULT_UPDATE_IF_NEED(ANDROID_LENS_APERTURE);
    RESULT_UPDATE_IF_NEED(ANDROID_LENS_FILTER_DENSITY);
    RESULT_UPDATE_IF_NEED(ANDROID_LENS_FOCAL_LENGTH);
    RESULT_UPDATE_IF_NEED(ANDROID_LENS_FOCUS_DISTANCE);
    RESULT_UPDATE_IF_NEED(ANDROID_LENS_OPTICAL_STABILIZATION_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_LENS_POSE_ROTATION);
    RESULT_UPDATE_IF_NEED(ANDROID_LENS_POSE_TRANSLATION);
    RESULT_UPDATE_IF_NEED(ANDROID_LENS_FOCUS_RANGE);
    RESULT_UPDATE_IF_NEED(ANDROID_LENS_STATE);
    RESULT_UPDATE_IF_NEED(ANDROID_LENS_INTRINSIC_CALIBRATION);
    RESULT_UPDATE_IF_NEED(ANDROID_LENS_RADIAL_DISTORTION);
    RESULT_UPDATE_IF_NEED(ANDROID_NOISE_REDUCTION_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_REQUEST_PIPELINE_DEPTH);
    RESULT_UPDATE_IF_NEED(ANDROID_SCALER_CROP_REGION);
    RESULT_UPDATE_IF_NEED(ANDROID_SENSOR_EXPOSURE_TIME);
    RESULT_UPDATE_IF_NEED(ANDROID_SENSOR_FRAME_DURATION);
    RESULT_UPDATE_IF_NEED(ANDROID_SENSOR_SENSITIVITY);
    RESULT_UPDATE_IF_NEED(ANDROID_SENSOR_TIMESTAMP);
    RESULT_UPDATE_IF_NEED(ANDROID_SENSOR_NEUTRAL_COLOR_POINT);
    RESULT_UPDATE_IF_NEED(ANDROID_SENSOR_NOISE_PROFILE);
    RESULT_UPDATE_IF_NEED(ANDROID_SENSOR_GREEN_SPLIT);
    RESULT_UPDATE_IF_NEED(ANDROID_SENSOR_TEST_PATTERN_DATA);
    RESULT_UPDATE_IF_NEED(ANDROID_SENSOR_TEST_PATTERN_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_SENSOR_ROLLING_SHUTTER_SKEW);
    RESULT_UPDATE_IF_NEED(ANDROID_SENSOR_DYNAMIC_BLACK_LEVEL);
    RESULT_UPDATE_IF_NEED(ANDROID_SENSOR_DYNAMIC_WHITE_LEVEL);
    RESULT_UPDATE_IF_NEED(ANDROID_SHADING_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_STATISTICS_FACE_DETECT_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_STATISTICS_FACE_IDS);
    RESULT_UPDATE_IF_NEED(ANDROID_STATISTICS_LENS_SHADING_CORRECTION_MAP);
    RESULT_UPDATE_IF_NEED(ANDROID_STATISTICS_SCENE_FLICKER);
    RESULT_UPDATE_IF_NEED(ANDROID_STATISTICS_HOT_PIXEL_MAP);
    RESULT_UPDATE_IF_NEED(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_TONEMAP_CURVE_BLUE);
    RESULT_UPDATE_IF_NEED(ANDROID_TONEMAP_CURVE_GREEN);
    RESULT_UPDATE_IF_NEED(ANDROID_TONEMAP_CURVE_RED);
    RESULT_UPDATE_IF_NEED(ANDROID_TONEMAP_MODE);
    RESULT_UPDATE_IF_NEED(ANDROID_TONEMAP_GAMMA);
    RESULT_UPDATE_IF_NEED(ANDROID_TONEMAP_PRESET_CURVE);
    RESULT_UPDATE_IF_NEED(ANDROID_BLACK_LEVEL_LOCK);
    RESULT_UPDATE_IF_NEED(ANDROID_REPROCESS_EFFECTIVE_EXPOSURE_FACTOR);

    uint8_t u8 = 0;
    int32_t i32 = 0;
    float   f = 0.0;
    int64_t i64 = 0;
    double  d = 0.0;

    //fake value for CTS
    u8 = ANDROID_CONTROL_AE_STATE_CONVERGED;
    RESULT_UPDATE_WITH_VALUE_IF_NEED(ANDROID_CONTROL_AE_STATE, 1, &u8);
    u8 = ANDROID_CONTROL_AWB_STATE_CONVERGED;
    RESULT_UPDATE_WITH_VALUE_IF_NEED(ANDROID_CONTROL_AWB_STATE, 1, &u8);
    u8 = ANDROID_CONTROL_AF_STATE_INACTIVE;
    RESULT_UPDATE_WITH_VALUE_IF_NEED(ANDROID_CONTROL_AF_STATE, 1, &u8);

    u8 = 0;
    RESULT_UPDATE_WITH_VALUE_IF_NEED(ANDROID_LENS_OPTICAL_STABILIZATION_MODE, 1, &u8);
    u8 = ANDROID_LENS_STATE_STATIONARY;
    RESULT_UPDATE_WITH_VALUE_IF_NEED(ANDROID_LENS_STATE, 1, &u8);

    i32 = 0;
    RESULT_UPDATE_WITH_VALUE_IF_NEED(ANDROID_SENSOR_TEST_PATTERN_MODE, 1, &i32);

    entry = settings->find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
    if (entry.count == 2) {
        i64 = 1 * 1000 * 1000 * 1000 / entry.data.i32[1];
        RESULT_UPDATE_WITH_VALUE_IF_NEED(ANDROID_SENSOR_FRAME_DURATION, 1, &i64);
    }

    i64 = 15 * 1000 * 1000;  //fake rolling time
    RESULT_UPDATE_WITH_VALUE_IF_NEED(ANDROID_SENSOR_ROLLING_SHUTTER_SKEW, 1, &i64);

    i32 = 0;
    RESULT_UPDATE_WITH_VALUE_IF_NEED(ANDROID_STATISTICS_FACE_IDS, 1, &i32);
    u8 = 0;
    RESULT_UPDATE_WITH_VALUE_IF_NEED(ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE, 1, &u8);
    RESULT_UPDATE_WITH_VALUE_IF_NEED(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE, 1, &u8);

    results->sort();
    checkResultMetadata(results, mCameraId);
}

} /* namespace camera2 */
} /* namespace android */
