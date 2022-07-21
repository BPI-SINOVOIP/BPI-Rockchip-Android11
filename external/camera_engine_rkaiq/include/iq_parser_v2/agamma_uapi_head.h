/*
 *  Copyright (c) 2021 Rockchip Corporation
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

#ifndef __AGAMMA_UAPI_HEAD_H__
#define __AGAMMA_UAPI_HEAD_H__

#include "agamma_head.h"

typedef struct Agamma_api_Fast_s {
    // M4_BOOL_DESC("en", "1")
    bool en;
    // M4_NUMBER_DESC("GammaCoef", "f32", M4_RANGE(0,100), "2.2", M4_DIGIT(2))
    float GammaCoef;
    // M4_NUMBER_DESC("SlopeAtZero", "f32", M4_RANGE(-0.05,0.05), "0", M4_DIGIT(2))
    float SlopeAtZero;
} Agamma_api_Fast_t;

typedef struct Agamma_api_manualV21_s {
    // M4_BOOL_DESC("Gamma_en", "1")
    bool Gamma_en;
    // M4_ENUM_DESC("Gamma_out_segnum", "GammaType_t", "GAMMATYPE_LOG")
    GammaType_t Gamma_out_segnum;
    // M4_NUMBER_DESC("Gamma_out_offset", "u16", M4_RANGE(0,4095), "0", M4_DIGIT(0))
    uint16_t Gamma_out_offset;
    // M4_ARRAY_MARK_DESC("Gamma_curve", "u16", M4_SIZE(1,45),  M4_RANGE(0, 4095), "[0, 6, 11, 17, 22, 28, 33, 39, 44, 55, 66, 77, 88, 109, 130, 150, 170, 210, 248, 286, 323, 393, 460, 525, 586, 702, 809, 909, 1002, 1172, 1325, 1462, 1588, 1811, 2004, 2174, 2327, 2590, 2813, 3006, 3177, 3467, 3708, 3915, 4095.0000]", M4_DIGIT(4), M4_DYNAMIC(0), "curve_table")
    uint16_t Gamma_curve[CALIBDB_AGAMMA_KNOTS_NUM];
} Agamma_api_manualV21_t;

typedef struct Agamma_api_manualV30_s {
    // M4_BOOL_DESC("Gamma_en", "1")
    bool Gamma_en;
    // M4_NUMBER_DESC("Gamma_out_offset", "u16", M4_RANGE(0,4095), "0", M4_DIGIT(0))
    uint16_t Gamma_out_offset;
    // M4_ARRAY_MARK_DESC("Gamma_curve", "u16", M4_SIZE(1,49),  M4_RANGE(0, 4095), "[0, 93, 128, 154, 175, 194, 211, 226, 240, 266, 289, 310, 329, 365, 396, 425, 451, 499, 543, 582, 618, 684, 744, 798, 848, 938, 1019, 1093, 1161, 1285, 1396, 1498, 1592, 1761, 1914, 2052, 2181, 2414, 2622, 2813, 2989, 3153, 3308, 3454, 4593, 3727, 3854, 3977, 4095]", M4_DIGIT(0), M4_DYNAMIC(0), "curve_table")
    uint16_t Gamma_curve[CALIBDB_AGAMMA_KNOTS_NUM_V30];
} Agamma_api_manualV30_t;

#endif /*__AGAMMA_UAPI_HEAD_H__*/
