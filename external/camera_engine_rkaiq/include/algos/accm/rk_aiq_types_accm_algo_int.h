/*
 * rk_aiq_types_accm_int.h
 *
 *  Copyright (c) 2019 Rockchip Corporation
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
 *
 */

#ifndef _RK_AIQ_TYPES_ACCM_ALGO_INT_H_
#define _RK_AIQ_TYPES_ACCM_ALGO_INT_H_
#include "RkAiqCalibDbTypes.h"
#include "RkAiqCalibDbTypesV2.h"

#include "accm/rk_aiq_types_accm_algo.h"

RKAIQ_BEGIN_DECLARE

#define RK_AIQ_ACCM_COLOR_GAIN_NUM 4

typedef struct accm_sw_info_s {
    float sensorGain;
    float awbGain[2];
    float awbIIRDampCoef;
    float varianceLuma;
    bool grayMode;
    bool awbConverged;
    int prepare_type;
    bool ccmConverged;
} accm_sw_info_t;

typedef struct rk_aiq_ccm_mccm_attrib_s {
    // M4_ARRAY_DESC("matrix", "f32", M4_SIZE(3,3), M4_RANGE(-8,7.992), "[1.0000,0.0000,0.0000,0.0000,1.0000,0.0000,0.0000,0.0000,1.0000]", M4_DIGIT(4), M4_DYNAMIC(0))
    float  matrix[9];
    // M4_ARRAY_DESC("offs", "f32", M4_SIZE(1,3), M4_RANGE(-4095,4095), "0", M4_DIGIT(1), M4_DYNAMIC(0))
    float  offs[3];
    float  alp_y[CCM_CURVE_DOT_NUM];
    float bound_bit;
} rk_aiq_ccm_mccm_attrib_t;

typedef struct rk_aiq_ccm_color_inhibition_s {
    float sensorGain[RK_AIQ_ACCM_COLOR_GAIN_NUM];
    float level[RK_AIQ_ACCM_COLOR_GAIN_NUM];//max value 100,default value 0
} rk_aiq_ccm_color_inhibition_t;

typedef struct rk_aiq_ccm_color_saturation_s {
    float sensorGain[RK_AIQ_ACCM_COLOR_GAIN_NUM];
    float level[RK_AIQ_ACCM_COLOR_GAIN_NUM];//max value 100, default value 100
} rk_aiq_ccm_color_saturation_t;

typedef struct rk_aiq_ccm_accm_attrib_s {
    rk_aiq_ccm_color_inhibition_t color_inhibition;
    rk_aiq_ccm_color_saturation_t color_saturation;
} rk_aiq_ccm_accm_attrib_t;

typedef enum rk_aiq_ccm_op_mode_s {
    RK_AIQ_CCM_MODE_INVALID                     = 0,        /**< initialization value */
    RK_AIQ_CCM_MODE_MANUAL                      = 1,        /**< run manual lens shading correction */
    RK_AIQ_CCM_MODE_AUTO                        = 2,        /**< run auto lens shading correction */
    RK_AIQ_CCM_MODE_TOOL                        = 3,        /**< config from stTool  */
    RK_AIQ_CCM_MODE_MAX
} rk_aiq_ccm_op_mode_t;

typedef struct rk_aiq_ccm_attrib_s {
    bool byPass;
    // M4_ENUM_DESC("mode", "rk_aiq_ccm_op_mode_t", "RK_AIQ_CCM_MODE_AUTO");
    rk_aiq_ccm_op_mode_t mode;
    // M4_STRUCT_DESC("stManual", "normal_ui_style")
    rk_aiq_ccm_mccm_attrib_t stManual;
    rk_aiq_ccm_accm_attrib_t stAuto;
    // to do whm
    CalibDbV2_Ccm_Para_V2_t stTool;
} rk_aiq_ccm_attrib_t;

typedef struct rk_aiq_ccm_querry_info_s {
    bool ccm_en;
    // M4_ARRAY_DESC("matrix", "f32", M4_SIZE(3,3), M4_RANGE(-8,7.992), "[1.0000,0.0000,0.0000,0.0000,1.0000,0.0000,0.0000,0.0000,1.0000]", M4_DIGIT(4), M4_DYNAMIC(0))
    float  matrix[9];
    // M4_ARRAY_DESC("offs", "f32", M4_SIZE(1,3), M4_RANGE(-4095,4095), "0", M4_DIGIT(1), M4_DYNAMIC(0))
    float  offs[3];
    float  alp_y[CCM_CURVE_DOT_NUM];
    float bound_bit;
    float color_inhibition_level;
    float color_saturation_level;
    // M4_NUMBER_DESC("CCM Saturation", "f32", M4_RANGE(0,200), "0", M4_DIGIT(2))
    float finalSat;
    // M4_STRING_DESC("usedCcm1", M4_SIZE(1,1), M4_RANGE(0, 25), "A_100",M4_DYNAMIC(0))
    char  ccmname1[25];
    // M4_STRING_DESC("usedCcm2", M4_SIZE(1,1), M4_RANGE(0, 25), "A_100",M4_DYNAMIC(0))
    char  ccmname2[25];
    // M4_STRING_DESC("illumination", M4_SIZE(1,1), M4_RANGE(0, 20), "A",M4_DYNAMIC(0))
    char  illumination[20];
} rk_aiq_ccm_querry_info_t;

RKAIQ_END_DECLARE

#endif

