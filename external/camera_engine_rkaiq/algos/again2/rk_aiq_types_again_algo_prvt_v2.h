/*
 *rk_aiq_types_alsc_algo_prvt.h
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

#ifndef _RK_AIQ_TYPES_AGAIN_ALGO_PRVT_V2_H_
#define _RK_AIQ_TYPES_AGAIN_ALGO_PRVT_V2_H_

#include "again2/rk_aiq_types_again_algo_int_v2.h"
#include "RkAiqCalibDbTypes.h"
#include "xcam_log.h"
#include "xcam_common.h"
#include "RkAiqCalibDbTypesV2.h"
#include "RkAiqCalibDbV2Helper.h"

//RKAIQ_BEGIN_DECLARE


typedef struct Again_GainState_V2_s {
    int gain_stat_full_last;
    int gainState;
    int gainState_last;
    float gain_th0[2];
    float gain_th1[2];
    float gain_cur;
    float ratio;
} Again_GainState_V2_t;


//anr context
typedef struct Again_Context_V2_s {
    Again_ExpInfo_V2_t stExpInfo;
    Again_State_V2_t eState;
    Again_OPMode_V2_t eMode;

    Again_Auto_Attr_V2_t stAuto;
    Again_Manual_Attr_V2_t stManual;

    Again_ProcResult_V2_t stProcResult;

    bool isIQParaUpdate;
    bool isGrayMode;
    Again_ParamMode_V2_t eParamMode;

    int rawWidth;
    int rawHeight;

    Again_GainState_V2_t stGainState;
    int prepare_type;

    int isReCalculate;

    CalibDbV2_GainV2_t gain_v2;

} Again_Context_V2_t;






//RKAIQ_END_DECLARE

#endif


