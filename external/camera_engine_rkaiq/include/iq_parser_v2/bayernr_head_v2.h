/*
 * bayernr_head_v1.h
 *
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

#ifndef __CALIBDBV2_BAYERNR_HEADER_V2_H__
#define __CALIBDBV2_BAYERNR_HEADER_V2_H__

#include "rk_aiq_comm.h"

RKAIQ_BEGIN_DECLARE

////////////////////////bayernr V2//////////////////////////////////////
typedef struct CalibDbV2_BayerNrV2_CalibPara_Setting_ISO_s {
    // M4_NUMBER_MARK_DESC("iso", "f32", M4_RANGE(50, 204800), "50", M4_DIGIT(1), "index2")
    float iso;

    // M4_ARRAY_DESC("lumapoint", "s32", M4_SIZE(1,16), M4_RANGE(0,65535), "0.0", M4_DIGIT(0), M4_DYNAMIC(0))
    int lumapoint[16];

    // M4_ARRAY_DESC("sigma", "s32", M4_SIZE(1,16), M4_RANGE(0,65535), "0.0", M4_DIGIT(0), M4_DYNAMIC(0))
    int sigma[16];

} CalibDbV2_BayerNrV2_CalibPara_Setting_ISO_t;

typedef struct CalibDbV2_BayerNrV2_CalibPara_Setting_s {
    // M4_STRING_MARK_DESC("SNR_Mode", M4_SIZE(1,1), M4_RANGE(0, 64), "LSNR",M4_DYNAMIC(0), "index1")
    char *SNR_Mode;
    // M4_STRING_DESC("Sensor_Mode", M4_SIZE(1,1), M4_RANGE(0, 64), "lcg", M4_DYNAMIC(0))
    char *Sensor_Mode;
    // M4_STRUCT_LIST_DESC("Calib_ISO", M4_SIZE_DYNAMIC, "double_index_list")
    CalibDbV2_BayerNrV2_CalibPara_Setting_ISO_t *Calib_ISO;
    int Calib_ISO_len;

} CalibDbV2_BayerNrV2_CalibPara_Setting_t;

typedef struct CalibDbV2_BayerNrV2_CalibPara_s {
    // M4_STRUCT_LIST_DESC("Setting", M4_SIZE_DYNAMIC, "double_index_list")
    CalibDbV2_BayerNrV2_CalibPara_Setting_t *Setting;
    int Setting_len;
} CalibDbV2_BayerNrV2_CalibPara_t;

typedef struct CalibDbV2_BayerNrV2_Bayernr2d_Setting_ISO_s {
    // M4_NUMBER_MARK_DESC("iso", "f32", M4_RANGE(50, 204800), "50", M4_DIGIT(1), "index2")
    float iso;

    // M4_BOOL_DESC("gauss_guide", "1")
    bool gauss_guide;

    // M4_NUMBER_DESC("filter_strength", "f32", M4_RANGE(0, 16), "0.5", M4_DIGIT(2))
    float filter_strength;

    // M4_NUMBER_DESC("edgesofts", "f32", M4_RANGE(0, 1.0), "1.0", M4_DIGIT(2))
    float edgesofts;

    // M4_NUMBER_DESC("ratio", "f32", M4_RANGE(0, 1.0), "0.2", M4_DIGIT(2))
    float ratio;

    // M4_NUMBER_DESC("weight", "f32", M4_RANGE(0, 1.0), "1.0", M4_DIGIT(2))
    float weight;

} CalibDbV2_BayerNrV2_Bayernr2d_Setting_ISO_t;

typedef struct CalibDbV2_BayerNrV2_Bayernr2d_Setting_s {
    // M4_STRING_MARK_DESC("SNR_Mode", M4_SIZE(1,1), M4_RANGE(0, 64), "LSNR",M4_DYNAMIC(0), "index1")
    char *SNR_Mode;
    // M4_STRING_DESC("Sensor_Mode", M4_SIZE(1,1), M4_RANGE(0, 64), "lcg", M4_DYNAMIC(0))
    char *Sensor_Mode;
    // M4_STRUCT_LIST_DESC("Tuning_ISO", M4_SIZE_DYNAMIC, "double_index_list")
    CalibDbV2_BayerNrV2_Bayernr2d_Setting_ISO_t *Tuning_ISO;
    int Tuning_ISO_len;
} CalibDbV2_BayerNrV2_Bayernr2d_Setting_t;

typedef struct CalibDbV2_BayerNrV2_Bayernr2d_s {
    // M4_BOOL_DESC("enable", "1")
    bool enable;
    // M4_STRUCT_LIST_DESC("Setting", M4_SIZE_DYNAMIC, "double_index_list")
    CalibDbV2_BayerNrV2_Bayernr2d_Setting_t *Setting;
    int Setting_len;
} CalibDbV2_BayerNrV2_Bayernr2d_t;

typedef struct CalibDbV2_BayerNrV2_Bayernr3d_Setting_ISO_s {
    // M4_NUMBER_MARK_DESC("iso", "f32", M4_RANGE(50, 204800), "50", M4_DIGIT(1), "index2")
    float iso;

    // M4_NUMBER_DESC("filter_strength", "f32", M4_RANGE(0, 1), "0.5", M4_DIGIT(2))
    float filter_strength;

    // M4_NUMBER_DESC("sp_filter_strength", "f32", M4_RANGE(0, 1), "0.5", M4_DIGIT(2), M4_HIDE(1))
    float sp_filter_strength;

    // M4_NUMBER_DESC("lo_clipwgt", "f32", M4_RANGE(0, 1.0), "0.01", M4_DIGIT(3))
    float lo_clipwgt;

    // M4_NUMBER_DESC("hi_clipwgt", "f32", M4_RANGE(0, 1.0), "0.01", M4_DIGIT(3))
    float hi_clipwgt;

    // M4_NUMBER_DESC("softwgt", "f32", M4_RANGE(0, 1), "0.0", M4_DIGIT(2))
    float softwgt;

} CalibDbV2_BayerNrV2_Bayernr3d_Setting_ISO_t;

typedef struct CalibDbV2_BayerNrV2_Bayernr3d_Setting_s {
    // M4_STRING_MARK_DESC("SNR_Mode", M4_SIZE(1,1), M4_RANGE(0, 64), "LSNR",M4_DYNAMIC(0), "index1")
    char *SNR_Mode;
    // M4_STRING_DESC("Sensor_Mode", M4_SIZE(1,1), M4_RANGE(0, 64), "lcg", M4_DYNAMIC(0))
    char *Sensor_Mode;
    // M4_STRUCT_LIST_DESC("Tuning_ISO", M4_SIZE_DYNAMIC, "double_index_list")
    CalibDbV2_BayerNrV2_Bayernr3d_Setting_ISO_t *Tuning_ISO;
    int Tuning_ISO_len;
} CalibDbV2_BayerNrV2_Bayernr3d_Setting_t;

typedef struct CalibDbV2_BayerNrV2_Bayernr3d_s {
    // M4_BOOL_DESC("enable", "1")
    bool enable;
    // M4_STRUCT_LIST_DESC("Setting", M4_SIZE_DYNAMIC, "double_index_list")
    CalibDbV2_BayerNrV2_Bayernr3d_Setting_t *Setting;
    int Setting_len;
} CalibDbV2_BayerNrV2_Bayernr3d_t;


typedef struct CalibDbV2_BayerNrV2_s {
    // M4_STRING_DESC("Version", M4_SIZE(1,1), M4_RANGE(0, 64), "V2", M4_DYNAMIC(0))
    char *Version;
    // M4_STRUCT_DESC("CalibPara", "normal_ui_style")
    CalibDbV2_BayerNrV2_CalibPara_t CalibPara;
    // M4_STRUCT_DESC("Bayernr2D", "normal_ui_style")
    CalibDbV2_BayerNrV2_Bayernr2d_t Bayernr2D;
    // M4_STRUCT_DESC("Bayernr3D", "normal_ui_style")
    CalibDbV2_BayerNrV2_Bayernr3d_t Bayernr3D;
} CalibDbV2_BayerNrV2_t;

RKAIQ_END_DECLARE

#endif
