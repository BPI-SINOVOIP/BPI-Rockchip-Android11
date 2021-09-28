/*
 * rk_aiq_algo_adpcc_itf.c
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

#include "rk_aiq_algo_types_int.h"
#include "adpcc/rk_aiq_algo_adpcc_itf.h"
#include "adpcc/rk_aiq_adpcc_algo.h"
#include "adpcc/rk_aiq_types_adpcc_algo_prvt.h"


RKAIQ_BEGIN_DECLARE


static XCamReturn
create_context(RkAiqAlgoContext **context, const AlgoCtxInstanceCfg* cfg)
{
    XCamReturn result = XCAM_RETURN_NO_ERROR;

    LOGI_ADPCC("%s: (enter)\n", __FUNCTION__ );
    AlgoCtxInstanceCfgInt *cfgInt = (AlgoCtxInstanceCfgInt*)cfg;

    AdpccContext_t* pAdpccCtx = NULL;
    AdpccResult_t ret = AdpccInit(&pAdpccCtx, cfgInt->calibv2);//load json paras
    if(ret != ADPCC_RET_SUCCESS) {
        result = XCAM_RETURN_ERROR_FAILED;
        LOGE_ADPCC("%s: Initializaion Adpcc failed (%d)\n", __FUNCTION__, ret);
    } else {
        *context = (RkAiqAlgoContext *)(pAdpccCtx);
    }

    LOGI_ADPCC("%s: (exit)\n", __FUNCTION__ );
    return result;
}

static XCamReturn
destroy_context(RkAiqAlgoContext *context)
{
    XCamReturn result = XCAM_RETURN_NO_ERROR;

    LOGI_ADPCC("%s: (enter)\n", __FUNCTION__ );

#if 1
    AdpccContext_t* pAdpccCtx = (AdpccContext_t*)context;
    AdpccResult_t ret = AdpccRelease(pAdpccCtx);
    if(ret != ADPCC_RET_SUCCESS) {
        result = XCAM_RETURN_ERROR_FAILED;
        LOGE_ADPCC("%s: release Adpcc failed (%d)\n", __FUNCTION__, ret);
    }
#endif

    LOGI_ADPCC("%s: (exit)\n", __FUNCTION__ );
    return result;
}

static XCamReturn
prepare(RkAiqAlgoCom* params)
{
    XCamReturn result = XCAM_RETURN_NO_ERROR;

    LOGE_ADPCC("%s: (enter)\n", __FUNCTION__ );

    AdpccContext_t* pAdpccCtx = (AdpccContext_t *)params->ctx;
    RkAiqAlgoConfigAdpccInt* pCfgParam = (RkAiqAlgoConfigAdpccInt*)params;
    AdpccConfig_t* pAdpccConfig = &pCfgParam->stAdpccConfig;
    pAdpccCtx->prepare_type = params->u.prepare.conf_type;

    if(!!(params->u.prepare.conf_type & RK_AIQ_ALGO_CONFTYPE_UPDATECALIB )) {
        AdpccResult_t ret = AdpccReloadPara(pAdpccCtx, pCfgParam->rk_com.u.prepare.calibv2);
        if(ret != ADPCC_RET_SUCCESS) {
            result = XCAM_RETURN_ERROR_FAILED;
            LOGE_ADPCC("%s: Adpcc Reload Para failed (%d)\n", __FUNCTION__, ret);
        }
    }

    LOGI_ADPCC("%s: (exit)\n", __FUNCTION__ );
    return result;
}

static XCamReturn
pre_process(const RkAiqAlgoCom* inparams, RkAiqAlgoResCom* outparams)
{
    return XCAM_RETURN_NO_ERROR;
}

static XCamReturn
processing(const RkAiqAlgoCom* inparams, RkAiqAlgoResCom* outparams)
{
    XCamReturn result = XCAM_RETURN_NO_ERROR;
    int iso;

    LOGI_ADPCC("%s: (enter)\n", __FUNCTION__ );

#if 1
    RkAiqAlgoProcAdpccInt* pAdpccProcParams = (RkAiqAlgoProcAdpccInt*)inparams;
    RkAiqAlgoProcResAdpccInt* pAdpccProcResParams = (RkAiqAlgoProcResAdpccInt*)outparams;
    AdpccContext_t* pAdpccCtx = (AdpccContext_t *)inparams->ctx;
    AdpccExpInfo_t stExpInfo;
    memset(&stExpInfo, 0x00, sizeof(AdpccExpInfo_t));

    LOGD_ADPCC("%s:%d init:%d hdr mode:%d  \n",
               __FUNCTION__, __LINE__,
               inparams->u.proc.init,
               pAdpccProcParams->hdr_mode);

    stExpInfo.hdr_mode = 0; //pAnrProcParams->hdr_mode;
    for(int i = 0; i < 3; i++) {
        stExpInfo.arPreResIso[i] = 50;
        stExpInfo.arPreResAGain[i] = 1.0;
        stExpInfo.arPreResDGain[i] = 1.0;
        stExpInfo.arPreResTime[i] = 0.01;

        stExpInfo.arProcResIso[i] = 50;
        stExpInfo.arProcResAGain[i] = 1.0;
        stExpInfo.arProcResDGain[i] = 1.0;
        stExpInfo.arProcResTime[i] = 0.01;
    }

    pAdpccCtx->isBlackSensor = pAdpccProcParams->rk_com.u.proc.is_bw_sensor;

    if(pAdpccProcParams->hdr_mode == RK_AIQ_WORKING_MODE_NORMAL) {
        stExpInfo.hdr_mode = 0;
    } else if(pAdpccProcParams->hdr_mode == RK_AIQ_ISP_HDR_MODE_2_FRAME_HDR
              || pAdpccProcParams->hdr_mode == RK_AIQ_ISP_HDR_MODE_2_LINE_HDR ) {
        stExpInfo.hdr_mode = 1;
    } else if(pAdpccProcParams->hdr_mode == RK_AIQ_ISP_HDR_MODE_3_FRAME_HDR
              || pAdpccProcParams->hdr_mode == RK_AIQ_ISP_HDR_MODE_3_LINE_HDR ) {
        stExpInfo.hdr_mode = 2;
    }

    RKAiqAecExpInfo_t* pAERes = pAdpccProcParams->rk_com.u.proc.curExp;

    if(pAERes != NULL) {
        if(pAdpccProcParams->hdr_mode == RK_AIQ_WORKING_MODE_NORMAL) {
            stExpInfo.arPreResAGain[0] = pAERes->LinearExp.exp_real_params.analog_gain;
            stExpInfo.arPreResDGain[0] = pAERes->LinearExp.exp_real_params.digital_gain;
            stExpInfo.arPreResTime[0] = pAERes->LinearExp.exp_real_params.integration_time;
            stExpInfo.arPreResIso[0] = stExpInfo.arPreResAGain[0] * 50;
        } else {
            for(int i = 0; i < 3; i++) {
                stExpInfo.arPreResAGain[i] = pAERes->HdrExp[i].exp_real_params.analog_gain;
                stExpInfo.arPreResDGain[i] = pAERes->HdrExp[i].exp_real_params.digital_gain;
                stExpInfo.arPreResTime[i] = pAERes->HdrExp[i].exp_real_params.integration_time;
                stExpInfo.arPreResIso[i] = stExpInfo.arPreResAGain[i] * stExpInfo.arPreResDGain[i] * 50;

                LOGD_ADPCC("%s:%d index:%d again:%f dgain:%f time:%f iso:%d hdr_mode:%d\n",
                           __FUNCTION__, __LINE__,
                           i,
                           stExpInfo.arPreResAGain[i],
                           stExpInfo.arPreResDGain[i],
                           stExpInfo.arPreResTime[i],
                           stExpInfo.arPreResIso[i],
                           stExpInfo.hdr_mode);
            }
        }
    } else {
        LOGE_ADPCC("%s:%d pAERes is NULL, so use default instead \n", __FUNCTION__, __LINE__);
    }

    AdpccResult_t ret = AdpccProcess(pAdpccCtx, &stExpInfo);
    if(ret != ADPCC_RET_SUCCESS) {
        result = XCAM_RETURN_ERROR_FAILED;
        LOGE_ADPCC("%s: processing Adpcc failed (%d)\n", __FUNCTION__, ret);
    }

    AdpccGetProcResult(pAdpccCtx, &pAdpccProcResParams->stAdpccProcResult);

    //sensor dpcc setting
    pAdpccProcResParams->adpcc_proc_res_com.SenDpccRes.enable = pAdpccCtx->SenDpccRes.enable;
    pAdpccProcResParams->adpcc_proc_res_com.SenDpccRes.total_dpcc = pAdpccCtx->SenDpccRes.total_dpcc;
    pAdpccProcResParams->adpcc_proc_res_com.SenDpccRes.cur_single_dpcc = pAdpccCtx->SenDpccRes.cur_single_dpcc;
    pAdpccProcResParams->adpcc_proc_res_com.SenDpccRes.cur_multiple_dpcc = pAdpccCtx->SenDpccRes.cur_multiple_dpcc;

#endif

    LOGI_ADPCC("%s: (exit)\n", __FUNCTION__ );
    return XCAM_RETURN_NO_ERROR;
}

static XCamReturn
post_process(const RkAiqAlgoCom * inparams, RkAiqAlgoResCom * outparams)
{
    return XCAM_RETURN_NO_ERROR;
}

RkAiqAlgoDescription g_RkIspAlgoDescAdpcc = {
    .common = {
        .version = RKISP_ALGO_ADPCC_VERSION,
        .vendor  = RKISP_ALGO_ADPCC_VENDOR,
        .description = RKISP_ALGO_ADPCC_DESCRIPTION,
        .type    = RK_AIQ_ALGO_TYPE_ADPCC,
        .id      = 0,
        .create_context  = create_context,
        .destroy_context = destroy_context,
    },
    .prepare = prepare,
    .pre_process = pre_process,
    .processing = processing,
    .post_process = post_process,
};

RKAIQ_END_DECLARE
