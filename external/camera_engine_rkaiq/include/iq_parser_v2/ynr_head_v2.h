/*
 * ynr_head_v1.h
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

#ifndef __CALIBDBV2_YNR_HEADER_V2_H__
#define __CALIBDBV2_YNR_HEADER_V2_H__

#include "rk_aiq_comm.h"

RKAIQ_BEGIN_DECLARE

///////////////////////////ynr v1//////////////////////////////////////

typedef struct CalibDbV2_YnrV2_CalibPara_Setting_ISO_s {
    // M4_NUMBER_MARK_DESC("iso", "f32", M4_RANGE(50, 204800), "50", M4_DIGIT(1), "index2")
    float iso;
    // M4_ARRAY_DESC("sigma_curve", "f32", M4_SIZE(1,5), M4_RANGE(-65535.0, 65535), "0.0", M4_DIGIT(6), M4_DYNAMIC(0))
    double sigma_curve[5];
    // M4_NUMBER_DESC("ynr_ci_l", "f32", M4_RANGE(0.0, 2.0), "0.5", M4_DIGIT(3))
    float ynr_ci_l;
    // M4_NUMBER_DESC("ynr_ci_h", "f32", M4_RANGE(0.0, 2.0), "0.5", M4_DIGIT(3))
    float ynr_ci_h;

} CalibDbV2_YnrV2_CalibPara_Setting_ISO_t;

typedef struct CalibDbV2_YnrV2_CalibPara_Setting_s {
    // M4_STRING_MARK_DESC("SNR_Mode", M4_SIZE(1,1), M4_RANGE(0, 64), "LSNR",M4_DYNAMIC(0), "index1")
    char *SNR_Mode;
    // M4_STRING_DESC("Sensor_Mode", M4_SIZE(1,1), M4_RANGE(0, 64), "lcg", M4_DYNAMIC(0))
    char *Sensor_Mode;
    // M4_STRUCT_LIST_DESC("Calib_ISO", M4_SIZE_DYNAMIC, "double_index_list")
    CalibDbV2_YnrV2_CalibPara_Setting_ISO_t *Calib_ISO;
    int Calib_ISO_len;
} CalibDbV2_YnrV2_CalibPara_Setting_t;

typedef struct CalibDbV2_YnrV2_CalibPara_s {
    // M4_STRUCT_LIST_DESC("Setting", M4_SIZE_DYNAMIC, "double_index_list")
    CalibDbV2_YnrV2_CalibPara_Setting_t *Setting;
    int Setting_len;
} CalibDbV2_YnrV2_CalibPara_t;


typedef struct CalibDbV2_YnrV2_TuningPara_Setting_ISO_s {
    // M4_NUMBER_MARK_DESC("iso", "f32", M4_RANGE(50, 204800), "50", M4_DIGIT(1), "index2")
    float iso;

    // M4_BOOL_DESC("ynr_bft3x3_bypass", "0")
    float ynr_bft3x3_bypass;
    // M4_BOOL_DESC("ynr_lbft5x5_bypass", "0")
    float ynr_lbft5x5_bypass;
    // M4_BOOL_DESC("ynr_lgft3x3_bypass", "0")
    float ynr_lgft3x3_bypass;
    // M4_BOOL_DESC("ynr_flt1x1_bypass", "0")
    float ynr_flt1x1_bypass;
    // M4_BOOL_DESC("ynr_sft5x5_bypass", "0")
    float ynr_sft5x5_bypass;

    // M4_NUMBER_DESC("low_bf_0", "f32", M4_RANGE(0.1, 512), "0.5", M4_DIGIT(2))
    float low_bf_0;
    // M4_NUMBER_DESC("low_bf_1", "f32", M4_RANGE(0.1, 512), "0.5", M4_DIGIT(2))
    float low_bf_1;
    // M4_NUMBER_DESC("low_thred_adj", "f32", M4_RANGE(0.0, 31.0), "0.25", M4_DIGIT(2))
    float low_thred_adj;
    // M4_NUMBER_DESC("low_peak_supress", "f32", M4_RANGE(0.0, 1.0), "0.5", M4_DIGIT(2))
    float low_peak_supress;
    // M4_NUMBER_DESC("low_edge_adj_thresh", "f32", M4_RANGE(0, 1023), "7", M4_DIGIT(2))
    float low_edge_adj_thresh;
    // M4_NUMBER_DESC("low_center_weight", "f32", M4_RANGE(0.0, 1.0), "0.5", M4_DIGIT(2))
    float low_center_weight;
    // M4_NUMBER_DESC("low_dist_adj", "f32", M4_RANGE(0.0, 63.0), "8.0", M4_DIGIT(2))
    float low_dist_adj;
    // M4_NUMBER_DESC("low_weight", "f32", M4_RANGE(0.0, 1.0), "0.5", M4_DIGIT(2))
    float low_weight;
    // M4_NUMBER_DESC("low_filt_strength_0", "f32", M4_RANGE(0.0, 1.0), "0.7", M4_DIGIT(2))
    float low_filt_strength_0;
    // M4_NUMBER_DESC("low_filt_strength_1", "f32", M4_RANGE(0.0, 1.0), "0.85", M4_DIGIT(2))
    float low_filt_strength_1;
    // M4_NUMBER_DESC("low_bi_weight", "f32", M4_RANGE(0.0, 1.0), "0.2", M4_DIGIT(2))
    float low_bi_weight;


    // M4_NUMBER_DESC("base_filter_weight_0", "f32", M4_RANGE(0.0, 1.0), "0.28", M4_DIGIT(2))
    float base_filter_weight_0;
    // M4_NUMBER_DESC("base_filter_weight_1", "f32", M4_RANGE(0.0, 1.0), "0.46", M4_DIGIT(2))
    float base_filter_weight_1;
    // M4_NUMBER_DESC("base_filter_weight_2", "f32", M4_RANGE(0.0, 1.0), "0.26", M4_DIGIT(2))
    float base_filter_weight_2;


    // M4_NUMBER_DESC("high_thred_adj", "f32", M4_RANGE(0.0, 31.0), "0.5", M4_DIGIT(2))
    float high_thred_adj;
    // M4_NUMBER_DESC("high_weight", "f32", M4_RANGE(0.0, 1.0), "0.5", M4_DIGIT(2))
    float high_weight;
    // M4_NUMBER_DESC("hi_min_adj", "f32", M4_RANGE(0.0, 1), "0.9", M4_DIGIT(2))
    float hi_min_adj;
    // M4_NUMBER_DESC("hi_edge_thed", "f32", M4_RANGE(0.0, 255.0), "100.0", M4_DIGIT(2))
    float hi_edge_thed;
    // M4_ARRAY_DESC("high_direction_weight", "f32", M4_SIZE(1,8), M4_RANGE(0,1.0), "[1  1  1  1  0.5  0.5  0.5  0.5]", M4_DIGIT(2), M4_DYNAMIC(0))
    float high_direction_weight[8];

    // M4_ARRAY_DESC("rnr_strength", "f32", M4_SIZE(1,17), M4_RANGE(0,16), "1.0", M4_DIGIT(3), M4_DYNAMIC(0))
    float rnr_strength[17];

} CalibDbV2_YnrV2_TuningPara_Setting_ISO_t;


typedef struct CalibDbV2_YnrV2_TuningPara_Setting_s {
    // M4_STRING_MARK_DESC("SNR_Mode", M4_SIZE(1,1), M4_RANGE(0, 64), "LSNR",M4_DYNAMIC(0), "index1")
    char *SNR_Mode;
    // M4_STRING_DESC("Sensor_Mode", M4_SIZE(1,1), M4_RANGE(0, 64), "lcg", M4_DYNAMIC(0))
    char *Sensor_Mode;
    // M4_STRUCT_LIST_DESC("Tuning_ISO", M4_SIZE_DYNAMIC, "double_index_list")
    CalibDbV2_YnrV2_TuningPara_Setting_ISO_t *Tuning_ISO;
    int Tuning_ISO_len;
} CalibDbV2_YnrV2_TuningPara_Setting_t;

typedef struct CalibDbV2_YnrV2_TuningPara_s {
    // M4_BOOL_DESC("enable", "1")
    bool enable;
    // M4_STRUCT_LIST_DESC("Setting", M4_SIZE_DYNAMIC, "double_index_list")
    CalibDbV2_YnrV2_TuningPara_Setting_t *Setting;
    int Setting_len;
} CalibDbV2_YnrV2_TuningPara_t;


typedef struct CalibDbV2_YnrV2_s {
    // M4_STRING_DESC("Version", M4_SIZE(1,1), M4_RANGE(0, 64), "V2", M4_DYNAMIC(0))
    char *Version;
    // M4_STRUCT_DESC("CalibPara", "normal_ui_style")
    CalibDbV2_YnrV2_CalibPara_t CalibPara;
    // M4_STRUCT_DESC("TuningPara", "normal_ui_style")
    CalibDbV2_YnrV2_TuningPara_t TuningPara;
} CalibDbV2_YnrV2_t;

RKAIQ_END_DECLARE

#endif
