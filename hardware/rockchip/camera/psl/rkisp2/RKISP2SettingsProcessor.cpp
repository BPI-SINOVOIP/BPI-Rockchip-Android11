/*
 * Copyright (C) 2017 Intel Corporation.
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

#define LOG_TAG "RKISP2SettingsProcessor"

#include "RKISP2SettingsProcessor.h"
#include "CameraMetadataHelper.h"
#include "PlatformData.h"

#include "LogHelper.h"

namespace android {
namespace camera2 {
namespace rkisp2 {

RKISP2SettingsProcessor::RKISP2SettingsProcessor(int cameraId) :
        mCameraId(cameraId)
{
    /**
     * cache some static value for later use
     */
    mAPA = PlatformData::getActivePixelArray(mCameraId);
    cacheStaticMetadata();
}

RKISP2SettingsProcessor::~RKISP2SettingsProcessor()
{
}

status_t RKISP2SettingsProcessor::init()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    // save state for fix focus;
    /* TODO */
    /* mFixedFocus = (m3aWrapper->getMinFocusDistance() == 0.0f); */

    return OK;
}

/**
 * processRequestSettings
 *
 * Analyze the request control metadata tags and prepare the configuration for
 * the AIQ algorithm to run.
 * \param settings [IN] settings from the request
 * \param reqAiqCfg [OUT] AIQ configuration
 */
status_t
RKISP2SettingsProcessor::processRequestSettings(const CameraMetadata &settings,
                                    RKISP2RequestCtrlState &reqAiqCfg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = OK;

    /**
     *  Process cropping first since it is used by other settings
     *  like AE
     **/
    processCroppingRegion(settings, reqAiqCfg);
    return status;
}

/**
 * processCroppingRegion
 *
 * Checks if cropping region is set in the capture request settings. If it is
 * then fills the corresponding region in the capture settings.
 * if it is not it sets the default value, the Active Pixel Array
 *
 * \param settings[IN] metadata buffer where the settings are stored
 * \param reqCfg[OUT] the cropping region is stored inside the capture settings
 *                    of this structure
 *
 */
void
RKISP2SettingsProcessor::processCroppingRegion(const CameraMetadata &settings,
                                   RKISP2RequestCtrlState &reqCfg)
{
    CameraWindow &cropRegion = reqCfg.captureSettings->cropRegion;

    // If crop region not available, fill active array size as the default value
    //# ANDROID_METADATA_Control android.scaler.cropRegion done
    camera_metadata_ro_entry entry = settings.find(ANDROID_SCALER_CROP_REGION);
    /**
     * Cropping region is invalid if width is 0 or if the rectangle is not
     * fully defined (you need 4 values)
     */
    //# ANDROID_METADATA_Dynamic android.scaler.cropRegion done
    if (entry.count < 4 || entry.data.i32[2] == 0) {
        //cropRegion is a reference and will alter captureSettings->cropRegion.
        cropRegion = mAPA;
        int32_t *cropWindow;
        ia_coordinate topLeft = {0, 0};
        cropRegion.init(topLeft,
                         mAPA.width(),  //width
                         mAPA.height(),  //height
                         0);
        // meteringRectangle is filling 4 coordinates and weight (5 values)
        // here crop region only needs the rectangle, so we copy only 4.
        cropWindow = (int32_t*)mAPA.meteringRectangle();
        reqCfg.ctrlUnitResult->update(ANDROID_SCALER_CROP_REGION, cropWindow, 4);
    } else {
        ia_coordinate topLeft = {entry.data.i32[0],entry.data.i32[1]};
        cropRegion.init(topLeft,
                        entry.data.i32[2],  //width
                        entry.data.i32[3],  //height
                        0);
        reqCfg.ctrlUnitResult->update(ANDROID_SCALER_CROP_REGION, entry.data.i32, 4);
    }

    // copy the crop region to the processingSettings so that tasks don't have
    // to break the Law-Of-Demeter.
    reqCfg.processingSettings->cropRegion = cropRegion;
}

void RKISP2SettingsProcessor::cacheStaticMetadata()
{
    const camera_metadata_t *meta = PlatformData::getStaticMetadata(mCameraId);
    mStaticMetadataCache.availableEffectModes =
            MetadataHelper::getMetadataEntry(meta, ANDROID_CONTROL_AVAILABLE_EFFECTS);
    mStaticMetadataCache.availableNoiseReductionModes =
            MetadataHelper::getMetadataEntry(meta, ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES);
    mStaticMetadataCache.availableTonemapModes =
            MetadataHelper::getMetadataEntry(meta, ANDROID_TONEMAP_AVAILABLE_TONE_MAP_MODES);
    mStaticMetadataCache.availableVideoStabilization =
            MetadataHelper::getMetadataEntry(meta, ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES);
    mStaticMetadataCache.availableOpticalStabilization =
            MetadataHelper::getMetadataEntry(meta, ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION);
    mStaticMetadataCache.currentAperture =
            MetadataHelper::getMetadataEntry(meta, ANDROID_LENS_INFO_AVAILABLE_APERTURES);
    mStaticMetadataCache.flashInfoAvailable =
            MetadataHelper::getMetadataEntry(meta, ANDROID_FLASH_INFO_AVAILABLE);
    mStaticMetadataCache.lensShadingMapSize =
            MetadataHelper::getMetadataEntry(meta, ANDROID_LENS_INFO_SHADING_MAP_SIZE);
    mStaticMetadataCache.currentFocalLength =
            MetadataHelper::getMetadataEntry(meta, ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS);
    mStaticMetadataCache.availableHotPixelMapModes =
            MetadataHelper::getMetadataEntry(meta, ANDROID_STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES);
    mStaticMetadataCache.availableHotPixelModes =
            MetadataHelper::getMetadataEntry(meta, ANDROID_HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES);
    mStaticMetadataCache.availableEdgeModes =
            MetadataHelper::getMetadataEntry(meta, ANDROID_EDGE_AVAILABLE_EDGE_MODES);
    mStaticMetadataCache.maxAnalogSensitivity =
            MetadataHelper::getMetadataEntry(meta, ANDROID_SENSOR_MAX_ANALOG_SENSITIVITY);
    mStaticMetadataCache.pipelineDepth =
            MetadataHelper::getMetadataEntry(meta, ANDROID_REQUEST_PIPELINE_MAX_DEPTH);
    mStaticMetadataCache.lensSupported =
            MetadataHelper::getMetadataEntry(meta, ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE);
    mStaticMetadataCache.availableTestPatternModes =
            MetadataHelper::getMetadataEntry(meta, ANDROID_SENSOR_AVAILABLE_TEST_PATTERN_MODES);
}

} /* namespace rkisp2 */
} /* namespace camera2 */
} /* namespace android */
