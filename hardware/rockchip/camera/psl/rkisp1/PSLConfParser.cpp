/*
 * Copyright (C) 2014-2017 Intel Corporation
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

#define LOG_TAG "PSLConfParser"

#include "PSLConfParser.h"

#include <CameraMetadata.h>
#include <string>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <v4l2device.h>
#include "RKISP1Common.h"
#include "LogHelper.h"
#include "GraphConfigManager.h"
#include "NodeTypes.h"
USING_METADATA_NAMESPACE;
using std::string;

namespace android {
namespace camera2 {

IPSLConfParser *PSLConfParser::sInstance = nullptr; // the singleton instance

std::vector<std::string> PSLConfParser::mImguMediaDevice;
std::vector<std::string> PSLConfParser::mSensorMediaDevice;

static const char *NVM_DATA_PATH = "/sys/bus/i2c/devices/";
#if defined(ANDROID_VERSION_ABOVE_8_X)
static const char *GRAPH_SETTINGS_FILE_PATH = "/vendor/etc/camera/";
#else
static const char *GRAPH_SETTINGS_FILE_PATH = "/etc/camera/";
#endif
IPSLConfParser *PSLConfParser::getInstance(std::string &xmlConfigName, const std::vector<SensorDriverDescriptor>& sensorNames)
{
    if (sInstance == nullptr) {
        sInstance = new PSLConfParser(xmlConfigName, sensorNames);
    }
    return sInstance;
}

void PSLConfParser::deleteInstance()
{
    if (sInstance != nullptr) {
        delete sInstance;
        sInstance = nullptr;
    }
}

PSLConfParser::PSLConfParser(std::string& xmlName, const std::vector<SensorDriverDescriptor>& sensorNames):
                IPSLConfParser(xmlName, sensorNames)
{
    mCurrentDataField = FIELD_INVALID;
    mSensorIndex  = -1;
    getPSLDataFromXmlFile();
    // Uncomment to display all the parsed values
    //dump();
}

PSLConfParser::~PSLConfParser()
{
    while (!mCaps.empty()) {
        RKISP1CameraCapInfo* info = static_cast<RKISP1CameraCapInfo*>(mCaps.front());
        mCaps.erase(mCaps.begin());
        ::operator delete(info->mNvmData.data);
        info->mNvmData.data = nullptr;
        delete info;
    }

    for (unsigned int i = 0; i < mDefaultRequests.size(); i++) {
       if (mDefaultRequests[i])
            free_camera_metadata(mDefaultRequests[i]);
    }

    mDefaultRequests.clear();

}

CameraCapInfo* PSLConfParser::getCameraCapInfo(int cameraId)
{
    if (cameraId > MAX_CAMERAS) {
        LOGE("ERROR @%s: Invalid camera: %d", __FUNCTION__, cameraId);
        cameraId = 0;
    }

    return mCaps[cameraId];
}

void PSLConfParser::checkRequestMetadata(const camera_metadata *request, int cameraId)
{
    const camera_metadata *staticMeta = PlatformData::getStaticMetadata(cameraId);
    camera_metadata_ro_entry entry;
    camera_metadata_ro_entry ro_entry;
    int ret = find_camera_metadata_ro_entry(staticMeta,
                                  ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS,
                                  &ro_entry);
    if(ret)
        LOGE("@%s %d: find_camera_metadata_ro_entry error, should not happe, fix me", __FUNCTION__, __LINE__);
    for(int i = 0; i < ro_entry.count; i++) {
        ret = find_camera_metadata_ro_entry(request, ro_entry.data.i32[i], &entry);
        if(ret) {
            LOGW("@%s %d: request key (%s) not included, CTS:testCameraDeviceXXXTemplate may fail",
                 __FUNCTION__, __LINE__, METAID2STR(metadataNames, entry.data.i32[i]));
        }
    }
}

uint8_t PSLConfParser::selectAfMode(const camera_metadata_t *staticMeta,
                                    int reqTemplate)
{
    // For initial value, use AF_MODE_OFF. That is the minimum,
    // for fixed-focus sensors. For request templates the desired
    // values will be defined below.
    uint8_t afMode = ANDROID_CONTROL_AF_MODE_OFF;

    const int MAX_AF_MODES = 6;
    // check this is the maximum number of enums defined by:
    // camera_metadata_enum_android_control_af_mode_t defined in
    // camera_metadata_tags.h
    camera_metadata_ro_entry ro_entry;
    CLEAR(ro_entry);
    bool modesAvailable[MAX_AF_MODES];
    CLEAR(modesAvailable);
    int ret = find_camera_metadata_ro_entry(staticMeta, ANDROID_CONTROL_AF_AVAILABLE_MODES,
                                  &ro_entry);
    if(!ret) {
        for (size_t i = 0; i < ro_entry.count; i++) {
            if (ro_entry.data.u8[i] <  MAX_AF_MODES)
                modesAvailable[ro_entry.data.u8[i]] = true;
        }
    } else {
        LOGE("@%s: Incomplete camera3_profiles.xml: available AF modes missing!!", __FUNCTION__);
    }

    switch (reqTemplate) {
       case ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE:
       case ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG:
       case ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW:
           if (modesAvailable[ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE])
               afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
           break;
       case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD:
       case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT:
           if (modesAvailable[ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO])
               afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
           break;
       case ANDROID_CONTROL_CAPTURE_INTENT_MANUAL:
           if (modesAvailable[ANDROID_CONTROL_AF_MODE_OFF])
               afMode = ANDROID_CONTROL_AF_MODE_OFF;
           break;
       case ANDROID_CONTROL_CAPTURE_INTENT_START:
       default:
           afMode = ANDROID_CONTROL_AF_MODE_AUTO;
           break;
       }
    return afMode;
}

uint8_t PSLConfParser::selectAeAntibandingMode(const camera_metadata_t *staticMeta,
                                               int reqTemplate)
{
    uint8_t aeAntibandingMode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF;

    const int MAX_AEANTIBANDING_MODES = 4;
    // check this is the maximum number of enums defined by:
    // camera_metadata_enum_android_control_af_mode_t defined in
    // camera_metadata_tags.h
    camera_metadata_ro_entry ro_entry;
    CLEAR(ro_entry);
    bool modesAvailable[MAX_AEANTIBANDING_MODES];
    CLEAR(modesAvailable);
    int ret = find_camera_metadata_ro_entry(staticMeta, ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES,
                                            &ro_entry);
    if(!ret) {
        for (size_t i = 0; i < ro_entry.count; i++) {
            if (ro_entry.data.u8[i] <  MAX_AEANTIBANDING_MODES)
                modesAvailable[ro_entry.data.u8[i]] = true;
        }
    } else {
        LOGE("@%s: Incomplete camera3_profiles.xml: available AeAntibanding modes missing!!", __FUNCTION__);
    }

    switch (reqTemplate) {
       case ANDROID_CONTROL_CAPTURE_INTENT_MANUAL:
           if (modesAvailable[ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF])
               aeAntibandingMode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF;
           break;
       case ANDROID_CONTROL_CAPTURE_INTENT_START:
       case ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE:
       case ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG:
       case ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW:
       case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD:
       case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT:
       default:
           if (modesAvailable[ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO])
               aeAntibandingMode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
           else // first entry has high priority
               aeAntibandingMode = ro_entry.data.u8[0];
           break;
       }
    return aeAntibandingMode;
}

uint8_t PSLConfParser::selectEdgeMode(const camera_metadata_t *staticMeta,
                                    int reqTemplate)
{
    uint8_t mode = ANDROID_EDGE_MODE_OFF;

    const int MAX_MODES = 4;
    camera_metadata_ro_entry ro_entry;
    CLEAR(ro_entry);
    bool modesAvailable[MAX_MODES];
    CLEAR(modesAvailable);
    int ret = find_camera_metadata_ro_entry(staticMeta, ANDROID_EDGE_AVAILABLE_EDGE_MODES,
                                  &ro_entry);
    if(!ret) {
        for (size_t i = 0; i < ro_entry.count; i++) {
            if (ro_entry.data.u8[i] <  MAX_MODES)
                modesAvailable[ro_entry.data.u8[i]] = true;
        }
    } else {
        LOGW("@%s: if support ZSL, CTS:CTS#testCameraDeviceZSLTemplate may failed for ANDROID_EDGE_MODE_ZERO_SHUTTER_LAG should be guranteed to supported", __FUNCTION__);
        return mode;
    }

    // CTS require different edge modes in corresponding template:
    // ANDROID_EDGE_MODE_HIGH_QUALITY in ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE
    // ANDROID_EDGE_MODE_FAST in ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW
    // ANDROID_EDGE_MODE_ZERO_SHUTTER_LAG in
    // ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG
    switch (reqTemplate) {
       case ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE:
           if (modesAvailable[ANDROID_EDGE_MODE_HIGH_QUALITY])
               mode = ANDROID_EDGE_MODE_HIGH_QUALITY;
           else
               mode = ANDROID_EDGE_MODE_OFF;
           break;
       case ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG:
           if (modesAvailable[ANDROID_EDGE_MODE_ZERO_SHUTTER_LAG])
               mode = ANDROID_EDGE_MODE_ZERO_SHUTTER_LAG;
           LOGE("@%s %d: ZERO_SHUTTER_LAG Template require ZERO_SHUTTER_LAG edge mode, CTS#testCameraDeviceZSLTemplate will fail", __FUNCTION__, __LINE__);
           break;
       case ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW:
       case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD:
           if (modesAvailable[ANDROID_EDGE_MODE_FAST])
               mode = ANDROID_EDGE_MODE_FAST;
           else
               mode = ANDROID_EDGE_MODE_OFF;
           break;
       default:
           mode = ANDROID_EDGE_MODE_OFF;
           break;
       }
    return mode;
}

uint8_t PSLConfParser::selectNrMode(const camera_metadata_t *staticMeta,
                                    int reqTemplate)
{
    uint8_t mode = ANDROID_NOISE_REDUCTION_MODE_OFF;

    const int MAX_MODES = 5;
    camera_metadata_ro_entry ro_entry;
    CLEAR(ro_entry);
    bool modesAvailable[MAX_MODES];
    CLEAR(modesAvailable);
    int ret = find_camera_metadata_ro_entry(staticMeta, ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES,
                                  &ro_entry);
    if(!ret) {
        LOGI("@%s %d: count :%d, %p", __FUNCTION__, __LINE__, ro_entry.count, ro_entry.data.u8);
        for (size_t i = 0; i < ro_entry.count; i++) {
            if (ro_entry.data.u8[i] <  MAX_MODES)
                modesAvailable[ro_entry.data.u8[i]] = true;
        }
    } else {
        return mode;
    }

    // CTS require different NR modes in corresponding template:
    switch (reqTemplate) {
       case ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE:
           if (modesAvailable[ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY])
               mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
           else
               mode = ANDROID_NOISE_REDUCTION_MODE_OFF;
           break;
       case ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG:
           if (modesAvailable[ANDROID_NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG])
               mode = ANDROID_NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG;
           LOGE("@%s %d: ZERO_SHUTTER_LAG Template require ZERO_SHUTTER_LAG Noise reduction mode, CTS#testCameraDeviceZSLTemplate will fail", __FUNCTION__, __LINE__);
           break;
       case ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW:
       case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD:
           if (modesAvailable[ANDROID_NOISE_REDUCTION_MODE_FAST])
               mode = ANDROID_NOISE_REDUCTION_MODE_FAST;
           else
               mode = ANDROID_NOISE_REDUCTION_MODE_OFF;
           break;
       default:
           mode = ANDROID_NOISE_REDUCTION_MODE_OFF;
           break;
       }
    return mode;
}

camera_metadata_t* PSLConfParser::constructDefaultMetadata(int cameraId, int requestTemplate)
{
    LOGI("@%s: %d", __FUNCTION__, requestTemplate);
    if (requestTemplate >= CAMERA_TEMPLATE_COUNT) {
        LOGE("ERROR @%s: bad template %d", __FUNCTION__, requestTemplate);
        return nullptr;
    }

    int index = cameraId * CAMERA_TEMPLATE_COUNT + requestTemplate;
    // vector::at is bound-checked
    camera_metadata_t * req = mDefaultRequests.at(index);

    if (req)
        return req;

    camera_metadata_t * meta = nullptr;
    meta = allocate_camera_metadata(DEFAULT_ENTRY_CAP, DEFAULT_DATA_CAP);
    if (meta == nullptr) {
        LOGE("ERROR @%s: Allocate memory failed", __FUNCTION__);
        return nullptr;
    }

    const camera_metadata_t *staticMeta = nullptr;
    staticMeta = PlatformData::getStaticMetadata(cameraId);
    if (staticMeta == nullptr) {
        LOGE("ERROR @%s: Could not get static metadata", __FUNCTION__);
        free_camera_metadata(meta);
        return nullptr;
    }

    CameraMetadata metadata; // no constructor from const camera_metadata_t*
    metadata = staticMeta;   // but assignment operator exists for const

    int64_t bogusValue = 0;  // 8 bytes of bogus
    int64_t bogusValueArray[] = {0, 0, 0, 0, 0};  // 40 bytes of bogus

    uint8_t value_u8;
    uint8_t value_i32;
    uint8_t value_f;

    uint8_t requestType = ANDROID_REQUEST_TYPE_CAPTURE;
    uint8_t intent = 0;

    uint8_t controlMode = ANDROID_CONTROL_MODE_AUTO;
    uint8_t afMode = selectAfMode(staticMeta, requestTemplate);
    uint8_t aeMode = ANDROID_CONTROL_AE_MODE_ON;
    uint8_t awbMode = ANDROID_CONTROL_AWB_MODE_AUTO;
    camera_metadata_entry entry;
    camera_metadata_entry reqestKey_entry;

    reqestKey_entry = metadata.find(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS);

#define TAGINFO(tag, value) \
    for(int i = 0; i < reqestKey_entry.count; i++) { \
        if (reqestKey_entry.data.i32[i] == tag) { \
            add_camera_metadata_entry(meta, tag, &value, 1); \
            break; \
        } \
        if (i == reqestKey_entry.count - 1) \
            LOGW("@%s %d: %s isn't included in request keys, no need to report", __FUNCTION__, __LINE__, METAID2STR(metadataNames, tag)); \
    }
#define TAGINFO_ARRAY(tag, value, cnt) \
    for(int i = 0; i < reqestKey_entry.count; i++) { \
        if (reqestKey_entry.data.i32[i] == tag) { \
            add_camera_metadata_entry(meta, tag, value, cnt); \
            break; \
        } \
        if (i == reqestKey_entry.count - 1) \
            LOGW("@%s %d: %s isn't included in request keys, no need to report", __FUNCTION__, __LINE__, METAID2STR(metadataNames, tag)); \
    }

    switch (requestTemplate) {
    case ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
        break;
    case ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE;
        break;
    case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD;
        break;
    case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT;
        break;
    case ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG;
        break;
    case ANDROID_CONTROL_CAPTURE_INTENT_MANUAL:
        controlMode = ANDROID_CONTROL_MODE_OFF;
        aeMode = ANDROID_CONTROL_AE_MODE_OFF;
        awbMode = ANDROID_CONTROL_AWB_MODE_OFF;
        intent = ANDROID_CONTROL_CAPTURE_INTENT_MANUAL;
        break;
    case ANDROID_CONTROL_CAPTURE_INTENT_START:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM;
        break;
    default:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM;
        break;
    }

    entry = metadata.find(ANDROID_CONTROL_MAX_REGIONS);
    // AE, AWB, AF
    if (entry.count == 3) {
        int meteringRegion[METERING_RECT_SIZE] = {0,0,0,0,0};
        if (entry.data.i32[0] == 1)
            TAGINFO_ARRAY(ANDROID_CONTROL_AE_REGIONS, meteringRegion, METERING_RECT_SIZE);

        if (entry.data.i32[2] == 1)
            TAGINFO_ARRAY(ANDROID_CONTROL_AF_REGIONS, meteringRegion, METERING_RECT_SIZE);
        // we do not support AWB region
    }

    uint8_t nrMode = selectNrMode(staticMeta, requestTemplate);
    uint8_t edgeMode = selectEdgeMode(staticMeta, requestTemplate);
    TAGINFO(ANDROID_NOISE_REDUCTION_MODE, nrMode);
    TAGINFO(ANDROID_EDGE_MODE, edgeMode);

    TAGINFO(ANDROID_CONTROL_CAPTURE_INTENT, intent);

    TAGINFO(ANDROID_CONTROL_MODE, controlMode);
    TAGINFO(ANDROID_CONTROL_EFFECT_MODE, bogusValue);
    TAGINFO(ANDROID_CONTROL_SCENE_MODE, bogusValue);
    TAGINFO(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, bogusValue);
    TAGINFO(ANDROID_CONTROL_AE_MODE, aeMode);
    TAGINFO(ANDROID_CONTROL_AE_LOCK, bogusValue);
    value_u8 = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE;
    TAGINFO(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER, value_u8);
    value_u8 = ANDROID_CONTROL_AF_TRIGGER_IDLE;
    TAGINFO(ANDROID_CONTROL_AF_TRIGGER, value_u8);
    value_u8 = ANDROID_HOT_PIXEL_MODE_FAST;
    TAGINFO(ANDROID_HOT_PIXEL_MODE, value_u8);
    value_u8 = ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF;
    TAGINFO(ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE, value_u8);
    value_u8 = ANDROID_STATISTICS_SCENE_FLICKER_NONE;
    TAGINFO(ANDROID_STATISTICS_SCENE_FLICKER, value_u8);
    TAGINFO(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, bogusValue);

    // Sensor settings.
    entry = metadata.find(ANDROID_LENS_INFO_AVAILABLE_APERTURES);
    if(entry.count > 0)
        TAGINFO(ANDROID_LENS_APERTURE, entry.data.f[0]);

    entry = metadata.find(ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES);
    if(entry.count > 0)
        TAGINFO(ANDROID_LENS_FILTER_DENSITY, entry.data.f[0]);

    entry = metadata.find(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS);
    if(entry.count > 0)
        TAGINFO(ANDROID_LENS_FOCAL_LENGTH, entry.data.f[0]);

    entry = metadata.find(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION);
    if(entry.count > 0)
        TAGINFO(ANDROID_LENS_OPTICAL_STABILIZATION_MODE, entry.data.u8[0]);

    value_f = 0.0;
    TAGINFO(ANDROID_LENS_FOCUS_DISTANCE, value_f);

    int32_t mode = ANDROID_SENSOR_TEST_PATTERN_MODE_OFF;
    TAGINFO(ANDROID_SENSOR_TEST_PATTERN_MODE, mode);
    TAGINFO(ANDROID_SENSOR_ROLLING_SHUTTER_SKEW, bogusValue);
    TAGINFO(ANDROID_SENSOR_EXPOSURE_TIME, bogusValue);
    TAGINFO(ANDROID_SENSOR_SENSITIVITY, bogusValue);
    int64_t frameDuration = 33000000;
    TAGINFO(ANDROID_SENSOR_FRAME_DURATION, frameDuration);

    // ISP-processing settings.
    value_u8 = ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF;
    TAGINFO(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE, value_u8);

    TAGINFO(ANDROID_SYNC_FRAME_NUMBER, bogusValue);

    // Default fps target range
    int32_t fpsRange_const[] = {30, 30};    //used for video
    int32_t fpsRange_variable[] = {15, 30}; //used for preview
    // find the max const fpsRange for video and max variable fpsRange for preview
    entry = metadata.find(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
    for (int i = 0, cnt = entry.count / 2; i < cnt; ++i) {
        if(entry.data.i32[2 * i] == entry.data.i32[2 * i + 1]) {
            fpsRange_const[0] = entry.data.i32[2 * i];
            fpsRange_const[1] = entry.data.i32[2 * i + 1];
        } else {
            fpsRange_variable[0] = entry.data.i32[2 * i];
            fpsRange_variable[1] = entry.data.i32[2 * i + 1];
        }
    }
    LOGD("@%s : fpsRange_const[%d %d], fpsRange_variable[%d %d]", __FUNCTION__,
         fpsRange_const[0], fpsRange_const[1], fpsRange_variable[0], fpsRange_variable[1]);
    // Stable range requried for video recording
    if (requestTemplate == ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD) {
        TAGINFO_ARRAY(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fpsRange_const, 2);
    } else {
        // variable range for preview and others
        TAGINFO_ARRAY(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fpsRange_variable, 2);
    }
    // select antibanding mode
    uint8_t antiBandingMode = selectAeAntibandingMode(staticMeta, requestTemplate);
    TAGINFO(ANDROID_CONTROL_AE_ANTIBANDING_MODE, antiBandingMode);
    TAGINFO(ANDROID_CONTROL_AWB_MODE, awbMode);
    TAGINFO(ANDROID_CONTROL_AWB_LOCK, bogusValue);
    TAGINFO(ANDROID_BLACK_LEVEL_LOCK, bogusValue);
    TAGINFO(ANDROID_CONTROL_AWB_STATE, bogusValue);
    TAGINFO(ANDROID_CONTROL_AF_MODE, afMode);

    value_u8 = ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF;
    TAGINFO(ANDROID_COLOR_CORRECTION_ABERRATION_MODE, value_u8);

    TAGINFO(ANDROID_FLASH_MODE, bogusValue);

    TAGINFO(ANDROID_REQUEST_TYPE, requestType);
    TAGINFO(ANDROID_REQUEST_METADATA_MODE, bogusValue);
    TAGINFO(ANDROID_REQUEST_FRAME_COUNT, bogusValue);

    TAGINFO_ARRAY(ANDROID_SCALER_CROP_REGION, bogusValueArray, 4);

    TAGINFO(ANDROID_STATISTICS_FACE_DETECT_MODE, bogusValue);

    // todo enable when region support is implemented
    // TAGINFO_ARRAY(ANDROID_CONTROL_AE_REGIONS, bogusValueArray, 5);

    TAGINFO(ANDROID_JPEG_QUALITY, JPEG_QUALITY_DEFAULT);
    TAGINFO(ANDROID_JPEG_THUMBNAIL_QUALITY, THUMBNAIL_QUALITY_DEFAULT);

    entry = metadata.find(ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES);
    int32_t thumbSize[] = { 0, 0 };
    if (entry.count >= 4) {
        thumbSize[0] = entry.data.i32[2];
        thumbSize[1] = entry.data.i32[3];
    } else {
        LOGE("Thumbnail size should have more than two resolutions: 0x0 and non zero size. Fix your camera profile");
        thumbSize[0] = 0;
        thumbSize[1] = 0;
    }

    TAGINFO_ARRAY(ANDROID_JPEG_THUMBNAIL_SIZE, thumbSize, 2);

    entry = metadata.find(ANDROID_TONEMAP_AVAILABLE_TONE_MAP_MODES);
    if (entry.count > 0) {
        value_u8 = entry.data.u8[0];
        for (uint32_t i = 0; i < entry.count; i++) {
            if (entry.data.u8[i] == ANDROID_TONEMAP_MODE_HIGH_QUALITY) {
                value_u8 = ANDROID_TONEMAP_MODE_HIGH_QUALITY;
                break;
            }
        }
        TAGINFO(ANDROID_TONEMAP_MODE, value_u8);
    }

    float colorTransform[9] = {1.0, 0.0, 0.0,
                               0.0, 1.0, 0.0,
                               0.0, 0.0, 1.0};

    camera_metadata_rational_t transformMatrix[9];
    for (int i = 0; i < 9; i++) {
        transformMatrix[i].numerator = colorTransform[i];
        transformMatrix[i].denominator = 1.0;
    }
    TAGINFO_ARRAY(ANDROID_COLOR_CORRECTION_TRANSFORM, transformMatrix, 9);

    float colorGains[4] = {1.0, 1.0, 1.0, 1.0};
    TAGINFO_ARRAY(ANDROID_COLOR_CORRECTION_GAINS, colorGains, 4);
    TAGINFO(ANDROID_COLOR_CORRECTION_MODE, bogusValue);

#undef TAGINFO
#undef TAGINFO_ARRAY

    checkRequestMetadata(meta, cameraId);

    int entryCount = get_camera_metadata_entry_count(meta);
    int dataCount = get_camera_metadata_data_count(meta);
    LOGI("%s: Real metadata entry count %d, data count %d", __FUNCTION__,
        entryCount, dataCount);
    if ((entryCount > DEFAULT_ENTRY_CAP - ENTRY_RESERVED)
        || (dataCount > DEFAULT_DATA_CAP - DATA_RESERVED))
        LOGW("%s: Need more memory, now entry %d (%d), data %d (%d)", __FUNCTION__,
        entryCount, DEFAULT_ENTRY_CAP, dataCount, DEFAULT_DATA_CAP);

    // sort the metadata before storing
    sort_camera_metadata(meta);
    if (mDefaultRequests.at(index)) {
        free_camera_metadata(mDefaultRequests.at(index));
    }
    mDefaultRequests.at(index) = meta;
    return meta;
}

status_t PSLConfParser::addCamera(int cameraId, const std::string &sensorName, const char* moduleIdStr)
{
    LOGI("%s: for camera %d, name: %s, moduleIdStr %s",
         __FUNCTION__, cameraId, sensorName.c_str(), moduleIdStr);
    camera_metadata_t * emptyReq = nullptr;
    CLEAR(emptyReq);

    SensorType type = SENSOR_TYPE_RAW;

    RKISP1CameraCapInfo * info = new RKISP1CameraCapInfo(type);

    info->mSensorName = sensorName;
    info->mModuleIndexStr = moduleIdStr;
    mCaps.push_back(info);

    for (int i = 0; i < CAMERA_TEMPLATE_COUNT; i++)
        mDefaultRequests.push_back(emptyReq);

    return NO_ERROR;
}

/**
 * This function will handle all the HAL parameters that are different
 * depending on the camera
 *
 * It will be called in the function startElement
 *
 * \param name: the element's name.
 * \param atts: the element's attribute.
 */
void PSLConfParser::handleHALTuning(const char *name, const char **atts)
{
    LOGI("@%s", __FUNCTION__);

    if (strcmp(atts[0], "value") != 0) {
        LOGE("@%s, name:%s, atts[0]:%s, xml format wrong", __func__, name, atts[0]);
        return;
    }

    RKISP1CameraCapInfo * info = static_cast<RKISP1CameraCapInfo*>(mCaps[mSensorIndex]);
    if (strcmp(name, "flipping") == 0) {
        info->mSensorFlipping = SENSOR_FLIP_OFF;
        if (strcmp(atts[0], "value") == 0 && strcmp(atts[1], "SENSOR_FLIP_H") == 0)
            info->mSensorFlipping |= SENSOR_FLIP_H;
        if (strcmp(atts[2], "value_v") == 0 && strcmp(atts[3], "SENSOR_FLIP_V") == 0)
            info->mSensorFlipping |= SENSOR_FLIP_V;
    } else if (strcmp(name, "supportIsoMap") == 0) {
        info->mSupportIsoMap = ((strcmp(atts[1], "true") == 0) ? true : false);
	} else if (strcmp(name, "forceAutoGenAndroidMetas") == 0) {
		info->mForceAutoGenAndroidMetas = ((strcasecmp(atts[1], "true") == 0) ? true : false);
    } else if (strcmp(name, "supportTuningSize") == 0) {
        struct FrameSize_t frame;
        char * endPtr = nullptr;
        const char * src = atts[1];
        do {
            frame.width = strtol(src, &endPtr, 0);
            if (endPtr != nullptr) {
                if (*endPtr == 'x') {
                    src = endPtr + 1;
                    frame.height = strtol(src, &endPtr, 0);
                    info->mSupportTuningSize.push_back(frame);
                } else if (*endPtr == ',') {
                    src = endPtr + 1;
                } else
                    break;
            } else {
                LOGE("@%s : supportTuningSize value format error, check camera3_profiles.xml", __FUNCTION__);
                break;
            }
        } while (1);
        for (auto it = info->mSupportTuningSize.begin(); it != info->mSupportTuningSize.end(); ++it) {
            LOGD("@%s : supportTuningSize: %dx%d", __FUNCTION__, (*it).width, (*it).height);
        }
    } else if (strcmp(name, "graphSettingsFile") == 0) {
        info->mGraphSettingsFile = atts[1];
    } else if (strcmp(name, "iqTuningFile") == 0) {
        info->mIqTuningFile = atts[1];
    }
}
// String format: "%d,%d,...,%d", or "%dx%d,...,%dx%d", or "(%d,...,%d),(%d,...,%d)
int PSLConfParser::convertXmlData(void * dest, int destMaxNum, const char * src, int type)
{
    int index = 0;
    char * endPtr = nullptr;
    union {
        uint8_t * u8;
        int32_t * i32;
        int64_t * i64;
        float * f;
        double * d;
    } data;
    data.u8 = (uint8_t *)dest;

    do {
        switch (type) {
        case TYPE_BYTE:
            data.u8[index] = (char)strtol(src, &endPtr, 0);
            LOGI("    - %d -", data.u8[index]);
            break;
        case TYPE_INT32:
        case TYPE_RATIONAL:
            data.i32[index] = strtol(src, &endPtr, 0);
            LOGI("    - %d -", data.i32[index]);
            break;
        case TYPE_INT64:
            data.i64[index] = strtol(src, &endPtr, 0);
            LOGI("    - %" PRId64 " -", data.i64[index]);
            break;
        case TYPE_FLOAT:
            data.f[index] = strtof(src, &endPtr);
            LOGI("    - %8.3f -", data.f[index]);
            break;
        case TYPE_DOUBLE:
            data.d[index] = strtof(src, &endPtr);
            LOGI("    - %8.3f -", data.d[index]);
            break;
        }
        index++;
        if (endPtr != nullptr) {
            if (*endPtr == ',' || *endPtr == 'x')
                src = endPtr + 1;
            else if (*endPtr == ')')
                src = endPtr + 3;
            else if (*endPtr == 0)
                break;
        }
    } while (index < destMaxNum);

    return (type == TYPE_RATIONAL) ? ((index + 1) / 2) : index;
}

/**
 * This function will handle all the parameters describing characteristic of
 * the sensor itself
 *
 * It will be called in the function startElement
 *
 * \param name: the element's name.
 * \param atts: the element's attribute.
 */
void PSLConfParser::handleSensorInfo(const char *name, const char **atts)
{
    LOGI("@%s", __FUNCTION__);
    if (strcmp(atts[0], "value") != 0) {
        LOGE("@%s, name:%s, atts[0]:%s, xml format wrong", __func__, name, atts[0]);
        return;
    }
    RKISP1CameraCapInfo * info = static_cast<RKISP1CameraCapInfo*>(mCaps[mSensorIndex]);

    if (strcmp(name, "sensorType") == 0) {
        info->mSensorType = ((strcmp(atts[1], "SENSOR_TYPE_RAW") == 0) ? SENSOR_TYPE_RAW : SENSOR_TYPE_SOC);
    }  else if (strcmp(name, "exposure.sync") == 0) {
        info->mExposureSync = ((strcmp(atts[1], "true") == 0) ? true : false);
    }  else if (strcmp(name, "sensor.digitalGain") == 0) {
        info->mDigiGainOnSensor = ((strcmp(atts[1], "true") == 0) ? true : false);
    } else if (strcmp(name, "gain.lag") == 0) {
        info->mGainLag = atoi(atts[1]);
    } else if (strcmp(name, "exposure.lag") == 0) {
        info->mExposureLag = atoi(atts[1]);
    } else if (strcmp(name, "gainExposure.compensation") == 0) {
        info->mGainExposureComp = ((strcmp(atts[1], "true") == 0) ? true : false);
    } else if (strcmp(name, "fov") == 0) {
        info->mFov[0] = atof(atts[1]);
        info->mFov[1] = atof(atts[3]);
    } else if (strcmp(name, "statistics.initialSkip") == 0) {
        info->mStatisticsInitialSkip = atoi(atts[1]);
    } else if (strcmp(name, "frame.initialSkip") == 0) {
        info->mFrameInitialSkip = atoi(atts[1]);
    } else if (strcmp(name, "cITMaxMargin") == 0) {
        info->mCITMaxMargin = atoi(atts[1]);
    } else if (strcmp(name, "nvmDirectory") == 0) {
        info->mNvmDirectory = atts[1];
        readNvmData();
    } else if (strcmp(name, "testPattern.bayerFormat") == 0) {
        info->mTestPatternBayerFormat = atts[1];
    }
}

/**
 * This function will handle all the camera pipe elements existing.
 * The goal is to enumerate all available camera media-ctl elements
 * from the camera profile file for later usage.
 *
 * It will be called in the function startElement
 *
 * \param name: the element's name.
 * \param atts: the element's attribute.
 */
void PSLConfParser::handleMediaCtlElements(const char *name, const char **atts)
{
    LOGI("@%s, type:%s", __FUNCTION__, name);

    RKISP1CameraCapInfo * info = static_cast<RKISP1CameraCapInfo*>(mCaps[mSensorIndex]);

    if (strcmp(name, "element") == 0) {
        MediaCtlElement currentElement;
        currentElement.isysNodeName = IMGU_NODE_NULL;
        while (*atts) {
            const XML_Char* attr_name = *atts++;
            const XML_Char* attr_value = *atts++;
            if (strcmp(attr_name, "name") == 0) {
                currentElement.name =
                    PlatformData::getCameraHWInfo()->getFullMediaCtlElementName(mElementNames, attr_value);
            } else if (strcmp(attr_name, "type") == 0) {
                currentElement.type = attr_value;
            } else if (strcmp(attr_name, "isysNodeName") == 0) {
                currentElement.isysNodeName = getIsysNodeNameAsValue(attr_value);
            } else {
                LOGW("Unhandled xml attribute in MediaCtl element (%s)", attr_name);
            }
        }
        if ((currentElement.type == "video_node") && (currentElement.isysNodeName == -1)) {
            LOGE("ISYS node name is not set for \"%s\"", currentElement.name.c_str());
            return;
        }
        info->mMediaCtlElements.push_back(currentElement);
    }
}

/**
 *
 * Checks whether the name of the sensor found in the XML file is present in the
 * list of sensors detected at runtime.
 *
 * TODO: Now we only check the name but to be completely future proof we need
 * to add to the XML the CSI port and also check here if the CSI port matches
 *
 * \param[in] sensorName: sensor name found in XML camera profile
 * \param[in] moduleId: module id found in XML camera profile
 *
 * \return true if sensor is also in the list of detected sensors, false
 *              otherwise
 */
bool PSLConfParser::isSensorPresent(const std::string &sensorName, const char* moduleId)
{
    for (size_t i = 0; i < mDetectedSensors.size(); i++) {
        if (mDetectedSensors[i].mSensorName == sensorName &&
            strcmp(mDetectedSensors[i].mModuleIndexStr.c_str(), moduleId) == 0) {
            return true;
        }
    }
    return false;
}

void PSLConfParser::checkField(const char *name, const char **atts)
{
    if (!strcmp(name, "Profiles")) {
        std::string sensorName;
        mUseProfile = true;
        /*
         * Parse the name of the sensor if available
         */
        if (atts[2] && (strcmp(atts[2], "name") == 0)) {
            if (atts[3]) {
                sensorName = atts[3];
                LOGI("@%s: mSensorIndex = %d, name = %s",
                        __FUNCTION__,
                        mSensorIndex,
                        sensorName.c_str());
                mUseProfile = isSensorPresent(sensorName, atts[5]);
                if (mUseProfile)
                    mSensorIndex++;
            } else {
                LOGE("No name provided for camera id[%d], fix your XML- FATAL",
                      mSensorIndex);
                return;
            }
        }

        if (mUseProfile) {
            if (mSensorIndex > MAX_CAMERAS) {
                LOGE("ERROR: bad camera id %d!", mSensorIndex);
                return;
            }
            addCamera(mSensorIndex, sensorName, atts[5]);
        }

    } else if (!strcmp(name, "Hal_tuning_RKISP1")) {
        mCurrentDataField = FIELD_HAL_TUNING_RKISP1;
    } else if (!strcmp(name, "Sensor_info_RKISP1")) {
        mCurrentDataField = FIELD_SENSOR_INFO_RKISP1;
    } else if (!strcmp(name, "MediaCtl_elements_RKISP1")) {
        mCurrentDataField = FIELD_MEDIACTL_ELEMENTS_RKISP1;
    } else if (isCommonSection(name)) {
        mCurrentDataField = (DataField) commonFieldForName(name);
    }
    LOGI("@%s: name:%s, field %d", __FUNCTION__, name, mCurrentDataField);
    return;
}

/**
 * the callback function of the libexpat for handling of one element start
 *
 * When it comes to the start of one element. This function will be called.
 *
 * \param userData: the pointer we set by the function XML_SetUserData.
 * \param name: the element's name.
 */
void PSLConfParser::startElement(void *userData, const char *name, const char **atts)
{
    PSLConfParser *parser = (PSLConfParser *)userData;

    if (parser->mCurrentDataField == FIELD_INVALID) {
        parser->checkField(name, atts);
        return;
    }
    /**
     * Skip the RKISP1 specific values (in the range between _INVALID and
     * MEDIACTL_CONFIG_RKISP1) if profile is not in use
     */
    if (!parser->mUseProfile &&
         parser->mCurrentDataField > FIELD_INVALID &&
         parser->mCurrentDataField <= FIELD_MEDIACTL_CONFIG_RKISP1) {
        return;
    }
    LOGD("@%s: name:%s, for sensor %d", __FUNCTION__, name, parser->mSensorIndex);

    switch (parser->mCurrentDataField) {
        case FIELD_HAL_TUNING_RKISP1:
            if (parser->mUseProfile)
                parser->handleHALTuning(name, atts);
            break;
        case FIELD_SENSOR_INFO_RKISP1:
            if (parser->mUseProfile)
                parser->handleSensorInfo(name, atts);
            break;
        case FIELD_MEDIACTL_ELEMENTS_RKISP1:
            if (parser->mUseProfile)
                parser->handleMediaCtlElements(name, atts);
            break;
        default:
            if(parser->isCommonSection(parser->mCurrentDataField)) {
                parser->handleCommonSection(parser->mCurrentDataField,
                                                parser->mSensorIndex,
                                                name, atts);
            } else {
                LOGE("@%s, line:%d, go to default handling",
                        __FUNCTION__, __LINE__);
            }
            break;
    }
}

/**
 * the callback function of the libexpat for handling of one element end
 *
 * When it comes to the end of one element. This function will be called.
 *
 * \param userData: the pointer we set by the function XML_SetUserData.
 * \param name: the element's name.
 */
void PSLConfParser::endElement(void *userData, const char *name)
{
    PSLConfParser *parser = (PSLConfParser *)userData;

    if (!strcmp(name, "Profiles")) {
        parser->mUseProfile = false;
        parser->mCurrentDataField = FIELD_INVALID;
    } else if (!strcmp(name, "Hal_tuning_RKISP1")
             || !strcmp(name, "Sensor_info_RKISP1")
             || !strcmp(name, "MediaCtl_elements_RKISP1") ) {
        parser->mCurrentDataField = FIELD_INVALID;
    } else if(parser->isCommonSection(name)) {
        parser->mCurrentDataField = FIELD_INVALID;
    }
    return;
}

/**
 * Get camera configuration from xml file
 *
 * The camera setting is stored inside this RKISP1CameraCapInfo class.
 *
 */
void PSLConfParser::getPSLDataFromXmlFile(void)
{
    int done;
    void *pBuf = nullptr;
    FILE *fp = nullptr;

    fp = ::fopen(mXmlFileName.c_str(), "r");
    if (nullptr == fp) {
        LOGE("@%s, line:%d, fp is nullptr", __FUNCTION__, __LINE__);
        return;
    }

    XML_Parser parser = ::XML_ParserCreate(nullptr);
    if (nullptr == parser) {
        LOGE("@%s, line:%d, parser is nullptr", __FUNCTION__, __LINE__);
        goto exit;
    }

    PlatformData::getCameraHWInfo()->getMediaCtlElementNames(mElementNames);
    ::XML_SetUserData(parser, this);
    ::XML_SetElementHandler(parser, startElement, endElement);

    pBuf = malloc(mBufSize);
    if (nullptr == pBuf) {
        LOGE("@%s, line:%d, pBuf is nullptr", __func__, __LINE__);
        goto exit;
    }

    do {
        int len = (int)::fread(pBuf, 1, mBufSize, fp);
        if (!len) {
            if (ferror(fp)) {
                clearerr(fp);
                goto exit;
            }
        }
        done = len < mBufSize;
        if (XML_Parse(parser, (const char *)pBuf, len, done) == XML_STATUS_ERROR) {
            LOGE("@%s, line:%d, XML_Parse error", __func__, __LINE__);
            goto exit;
        }
    } while (!done);

exit:
    if (parser)
        ::XML_ParserFree(parser);
    if (pBuf)
        free(pBuf);
    if (fp)
        ::fclose(fp);
}

/*
 * Helper function for converting string to int for the
 * stream formats pixel setting.
 * Android specific pixel format, requested by the application
 */
int PSLConfParser::getStreamFormatAsValue(const char* format)
{
    /* Linear color formats */
    if (!strcmp(format, "HAL_PIXEL_FORMAT_RGBA_8888")) {
        return HAL_PIXEL_FORMAT_RGBA_8888;
    } else if (!strcmp(format, "HAL_PIXEL_FORMAT_RGBX_8888")) {
        return HAL_PIXEL_FORMAT_RGBX_8888;
    } else if (!strcmp(format, "HAL_PIXEL_FORMAT_RGB_888")) {
        return HAL_PIXEL_FORMAT_RGB_888;
    } else if (!strcmp(format, "HAL_PIXEL_FORMAT_RGB_565")) {
        return HAL_PIXEL_FORMAT_RGB_565;
    } else if (!strcmp(format, "HAL_PIXEL_FORMAT_BGRA_8888")) {
        return HAL_PIXEL_FORMAT_BGRA_8888;
    /* YUV format */
    } else if (!strcmp(format, "HAL_PIXEL_FORMAT_YV12")) {
        return HAL_PIXEL_FORMAT_YV12;
    /* Y8 - Y16*/
    } else if (!strcmp(format, "HAL_PIXEL_FORMAT_Y8")) {
        return HAL_PIXEL_FORMAT_Y8;
    } else if (!strcmp(format, "HAL_PIXEL_FORMAT_Y16")) {
        return HAL_PIXEL_FORMAT_Y16;
    /* OTHERS */
    } else if (!strcmp(format, "HAL_PIXEL_FORMAT_RAW_SENSOR")) {
        return HAL_PIXEL_FORMAT_RAW16;
    } else if (!strcmp(format, "HAL_PIXEL_FORMAT_BLOB")) {
        return HAL_PIXEL_FORMAT_BLOB;
    } else if (!strcmp(format, "HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED")) {
        return HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
    /* YCbCr pixel format*/
    } else if (!strcmp(format, "HAL_PIXEL_FORMAT_YCbCr_420_888")) {
        return HAL_PIXEL_FORMAT_YCbCr_420_888;
    } else if (!strcmp(format, "HAL_PIXEL_FORMAT_YCbCr_422_SP")) {
        return HAL_PIXEL_FORMAT_YCbCr_422_SP;
    } else if (!strcmp(format, "HAL_PIXEL_FORMAT_YCrCb_420_SP")) {
        return HAL_PIXEL_FORMAT_YCrCb_420_SP;
    } else if (!strcmp(format, "HAL_PIXEL_FORMAT_YCbCr_422_I")) {
        return HAL_PIXEL_FORMAT_YCbCr_422_I;
    } else {
        LOGE("%s, Unknown Stream Format (%s)", __FUNCTION__, format);
        return -1;
    }
}

/*
 * Helper function for converting string to int for the
 * v4l2 command.
 * selection target defines what action we do on selection
 */
int PSLConfParser::getSelectionTargetAsValue(const char* target)
{
    if (!strcmp(target, "V4L2_SEL_TGT_CROP")) {
        return V4L2_SEL_TGT_CROP;
    } else if (!strcmp(target, "V4L2_SEL_TGT_CROP_DEFAULT")) {
        return V4L2_SEL_TGT_CROP_DEFAULT;
    } else if (!strcmp(target, "V4L2_SEL_TGT_CROP_BOUNDS")) {
        return V4L2_SEL_TGT_CROP_BOUNDS;
    } else if (!strcmp(target, "V4L2_SEL_TGT_COMPOSE")) {
        return V4L2_SEL_TGT_COMPOSE;
    } else if (!strcmp(target, "V4L2_SEL_TGT_COMPOSE_DEFAULT")) {
        return V4L2_SEL_TGT_COMPOSE_DEFAULT;
    } else if (!strcmp(target, "V4L2_SEL_TGT_COMPOSE_BOUNDS")) {
        return V4L2_SEL_TGT_COMPOSE_BOUNDS;
    } else if (!strcmp(target, "V4L2_SEL_TGT_COMPOSE_PADDED")) {
        return V4L2_SEL_TGT_COMPOSE_PADDED;
    } else {
        LOGE("%s, Unknown V4L2 Selection Target (%s)", __FUNCTION__, target);
        return -1;
    }
}

/*
 * Helper function for converting string to int for the
 * v4l2 command.
 */
int PSLConfParser::getControlIdAsValue(const char* format)
{
    if (!strcmp(format, "V4L2_CID_LINK_FREQ")) {
        return V4L2_CID_LINK_FREQ;
    } else if (!strcmp(format, "V4L2_CID_VBLANK")) {
        return V4L2_CID_VBLANK;
    } else if (!strcmp(format, "V4L2_CID_HBLANK")) {
        return V4L2_CID_HBLANK;
    } else if (!strcmp(format, "V4L2_CID_EXPOSURE")) {
        return V4L2_CID_EXPOSURE;
    } else if (!strcmp(format, "V4L2_CID_ANALOGUE_GAIN")) {
        return V4L2_CID_ANALOGUE_GAIN;
    } else if (!strcmp(format, "V4L2_CID_HFLIP")) {
        return V4L2_CID_HFLIP;
    } else if (!strcmp(format, "V4L2_CID_VFLIP")) {
        return V4L2_CID_VFLIP;
    } else if (!strcmp(format, "V4L2_CID_TEST_PATTERN")) {
        return V4L2_CID_TEST_PATTERN;
    } else {
        LOGE("%s, Unknown V4L2 ControlID (%s)", __FUNCTION__, format);
        return -1;
    }
}

/*
 * Helper function for converting string to int for the
 * ISYS node name.
 */
int PSLConfParser::getIsysNodeNameAsValue(const char* isysNodeName)
{
    if (!strcmp(isysNodeName, "ISYS_NODE_RAW")) {
        return ISYS_NODE_RAW;
    } else {
        LOGE("Unknown ISYS node name (%s)", isysNodeName);
        return IMGU_NODE_NULL;
    }
}

/**
 * The function reads a binary file containing NVM data from sysfs. NVM data is
 * camera module calibration data which is written into the camera module in
 * production line, and at runtime read by the driver and written into sysfs.
 * The data is in the format in which the module manufacturer has provided it in.
 */
int PSLConfParser::readNvmData()
{
    LOGD("@%s", __FUNCTION__);
    std::string sensorName;
    std::string nvmDirectory;
    ia_binary_data nvmData;
    FILE *nvmFile;
    std::string nvmDataPath(NVM_DATA_PATH);

    RKISP1CameraCapInfo *info = static_cast<RKISP1CameraCapInfo*>(mCaps[mSensorIndex]);
    if (info == nullptr) {
        LOGE("Could not get Camera capability info");
        return UNKNOWN_ERROR;
    }

    sensorName = info->getSensorName();
    nvmDirectory = info->getNvmDirectory();

    if (nvmDirectory.length() == 0) {
        LOGW("NVM dirctory from config is null");
        return UNKNOWN_ERROR;
    }

    nvmData.size = 0;
    nvmData.data = nullptr;
    //check separator of path name
    if (nvmDataPath.back() != '/')
        nvmDataPath.append("/");

    nvmDataPath.append(nvmDirectory);
    //check separator of path name
    if (nvmDataPath.back() != '/')
        nvmDataPath.append("/");

    nvmDataPath.append("eeprom");
    LOGI("NVM data for %s is located in %s", sensorName.c_str(), nvmDataPath.c_str());

    nvmFile = fopen(nvmDataPath.c_str(), "rb");
    if (!nvmFile) {
        LOGE("Failed to open NVM file: %s", nvmDataPath.c_str());
        return UNKNOWN_ERROR;
    }

    fseek(nvmFile, 0, SEEK_END);
    nvmData.size = ftell(nvmFile);
    fseek(nvmFile, 0, SEEK_SET);

    nvmData.data = ::operator new(nvmData.size);

    LOGI("NVM file size: %d bytes", nvmData.size);
    int ret = fread(nvmData.data, nvmData.size, 1, nvmFile);
    if (ret == 0) {
        LOGE("Cannot read nvm data");
        ::operator delete(nvmData.data);
        fclose(nvmFile);
        return UNKNOWN_ERROR;
    }
    fclose(nvmFile);
    info->mNvmData = nvmData;
    return OK;
}

std::string PSLConfParser::getSensorMediaDevice(int cameraId)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    const RKISP1CameraCapInfo *cap = getRKISP1CameraCapInfo(cameraId);
    CheckError(!cap, "none", "@%s, failed to get RKISP1CameraCapInfo", __FUNCTION__);
    string sensorName = cap->getSensorName();
    string moduleIdStr = cap->mModuleIndexStr;

    const std::vector<struct SensorDriverDescriptor>& sensorInfo = PlatformData::getCameraHWInfo()->mSensorInfo;
    for (auto it = sensorInfo.begin(); it != sensorInfo.end(); ++it) {
        if((*it).mSensorName == sensorName &&
           (*it).mModuleIndexStr == moduleIdStr)
            return (*it).mParentMediaDev;
    }
    LOGE("@%s : Can't get SensorMediaDevice, cameraId: %d, sensorName:%s", __FUNCTION__,
         cameraId, sensorName.c_str());
    return "none";
}

std::string PSLConfParser::getImguMediaDevice(int cameraId)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    return getSensorMediaDevice(cameraId);
}

std::vector<std::string> PSLConfParser::getSensorMediaDevicePath()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    std::vector<std::string> mediaDevicePaths;
    std::vector<std::string> mediaDevicePath;
    std::vector<std::string> mediaDeviceNames {"rkisp1", "rkcif"};
    for (auto it : mediaDeviceNames) {
       mediaDevicePath = getMediaDeviceByName(it);
       mediaDevicePaths.insert(mediaDevicePaths.end(), mediaDevicePath.begin(), mediaDevicePath.end());
    }

    return mediaDevicePaths;
}

std::vector<std::string> PSLConfParser::getMediaDeviceByName(std::string driverName)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    LOGI("@%s, Target name: %s", __FUNCTION__, driverName.c_str());
    const char *MEDIADEVICES = "media";
    const char *DEVICE_PATH = "/dev/";

    std::vector<std::string> mediaDevicePath;
    DIR *dir;
    dirent *dirEnt;

    std::vector<std::string> candidates;

    candidates.clear();
    if ((dir = opendir(DEVICE_PATH)) != nullptr) {
        while ((dirEnt = readdir(dir)) != nullptr) {
            std::string candidatePath = dirEnt->d_name;
            std::size_t pos = candidatePath.find(MEDIADEVICES);
            if (pos != std::string::npos) {
                LOGD("Found media device candidate: %s", candidatePath.c_str());
                std::string found_one = DEVICE_PATH;
                found_one += candidatePath;
                candidates.push_back(found_one);
            }
        }
        closedir(dir);
    } else {
        LOGW("Failed to open directory: %s", DEVICE_PATH);
    }

    status_t retVal = NO_ERROR;
    // let media0 place before media1
    std::sort(candidates.begin(), candidates.end());
    for (const auto &candidate : candidates) {
        MediaController controller(candidate.c_str());
        retVal = controller.init();

        // We may run into devices that this HAL won't use -> skip to next
        if (retVal == PERMISSION_DENIED) {
            LOGD("Not enough permissions to access %s.", candidate.c_str());
            continue;
        }

        media_device_info info;
        int ret = controller.getMediaDevInfo(info);
        if (ret != OK) {
            LOGE("Cannot get media device information.");
            return mediaDevicePath;
        }

        if (strncmp(info.driver, driverName.c_str(),
                    MIN(sizeof(info.driver),
                    driverName.size())) == 0) {
            LOGD("Found device that matches: %s", driverName.c_str());
            //mediaDevicePath += candidate;
            mediaDevicePath.push_back(candidate);
        }
    }

    return mediaDevicePath;
}

void PSLConfParser::dumpHalTuningSection(int cameraId)
{
    LOGD("@%s", __FUNCTION__);

    RKISP1CameraCapInfo * info = static_cast<RKISP1CameraCapInfo*>(mCaps[cameraId]);

    LOGD("element name: flipping, element value = %d", info->mSensorFlipping);
}

void PSLConfParser::dumpSensorInfoSection(int cameraId){
    LOGD("@%s", __FUNCTION__);

    RKISP1CameraCapInfo * info = static_cast<RKISP1CameraCapInfo*>(mCaps[cameraId]);

    LOGD("element name: sensorType, element value = %d", info->mSensorType);
    LOGD("element name: gain.lag, element value = %d", info->mGainLag);
    LOGD("element name: exposure.lag, element value = %d", info->mExposureLag);
    LOGD("element name: fov, element value = %f, %f", info->mFov[0], info->mFov[1]);
    LOGD("element name: statistics.initialSkip, element value = %d", info->mStatisticsInitialSkip);
    LOGD("element name: testPattern.bayerFormat, element value = %s", info->mTestPatternBayerFormat.c_str());
}

void PSLConfParser::dumpMediaCtlElementsSection(int cameraId){
    LOGD("@%s", __FUNCTION__);

    unsigned int numidx;

    RKISP1CameraCapInfo * info = static_cast<RKISP1CameraCapInfo*>(mCaps[cameraId]);
    const MediaCtlElement *currentElement;
    for (numidx = 0; numidx < info->mMediaCtlElements.size(); numidx++) {
        currentElement = &info->mMediaCtlElements[numidx];
        LOGD("MediaCtl element name=%s ,type=%s, isysNodeName=%d"
             ,currentElement->name.c_str(),
             currentElement->type.c_str(),
             currentElement->isysNodeName);
    }
}

// To be modified when new elements or sections are added
// Use LOGD for traces to be visible
void PSLConfParser::dump()
{
    LOGD("===========================@%s======================", __FUNCTION__);
    for (unsigned int i = 0; i < mCaps.size(); i++) {
        dumpHalTuningSection(i);
        dumpSensorInfoSection(i);
        dumpMediaCtlElementsSection(i);
    }

    LOGD("===========================end======================");
}

} // namespace camera2
} // namespace android
