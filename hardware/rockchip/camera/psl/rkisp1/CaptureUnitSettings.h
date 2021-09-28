/*
 * Copyright (C) 2015-2017 Intel Corporation
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

#ifndef CAMERA3_HAL_CAPTUREUNITSETTINGS_H_
#define CAMERA3_HAL_CAPTUREUNITSETTINGS_H_

#include "CameraWindow.h"
#include "AndroidIspControls.h"
#include "RKISP1Common.h"
#include "CommonUtilMacros.h"

namespace android {
namespace camera2 {


/**
 * This struct groups all of the COM_RK_IMAGE_ENHANCE settings.
 * They are Rockchip specific and thus not part of the Google Properties.
 */
struct ImageEnhancementSettings {
    char manualBrightness;
    char manualContrast;
    char manualHue;
    char manualSaturation;
    char manualSharpness;
};

struct IspSettings {
    /* ia_isp_feature_setting nrSetting; */
    /* ia_isp_feature_setting eeSetting; */
    /* ia_isp_effect effects; */
    ImageEnhancementSettings manualSettings;
    IspSettings() { CLEAR(*this); }
};

/**
 * \struct CaptureUnitSettings
 *
 * Contains all the settings the capture unit needs to know about a particular
 * capture.
 * This is mainly the results from AIQ (3A + AIC) algorithms.
 * But there may be other settings that are needed. Or the 3A results need to be
 * overridden by user commands. In this case the setting that the HW will
 * receive are stored here, outside the 3A result structure.
 * These settings are sent through CaptureUnit to sensor, lens and ISP.
 */
struct CaptureUnitSettings {

    CameraWindow cropRegion; /**< Crop region in ANDROID_COORDINATES */
    CameraWindow aeRegion;   /**< AE region in ANDROID_COORDINATES */
    uint8_t videoStabilizationMode; /**< ANDROID_CONTROL_VIDEO_STABILIZATION_MODE */
    uint8_t opticalStabilizationMode; /**< ANDROID_LENS_OPTICAL_STABILIZATION_MODE */
    uint8_t tonemapMode;        /**< ANDROID_TONEMAP_MODE */
    uint8_t shadingMode;        /**< ANDROID_SHADING_MODE */
    uint8_t shadingMapMode;     /**< ANDROID_STATISTICS_LENS_SHADING_MAP_MODE */
    uint8_t hotPixelMode;       /**< ANDROID_HOT_PIXEL_MODE */
    uint8_t hotPixelMapMode;    /**< ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE */
    uint8_t controlMode;        /**< ANDROID_CONTROL_MODE */
    uint8_t controlAeMode;      /**< ANDROID_CONTROL_AE_MODE */
    uint8_t presetCurve;        /**< ANDROID_TONEMAP_PRESET_CURVE */
    float gammaValue;           /**< ANDROID_TONEMAP_GAMMA */
    int32_t testPatternMode;    /**< ANDROID_SENSOR_TEST_PATTERN_MODE */

    bool flashFired; /**< 'true' if flash was succesfully lit for the capture */
    bool torchAsked; /**< 'true' if client asks for torch */

    bool dump; /**< 'true' if (PAL) dump needs to be done */

    /*
     * ispControl is just a struct to keep the values found in the request
     * metadata settings and not having to search again.
     * ispSettings are the struct where we prepare those controls in a format
     * that the ISP adaptation layer will understand (ia_isp_bxt)
     */
    IspSettings         ispSettings; /**< settings ready for ia_isp_bxt */
    AndroidIspControls  ispControls; /**< keep original control values */

    MakernoteData makernote;     /**< Makernote info TODO: use in ProcessingUnitSettings */

    /**
     * Synchronization control
     */
    uint32_t inEffectFrom; /**< exposure id where the exposure settings
                                are effective */

    int64_t timestamp; /**< android capture timestamp. */

    int64_t settingsIdentifier; /**< Identifier for the settings instance. Will
                                     grow for every new settings. */
    CaptureUnitSettings() :
        videoStabilizationMode(0),
        opticalStabilizationMode(0),
        tonemapMode(0),
        shadingMode(0),
        shadingMapMode(0),
        hotPixelMode(0),
        hotPixelMapMode(0),
        controlMode(0),
        controlAeMode(0),
        presetCurve(0),
        gammaValue(0.0),
        testPatternMode(0),
        flashFired(false),
        torchAsked(false),
        dump(false),
        inEffectFrom(UINT32_MAX),
        timestamp(0),
        settingsIdentifier(0) {
        CLEAR(makernote);
    }
};

} // namespace camera2
} // namespace android


#endif /* CAMERA3_HAL_CAPTUREUNITSETTINGS_H_ */
