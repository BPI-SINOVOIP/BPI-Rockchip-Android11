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

#include "rk_aiq_algo_camgroup_atnr_itf.h"
#include "rk_aiq_algo_camgroup_types.h"
#include "rk_aiq_types_camgroup_atnr_prvt.h"
#include "abayertnr2/rk_aiq_abayertnr_algo_itf_v2.h"
#include "abayertnr2/rk_aiq_abayertnr_algo_v2.h"
#include "arawnr2/rk_aiq_abayernr_algo_itf_v2.h"
#include "arawnr2/rk_aiq_abayernr_algo_v2.h"



RKAIQ_BEGIN_DECLARE

static abayertnr_hardware_version_t g_abayertnr_hw_ver = ABAYERTNR_HARDWARE_V2;

static XCamReturn groupAbayertnrCreateCtx(RkAiqAlgoContext **context, const AlgoCtxInstanceCfg* cfg)
{
    LOGI_ANR("%s enter \n", __FUNCTION__ );

    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    CamGroup_Abayertnr_Contex_t *abayertnr_group_contex = NULL;
    AlgoCtxInstanceCfgCamGroup *cfgInt = (AlgoCtxInstanceCfgCamGroup*)cfg;
    //g_abayertnr_hw_ver = (abayertnr_hardware_version_t)(cfgInt->cfg_com.module_hw_version);

    if (CHECK_ISP_HW_V21()) {
        g_abayertnr_hw_ver = ABAYERTNR_HARDWARE_V1;
    } else if(CHECK_ISP_HW_V3X()) {
        g_abayertnr_hw_ver = ABAYERTNR_HARDWARE_V2;
    } else {
        g_abayertnr_hw_ver = ABAYERTNR_HARDWARE_MIN;
    }

    if(g_abayertnr_hw_ver == ABAYERTNR_HARDWARE_V2) {
        abayertnr_group_contex = (CamGroup_Abayertnr_Contex_t*)malloc(sizeof(CamGroup_Abayertnr_Contex_t));
#if ABAYERTNR_USE_JSON_FILE_V2
        Abayertnr_result_V2_t ret_v2 = ABAYERTNRV2_RET_SUCCESS;
        ret_v2 = Abayertnr_Init_V2(&(abayertnr_group_contex->abayertnr_contex_v2), (void *)cfgInt->s_calibv2);
        if(ret_v2 != ABAYERTNRV2_RET_SUCCESS) {
            ret = XCAM_RETURN_ERROR_FAILED;
            LOGE_ANR("%s: Initializaion ANR failed (%d)\n", __FUNCTION__, ret);
        }
#endif
    } else {
        ret = XCAM_RETURN_ERROR_FAILED;
        LOGE_ANR("module_hw_version of abayertnr (%d) is invalid!!!!", g_abayertnr_hw_ver);
    }

    if(ret != XCAM_RETURN_NO_ERROR) {
        LOGE_ANR("%s: Initializaion group bayertnr failed (%d)\n", __FUNCTION__, ret);
    } else {
        // to do got abayertnrSurrViewClib and initinal paras for for surround view
        abayertnr_group_contex->group_CalibV2.groupMethod = CalibDbV2_CAMGROUP_ABAYERTNR_METHOD_MEAN;// to do from json
        abayertnr_group_contex->camera_Num = cfgInt->camIdArrayLen;

        *context = (RkAiqAlgoContext *)(abayertnr_group_contex);

        LOGI_ANR("%s:%d surrViewMethod(1-mean):%d, cameraNum %d \n",
                 __FUNCTION__, __LINE__,
                 abayertnr_group_contex->group_CalibV2.groupMethod,
                 abayertnr_group_contex->camera_Num);
    }

    LOGI_ANR("%s exit ret:%d\n", __FUNCTION__, ret);
    return ret;

}

static XCamReturn groupAbayertnrDestroyCtx(RkAiqAlgoContext *context)
{
    LOGI_ANR("%s enter \n", __FUNCTION__ );

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    CamGroup_Abayertnr_Contex_t *abayertnr_group_contex = (CamGroup_Abayertnr_Contex_t*)context;

    if(g_abayertnr_hw_ver == ABAYERTNR_HARDWARE_V2) {
        Abayertnr_result_V2_t ret_v2 = ABAYERTNRV2_RET_SUCCESS;
        ret_v2 = Abayertnr_Release_V2(abayertnr_group_contex->abayertnr_contex_v2);
        if(ret_v2 != ABAYERTNRV2_RET_SUCCESS) {
            ret = XCAM_RETURN_ERROR_FAILED;
            LOGE_ANR("%s: Initializaion ANR failed (%d)\n", __FUNCTION__, ret);
        }
    }
    else {
        ret = XCAM_RETURN_ERROR_FAILED;
        LOGE_ANR("module_hw_version of awb (%d) is isvalid!!!!", g_abayertnr_hw_ver);
    }

    if(ret != XCAM_RETURN_NO_ERROR) {
        LOGE_ANR("%s: release ANR failed (%d)\n", __FUNCTION__, ret);
    } else {
        free(abayertnr_group_contex);
    }


    LOGI_ANR("%s exit ret:%d\n", __FUNCTION__, ret);
    return ret;
}

static XCamReturn groupAbayertnrPrepare(RkAiqAlgoCom* params)
{
    LOGI_ANR("%s enter \n", __FUNCTION__ );

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    CamGroup_Abayertnr_Contex_t * abayertnr_group_contex = (CamGroup_Abayertnr_Contex_t *)params->ctx;
    RkAiqAlgoCamGroupPrepare* para = (RkAiqAlgoCamGroupPrepare*)params;

    if(g_abayertnr_hw_ver == ABAYERTNR_HARDWARE_V2) {
        Abayertnr_Context_V2_t * abayertnr_contex_v2 = abayertnr_group_contex->abayertnr_contex_v2;
        if(!!(params->u.prepare.conf_type & RK_AIQ_ALGO_CONFTYPE_UPDATECALIB )) {
            // todo  update calib pars for surround view
#if ABAYERTNR_USE_JSON_FILE_V2
            void *pCalibdbV2 = (void*)(para->s_calibv2);
            CalibDbV2_BayerTnr_V2_t *bayertnr_v2 = (CalibDbV2_BayerTnr_V2_t*)(CALIBDBV2_GET_MODULE_PTR((void*)pCalibdbV2, bayertnr_v2));
            abayertnr_contex_v2->bayertnr_v2 = *bayertnr_v2;
            abayertnr_contex_v2->isIQParaUpdate = true;
            abayertnr_contex_v2->isReCalculate |= 1;
#endif
        }
        Abayertnr_Config_V2_t stAbayertnrConfigV2;
        Abayertnr_result_V2_t ret_v2 = ABAYERTNRV2_RET_SUCCESS;
        ret_v2 = Abayertnr_Prepare_V2(abayertnr_contex_v2, &stAbayertnrConfigV2);
        if(ret_v2 != ABAYERTNRV2_RET_SUCCESS) {
            ret = XCAM_RETURN_ERROR_FAILED;
            LOGE_ANR("%s: config ANR failed (%d)\n", __FUNCTION__, ret);
        }
    }
    else {
        ret = XCAM_RETURN_ERROR_FAILED;
        LOGE_ANR("module_hw_version of awb (%d) is isvalid!!!!", g_abayertnr_hw_ver);
    }

    LOGI_ANR("%s exit ret:%d\n", __FUNCTION__, ret);
    return ret;
}

static XCamReturn groupAbayertnrProcessing(const RkAiqAlgoCom* inparams, RkAiqAlgoResCom* outparams)
{
    LOGI_ANR("%s enter \n", __FUNCTION__ );
    LOGI_ANR("----------------------------------------------frame_id (%d)----------------------------------------------\n", inparams->frame_id);

    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RkAiqAlgoCamGroupProcIn* procParaGroup = (RkAiqAlgoCamGroupProcIn*)inparams;
    RkAiqAlgoCamGroupProcOut* procResParaGroup = (RkAiqAlgoCamGroupProcOut*)outparams;
    CamGroup_Abayertnr_Contex_t * abayertnr_group_contex = (CamGroup_Abayertnr_Contex_t *)inparams->ctx;
    int deltaIso = 0;

    //method error
    if (abayertnr_group_contex->group_CalibV2.groupMethod <= CalibDbV2_CAMGROUP_ABAYERTNR_METHOD_MIN
            ||  abayertnr_group_contex->group_CalibV2.groupMethod >=  CalibDbV2_CAMGROUP_ABAYERTNR_METHOD_MAX) {
        return (ret);
    }

    //group empty
    if(procParaGroup->camgroupParmasArray == nullptr) {
        LOGE_ANR("camgroupParmasArray is null");
        return(XCAM_RETURN_ERROR_FAILED);
    }

    //get cur ae exposure
    Abayertnr_ExpInfo_V2_t stExpInfoV2;
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



    if(g_abayertnr_hw_ver == ABAYERTNR_HARDWARE_V2) {
        Abayertnr_Context_V2_t * abayertnr_contex_v2 = abayertnr_group_contex->abayertnr_contex_v2;
        Abayertnr_ProcResult_V2_t stAbayertnrResultV2;
        deltaIso = abs(stExpInfoV2.arIso[stExpInfoV2.hdr_mode] - abayertnr_contex_v2->stExpInfo.arIso[stExpInfoV2.hdr_mode]);
        if(deltaIso > ABAYERTNRV2_RECALCULATE_DELTA_ISO) {
            abayertnr_contex_v2->isReCalculate |= 1;
        }
        if(abayertnr_contex_v2->isReCalculate) {
            Abayertnr_result_V2_t ret_v2 = ABAYERTNRV2_RET_SUCCESS;
            ret_v2 = Abayertnr_Process_V2(abayertnr_contex_v2, &stExpInfoV2);
            if(ret_v2 != ABAYERTNRV2_RET_SUCCESS) {
                ret = XCAM_RETURN_ERROR_FAILED;
                LOGE_ANR("%s: processing ANR failed (%d)\n", __FUNCTION__, ret);
            }
            Abayertnr_GetProcResult_V2(abayertnr_contex_v2, &stAbayertnrResultV2);
            stAbayertnrResultV2.isNeedUpdate = true;
            LOGD_ANR("recalculate: %d delta_iso:%d \n ", abayertnr_contex_v2->isReCalculate, deltaIso);
        } else {
            stAbayertnrResultV2 = abayertnr_contex_v2->stProcResult;
            stAbayertnrResultV2.isNeedUpdate = true;
        }

        for (int i = 0; i < procResParaGroup->arraySize; i++) {
            *(procResParaGroup->camgroupParmasArray[i]->abayertnr._abayertnr_procRes_v2) = stAbayertnrResultV2.st3DFix;
        }
        abayertnr_contex_v2->isReCalculate = 0;
    }


    else {
        ret = XCAM_RETURN_ERROR_FAILED;
        LOGE_ANR("module_hw_version of awb (%d) is isvalid!!!!", g_abayertnr_hw_ver);
    }

    LOGI_ANR("%s exit\n", __FUNCTION__);
    return ret;
}

RkAiqAlgoDescription g_RkIspAlgoDescCamgroupAbayertnr = {
    .common = {
        .version = RKISP_ALGO_CAMGROUP_ABAYERTNR_VERSION,
        .vendor  = RKISP_ALGO_CAMGROUP_ABAYERTNR_VENDOR,
        .description = RKISP_ALGO_CAMGROUP_ABAYERTNR_DESCRIPTION,
        .type    = RK_AIQ_ALGO_TYPE_AMFNR,
        .id      = 0,
        .create_context  = groupAbayertnrCreateCtx,
        .destroy_context = groupAbayertnrDestroyCtx,
    },
    .prepare = groupAbayertnrPrepare,
    .pre_process = NULL,
    .processing = groupAbayertnrProcessing,
    .post_process = NULL,
};

RKAIQ_END_DECLARE
