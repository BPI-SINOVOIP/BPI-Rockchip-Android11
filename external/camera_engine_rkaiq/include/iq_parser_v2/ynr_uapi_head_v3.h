/*
 * ynr_uapi_head_v3.h
 *
 *  Copyright (c) 2022 Rockchip Corporation
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

#ifndef __CALIBDBV2_UAPI_YNRV3_HEADER_H__
#define __CALIBDBV2_UAPI_YNRV3_HEADER_H__

#include "rk_aiq_comm.h"

RKAIQ_BEGIN_DECLARE

#define YNR_V3_ISO_CURVE_POINT_BIT          4
#define YNR_V3_ISO_CURVE_POINT_NUM          ((1 << YNR_V3_ISO_CURVE_POINT_BIT)+1)

typedef struct RK_YNR_Params_V3_Select_s
{
    // M4_BOOL_DESC("enable", "1")
    int enable;

    // M4_BOOL_DESC("ynr_bft3x3_bypass", "0")
    int ynr_bft3x3_bypass;
    // M4_BOOL_DESC("ynr_lbft5x5_bypass", "0")
    int ynr_lbft5x5_bypass;
    // M4_BOOL_DESC("ynr_lgft3x3_bypass", "0")
    int ynr_lgft3x3_bypass;
    // M4_BOOL_DESC("ynr_flt1x1_bypass", "0")
    int ynr_flt1x1_bypass;
    // M4_BOOL_DESC("ynr_sft5x5_bypass", "0")
    int ynr_sft5x5_bypass;


    // M4_ARRAY_DESC("sigma", "f32", M4_SIZE(1,17), M4_RANGE(0, 4095), "0", M4_DIGIT(3), M4_DYNAMIC(0))
    float sigma[YNR_V3_ISO_CURVE_POINT_NUM];

    // M4_ARRAY_DESC("lumaPoint", "s16", M4_SIZE(1,17), M4_RANGE(0, 1024), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    int16_t lumaPoint[YNR_V3_ISO_CURVE_POINT_NUM];

    // M4_ARRAY_DESC("lo_lumaPoint", "f32", M4_SIZE(1,6), M4_RANGE(0,256), "[0,32,64,128,192,256]", M4_DIGIT(0), M4_DYNAMIC(0))
    float lo_lumaPoint[6];
    // M4_ARRAY_DESC("lo_ratio", "f32", M4_SIZE(1,6), M4_RANGE(0,2), "[1,1,1,1,1,1]", M4_DIGIT(2), M4_DYNAMIC(0))
    float lo_ratio[6];
    // M4_ARRAY_DESC("hi_lumaPoint", "f32", M4_SIZE(1,6), M4_RANGE(0,256), "[0,32,64,128,192,256]", M4_DIGIT(0), M4_DYNAMIC(0))
    float hi_lumaPoint[6];
    // M4_ARRAY_DESC("hi_ratio", "f32", M4_SIZE(1,6), M4_RANGE(0,2), "[1,1,1,1,1,1]", M4_DIGIT(2), M4_DYNAMIC(0))
    float hi_ratio[6];

    // M4_ARRAY_DESC("rnr_strength", "f32", M4_SIZE(1,17), M4_RANGE(0,16), "1.0", M4_DIGIT(3), M4_DYNAMIC(0))
    float rnr_strength[17];

    // M4_NUMBER_DESC("lci", "f32", M4_RANGE(0.0, 2.0), "0.5", M4_DIGIT(3))
    float lci;
    // M4_NUMBER_DESC("hci", "f32", M4_RANGE(0.0, 2.0), "0.5", M4_DIGIT(3))
    float hci;

    // M4_NUMBER_DESC("ynr_global_gain_alpha", "f32", M4_RANGE(0, 1.0), "0", M4_DIGIT(2))
    float ynr_global_gain_alpha;
    // M4_NUMBER_DESC("ynr_global_gain", "f32", M4_RANGE(0, 64.0), "1.0", M4_DIGIT(2))
    float ynr_global_gain;
    // M4_NUMBER_DESC("ynr_adjust_thresh", "f32", M4_RANGE(0, 1.0), "1.0", M4_DIGIT(2))
    float ynr_adjust_thresh;
    // M4_NUMBER_DESC("ynr_adjust_scale", "f32", M4_RANGE(0, 16.0), "1.0", M4_DIGIT(2))
    float ynr_adjust_scale;

    // M4_NUMBER_DESC("low_bf1", "f32", M4_RANGE(0.1, 32), "0.5", M4_DIGIT(2))
    float low_bf1;
    // M4_NUMBER_DESC("low_bf2", "f32", M4_RANGE(0.1, 32), "0.5", M4_DIGIT(2))
    float low_bf2;

    // M4_NUMBER_DESC("low_thred_adj", "f32", M4_RANGE(0.0, 31.0), "0.25", M4_DIGIT(2))
    float low_thred_adj;
    // M4_NUMBER_DESC("low_peak_supress", "f32", M4_RANGE(0.0, 1.0), "0.5", M4_DIGIT(2))
    float low_peak_supress;
    // M4_NUMBER_DESC("low_edge_adj_thresh", "f32", M4_RANGE(0.0, 1023.0), "7", M4_DIGIT(2))
    float low_edge_adj_thresh;
    // M4_NUMBER_DESC("low_lbf_weight_thresh", "f32", M4_RANGE(0.0, 1.0), "0.25", M4_DIGIT(2))
    float low_lbf_weight_thresh;
    // M4_NUMBER_DESC("low_center_weight", "f32", M4_RANGE(0.0, 1.0), "0.5", M4_DIGIT(2))
    float low_center_weight;
    // M4_NUMBER_DESC("low_dist_adj", "f32", M4_RANGE(0.0, 127.0), "8.0", M4_DIGIT(2))
    float low_dist_adj;
    // M4_NUMBER_DESC("low_weight", "f32", M4_RANGE(0.0, 1.0), "0.5", M4_DIGIT(2))
    float low_weight;
    // M4_NUMBER_DESC("low_filt1_strength", "f32", M4_RANGE(0.0, 1.0), "0.7", M4_DIGIT(2))
    float low_filt1_strength;
    // M4_NUMBER_DESC("low_filt2_strength", "f32", M4_RANGE(0.0, 1.0), "0.85", M4_DIGIT(2))
    float low_filt2_strength;
    // M4_NUMBER_DESC("low_bi_weight", "f32", M4_RANGE(0.0, 1.0), "0.2", M4_DIGIT(2))
    float low_bi_weight;

    // M4_NUMBER_DESC("base_filter_weight1", "f32", M4_RANGE(0.0, 1.0), "0.28", M4_DIGIT(2))
    float base_filter_weight1;
    // M4_NUMBER_DESC("base_filter_weight2", "f32", M4_RANGE(0.0, 1.0), "0.46", M4_DIGIT(2))
    float base_filter_weight2;
    // M4_NUMBER_DESC("base_filter_weight3", "f32", M4_RANGE(0.0, 1.0), "0.26", M4_DIGIT(2))
    float base_filter_weight3;


    // M4_NUMBER_DESC("high_thred_adj", "f32", M4_RANGE(0.0, 31.0), "0.5", M4_DIGIT(2))
    float high_thred_adj;
    // M4_NUMBER_DESC("high_weight", "f32", M4_RANGE(0.0, 1.0), "0.5", M4_DIGIT(2))
    float high_weight;
    // M4_NUMBER_DESC("hi_min_adj", "f32", M4_RANGE(0.0, 1.0), "0.9", M4_DIGIT(2))
    float hi_min_adj;
    // M4_NUMBER_DESC("hi_edge_thed", "f32", M4_RANGE(0.0, 255.0), "100.0", M4_DIGIT(2))
    float hi_edge_thed;
    // M4_ARRAY_DESC("high_direction_weight", "f32", M4_SIZE(1,8), M4_RANGE(0,1.0), "[1  1  1  1  0.5  0.5  0.5  0.5]", M4_DIGIT(2), M4_DYNAMIC(0))
    float high_direction_weight[8];

} RK_YNR_Params_V3_Select_t;

RKAIQ_END_DECLARE

#endif
