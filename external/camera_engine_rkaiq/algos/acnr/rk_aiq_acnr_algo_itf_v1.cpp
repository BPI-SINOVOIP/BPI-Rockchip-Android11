/*
 * rk_aiq_algo_anr_itf.c
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
#include "acnr/rk_aiq_acnr_algo_itf_v1.h"
#include "acnr/rk_aiq_acnr_algo_v1.h"

RKAIQ_BEGIN_DECLARE

typedef struct _RkAiqAlgoContext {
    void* place_holder[0];
} RkAiqAlgoContext;


static XCamReturn
create_context(RkAiqAlgoContext **context, const AlgoCtxInstanceCfg* cfg)
{

    XCamReturn result = XCAM_RETURN_NO_ERROR;
    AlgoCtxInstanceCfgInt *cfgInt = (AlgoCtxInstanceCfgInt*)cfg;
    LOGI_ANR("%s: (enter)\n", __FUNCTION__ );

#if 1
    Acnr_Context_V1_t* pAcnrCtx = NULL;
#if ACNR_USE_JSON_FILE_V1
    Acnr_result_t ret = Acnr_Init_V1(&pAcnrCtx, cfgInt->calibv2);
#else
    Acnr_result_t ret = Acnr_Init_V1(&pAcnrCtx, cfgInt->calib);
#endif

    if(ret != ACNR_RET_SUCCESS) {
        result = XCAM_RETURN_ERROR_FAILED;
        LOGE_ANR("%s: Initializaion ANR failed (%d)\n", __FUNCTION__, ret);
    } else {
        *context = (RkAiqAlgoContext *)(pAcnrCtx);
    }
#endif

    LOGI_ANR("%s: (exit)\n", __FUNCTION__ );
    return result;
}

static XCamReturn
destroy_context(RkAiqAlgoContext *context)
{
    XCamReturn result = XCAM_RETURN_NO_ERROR;

    LOGI_ANR("%s: (enter)\n", __FUNCTION__ );

#if 1
    Acnr_Context_V1_t* pAcnrCtx = (Acnr_Context_V1_t*)context;
    Acnr_result_t ret = Acnr_Release_V1(pAcnrCtx);
    if(ret != ACNR_RET_SUCCESS) {
        result = XCAM_RETURN_ERROR_FAILED;
        LOGE_ANR("%s: release ANR failed (%d)\n", __FUNCTION__, ret);
    }
#endif

    LOGI_ANR("%s: (exit)\n", __FUNCTION__ );
    return result;
}

static XCamReturn
prepare(RkAiqAlgoCom* params)
{
    XCamReturn result = XCAM_RETURN_NO_ERROR;

    LOGI_ANR("%s: (enter)\n", __FUNCTION__ );

    Acnr_Context_V1_t* pAcnrCtx = (Acnr_Context_V1_t *)params->ctx;
    RkAiqAlgoConfigAcnrV1Int* pCfgParam = (RkAiqAlgoConfigAcnrV1Int*)params;
    pAcnrCtx->prepare_type = params->u.prepare.conf_type;

    if(!!(params->u.prepare.conf_type & RK_AIQ_ALGO_CONFTYPE_UPDATECALIB )) {
#if ACNR_USE_JSON_FILE_V1
        void *pCalibDbV2 = (void*)(pCfgParam->rk_com.u.prepare.calibv2);
        CalibDbV2_CNR_t *cnr_v1 =
            (CalibDbV2_CNR_t*)(CALIBDBV2_GET_MODULE_PTR((void*)pCalibDbV2, cnr_v1));
        pAcnrCtx->cnr_v1 = *cnr_v1;
#else
        void *pCalibDb = (void*)(pCfgParam->rk_com.u.prepare.calib);
        pAcnrCtx->list_cnr_v1 =
            (struct list_head*)(CALIBDB_GET_MODULE_PTR((void*)pCalibDb, uvnr));

#endif
        pAcnrCtx->isIQParaUpdate = true;
        pAcnrCtx->isReCalculate |= 1;

    }
    Acnr_result_t ret = Acnr_Prepare_V1(pAcnrCtx, &pCfgParam->stAcnrConfig);
    if(ret != ACNR_RET_SUCCESS) {
        result = XCAM_RETURN_ERROR_FAILED;
        LOGE_ANR("%s: config ANR failed (%d)\n", __FUNCTION__, ret);
    }

    LOGI_ANR("%s: (exit)\n", __FUNCTION__ );
    return result;
}

static XCamReturn
pre_process(const RkAiqAlgoCom* inparams, RkAiqAlgoResCom* outparams)
{
    XCamReturn result = XCAM_RETURN_NO_ERROR;
    bool oldGrayMode = false;

    LOGI_ANR("%s: (enter)\n", __FUNCTION__ );
    Acnr_Context_V1_t* pAcnrCtx = (Acnr_Context_V1_t *)inparams->ctx;

    RkAiqAlgoPreAcnrV1Int* pAnrPreParams = (RkAiqAlgoPreAcnrV1Int*)inparams;

    oldGrayMode = pAcnrCtx->isGrayMode;
    if (pAnrPreParams->rk_com.u.proc.gray_mode) {
        pAcnrCtx->isGrayMode = true;
    } else {
        pAcnrCtx->isGrayMode = false;
    }

    if(oldGrayMode != pAcnrCtx->isGrayMode) {
        pAcnrCtx->isReCalculate |= 1;
    }

    Acnr_result_t ret = Acnr_PreProcess_V1(pAcnrCtx);
    if(ret != ACNR_RET_SUCCESS) {
        result = XCAM_RETURN_ERROR_FAILED;
        LOGE_ANR("%s: ANRPreProcess failed (%d)\n", __FUNCTION__, ret);
    }

    LOGI_ANR("%s: (exit)\n", __FUNCTION__ );
    return result;
}

static XCamReturn
processing(const RkAiqAlgoCom* inparams, RkAiqAlgoResCom* outparams)
{
    XCamReturn result = XCAM_RETURN_NO_ERROR;
    int DeltaISO = 0;
    LOGI_ANR("%s: (enter)\n", __FUNCTION__ );

#if 1
    RkAiqAlgoProcAcnrV1Int* pAcnrProcParams = (RkAiqAlgoProcAcnrV1Int*)inparams;
    RkAiqAlgoProcResAcnrV1Int* pAcnrProcResParams = (RkAiqAlgoProcResAcnrV1Int*)outparams;
    Acnr_Context_V1_t* pAcnrCtx = (Acnr_Context_V1_t *)inparams->ctx;
    Acnr_ExpInfo_t stExpInfo;
    memset(&stExpInfo, 0x00, sizeof(Acnr_ExpInfo_t));

    LOGD_ANR("%s:%d init:%d hdr mode:%d  \n",
             __FUNCTION__, __LINE__,
             inparams->u.proc.init,
             pAcnrProcParams->hdr_mode);

    stExpInfo.hdr_mode = 0; //pAnrProcParams->hdr_mode;
    for(int i = 0; i < 3; i++) {
        stExpInfo.arIso[i] = 50;
        stExpInfo.arAGain[i] = 1.0;
        stExpInfo.arDGain[i] = 1.0;
        stExpInfo.arTime[i] = 0.01;
    }

    if(pAcnrProcParams->hdr_mode == RK_AIQ_WORKING_MODE_NORMAL) {
        stExpInfo.hdr_mode = 0;
    } else if(pAcnrProcParams->hdr_mode == RK_AIQ_ISP_HDR_MODE_2_FRAME_HDR
              || pAcnrProcParams->hdr_mode == RK_AIQ_ISP_HDR_MODE_2_LINE_HDR ) {
        stExpInfo.hdr_mode = 1;
    } else if(pAcnrProcParams->hdr_mode == RK_AIQ_ISP_HDR_MODE_3_FRAME_HDR
              || pAcnrProcParams->hdr_mode == RK_AIQ_ISP_HDR_MODE_3_LINE_HDR ) {
        stExpInfo.hdr_mode = 2;
    }
    stExpInfo.snr_mode = 0;

#if 1// TODO Merge:
    XCamVideoBuffer* xCamAePreRes = pAcnrProcParams->rk_com.u.proc.res_comb->ae_pre_res;
    RkAiqAlgoPreResAeInt* pAEPreRes = nullptr;
    if (xCamAePreRes) {
        // xCamAePreRes->ref(xCamAePreRes);
        pAEPreRes = (RkAiqAlgoPreResAeInt*)xCamAePreRes->map(xCamAePreRes);
        if (!pAEPreRes) {
            LOGE_ANR("ae pre result is null");
        } else {
        }
        // xCamAePreRes->unref(xCamAePreRes);
    }
#endif


    RKAiqAecExpInfo_t *curExp = pAcnrProcParams->rk_com.u.proc.curExp;
    if(curExp != NULL) {
        stExpInfo.snr_mode = curExp->CISFeature.SNR;
        if(pAcnrProcParams->hdr_mode == RK_AIQ_WORKING_MODE_NORMAL) {
            stExpInfo.hdr_mode = 0;
            stExpInfo.arAGain[0] = curExp->LinearExp.exp_real_params.analog_gain;
            stExpInfo.arDGain[0] = curExp->LinearExp.exp_real_params.digital_gain;
            stExpInfo.arTime[0] = curExp->LinearExp.exp_real_params.integration_time;
            stExpInfo.arIso[0] = stExpInfo.arAGain[0] * stExpInfo.arDGain[0] * 50;
        } else {
            for(int i = 0; i < 3; i++) {
                stExpInfo.arAGain[i] = curExp->HdrExp[i].exp_real_params.analog_gain;
                stExpInfo.arDGain[i] = curExp->HdrExp[i].exp_real_params.digital_gain;
                stExpInfo.arTime[i] = curExp->HdrExp[i].exp_real_params.integration_time;
                stExpInfo.arIso[i] = stExpInfo.arAGain[i] * stExpInfo.arDGain[i] * 50;

                LOGD_ANR("%s:%d index:%d again:%f dgain:%f time:%f iso:%d hdr_mode:%d\n",
                         __FUNCTION__, __LINE__,
                         i,
                         stExpInfo.arAGain[i],
                         stExpInfo.arDGain[i],
                         stExpInfo.arTime[i],
                         stExpInfo.arIso[i],
                         stExpInfo.hdr_mode);
            }
        }
    } else {
        LOGE_ANR("%s:%d curExp is NULL, so use default instead \n", __FUNCTION__, __LINE__);
    }

#if 0
    static int anr_cnt = 0;
    anr_cnt++;

    if(anr_cnt % 50 == 0) {
        for(int i = 0; i < stExpInfo.hdr_mode + 1; i++) {
            printf("%s:%d index:%d again:%f dgain:%f time:%f iso:%d hdr_mode:%d snr_mode:%d\n",
                   __FUNCTION__, __LINE__,
                   i,
                   stExpInfo.arAGain[i],
                   stExpInfo.arDGain[i],
                   stExpInfo.arTime[i],
                   stExpInfo.arIso[i],
                   stExpInfo.hdr_mode,
                   stExpInfo.snr_mode);
        }
    }
#endif

    DeltaISO = abs(stExpInfo.arIso[stExpInfo.hdr_mode] - pAcnrCtx->stExpInfo.arIso[pAcnrCtx->stExpInfo.hdr_mode]);
    if(DeltaISO > ACNRV1_RECALCULATE_DELTA_ISO) {
        pAcnrCtx->isReCalculate |= 1;
    }

    if(pAcnrCtx->isReCalculate) {
        Acnr_result_t ret = Acnr_Process_V1(pAcnrCtx, &stExpInfo);
        if(ret != ACNR_RET_SUCCESS) {
            result = XCAM_RETURN_ERROR_FAILED;
            LOGE_ANR("%s: processing ANR failed (%d)\n", __FUNCTION__, ret);
        }

        Acnr_GetProcResult_V1(pAcnrCtx, &pAcnrProcResParams->stAcnrProcResult);
        pAcnrProcResParams->stAcnrProcResult.isNeedUpdate = true;
        LOGD_ANR("recalculate: %d delta_iso:%d \n ", pAcnrCtx->isReCalculate, DeltaISO);
    } else {
        pAcnrProcResParams->stAcnrProcResult.isNeedUpdate = false;
    }
#endif

    pAcnrCtx->isReCalculate = 0;
    LOGI_ANR("%s: (exit)\n", __FUNCTION__ );
    return XCAM_RETURN_NO_ERROR;
}

static XCamReturn
post_process(const RkAiqAlgoCom* inparams, RkAiqAlgoResCom* outparams)
{
    LOGI_ANR("%s: (enter)\n", __FUNCTION__ );

    //nothing todo now

    LOGI_ANR("%s: (exit)\n", __FUNCTION__ );
    return XCAM_RETURN_NO_ERROR;
}

RkAiqAlgoDescription g_RkIspAlgoDescAcnrV1 = {
    .common = {
        .version = RKISP_ALGO_ACNR_VERSION_V1,
        .vendor  = RKISP_ALGO_ACNR_VENDOR_V1,
        .description = RKISP_ALGO_ACNR_DESCRIPTION_V1,
        .type    = RK_AIQ_ALGO_TYPE_ACNR,
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
