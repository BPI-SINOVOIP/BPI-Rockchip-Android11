/*
 * rk_aiq_algo_camgroup_awb_itf.c
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

#include "rk_aiq_algo_camgroup_again_itf.h"
#include "rk_aiq_algo_camgroup_types.h"
#include "rk_aiq_types_camgroup_again_prvt.h"
#include "again2/rk_aiq_again_algo_itf_v2.h"
#include "again2/rk_aiq_again_algo_v2.h"



RKAIQ_BEGIN_DECLARE

static again_hardware_version_t g_again_hw_ver = AGAIN_HARDWARE_V2;

static XCamReturn groupAgainCreateCtx(RkAiqAlgoContext **context, const AlgoCtxInstanceCfg* cfg)
{
    LOGI_ANR("%s enter \n", __FUNCTION__ );

    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    CamGroup_Again_Contex_t *again_group_contex = NULL;
    AlgoCtxInstanceCfgCamGroup *cfgInt = (AlgoCtxInstanceCfgCamGroup*)cfg;
    //g_again_hw_ver = (again_hardware_version_t)(cfgInt->cfg_com.module_hw_version);

    if(CHECK_ISP_HW_V3X()) {
        g_again_hw_ver = AGAIN_HARDWARE_V2;
    } else {
        g_again_hw_ver = AGAIN_HARDWARE_MIN;
    }

    if(g_again_hw_ver == AGAIN_HARDWARE_V2) {
        again_group_contex = (CamGroup_Again_Contex_t*)malloc(sizeof(CamGroup_Again_Contex_t));
#if AGAIN_USE_JSON_FILE_V2
        Again_result_V2_t ret_v2 = AGAINV2_RET_SUCCESS;
        ret_v2 = Again_Init_V2(&(again_group_contex->again_contex_v2), (CamCalibDbV2Context_t *)cfgInt->s_calibv2);
        if(ret_v2 != AGAINV2_RET_SUCCESS) {
            ret = XCAM_RETURN_ERROR_FAILED;
            LOGE_ANR("%s: Initializaion ANR failed (%d)\n", __FUNCTION__, ret);
        }
#endif
    }
    else {
        ret = XCAM_RETURN_ERROR_FAILED;
        LOGE_ANR("module_hw_version of again (%d) is invalid!!!!", g_again_hw_ver);
    }

    if(ret != XCAM_RETURN_NO_ERROR) {
        LOGE_ANR("%s: Initializaion group gain failed (%d)\n", __FUNCTION__, ret);
    } else {
        // to do got againSurrViewClib and initinal paras for for surround view
        again_group_contex->group_CalibV2.groupMethod = CalibDbV2_CAMGROUP_AGAIN_METHOD_MEAN;// to do from json
        again_group_contex->camera_Num = cfgInt->camIdArrayLen;

        *context = (RkAiqAlgoContext *)(again_group_contex);

        LOGI_ANR("%s:%d surrViewMethod(1-mean):%d, cameraNum %d \n",
                 __FUNCTION__, __LINE__,
                 again_group_contex->group_CalibV2.groupMethod,
                 again_group_contex->camera_Num);
    }

    LOGI_ANR("%s exit ret:%d\n", __FUNCTION__, ret);
    return ret;

}

static XCamReturn groupAgainDestroyCtx(RkAiqAlgoContext *context)
{
    LOGI_ANR("%s enter \n", __FUNCTION__ );

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    CamGroup_Again_Contex_t *again_group_contex = (CamGroup_Again_Contex_t*)context;

    if(g_again_hw_ver == AGAIN_HARDWARE_V2) {
        Again_result_V2_t ret_v2 = AGAINV2_RET_SUCCESS;
        ret_v2 = Again_Release_V2(again_group_contex->again_contex_v2);
        if(ret_v2 != AGAINV2_RET_SUCCESS) {
            ret = XCAM_RETURN_ERROR_FAILED;
            LOGE_ANR("%s: Initializaion ANR failed (%d)\n", __FUNCTION__, ret);
        }
    }
    else {
        ret = XCAM_RETURN_ERROR_FAILED;
        LOGE_ANR("module_hw_version of awb (%d) is isvalid!!!!", g_again_hw_ver);
    }

    if(ret != XCAM_RETURN_NO_ERROR) {
        LOGE_ANR("%s: release ANR failed (%d)\n", __FUNCTION__, ret);
    } else {
        free(again_group_contex);
    }


    LOGI_ANR("%s exit ret:%d\n", __FUNCTION__, ret);
    return ret;
}

static XCamReturn groupAgainPrepare(RkAiqAlgoCom* params)
{
    LOGI_ANR("%s enter \n", __FUNCTION__ );

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    CamGroup_Again_Contex_t * again_group_contex = (CamGroup_Again_Contex_t *)params->ctx;
    RkAiqAlgoCamGroupPrepare* para = (RkAiqAlgoCamGroupPrepare*)params;

    if(g_again_hw_ver == AGAIN_HARDWARE_V2) {
        Again_Context_V2_t * again_contex_v2 = again_group_contex->again_contex_v2;
        if(!!(params->u.prepare.conf_type & RK_AIQ_ALGO_CONFTYPE_UPDATECALIB )) {
#if AGAIN_USE_JSON_FILE_V2
#if 1
            void *pCalibDbV2 = (void*)(para->s_calibv2);
            CalibDbV2_GainV2_t * pcalibdbV2_gain_v2 =
                (CalibDbV2_GainV2_t *)(CALIBDBV2_GET_MODULE_PTR((CamCalibDbV2Context_t*)pCalibDbV2, gain_v2));

            again_contex_v2->gain_v2 = *pcalibdbV2_gain_v2;
            again_contex_v2->isIQParaUpdate = true;
            again_contex_v2->isReCalculate |= 1;
#endif
#endif
        }
        Again_Config_V2_t stAgainConfigV2;
        Again_result_V2_t ret_v2 = AGAINV2_RET_SUCCESS;
        ret_v2 = Again_Prepare_V2(again_contex_v2, &stAgainConfigV2);
        if(ret_v2 != AGAINV2_RET_SUCCESS) {
            ret = XCAM_RETURN_ERROR_FAILED;
            LOGE_ANR("%s: config ANR failed (%d)\n", __FUNCTION__, ret);
        }
    }
    else {
        ret = XCAM_RETURN_ERROR_FAILED;
        LOGE_ANR("module_hw_version of awb (%d) is isvalid!!!!", g_again_hw_ver);
    }

    LOGI_ANR("%s exit ret:%d\n", __FUNCTION__, ret);
    return ret;
}

static XCamReturn groupAgainProcessing(const RkAiqAlgoCom* inparams, RkAiqAlgoResCom* outparams)
{
    LOGI_ANR("%s enter \n", __FUNCTION__ );
    LOGI_ANR("----------------------------------------------frame_id (%d)----------------------------------------------\n", inparams->frame_id);

    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RkAiqAlgoCamGroupProcIn* procParaGroup = (RkAiqAlgoCamGroupProcIn*)inparams;
    RkAiqAlgoCamGroupProcOut* procResParaGroup = (RkAiqAlgoCamGroupProcOut*)outparams;
    CamGroup_Again_Contex_t * again_group_contex = (CamGroup_Again_Contex_t *)inparams->ctx;
    int deltaIso = 0;

    //method error
    if (again_group_contex->group_CalibV2.groupMethod <= CalibDbV2_CAMGROUP_AGAIN_METHOD_MIN
            ||  again_group_contex->group_CalibV2.groupMethod >=  CalibDbV2_CAMGROUP_AGAIN_METHOD_MAX) {
        return (ret);
    }

    //group empty
    if(procParaGroup->camgroupParmasArray == nullptr) {
        LOGE_ANR("camgroupParmasArray is null");
        return(XCAM_RETURN_ERROR_FAILED);
    }

    //get cur ae exposure
    Again_ExpInfo_V2_t stExpInfoV2;
    memset(&stExpInfoV2, 0x00, sizeof(stExpInfoV2));
    stExpInfoV2.hdr_mode = 0; //pAnrProcParams->hdr_mode;
    stExpInfoV2.snr_mode = 0;
    for(int i = 0; i < 3; i++) {
        stExpInfoV2.arIso[i] = 50;
        stExpInfoV2.arAGain[i] = 1.0;
        stExpInfoV2.arDGain[i] = 1.0;
        stExpInfoV2.arTime[i] = 0.01;
    }


    //merge ae result, iso mean value
    rk_aiq_singlecam_3a_result_t* scam_3a_res = procParaGroup->camgroupParmasArray[0];
    if(scam_3a_res->aec._bEffAecExpValid) {
        RKAiqAecExpInfo_t* pCurExp = &scam_3a_res->aec._effAecExpInfo;
        stExpInfoV2.snr_mode = pCurExp->CISFeature.SNR;
        if((rk_aiq_working_mode_t)procParaGroup->working_mode == RK_AIQ_WORKING_MODE_NORMAL) {
            stExpInfoV2.hdr_mode = 0;
            stExpInfoV2.arAGain[0] = pCurExp->LinearExp.exp_real_params.analog_gain;
            stExpInfoV2.arDGain[0] = pCurExp->LinearExp.exp_real_params.digital_gain;
            stExpInfoV2.arTime[0] = pCurExp->LinearExp.exp_real_params.integration_time;
            stExpInfoV2.arIso[0] = stExpInfoV2.arAGain[0] * stExpInfoV2.arDGain[0] * 50;

        } else {
            if(procParaGroup->working_mode == RK_AIQ_ISP_HDR_MODE_2_FRAME_HDR
                    || procParaGroup->working_mode == RK_AIQ_ISP_HDR_MODE_2_LINE_HDR)
                stExpInfoV2.hdr_mode = 1;
            else if (procParaGroup->working_mode == RK_AIQ_ISP_HDR_MODE_3_FRAME_HDR
                     || procParaGroup->working_mode == RK_AIQ_ISP_HDR_MODE_3_LINE_HDR)
                stExpInfoV2.hdr_mode = 2;
            else {
                stExpInfoV2.hdr_mode = 0;
                LOGE_ANR("mode error\n");
            }

            for(int i = 0; i < 3; i++) {
                stExpInfoV2.arAGain[i] = pCurExp->HdrExp[i].exp_real_params.analog_gain;
                stExpInfoV2.arDGain[i] = pCurExp->HdrExp[i].exp_real_params.digital_gain;
                stExpInfoV2.arTime[i] = pCurExp->HdrExp[i].exp_real_params.integration_time;
                stExpInfoV2.arIso[i] = stExpInfoV2.arAGain[i] * stExpInfoV2.arDGain[i] * 50;
            }

        }
    } else {
        LOGW("fail to get sensor gain form AE module,use default value ");
    }



    if(g_again_hw_ver == AGAIN_HARDWARE_V2) {
        Again_Context_V2_t * again_contex_v2 = again_group_contex->again_contex_v2;
        Again_ProcResult_V2_t stAgainResultV2;
        deltaIso = abs(stExpInfoV2.arIso[stExpInfoV2.hdr_mode] - again_contex_v2->stExpInfo.arIso[stExpInfoV2.hdr_mode]);
#if 0
        if(deltaIso > AGAIN_RECALCULATE_DELTA_ISO) {
            again_contex_v2->isReCalculate |= 1;
        }
#endif

        if(again_contex_v2->isReCalculate) {
            Again_result_V2_t ret_v2 = AGAINV2_RET_SUCCESS;
            ret_v2 = Again_Process_V2(again_contex_v2, &stExpInfoV2);
            if(ret_v2 != AGAINV2_RET_SUCCESS) {
                ret = XCAM_RETURN_ERROR_FAILED;
                LOGE_ANR("%s: processing ANR failed (%d)\n", __FUNCTION__, ret);
            }
            Again_GetProcResult_V2(again_contex_v2, &again_contex_v2->stProcResult);
            again_contex_v2->stProcResult.isNeedUpdate = true;
            LOGD_ANR("recalculate: %d delta_iso:%d \n ", again_contex_v2->isReCalculate, deltaIso);
        } else {
            again_contex_v2->stProcResult.isNeedUpdate = false;
        }

        for (int i = 0; i < procResParaGroup->arraySize; i++) {
            *(procResParaGroup->camgroupParmasArray[i]->again._again_procRes_v2) = again_contex_v2->stProcResult.stFix;
        }
        again_contex_v2->isReCalculate = 0;
    }
    else {
        ret = XCAM_RETURN_ERROR_FAILED;
        LOGE_ANR("module_hw_version of awb (%d) is isvalid!!!!", g_again_hw_ver);
    }

    LOGI_ANR("%s exit\n", __FUNCTION__);
    return ret;
}

RkAiqAlgoDescription g_RkIspAlgoDescCamgroupAgain = {
    .common = {
        .version = RKISP_ALGO_CAMGROUP_AGAIN_VERSION,
        .vendor  = RKISP_ALGO_CAMGROUP_AGAIN_VENDOR,
        .description = RKISP_ALGO_CAMGROUP_AGAIN_DESCRIPTION,
        .type    = RK_AIQ_ALGO_TYPE_AGAIN,
        .id      = 0,
        .create_context  = groupAgainCreateCtx,
        .destroy_context = groupAgainDestroyCtx,
    },
    .prepare = groupAgainPrepare,
    .pre_process = NULL,
    .processing = groupAgainProcessing,
    .post_process = NULL,
};

RKAIQ_END_DECLARE
