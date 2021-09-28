/*
 * Copyright 2019 Rockchip Electronics Co. LTD
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
 *
 * This class/file is defined to read video_setting_configure.xml
 * to get the information of which video formats can be hw decoder
 */

#ifndef SRC_RT_BASE_INCLUDE_RT_CHIPS_H_
#define SRC_RT_BASE_INCLUDE_RT_CHIPS_H_

typedef enum _RKChipType {
    RK_CHIP_UNKOWN = 0,

    //  2928 and 3036 no iep
    RK_CHIP_2928,
    RK_CHIP_3036,

    RK_CHIP_3066,
    RK_CHIP_3188,

    //  iep
    RK_CHIP_3368H,
    RK_CHIP_3128H,
    RK_CHIP_3128M,
    RK_CHIP_312X,
    RK_CHIP_3326,

    //  support 10bit chips
    RK_CHIP_10BIT_SUPPORT_BEGIN,

    //  3288 support max width to 3840
    RK_CHIP_3288,

    //  support max width 4096 chips
    RK_CHIP_4096_SUPPORT_BEGIN,
    RK_CHIP_322X_SUPPORT_BEGIN,
    RK_CHIP_3228A,
    RK_CHIP_3228B,
    RK_CHIP_3228H,
    RK_CHIP_3328,
    RK_CHIP_3229,
    RK_CHIP_322X_SUPPORT_END,
    RK_CHIP_3399,
    RK_CHIP_10BIT_SUPPORT_END,

    RK_CHIP_3368,
    RK_CHIP_4096_SUPPORT_END,
} RKChipType;

typedef struct {
    const char *name;
    RKChipType  type;
} RKChipInfo;

static const RKChipInfo ChipList[] = {
    {"unkown",    RK_CHIP_UNKOWN},
    {"rk2928",    RK_CHIP_2928},
    {"rk3036",    RK_CHIP_3036},
    {"rk3066",    RK_CHIP_3066},
    {"rk3188",    RK_CHIP_3188},
    {"rk312x",    RK_CHIP_312X},
    /* 3128h first for string matching */
    {"rk3128h",   RK_CHIP_3128H},
    {"rk3128m",   RK_CHIP_3128M},
    {"rk3128",    RK_CHIP_312X},
    {"rk3126",    RK_CHIP_312X},
    {"rk3288",    RK_CHIP_3288},
    {"rk3228a",   RK_CHIP_3228A},
    {"rk3228b",   RK_CHIP_3228B},
    {"rk322x",    RK_CHIP_3229},
    {"rk3229",    RK_CHIP_3229},
    {"rk3228h",   RK_CHIP_3228H},
    {"rk3328",    RK_CHIP_3328},
    {"rk3399",    RK_CHIP_3399},
    {"rk3368h",   RK_CHIP_3368H},
    {"rk3368",    RK_CHIP_3368},
    {"rk3326",    RK_CHIP_3326},
    {"px30",      RK_CHIP_3326},
};

RKChipInfo* getChipName();

#endif  // SRC_RT_BASE_INCLUDE_RT_CHIPS_H_

