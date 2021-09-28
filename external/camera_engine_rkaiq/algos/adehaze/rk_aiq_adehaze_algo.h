/*
 * rk_aiq_adehaze_algo.h
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

#ifndef __RK_AIQ_ADEHAZE_ALGO_H__
#define __RK_AIQ_ADEHAZE_ALGO_H__

#include "rk_aiq_comm.h"
#include "RkAiqCalibDbTypes.h"
#include "RkAiqCalibDbV2Helper.h"
#include "adehaze/rk_aiq_types_adehaze_algo_prvt.h"
#include "rk_aiq_types_adehaze_stat.h"
#include "rk_aiq_algo_types_int.h"



RKAIQ_BEGIN_DECLARE

void AdehazeGetStats(AdehazeHandle_t* pAdehazeCtx, rkisp_adehaze_stats_t* ROData);
XCamReturn AdehazeInit(AdehazeHandle_t** para, CamCalibDbV2Context_t* calib);
XCamReturn AdehazeRelease(AdehazeHandle_t* para);
XCamReturn AdehazeProcess(AdehazeHandle_t* para, AdehazeVersion_t version);
bool AdehazeByPassProcessing(AdehazeHandle_t* pAdehazeCtx);
void AdehazeGetEnvLv(AdehazeHandle_t* pAdehazeCtx, RkAiqAlgoPreResAeInt* pAecPreRes);


RKAIQ_END_DECLARE

#endif //_RK_AIQ_ALGO_ACPRC_ITF_H_
