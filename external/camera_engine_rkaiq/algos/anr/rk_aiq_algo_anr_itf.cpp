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
#include "anr/rk_aiq_algo_anr_itf.h"
#include "anr/rk_aiq_anr_algo.h"
#include "shared_data_api_wrapper.h"

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
    ANRContext_t* pAnrCtx = NULL;
    #if(ANR_USE_JSON_PARA)
	ANRresult_t ret = ANRInit_json(&pAnrCtx, cfgInt->calibv2);
    #else
    ANRresult_t ret = ANRInit(&pAnrCtx, cfgInt->calib);
    #endif
    if(ret != ANR_RET_SUCCESS) {
        result = XCAM_RETURN_ERROR_FAILED;
        LOGE_ANR("%s: Initializaion ANR failed (%d)\n", __FUNCTION__, ret);
    } else {
        *context = (RkAiqAlgoContext *)(pAnrCtx);
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
    ANRContext_t* pAnrCtx = (ANRContext_t*)context;
    ANRresult_t ret = ANRRelease(pAnrCtx);
    if(ret != ANR_RET_SUCCESS) {
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

    ANRContext_t* pAnrCtx = (ANRContext_t *)params->ctx;
    RkAiqAlgoConfigAnrInt* pCfgParam = (RkAiqAlgoConfigAnrInt*)params;	
	pAnrCtx->prepare_type = params->u.prepare.conf_type;

	if(!!(params->u.prepare.conf_type & RK_AIQ_ALGO_CONFTYPE_UPDATECALIB )){
		#if(ANR_USE_JSON_PARA)
		void *pCalibDbV2 = (void*)(pCfgParam->rk_com.u.prepare.calibv2);
		CalibDbV2_BayerNrV1_t *bayernr_v1 = (CalibDbV2_BayerNrV1_t*)(CALIBDBV2_GET_MODULE_PTR((void*)pCalibDbV2, bayernr_v1));
		bayernr_calibdbV2_assign(&pAnrCtx->bayernr_v1, bayernr_v1);
		CalibDbV2_MFNR_t *mfnr_v1 = (CalibDbV2_MFNR_t*)(CALIBDBV2_GET_MODULE_PTR((void*)pCalibDbV2, mfnr_v1));
    	mfnr_calibdbV2_assign(&pAnrCtx->mfnr_v1, mfnr_v1);
		CalibDbV2_YnrV1_t *ynr_v1 = (CalibDbV2_YnrV1_t*)(CALIBDBV2_GET_MODULE_PTR((void*)pCalibDbV2, ynr_v1));
   		ynr_calibdbV2_assign(&pAnrCtx->ynr_v1, ynr_v1);
		CalibDbV2_UVNR_t *uvnr_v1 = (CalibDbV2_UVNR_t*)(CALIBDBV2_GET_MODULE_PTR((void*)pCalibDbV2, uvnr_v1));
    	uvnr_calibdbV2_assign(&pAnrCtx->uvnr_v1, uvnr_v1);
		#else
		void *pCalibDb = (void*)(pCfgParam->rk_com.u.prepare.calib);
        pAnrCtx->stBayernrCalib =
            *(CalibDb_BayerNr_2_t*)(CALIBDB_GET_MODULE_PTR((void*)pCalibDb, bayerNr));
        pAnrCtx->stUvnrCalib =
            *(CalibDb_UVNR_2_t*)(CALIBDB_GET_MODULE_PTR((void*)pCalibDb, uvnr));
        pAnrCtx->stYnrCalib =
            *(CalibDb_YNR_2_t*)(CALIBDB_GET_MODULE_PTR((void*)pCalibDb, ynr));
        pAnrCtx->stMfnrCalib =
            *(CalibDb_MFNR_2_t*)(CALIBDB_GET_MODULE_PTR((void*)pCalibDb, mfnr));
		#endif
		pAnrCtx->isIQParaUpdate = true;
    }

    ANRresult_t ret = ANRPrepare(pAnrCtx, &pCfgParam->stANRConfig);
    if(ret != ANR_RET_SUCCESS) {
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

    LOGI_ANR("%s: (enter)\n", __FUNCTION__ );
    ANRContext_t* pAnrCtx = (ANRContext_t *)inparams->ctx;

    RkAiqAlgoPreAnrInt* pAnrPreParams = (RkAiqAlgoPreAnrInt*)inparams;

    if (pAnrPreParams->rk_com.u.proc.gray_mode) {
        pAnrCtx->isGrayMode = true;
    }else {
        pAnrCtx->isGrayMode = false;
    }

    ANRresult_t ret = ANRPreProcess(pAnrCtx);
    if(ret != ANR_RET_SUCCESS) {
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

    LOGI_ANR("%s: (enter)\n", __FUNCTION__ );

#if 1
    RkAiqAlgoProcAnrInt* pAnrProcParams = (RkAiqAlgoProcAnrInt*)inparams;
    RkAiqAlgoProcResAnrInt* pAnrProcResParams = (RkAiqAlgoProcResAnrInt*)outparams;
    ANRContext_t* pAnrCtx = (ANRContext_t *)inparams->ctx;
    ANRExpInfo_t stExpInfo;
    memset(&stExpInfo, 0x00, sizeof(ANRExpInfo_t));

    LOGD_ANR("%s:%d init:%d hdr mode:%d  \n",
             __FUNCTION__, __LINE__,
             inparams->u.proc.init,
             pAnrProcParams->hdr_mode);

    stExpInfo.hdr_mode = 0; //pAnrProcParams->hdr_mode;
    for(int i = 0; i < 3; i++) {
        stExpInfo.arIso[i] = 50;
        stExpInfo.arAGain[i] = 1.0;
        stExpInfo.arDGain[i] = 1.0;
        stExpInfo.arTime[i] = 0.01;
    }

    if(pAnrProcParams->hdr_mode == RK_AIQ_WORKING_MODE_NORMAL) {
        stExpInfo.hdr_mode = 0;
    } else if(pAnrProcParams->hdr_mode == RK_AIQ_ISP_HDR_MODE_2_FRAME_HDR
              || pAnrProcParams->hdr_mode == RK_AIQ_ISP_HDR_MODE_2_LINE_HDR ) {
        stExpInfo.hdr_mode = 1;
    } else if(pAnrProcParams->hdr_mode == RK_AIQ_ISP_HDR_MODE_3_FRAME_HDR
              || pAnrProcParams->hdr_mode == RK_AIQ_ISP_HDR_MODE_3_LINE_HDR ) {
        stExpInfo.hdr_mode = 2;
    }
	stExpInfo.snr_mode = 0;

#if 1
    RKAiqAecExpInfo_t *preExp = pAnrProcParams->rk_com.u.proc.preExp;
    RKAiqAecExpInfo_t *curExp = pAnrProcParams->rk_com.u.proc.curExp;

    if(preExp != NULL && curExp != NULL) {
        stExpInfo.cur_snr_mode = curExp->CISFeature.SNR;
        stExpInfo.pre_snr_mode = preExp->CISFeature.SNR;
        if(pAnrProcParams->hdr_mode == RK_AIQ_WORKING_MODE_NORMAL) {
            stExpInfo.hdr_mode = 0;
            stExpInfo.arAGain[0] = curExp->LinearExp.exp_real_params.analog_gain;
            stExpInfo.arDGain[0] = curExp->LinearExp.exp_real_params.digital_gain;
            stExpInfo.arTime[0] = curExp->LinearExp.exp_real_params.integration_time;
			stExpInfo.arDcgMode[0] = curExp->LinearExp.exp_real_params.dcg_mode;
            stExpInfo.arIso[0] = stExpInfo.arAGain[0] * stExpInfo.arDGain[0] * 50;

			stExpInfo.preAGain[0] = preExp->LinearExp.exp_real_params.analog_gain;
            stExpInfo.preDGain[0] = preExp->LinearExp.exp_real_params.digital_gain;
            stExpInfo.preTime[0] = preExp->LinearExp.exp_real_params.integration_time;
			stExpInfo.preDcgMode[0] = preExp->LinearExp.exp_real_params.dcg_mode;
            stExpInfo.preIso[0] = stExpInfo.preAGain[0] * stExpInfo.preDGain[0] * 50;
            LOGD_ANR("anr: %s-%d, preExp(%f, %f, %f, %d, %d), curExp(%f, %f, %f, %d, %d)\n",
                    __FUNCTION__, __LINE__,
                    preExp->LinearExp.exp_real_params.analog_gain,
                    preExp->LinearExp.exp_real_params.integration_time,
                    preExp->LinearExp.exp_real_params.digital_gain,
                    preExp->LinearExp.exp_real_params.dcg_mode,
                    preExp->CISFeature.SNR,
                    curExp->LinearExp.exp_real_params.analog_gain,
                    curExp->LinearExp.exp_real_params.integration_time,
                    curExp->LinearExp.exp_real_params.digital_gain,
                    curExp->LinearExp.exp_real_params.dcg_mode,
                    curExp->CISFeature.SNR);

            /* test shared data api */
            RKAiqAecExpInfo_t *exp_param = nullptr;
            shared_data_api_get_exposure_params(inparams->frame_id, &exp_param);
            if (exp_param != nullptr)
                LOGD_ANR("%s: frame_id %d, get exp param: (%f, %f)",
                        __FUNCTION__,
                        inparams->frame_id,
                        exp_param->LinearExp.exp_real_params.analog_gain,
                        exp_param->LinearExp.exp_real_params.integration_time);
        } else {
            for(int i = 0; i < 3; i++) {
                stExpInfo.arAGain[i] =  curExp->HdrExp[i].exp_real_params.analog_gain,
                stExpInfo.arDGain[i] = curExp->HdrExp[i].exp_real_params.digital_gain;
                stExpInfo.arTime[i] = curExp->HdrExp[i].exp_real_params.integration_time;
				stExpInfo.arDcgMode[i] = curExp->HdrExp[i].exp_real_params.dcg_mode;
                stExpInfo.arIso[i] = stExpInfo.arAGain[i] * stExpInfo.arDGain[i] * 50;

				stExpInfo.preAGain[i] =  preExp->HdrExp[i].exp_real_params.analog_gain,
                stExpInfo.preDGain[i] = preExp->HdrExp[i].exp_real_params.digital_gain;
                stExpInfo.preTime[i] = preExp->HdrExp[i].exp_real_params.integration_time;
				stExpInfo.preDcgMode[i] = preExp->HdrExp[i].exp_real_params.dcg_mode;
                stExpInfo.preIso[i] = stExpInfo.preAGain[i] * stExpInfo.preDGain[i] * 50;

                LOGD_ANR("%s:%d index:%d again:%f %f dgain:%f %f time:%f %f iso:%d %d hdr_mode:%d  \n",
                         __FUNCTION__, __LINE__,
                         i,
                         stExpInfo.preAGain[i], stExpInfo.arAGain[i],
                         stExpInfo.preDGain[i], stExpInfo.arDGain[i],
                         stExpInfo.preTime[i], stExpInfo.arTime[i],
                         stExpInfo.preIso[i], stExpInfo.arIso[i],
                         stExpInfo.hdr_mode);
            }
        }
    } else {
        LOGE_ANR("%s:%d preExp(%p) or curExp(%p) is NULL, so use default instead \n",
                 __FUNCTION__, __LINE__, preExp, curExp);
    }

#if 0
    static int anr_cnt = 0;
    anr_cnt++;

    if(anr_cnt % 50 == 0) {
        for(int i = 0; i < stExpInfo.hdr_mode + 1; i++) {
            printf("%s:%d index:%d again:%f dgain:%f time:%f iso:%d hdr_mode:%d\n",
                   __FUNCTION__, __LINE__,
                   i,
                   stExpInfo.arAGain[i],
                   stExpInfo.arDGain[i],
                   stExpInfo.arTime[i],
                   stExpInfo.arIso[i],
                   stExpInfo.hdr_mode);
        }
    }
#endif


#endif

    ANRresult_t ret = ANRProcess(pAnrCtx, &stExpInfo);
    if(ret != ANR_RET_SUCCESS) {
        result = XCAM_RETURN_ERROR_FAILED;
        LOGE_ANR("%s: processing ANR failed (%d)\n", __FUNCTION__, ret);
    }

    ANRGetProcResult(pAnrCtx, &pAnrProcResParams->stAnrProcResult);
#endif

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

RkAiqAlgoDescription g_RkIspAlgoDescAnr = {
    .common = {
        .version = RKISP_ALGO_ANR_VERSION,
        .vendor  = RKISP_ALGO_ANR_VENDOR,
        .description = RKISP_ALGO_ANR_DESCRIPTION,
        .type    = RK_AIQ_ALGO_TYPE_ANR,
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
