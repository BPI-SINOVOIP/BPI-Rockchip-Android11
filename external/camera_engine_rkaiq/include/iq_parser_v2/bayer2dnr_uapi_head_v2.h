/*
 * bayer2dnr_uapi_head_v2.h
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

#ifndef __CALIBDBV2_UAPI_BAYER_2DNR_HEADER_V2_H__
#define __CALIBDBV2_UAPI_BAYER_2DNR_HEADER_V2_H__

#include "rk_aiq_comm.h"

RKAIQ_BEGIN_DECLARE


typedef struct RK_Bayer2dnr_Params_V2_Select_s
{
    // M4_BOOL_DESC("enable", "1")
    int enable;
    // M4_BOOL_DESC("gauss_guide", "1")
    int gauss_guide;


    // M4_ARRAY_DESC("lumapoint", "s32", M4_SIZE(1,16), M4_RANGE(0,65535), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    int lumapoint[16];
    // M4_ARRAY_DESC("sigma", "s32", M4_SIZE(1,16), M4_RANGE(0,65535), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    int sigma[16];

    // M4_NUMBER_DESC("filter_strength", "f32", M4_RANGE(0, 16), "0.5", M4_DIGIT(2))
    float filter_strength;
    // M4_NUMBER_DESC("edgesofts", "f32", M4_RANGE(0, 16.0), "1.0", M4_DIGIT(2))
    float edgesofts;
    // M4_NUMBER_DESC("ratio", "f32", M4_RANGE(0, 1.0), "0.2", M4_DIGIT(2))
    float ratio;
    // M4_NUMBER_DESC("weight", "f32", M4_RANGE(0, 1.0), "1.0", M4_DIGIT(2))
    float weight;

    // M4_NUMBER_DESC("pix_diff", "s32", M4_RANGE(0, 16383), "16383", M4_DIGIT(0))
    int pix_diff;
    // M4_NUMBER_DESC("diff_thld", "s32", M4_RANGE(0, 1024), "1024", M4_DIGIT(0))
    int diff_thld;

    // M4_BOOL_DESC("hdrdgain_ctrl_en", "0")
    bool hdrdgain_ctrl_en;
    // M4_NUMBER_DESC("hdr_dgain_scale_s", "f32", M4_RANGE(0, 128.0), "1.0", M4_DIGIT(2))
    float hdr_dgain_scale_s;
    // M4_NUMBER_DESC("hdr_dgain_scale_m", "f32", M4_RANGE(0, 128.0), "1.0", M4_DIGIT(2))
    float hdr_dgain_scale_m;


} RK_Bayer2dnr_Params_V2_Select_t;


RKAIQ_END_DECLARE

#endif
