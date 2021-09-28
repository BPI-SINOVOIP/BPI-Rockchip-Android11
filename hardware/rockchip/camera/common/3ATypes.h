/*
 * Copyright (C) 2012-2017 Intel Corporation
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

#ifndef _HAL_3A_TYPES_H_
#define _HAL_3A_TYPES_H_

#define EV_LOWER_BOUND         (-100)
#define EV_UPPER_BOUND          100

#define GAMMA_LUT_LOWER_BOUND  0
#define GAMMA_LUT_UPPER_BOUND  255

NAMESPACE_DECLARATION {

enum AeMode
{
    CAM_AE_MODE_NOT_SET = -1,
    CAM_AE_MODE_AUTO,
    CAM_AE_MODE_MANUAL,
    CAM_AE_MODE_AUTO_FLASH,
    CAM_AE_MODE_ALWAYS_FLASH,
    CAM_AE_MODE_AUTO_FLASH_REDEYE,
    CAM_AE_MODE_SHUTTER_PRIORITY,
    CAM_AE_MODE_APERTURE_PRIORITY
};

enum AeLock
{
    CAM_AE_LOCK_OFF = 0,
    CAM_AE_LOCK_ON
};

enum AwbLock
{
    CAM_AWB_LOCK_OFF = 0,
    CAM_AWB_LOCK_ON
};

enum BlackLevelLock
{
    CAM_BLACK_LEVEL_LOCK_OFF = 0,
    CAM_BLACK_LEVEL_LOCK_ON
};

enum AwbMode
{
    CAM_AWB_MODE_NOT_SET = -1,
    CAM_AWB_MODE_OFF,
    CAM_AWB_MODE_AUTO,
    CAM_AWB_MODE_MANUAL_INPUT,
    CAM_AWB_MODE_DAYLIGHT,
    CAM_AWB_MODE_SUNSET,
    CAM_AWB_MODE_CLOUDY,
    CAM_AWB_MODE_TUNGSTEN,
    CAM_AWB_MODE_FLUORESCENT,
    CAM_AWB_MODE_WARM_FLUORESCENT,
    CAM_AWB_MODE_SHADOW,
    CAM_AWB_MODE_WARM_INCANDESCENT
};

enum ColorCorrectMode
{
    CAM_COLOR_CORRECT_MODE_NOT_SET = -1,
    CAM_COLOR_CORRECT_MODE_TRANSFORM_MATRIX,
    CAM_COLOR_CORRECT_MODE_FAST,
    CAM_COLOR_CORRECT_MODE_HIGH_QUALITY
};

enum ColorCorrectAberrationMode
{
    CAM_COLOR_CORRECT_ABERRATION_OFF = 0,
    CAM_COLOR_CORRECT_ABERRATION_ON,
};

enum MeteringMode
{
    CAM_AE_METERING_MODE_NOT_SET = -1,
    CAM_AE_METERING_MODE_AUTO,
    CAM_AE_METERING_MODE_SPOT,
    CAM_AE_METERING_MODE_CENTER,
    CAM_AE_METERING_MODE_CUSTOMIZED
};

/** add Iso mode*/
/* ISO control mode setting */
enum IsoMode {
    CAM_AE_ISO_MODE_NOT_SET = -1,
    CAM_AE_ISO_MODE_AUTO,   /* Automatic */
    CAM_AE_ISO_MODE_MANUAL  /* Manual */
};

struct SensorAeConfig
{
    float evBias;
    int expTime;
    short unsigned int aperture_num;
    short unsigned int aperture_denum;
    short unsigned int fn_num;
    short unsigned int fn_denum;
    int aecApexTv;
    int aecApexSv;
    int aecApexAv;
    float digitalGain;
    float totalGain;
};

enum FlashMode
{
    CAM_AE_FLASH_MODE_NOT_SET = -1,
    CAM_AE_FLASH_MODE_AUTO,
    CAM_AE_FLASH_MODE_OFF,
    CAM_AE_FLASH_MODE_ON,
    CAM_AE_FLASH_MODE_DAY_SYNC,
    CAM_AE_FLASH_MODE_SLOW_SYNC,
    CAM_AE_FLASH_MODE_TORCH,
    CAM_AE_FLASH_MODE_SINGLE
};

enum FlashStage
{
    CAM_FLASH_STAGE_NOT_SET = -1,
    CAM_FLASH_STAGE_NONE,
    CAM_FLASH_STAGE_PRE,
    CAM_FLASH_STAGE_MAIN
};

enum FlickerMode
{
    CAM_AE_FLICKER_MODE_NOT_SET = -1,
    CAM_AE_FLICKER_MODE_OFF,
    CAM_AE_FLICKER_MODE_50HZ,
    CAM_AE_FLICKER_MODE_60HZ,
    CAM_AE_FLICKER_MODE_AUTO
};

struct AAAWindowInfo {
    unsigned width;
    unsigned height;
};

/**
 *  \brief Bundles binary data pointer with size.
 */
typedef struct
{
    void        *data;
    unsigned int size;
} ia_binary_data; /* ia_binay_data owns data */

/** Coordinate, used in red-eye correction. */
typedef struct {
    int x;
    int y;
} ia_coordinate;

} NAMESPACE_DECLARATION_END

#endif // _HAL_3A_TYPES_H_
