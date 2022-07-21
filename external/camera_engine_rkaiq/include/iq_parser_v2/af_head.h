/*
 * af_head.h
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

#ifndef __CALIBDBV2_AF_HEADER_H__
#define __CALIBDBV2_AF_HEADER_H__

#include "rk_aiq_comm.h"

RKAIQ_BEGIN_DECLARE

#define CALIBDBV2_MAX_ISO_LEVEL                    13
#define CALIBDBV2_ZOOM_FOCUS_POSITION_NUM          32
#define CALIBDBV2_ZOOM_FOCUS_ZOOM_MOVE_TBL_SIZE    32
#define CALIBDBV2_AF_CORRECT_SEARCHTBL_MAX         100

typedef enum CalibDbV2_Af_SearchDir_s {
    CalibDbV2_CAM_AFM_POSITIVE_SEARCH     = 0,
    CalibDbV2_CAM_AFM_NEGATIVE_SEARCH     = 1,
    CalibDbV2_CAM_AFM_ADAPTIVE_SEARCH     = 2
} CalibDbV2_Af_SearchDir_t;

typedef enum CalibDbV2_Af_SS_s {
    CalibDbV2_CAM_AFM_FSS_INVALID         = 0,
    CalibDbV2_CAM_AFM_FSS_FULLRANGE       = 1,    /**< scan the full focus range to find the point of best focus */
    CalibDbV2_CAM_AFM_FSS_HILLCLIMBING    = 2,    /**< use hillclimbing search */
    CalibDbV2_CAM_AFM_FSS_ADAPTIVE_RANGE  = 3,    /**< similar to full range search, but with multiple subsequent scans
                                                       with decreasing range and step size will be performed. */
    CalibDbV2_CAM_AFM_FSS_MUTIWINDOW      = 4,    /**< search by muti-window statistics */
    CalibDbV2_CAM_AFM_FSS_MAX
} CalibDbV2_Af_SS_t;

typedef enum CalibDbV2_AF_MODE_s
{
    CalibDbV2_AF_MODE_NOT_SET = -1,
    CalibDbV2_AF_MODE_AUTO,
    CalibDbV2_AF_MODE_MACRO,
    CalibDbV2_AF_MODE_INFINITY,
    CalibDbV2_AF_MODE_FIXED,
    CalibDbV2_AF_MODE_EDOF,
    CalibDbV2_AF_MODE_CONTINUOUS_VIDEO,
    CalibDbV2_AF_MODE_CONTINUOUS_PICTURE,
    CalibDbV2_AF_MODE_ONESHOT_AFTER_ZOOM,
} CalibDbV2_AF_MODE_t;

typedef enum CalibDbV2_PDAF_DATA_DIR_s {
    CalibDbV2_PDAF_DIR_INVAL = 0,
    CalibDbV2_PDAF_DIR_LEFT  = 1,
    CalibDbV2_PDAF_DIR_RIGHT = 2,
    CalibDbV2_PDAF_DIR_TOP   = 3,
    CalibDbV2_PDAF_DIR_DOWN  = 4
} CalibDbV2_PDAF_DATA_DIR_t;

typedef struct CalibDbV2_Af_ContrastZoom_s {
    // M4_ARRAY_DESC("QuickFoundThersZoomIdx", "u16", M4_SIZE(1,32), M4_RANGE(0,65535), "0", M4_DIGIT(0), M4_DYNAMIC(1))
    unsigned short          *QuickFoundThersZoomIdx;
    int                     QuickFoundThersZoomIdx_len;
    // M4_ARRAY_DESC("QuickFoundThers", "f32", M4_SIZE(1,32), M4_RANGE(0,1), "0", M4_DIGIT(3), M4_DYNAMIC(1))
    float                   *QuickFoundThers;
    int                     QuickFoundThers_len;
    // M4_ARRAY_DESC("SearchStepZoomIdx", "u16", M4_SIZE(1,32), M4_RANGE(0,65535), "0", M4_DIGIT(0), M4_DYNAMIC(1))
    unsigned short          *SearchStepZoomIdx;
    int                     SearchStepZoomIdx_len;
    // M4_ARRAY_DESC("SearchStep", "u16", M4_SIZE(1,32), M4_RANGE(0,65535), "0", M4_DIGIT(0), M4_DYNAMIC(1))
    unsigned short          *SearchStep;
    int                     SearchStep_len;
    // M4_ARRAY_DESC("StopStepZoomIdx", "u16", M4_SIZE(1,32), M4_RANGE(0,65535), "0", M4_DIGIT(0), M4_DYNAMIC(1))
    unsigned short          *StopStepZoomIdx;
    int                     StopStepZoomIdx_len;
    // M4_ARRAY_DESC("StopStep", "u16", M4_SIZE(1,32), M4_RANGE(0,65535), "0", M4_DIGIT(0), M4_DYNAMIC(1))
    unsigned short          *StopStep;
    int                     StopStep_len;
    // M4_NUMBER_DESC("SkipHighPassZoomIdx", "u16", M4_RANGE(0, 65535), "0", M4_DIGIT(0))
    unsigned short          SkipHighPassZoomIdx;
    // M4_NUMBER_DESC("SkipHighPassGain", "f32", M4_RANGE(0, 1000), "0", M4_DIGIT(3))
    float                   SkipHighPassGain;
    // M4_NUMBER_DESC("SwitchDirZoomIdx", "u16", M4_RANGE(0, 65535), "0", M4_DIGIT(0))
    unsigned short          SwitchDirZoomIdx;

    // M4_NUMBER_DESC("Spotlight HighlightRatio", "f32", M4_RANGE(0, 1), "0.014", M4_DIGIT(3))
    float                   SpotlightHighlightRatio;
    // M4_ARRAY_DESC("Spotlight LumaRatio", "f32", M4_SIZE(1,3), M4_RANGE(0,1), "[0.3, 0.5, 0.8]", M4_DIGIT(3), M4_DYNAMIC(0))
    float                   SpotlightLumaRatio[3];
    // M4_ARRAY_DESC("Spotlight BlkCnt", "f32", M4_SIZE(1,3), M4_RANGE(0,1), "[0.2, 0.5, 0.25]", M4_DIGIT(3), M4_DYNAMIC(0))
    float                   SpotlightBlkCnt[3];
} CalibDbV2_Af_ContrastZoom_t;

typedef struct CalibDbV2_Af_Contrast_s {
    // M4_BOOL_DESC("enable", "0")
    bool enable;
    // M4_ENUM_DESC("SearchStrategy", "CalibDbV2_Af_SS_t", "CalibDbV2_CAM_AFM_FSS_ADAPTIVE_RANGE")
    CalibDbV2_Af_SS_t         Afss;                       /**< enumeration type for search strategy */
    // M4_ENUM_DESC("FullDir", "CalibDbV2_Af_SearchDir_t", "CalibDbV2_CAM_AFM_ADAPTIVE_SEARCH")
    CalibDbV2_Af_SearchDir_t  FullDir;
    // M4_ARRAY_DESC("FullRangeTbl", "u16", M4_SIZE(1,65), M4_RANGE(0,64), "0", M4_DIGIT(0), M4_DYNAMIC(1))
    unsigned short          *FullRangeTbl;             /**< full range search table*/
    int                     FullRangeTbl_len;
    // M4_ENUM_DESC("AdaptiveDir", "CalibDbV2_Af_SearchDir_t", "CalibDbV2_CAM_AFM_ADAPTIVE_SEARCH")
    CalibDbV2_Af_SearchDir_t  AdaptiveDir;
    // M4_ARRAY_DESC("AdaptRangeTbl", "u16", M4_SIZE(1,65), M4_RANGE(0,64), "0", M4_DIGIT(0), M4_DYNAMIC(1))
    unsigned short          *AdaptRangeTbl;            /**< adaptive range search table*/
    int                     AdaptRangeTbl_len;
    // M4_ARRAY_DESC("TrigThers", "f32", M4_SIZE(1,32), M4_RANGE(0,1), "0", M4_DIGIT(3), M4_DYNAMIC(1))
    float                   *TrigThers;                    /**< AF trigger threshold */
    int                     TrigThers_len;
    // M4_ARRAY_DESC("TrigThersFv", "f32", M4_SIZE(1,32), M4_RANGE(0,100000000000), "0", M4_DIGIT(0), M4_DYNAMIC(1))
    float                   *TrigThersFv;                    /**< AF trigger threshold */
    int                     TrigThersFv_len;
    // M4_NUMBER_DESC("LumaTrigThers", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float                   LumaTrigThers;
    // M4_NUMBER_DESC("ExpTrigThers", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float                   ExpTrigThers;

    // M4_NUMBER_DESC("StableThers", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float                   StableThers;                  /**< AF stable threshold */
    // M4_NUMBER_DESC("StableFrames", "u16", M4_RANGE(0,65535), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short          StableFrames;                 /**< AF stable  status must hold frames */
    // M4_NUMBER_DESC("StableTime", "u16", M4_RANGE(0,65535), "0", M4_DIGIT(3),M4_HIDE(0))
    unsigned short          StableTime;                   /**< AF stable status must hold time */

    // M4_NUMBER_DESC("SceneDiffEnable", "u8", M4_RANGE(0, 1), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned char           SceneDiffEnable;
    // M4_NUMBER_DESC("SceneDiffThers", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float                   SceneDiffThers;
    // M4_NUMBER_DESC("SceneDiffBlkThers", "u16", M4_RANGE(0,225), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short          SceneDiffBlkThers;
    // M4_NUMBER_DESC("CenterSceneDiffThers", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float                   CenterSceneDiffThers;

    // M4_NUMBER_DESC("ValidMaxMinRatio", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float                   ValidMaxMinRatio;
    // M4_NUMBER_DESC("ValidValueThers", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float                   ValidValueThers;

    // M4_NUMBER_DESC("OutFocusValue", "f32", M4_RANGE(0,1000000), "0", M4_DIGIT(3),M4_HIDE(0))
    float                   OutFocusValue;                /**< out of focus vlaue*/
    // M4_NUMBER_DESC("OutFocusPos", "u16", M4_RANGE(0,65), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short          OutFocusPos;                  /**< out of focus position*/

    // M4_NUMBER_DESC("WeightEnable", "u8", M4_RANGE(0, 1), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned char           WeightEnable;
    // M4_ARRAY_DESC("Weight", "u16", M4_SIZE(15,15), M4_RANGE(0,65535), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    unsigned short          Weight[225];                  /**< weight */

    // M4_NUMBER_DESC("SearchPauseLumaEnable", "u8", M4_RANGE(0, 1), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned char           SearchPauseLumaEnable;
    // M4_NUMBER_DESC("SearchPauseLumaThers", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float                   SearchPauseLumaThers;
    // M4_NUMBER_DESC("StableFrames", "u16", M4_RANGE(0,65535), "0", M4_DIGIT(3),M4_HIDE(0))
    unsigned short          SearchLumaStableFrames;
    // M4_NUMBER_DESC("SearchLumaStableThers", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float                   SearchLumaStableThers;

    // M4_NUMBER_DESC("Stage1QuickFoundThers", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float                   Stage1QuickFoundThers;
    // M4_NUMBER_DESC("Stage2QuickFoundThers", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float                   Stage2QuickFoundThers;

    // M4_NUMBER_DESC("FlatValue", "f32", M4_RANGE(0,65535), "0", M4_DIGIT(3),M4_HIDE(0))
    float                   FlatValue;

    // M4_NUMBER_DESC("PointLightLumaTh", "u16", M4_RANGE(0,65535), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short          PointLightLumaTh;                 /**< point light luma threshold */
    // M4_NUMBER_DESC("PointLightCntTh", "u16", M4_RANGE(0,65535), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short          PointLightCntTh;                  /**< point light count threshold */

    // M4_STRUCT_DESC("ZoomConfig", "normal_ui_style")
    CalibDbV2_Af_ContrastZoom_t ZoomCfg;
} CalibDbV2_Af_Contrast_t;

typedef struct CalibDbV2_Af_Laser_s {
    // M4_BOOL_DESC("enable", "0")
    bool enable;
    // M4_ARRAY_DESC("vcmDot", "f32", M4_SIZE(1,7), M4_RANGE(0,64), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    float vcmDot[7];
    // M4_ARRAY_DESC("distanceDot", "f32", M4_SIZE(1,7), M4_RANGE(0,65535), "0", M4_DIGIT(4), M4_DYNAMIC(0))
    float distanceDot[7];
} CalibDbV2_Af_Laser_t;

typedef struct CalibDbV2_Af_Pdaf_fineSearch_s {
    // M4_NUMBER_DESC("confidence", "u32", M4_RANGE(0,1000), "0", M4_DIGIT(0))
    unsigned int confidence;
    // M4_NUMBER_DESC("range", "s32", M4_RANGE(0,64), "0", M4_DIGIT(0))
    int range;
    // M4_NUMBER_DESC("stepPos", "s32", M4_RANGE(1,64), "1", M4_DIGIT(0))
    int stepPos;
} CalibDbV2_Af_Pdaf_fineSearch_t;

typedef struct CalibDbV2_Af_PdafIsoPara_s {
    // M4_NUMBER_DESC("iso", "u32", M4_RANGE(0, 1000000), "0", M4_DIGIT(0))
    int iso;
    // M4_NUMBER_DESC("pdNoiseFactor", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float pdNoiseFactor;
    // M4_NUMBER_DESC("pdConfdRatio1", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float pdConfdRatio1;
    // M4_NUMBER_DESC("pdConfdRatio2", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float pdConfdRatio2;
    // M4_NUMBER_DESC("pdNoiseBias", "f32", M4_RANGE(0,100), "0", M4_DIGIT(3),M4_HIDE(0))
    float pdNoiseBias;
    // M4_NUMBER_DESC("pdConfdRhresh", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float pdConfdThresh;
    // M4_NUMBER_DESC("defocusPdThresh", "u16", M4_RANGE(0,1023), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short defocusPdThresh;
    // M4_NUMBER_DESC("stablePdRatio", "f32", M4_RANGE(0,255), "0", M4_DIGIT(3),M4_HIDE(0))
    float stablePdRatio;
    // M4_NUMBER_DESC("stablePdOffset", "u16", M4_RANGE(0,1023), "0", M4_DIGIT(3),M4_HIDE(0))
    unsigned short stablePdOffset;
    // M4_NUMBER_DESC("stableCntRatio", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float stableCntRatio;
    // M4_NUMBER_DESC("noconfCntThresh", "u16", M4_RANGE(0,255), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short noconfCntThresh;
    // M4_STRUCT_LIST_DESC("fineSearchTbl", M4_SIZE(1,10), "normal_ui_style")
    CalibDbV2_Af_Pdaf_fineSearch_t* fineSearchTbl;
    int fineSearchTbl_len;
} CalibDbV2_Af_PdafIsoPara_t;

typedef struct CalibDbV2_Af_Pdaf_s {
    // M4_BOOL_DESC("enable", "0")
    bool enable;
    // M4_NUMBER_DESC("pdVsCdDebug", "u8", M4_RANGE(0, 1), "0", M4_DIGIT(0))
    unsigned char pdVsCdDebug;
    // M4_NUMBER_DESC("pdDumpDebug", "u8", M4_RANGE(0, 1), "0", M4_DIGIT(0))
    unsigned char pdDumpDebug;
    // M4_NUMBER_DESC("pdDataBit", "u16", M4_RANGE(1,16), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short pdDataBit;
    // M4_NUMBER_DESC("pdBlkLevel", "u16", M4_RANGE(1,1023), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short pdBlkLevel;
    // M4_NUMBER_DESC("pdSearchRadius", "u16", M4_RANGE(0,32), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short pdSearchRadius;
    // M4_NUMBER_DESC("pdMirrorInCalib", "u8", M4_RANGE(0, 1), "0", M4_DIGIT(0))
    unsigned char pdMirrorInCalib;
    // M4_NUMBER_DESC("pdVsImgoutMirror", "u8", M4_RANGE(0, 1), "0", M4_DIGIT(0))
    unsigned char pdVsImgoutMirror;
    // M4_NUMBER_DESC("pdWidth", "u16", M4_RANGE(0, 65535), "0", M4_DIGIT(0))
    unsigned short pdWidth;
    // M4_NUMBER_DESC("pdHeight", "u16", M4_RANGE(0, 65535), "0", M4_DIGIT(0))
    unsigned short pdHeight;
    // M4_NUMBER_DESC("pdConfdMwinFactor", "u16", M4_RANGE(0,225), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short pdConfdMwinFactor;
    // M4_ARRAY_DESC("pdStepRatio", "f32", M4_SIZE(1,7), M4_RANGE(0,1), "0.5", M4_DIGIT(3), M4_DYNAMIC(0))
    float pdStepRatio[7];
    // M4_ARRAY_DESC("pdStepDefocus", "u16", M4_SIZE(1,7), M4_RANGE(0,1023), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    unsigned short pdStepDefocus[7];
    // M4_STRUCT_LIST_DESC("pdIsoPara", M4_SIZE(1,16), "normal_ui_style")
    CalibDbV2_Af_PdafIsoPara_t* pdIsoPara;
    int pdIsoPara_len;
} CalibDbV2_Af_Pdaf_t;

typedef struct CalibDbV2_Af_VcmCfg_s {
    // M4_NUMBER_DESC("start current", "s32", M4_RANGE(-1, 2048), "-1", M4_DIGIT(0))
    int start_current;
    // M4_NUMBER_DESC("rated current", "s32", M4_RANGE(-1, 2048), "-1", M4_DIGIT(0))
    int rated_current;
    // M4_NUMBER_DESC("step mode", "s32", M4_RANGE(-1, 1000), "-1", M4_DIGIT(0))
    int step_mode;
    // M4_NUMBER_DESC("extra delay", "s32", M4_RANGE(-10000,10000), "0", M4_DIGIT(0))
    int extra_delay;
    // M4_NUMBER_DESC("posture diff", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3))
    float posture_diff;
} CalibDbV2_Af_VcmCfg_t;

typedef struct CalibDbV2_Af_MeasIsoCfg_s {
    // M4_NUMBER_DESC("iso", "u32", M4_RANGE(0, 1000000), "0", M4_DIGIT(0))
    int iso;
    // M4_NUMBER_DESC("afmThres", "u16", M4_RANGE(0, 255), "4", M4_DIGIT(0))
    unsigned short afmThres;
    // M4_ARRAY_MARK_DESC("Gamma_curve", "u16", M4_SIZE(1,17),  M4_RANGE(0, 1023), "[0,45,108,179,245,344,409,459,500,567,622,676,759,833,896,962,1023]", M4_DIGIT(0), M4_DYNAMIC(0), "curve_table")
    unsigned short gammaY[17];
    // M4_ARRAY_DESC("gaussWeight", "u8", M4_SIZE(1,3), M4_RANGE(0,128), "[16,14,14]", M4_DIGIT(0), M4_DYNAMIC(0))
    unsigned char gaussWeight[3];
} CalibDbV2_Af_MeasIsoCfg_t;

typedef struct CalibDbV2_Af_DefCode_s {
    // M4_NUMBER_DESC("code", "u8", M4_RANGE(0, 64), "0", M4_DIGIT(0))
    unsigned char code;
} CalibDbV2_Af_DefCode_t;

typedef struct CalibDbV2_Af_FocusCode_s {
    // M4_NUMBER_DESC("focus position", "f32", M4_RANGE(0, 1000000), "0", M4_DIGIT(3), M4_HIDE(1))
    float pos;
    // M4_ARRAY_DESC("focus code", "s16", M4_SIZE(1,5000), M4_RANGE(-32768,32767), "0", M4_DIGIT(0), M4_DYNAMIC(1), M4_HIDE(1))
    short *code;
    int code_len;
} CalibDbV2_Af_FocusCode_t;

typedef struct CalibDbV2_Af_ZoomFocusTbl_s {
    // M4_NUMBER_DESC("widemod deviate", "u32", M4_RANGE(0, 1000), "0", M4_DIGIT(0))
    int widemod_deviate;
    // M4_NUMBER_DESC("telemod deviate", "u32", M4_RANGE(0, 1000), "0", M4_DIGIT(0))
    int telemod_deviate;
    // M4_NUMBER_DESC("focus backward value", "u32", M4_RANGE(0, 1000), "0", M4_DIGIT(0))
    int focus_backval;
    // M4_ARRAY_DESC("zoom move dot", "u32", M4_SIZE(1,32), M4_RANGE(0,1000000), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    int *zoom_move_dot;
    int zoom_move_dot_len;
    // M4_ARRAY_DESC("zoom move step", "u32", M4_SIZE(1,32), M4_RANGE(0,1000000), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    int *zoom_move_step;
    int zoom_move_step_len;
    // M4_ARRAY_DESC("focal length", "f32", M4_SIZE(1,5000), M4_RANGE(0,10000), "0", M4_DIGIT(3), M4_DYNAMIC(1), M4_HIDE(1))
    float *focal_length;
    int focal_length_len;
    // M4_ARRAY_DESC("zoom code", "s16", M4_SIZE(1,5000), M4_RANGE(-32768,32767), "0", M4_DIGIT(0), M4_DYNAMIC(1), M4_HIDE(1))
    short *zoomcode;
    int zoomcode_len;
    // M4_STRUCT_LIST_DESC("focus code", M4_SIZE_DYNAMIC, "normal_ui_style", M4_HIDE(1))
    CalibDbV2_Af_FocusCode_t *focuscode;
    int focuscode_len;

    // M4_ARRAY_DESC("zoom search table", "s32", M4_SIZE(1,100), M4_RANGE(-32768,32767), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    int *ZoomSearchTbl;
    // M4_NUMBER_DESC("zoom search table number", "u32", M4_RANGE(0, 100), "0", M4_DIGIT(0))
    int ZoomSearchTbl_len;
    // M4_NUMBER_DESC("zoom search reference curve", "u32", M4_RANGE(0, 32), "0", M4_DIGIT(0))
    int ZoomSearchRefCurveIdx;
    // M4_NUMBER_DESC("zoom search margin", "u32", M4_RANGE(0, 100000), "0", M4_DIGIT(0))
    int FocusSearchMargin;
    // M4_ARRAY_DESC("zoom search plus range", "u32", M4_SIZE(1,100), M4_RANGE(0,32767), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    int *FocusSearchPlusRange;
    int FocusSearchPlusRange_len;
    // M4_NUMBER_DESC("focus stage1 step", "u32", M4_RANGE(0, 100), "0", M4_DIGIT(0))
    int FocusStage1Step;
    // M4_NUMBER_DESC("quickFndRate", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3),M4_HIDE(0))
    float QuickFndRate;
    // M4_NUMBER_DESC("quickFndMinFv", "f32", M4_RANGE(0,2147483647), "0", M4_DIGIT(0),M4_HIDE(0))
    float QuickFndMinFv;
    // M4_NUMBER_DESC("search zoom range", "u32", M4_RANGE(0, 100000), "0", M4_DIGIT(0))
    int searchZoomRange;
    // M4_NUMBER_DESC("search focus range", "u32", M4_RANGE(0, 100000), "0", M4_DIGIT(0))
    int searchFocusRange;
    // M4_NUMBER_DESC("search emax", "f32", M4_RANGE(0,100000), "0", M4_DIGIT(3),M4_HIDE(0))
    float searchEmax;
    // M4_NUMBER_DESC("search eavg", "f32", M4_RANGE(0,100000), "0", M4_DIGIT(3),M4_HIDE(0))
    float searchEavg;

    // M4_NUMBER_DESC("IsZoomFocusRec", "u8", M4_RANGE(0, 1), "0", M4_DIGIT(0))
    unsigned char IsZoomFocusRec;
    // M4_STRING_DESC("ZoomInfoDir", M4_SIZE(1,1), M4_RANGE(0, 64), "/data/", M4_DYNAMIC(0))
    char *ZoomInfoDir;
} CalibDbV2_Af_ZoomFocusTbl_t;

typedef struct CalibDb_Af_LdgParam_s {
    // M4_NUMBER_DESC("enable", "u8", M4_RANGE(0, 1), "0", M4_DIGIT(0))
    unsigned char enable;
    // M4_NUMBER_DESC("ldg xl", "u32", M4_RANGE(0, 255), "0", M4_DIGIT(0))
    int ldg_xl;
    // M4_NUMBER_DESC("ldg yl", "u32", M4_RANGE(0, 255), "0", M4_DIGIT(0))
    int ldg_yl;
    // M4_NUMBER_DESC("ldg kl", "u32", M4_RANGE(0, 2147483647), "0", M4_DIGIT(0))
    int ldg_kl;
    // M4_NUMBER_DESC("ldg xh", "u32", M4_RANGE(0, 255), "0", M4_DIGIT(0))
    int ldg_xh;
    // M4_NUMBER_DESC("ldg yh", "u32", M4_RANGE(0, 255), "0", M4_DIGIT(0))
    int ldg_yh;
    // M4_NUMBER_DESC("ldg kh", "u32", M4_RANGE(0, 2147483647), "0", M4_DIGIT(0))
    int ldg_kh;
} CalibDbV2_Af_LdgParam_t;

typedef struct CalibDb_Af_HighLightParam_s {
    // M4_NUMBER_DESC("ther0", "u32", M4_RANGE(0, 255), "245", M4_DIGIT(0))
    int ther0;
    // M4_NUMBER_DESC("ther1", "u32", M4_RANGE(0, 255), "200", M4_DIGIT(0))
    int ther1;
} CalibDbV2_Af_HighLightParam_t;

typedef struct CalibDbV2_AF_Tuning_Para_s {
    // M4_ENUM_DESC("mode", "CalibDbV2_AF_MODE_t", "CalibDbV2_AF_MODE_CONTINUOUS_PICTURE")
    CalibDbV2_AF_MODE_t af_mode;
    // M4_NUMBER_DESC("win_h_offs", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short win_h_offs;
    // M4_NUMBER_DESC("win_v_offs", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short win_v_offs;
    // M4_NUMBER_DESC("win_h_size", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short win_h_size;
    // M4_NUMBER_DESC("win_v_size", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short win_v_size;
    // M4_NUMBER_DESC("win_h_offs in video", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short video_win_h_offs;
    // M4_NUMBER_DESC("win_v_offs in video", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short video_win_v_offs;
    // M4_NUMBER_DESC("win_h_size in video", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short video_win_h_size;
    // M4_NUMBER_DESC("win_v_size in video", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short video_win_v_size;
    // M4_STRUCT_DESC("fixed mode", "normal_ui_style")
    CalibDbV2_Af_DefCode_t fixed_mode;
    // M4_STRUCT_DESC("macro mode", "normal_ui_style")
    CalibDbV2_Af_DefCode_t macro_mode;
    // M4_STRUCT_DESC("infinity mode", "normal_ui_style")
    CalibDbV2_Af_DefCode_t infinity_mode;
    // M4_STRUCT_DESC("ldg param", "normal_ui_style")
    CalibDbV2_Af_LdgParam_t ldg_param;
    // M4_STRUCT_DESC("high light", "normal_ui_style")
    CalibDbV2_Af_HighLightParam_t highlight;
    // M4_STRUCT_DESC("contrast af", "normal_ui_style")
    CalibDbV2_Af_Contrast_t contrast_af;
    // M4_STRUCT_DESC("video contrast af", "normal_ui_style")
    CalibDbV2_Af_Contrast_t video_contrast_af;
    // M4_STRUCT_DESC("laser af", "normal_ui_style")
    CalibDbV2_Af_Laser_t laser_af;
    // M4_STRUCT_DESC("pdaf", "normal_ui_style")
    CalibDbV2_Af_Pdaf_t pdaf;
    // M4_STRUCT_DESC("vcmcfg", "normal_ui_style")
    CalibDbV2_Af_VcmCfg_t vcmcfg;
    // M4_STRUCT_DESC("zoomfocus_tbl", "normal_ui_style")
    CalibDbV2_Af_ZoomFocusTbl_t zoomfocus_tbl;
    // M4_STRUCT_LIST_DESC("meas iso config", M4_SIZE(1,13), "normal_ui_style")
    CalibDbV2_Af_MeasIsoCfg_t measiso_cfg[CALIBDBV2_MAX_ISO_LEVEL];
} CalibDbV2_AF_Tuning_Para_t;

typedef struct {
    // M4_STRUCT_DESC("TuningPara", "normal_ui_style")
    CalibDbV2_AF_Tuning_Para_t TuningPara;
} CalibDbV2_AF_t;

typedef struct CalibDbV2_AfV30_MeasCfg_s {
    // M4_NUMBER_DESC("table index", "u32", M4_RANGE(0, 1000000), "0", M4_DIGIT(0))
    unsigned int tbl_idx;
    // M4_NUMBER_DESC("afmThres", "u16", M4_RANGE(0, 65535), "4", M4_DIGIT(0))
    unsigned short afmThres;
    // M4_ARRAY_MARK_DESC("Gamma Curve", "u16", M4_SIZE(1,17),  M4_RANGE(0, 1023), "[0,45,108,179,245,344,409,459,500,567,622,676,759,833,896,962,1023]", M4_DIGIT(0), M4_DYNAMIC(0), "curve_table")
    unsigned short gammaY[17];
    // M4_NUMBER_DESC("v1fv reliable", "f32", M4_RANGE(0, 1), "0", M4_DIGIT(3))
    float v1fv_reliable;
    // M4_NUMBER_DESC("v2fv reliable", "f32", M4_RANGE(0, 1), "0", M4_DIGIT(3))
    float v2fv_reliable;
    // M4_NUMBER_DESC("v1 fir sel", "u8", M4_RANGE(0, 1), "0", M4_DIGIT(0))
    unsigned char v1_fir_sel;
    // M4_ARRAY_DESC("v1 band", "f32", M4_SIZE(1,2), M4_RANGE(0, 1), "0", M4_DIGIT(3), M4_DYNAMIC(0))
    float v1_band[2];
    // M4_ARRAY_DESC("vertical first iir filter", "s16", M4_SIZE(1,9), M4_RANGE(-2047,2047), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    short v1_iir_coe[9];
    // M4_ARRAY_DESC("vertical first fir filter", "s16", M4_SIZE(1,3), M4_RANGE(-2047,2047), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    short v1_fir_coe[3];
    // M4_ARRAY_DESC("v2 band", "f32", M4_SIZE(1,2), M4_RANGE(0, 1), "0", M4_DIGIT(3), M4_DYNAMIC(0))
    float v2_band[2];
    // M4_ARRAY_DESC("vertical second iir filter", "s16", M4_SIZE(1,3), M4_RANGE(-2047,2047), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    short v2_iir_coe[3];
    // M4_ARRAY_DESC("vertical second fir filter", "s16", M4_SIZE(1,3), M4_RANGE(-2047,2047), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    short v2_fir_coe[3];
    // M4_ARRAY_DESC("h1 band", "f32", M4_SIZE(1,2), M4_RANGE(0, 1), "0", M4_DIGIT(3), M4_DYNAMIC(0))
    float h1_band[2];
    // M4_ARRAY_DESC("horizontal first iir1 filter", "s16", M4_SIZE(1,6), M4_RANGE(-2047,2047), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    short h1_iir1_coe[6];
    // M4_ARRAY_DESC("horizontal first iir2 filter", "s16", M4_SIZE(1,6), M4_RANGE(-2047,2047), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    short h1_iir2_coe[6];
    // M4_ARRAY_DESC("h2 band", "f32", M4_SIZE(1,2), M4_RANGE(0, 1), "0", M4_DIGIT(3), M4_DYNAMIC(0))
    float h2_band[2];
    // M4_ARRAY_DESC("horizontal second iir1 filter", "s16", M4_SIZE(1,6), M4_RANGE(-2047,2047), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    short h2_iir1_coe[6];
    // M4_ARRAY_DESC("horizontal second iir2 filter", "s16", M4_SIZE(1,6), M4_RANGE(-2047,2047), "0", M4_DIGIT(0), M4_DYNAMIC(0))
    short h2_iir2_coe[6];
    // M4_NUMBER_DESC("ldg enable", "u8", M4_RANGE(0, 1), "0", M4_DIGIT(0))
    unsigned char ldg_en;
    // M4_NUMBER_DESC("vertical minluma thresh", "u8", M4_RANGE(0, 255), "0", M4_DIGIT(0))
    unsigned char ve_ldg_lumth_l;
    // M4_NUMBER_DESC("vertical gain for minluma", "u8", M4_RANGE(0, 255), "0", M4_DIGIT(0))
    unsigned char ve_ldg_gain_l;
    // M4_NUMBER_DESC("vertical slope low", "u16", M4_RANGE(0, 8191), "0", M4_DIGIT(0))
    unsigned short ve_ldg_gslp_l;
    // M4_NUMBER_DESC("vertical maxluma thresh", "u8", M4_RANGE(0, 255), "0", M4_DIGIT(0))
    unsigned char ve_ldg_lumth_h;
    // M4_NUMBER_DESC("vertical gain for maxluma", "u8", M4_RANGE(0, 255), "0", M4_DIGIT(0))
    unsigned char ve_ldg_gain_h;
    // M4_NUMBER_DESC("vertical slope high", "u16", M4_RANGE(0, 8191), "0", M4_DIGIT(0))
    unsigned short ve_ldg_gslp_h;
    // M4_NUMBER_DESC("horizontal minluma thresh", "u8", M4_RANGE(0, 255), "0", M4_DIGIT(0))
    unsigned char ho_ldg_lumth_l;
    // M4_NUMBER_DESC("horizontal gain for minluma", "u8", M4_RANGE(0, 255), "0", M4_DIGIT(0))
    unsigned char ho_ldg_gain_l;
    // M4_NUMBER_DESC("horizontal slope low", "u16", M4_RANGE(0, 8191), "0", M4_DIGIT(0))
    unsigned short ho_ldg_gslp_l;
    // M4_NUMBER_DESC("horizontal maxluma thresh", "u8", M4_RANGE(0, 255), "0", M4_DIGIT(0))
    unsigned char ho_ldg_lumth_h;
    // M4_NUMBER_DESC("horizontal gain for maxluma", "u8", M4_RANGE(0, 255), "0", M4_DIGIT(0))
    unsigned char ho_ldg_gain_h;
    // M4_NUMBER_DESC("horizontal slope high", "u16", M4_RANGE(0, 8191), "0", M4_DIGIT(0))
    unsigned short ho_ldg_gslp_h;
    // M4_NUMBER_DESC("vertical fv thresh", "u16", M4_RANGE(0, 4095), "0", M4_DIGIT(0))
    unsigned short v_fv_thresh;
    // M4_NUMBER_DESC("horizontal fv thresh", "u16", M4_RANGE(0, 4095), "0", M4_DIGIT(0))
    unsigned short h_fv_thresh;
    // M4_NUMBER_DESC("highlight thresh", "u16", M4_RANGE(0, 4095), "0", M4_DIGIT(0))
    unsigned short highlit_thresh;
    // M4_NUMBER_DESC("vertical fv ratio", "f32", M4_RANGE(0, 1), "0.5", M4_DIGIT(3))
    float v_fv_ratio;
} CalibDbV2_AfV30_MeasCfg_t;

typedef struct CalibDbV2_AfV30_IsoMeasCfg_s {
    // M4_NUMBER_MARK_DESC("iso", "f32", M4_RANGE(50, 204800), "50", M4_DIGIT(1), "index2")
    float iso;
    // M4_NUMBER_DESC("meas table index", "u32", M4_RANGE(0, 100), "0", M4_DIGIT(0))
    int idx;
    // M4_NUMBER_DESC("spotlight scene meas table index", "u32", M4_RANGE(0, 100), "0", M4_DIGIT(0))
    int spotlt_scene_idx;
} CalibDbV2_AfV30_IsoMeasCfg_t;

typedef struct CalibDbV2_AfV30_ZoomMeas_s {
    // M4_NUMBER_MARK_DESC("zoom index", "u32", M4_RANGE(0, 100000), "0", M4_DIGIT(0), "index1")
    int zoom_idx;
    // M4_STRUCT_LIST_DESC("meas iso config", M4_SIZE(1,13), "double_index_list")
    CalibDbV2_AfV30_IsoMeasCfg_t measiso[CALIBDBV2_MAX_ISO_LEVEL];
} CalibDbV2_AfV30_ZoomMeas_t;

typedef struct CalibDbV2_AFV30_Tuning_Para_s {
    // M4_ENUM_DESC("mode", "CalibDbV2_AF_MODE_t", "CalibDbV2_AF_MODE_CONTINUOUS_PICTURE")
    CalibDbV2_AF_MODE_t af_mode;
    // M4_NUMBER_DESC("win_h_offs", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short win_h_offs;
    // M4_NUMBER_DESC("win_v_offs", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short win_v_offs;
    // M4_NUMBER_DESC("win_h_size", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short win_h_size;
    // M4_NUMBER_DESC("win_v_size", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short win_v_size;
    // M4_NUMBER_DESC("win_h_offs in video", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short video_win_h_offs;
    // M4_NUMBER_DESC("win_v_offs in video", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short video_win_v_offs;
    // M4_NUMBER_DESC("win_h_size in video", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short video_win_h_size;
    // M4_NUMBER_DESC("win_v_size in video", "u16", M4_RANGE(0,2000), "0", M4_DIGIT(0),M4_HIDE(0))
    unsigned short video_win_v_size;
    // M4_STRUCT_DESC("fixed mode", "normal_ui_style")
    CalibDbV2_Af_DefCode_t fixed_mode;
    // M4_STRUCT_DESC("macro mode", "normal_ui_style")
    CalibDbV2_Af_DefCode_t macro_mode;
    // M4_STRUCT_DESC("infinity mode", "normal_ui_style")
    CalibDbV2_Af_DefCode_t infinity_mode;
    // M4_STRUCT_DESC("contrast af", "normal_ui_style")
    CalibDbV2_Af_Contrast_t contrast_af;
    // M4_STRUCT_DESC("video contrast af", "normal_ui_style")
    CalibDbV2_Af_Contrast_t video_contrast_af;
    // M4_STRUCT_DESC("laser af", "normal_ui_style")
    CalibDbV2_Af_Laser_t laser_af;
    // M4_STRUCT_DESC("pdaf", "normal_ui_style")
    CalibDbV2_Af_Pdaf_t pdaf;
    // M4_STRUCT_DESC("vcmcfg", "normal_ui_style")
    CalibDbV2_Af_VcmCfg_t vcmcfg;
    // M4_STRUCT_DESC("zoomfocus_tbl", "normal_ui_style")
    CalibDbV2_Af_ZoomFocusTbl_t zoomfocus_tbl;
    // M4_STRUCT_LIST_DESC("zoom meas", M4_SIZE_DYNAMIC, "double_index_list")
    CalibDbV2_AfV30_ZoomMeas_t *zoom_meas;
    int zoom_meas_len;
    // M4_STRUCT_LIST_DESC("meas config table", M4_SIZE_DYNAMIC, "normal_ui_style")
    CalibDbV2_AfV30_MeasCfg_t *meascfg_tbl;
    int meascfg_tbl_len;
} CalibDbV2_AFV30_Tuning_Para_t;

typedef struct {
    // M4_STRUCT_DESC("TuningPara", "normal_ui_style")
    CalibDbV2_AFV30_Tuning_Para_t TuningPara;
} CalibDbV2_AFV30_t;

RKAIQ_END_DECLARE

#endif
