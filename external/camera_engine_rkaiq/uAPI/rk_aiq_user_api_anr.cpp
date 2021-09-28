/*
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

#include "rk_aiq_user_api_anr.h"
#include "RkAiqHandleInt.h"


RKAIQ_BEGIN_DECLARE

#ifdef RK_SIMULATOR_HW
#define CHECK_USER_API_ENABLE
#endif

XCamReturn
rk_aiq_user_api_anr_SetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_nr_attrib_t *attr)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

#if ANR_NO_SEPERATE_MARCO
    CHECK_USER_API_ENABLE2(sys_ctx);
    CHECK_USER_API_ENABLE(RK_AIQ_ALGO_TYPE_ANR);
    RKAIQ_API_SMART_LOCK(sys_ctx);
    RkAiqAnrHandleInt* algo_handle =
        algoHandle<RkAiqAnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ANR);

    if (algo_handle) {
        ret = algo_handle->setAttrib(attr);
    }
#else

    CHECK_USER_API_ENABLE2(sys_ctx);
    CHECK_USER_API_ENABLE(RK_AIQ_ALGO_TYPE_ARAWNR);
    CHECK_USER_API_ENABLE(RK_AIQ_ALGO_TYPE_AMFNR);
    CHECK_USER_API_ENABLE(RK_AIQ_ALGO_TYPE_AYNR);
    CHECK_USER_API_ENABLE(RK_AIQ_ALGO_TYPE_ACNR);

    RKAIQ_API_SMART_LOCK(sys_ctx);
    RkAiqArawnrHandleInt* rawnr_algo_handle =
        algoHandle<RkAiqArawnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);
    if (rawnr_algo_handle) {
        rk_aiq_bayernr_attrib_v1_t bayernr_attr;
        memset(&bayernr_attr, 0x00, sizeof(bayernr_attr));
        bayernr_attr.eMode = (Abayernr_OPMode_V1_t)attr->eMode;
        bayernr_attr.stAuto.bayernrEn = attr->stAuto.bayernrEn;
        memcpy(&bayernr_attr.stAuto.stParams, &attr->stAuto.stBayernrParams, sizeof(bayernr_attr.stAuto.stParams));
        memcpy(&bayernr_attr.stAuto.stSelect, &attr->stAuto.stBayernrParamSelect, sizeof(bayernr_attr.stAuto.stSelect));
        bayernr_attr.stManual.bayernrEn = attr->stManual.bayernrEn;
        memcpy(&bayernr_attr.stManual.stSelect, &attr->stManual.stBayernrParamSelect, sizeof(bayernr_attr.stManual.stSelect));
        ret = rawnr_algo_handle->setAttrib(&bayernr_attr);
    }

    RkAiqAmfnrHandleInt* mfnr_algo_handle =
        algoHandle<RkAiqAmfnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_AMFNR);
    if (mfnr_algo_handle) {
        rk_aiq_mfnr_attrib_v1_t mfnr_attr;
        memset(&mfnr_attr, 0x00, sizeof(mfnr_attr));
        mfnr_attr.eMode = (Amfnr_OPMode_V1_t)(attr->eMode);
        mfnr_attr.stAuto.mfnrEn = attr->stAuto.mfnrEn;
        memcpy(&mfnr_attr.stAuto.stParams, &attr->stAuto.stMfnrParams, sizeof(mfnr_attr.stAuto.stParams ));
        memcpy(&mfnr_attr.stAuto.stSelect, &attr->stAuto.stMfnrParamSelect, sizeof(mfnr_attr.stAuto.stSelect));
        memcpy(&mfnr_attr.stAuto.stMfnr_dynamic, &attr->stAuto.stMfnr_dynamic, sizeof(mfnr_attr.stAuto.stMfnr_dynamic));
        mfnr_attr.stManual.mfnrEn = attr->stManual.mfnrEn;
        memcpy(&mfnr_attr.stManual.stSelect, &attr->stManual.stMfnrParamSelect, sizeof(mfnr_attr.stManual.stSelect));
        ret = mfnr_algo_handle->setAttrib(&mfnr_attr);
    }

    RkAiqAynrHandleInt* ynr_algo_handle =
        algoHandle<RkAiqAynrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_AYNR);
    if (ynr_algo_handle) {
        rk_aiq_ynr_attrib_v1_t ynr_attr;
        memset(&ynr_attr, 0x00, sizeof(ynr_attr));
        ynr_attr.eMode = (Aynr_OPMode_V1_t)(attr->eMode);
        ynr_attr.stAuto.ynrEn = attr->stAuto.ynrEn;
        memcpy(&ynr_attr.stAuto.stParams, &attr->stAuto.stYnrParams, sizeof(ynr_attr.stAuto.stParams));
        memcpy(&ynr_attr.stAuto.stSelect, &attr->stAuto.stYnrParamSelect, sizeof(ynr_attr.stAuto.stSelect));
        ynr_attr.stManual.ynrEn = attr->stManual.ynrEn;
        memcpy(&ynr_attr.stManual.stSelect, &attr->stManual.stYnrParamSelect, sizeof(ynr_attr.stManual.stSelect));
        ret = ynr_algo_handle->setAttrib(&ynr_attr);
    }

    RkAiqAcnrHandleInt* uvnr_algo_handle =
        algoHandle<RkAiqAcnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ACNR);
    if (uvnr_algo_handle) {
        rk_aiq_uvnr_attrib_v1_t uvnr_attr;
        memset(&uvnr_attr, 0x00, sizeof(uvnr_attr));
        uvnr_attr.eMode = (Auvnr_OPMode_t)(attr->eMode);
        uvnr_attr.stAuto.uvnrEn = attr->stAuto.uvnrEn;
        memcpy(&uvnr_attr.stAuto.stParams, &attr->stAuto.stUvnrParams, sizeof(uvnr_attr.stAuto.stParams));
        memcpy(&uvnr_attr.stAuto.stSelect, &attr->stAuto.stUvnrParamSelect, sizeof(uvnr_attr.stAuto.stSelect));
        uvnr_attr.stManual.uvnrEn = attr->stManual.uvnrEn;
        memcpy(&uvnr_attr.stManual.stSelect, &attr->stManual.stUvnrParamSelect, sizeof(uvnr_attr.stManual.stSelect));
        ret = uvnr_algo_handle->setAttrib(&uvnr_attr);
    }
#endif

    return ret;
}

XCamReturn
rk_aiq_user_api_anr_GetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_nr_attrib_t *attr)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);

#if ANR_NO_SEPERATE_MARCO
    RkAiqAnrHandleInt* algo_handle =
        algoHandle<RkAiqAnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ANR);

    if (algo_handle) {
        ret = algo_handle->getAttrib(attr);
    }
#else
    RkAiqArawnrHandleInt* rawnr_algo_handle =
        algoHandle<RkAiqArawnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);
    if (rawnr_algo_handle) {
        rk_aiq_bayernr_attrib_v1_t bayernr_attr;
        memset(&bayernr_attr, 0x00, sizeof(bayernr_attr));
        ret = rawnr_algo_handle->getAttrib(&bayernr_attr);
        attr->eMode = (ANROPMode_t)(bayernr_attr.eMode);
        attr->stAuto.bayernrEn = bayernr_attr.stAuto.bayernrEn;
        memcpy(&attr->stAuto.stBayernrParams, &bayernr_attr.stAuto.stParams, sizeof(attr->stAuto.stBayernrParams));
        memcpy(&attr->stAuto.stBayernrParamSelect, &bayernr_attr.stAuto.stSelect, sizeof(attr->stAuto.stBayernrParamSelect));
        attr->stManual.bayernrEn = bayernr_attr.stManual.bayernrEn;
        memcpy(&attr->stManual.stBayernrParamSelect, &bayernr_attr.stManual.stSelect, sizeof(attr->stManual.stBayernrParamSelect));
    }


    RkAiqAmfnrHandleInt* mfnr_algo_handle =
        algoHandle<RkAiqAmfnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_AMFNR);
    if (mfnr_algo_handle) {
        rk_aiq_mfnr_attrib_v1_t mfnr_attr;
        memset(&mfnr_attr, 0x00, sizeof(mfnr_attr));
        ret = mfnr_algo_handle->getAttrib(&mfnr_attr);
        attr->eMode = (ANROPMode_t)(mfnr_attr.eMode);
        attr->stAuto.mfnrEn = mfnr_attr.stAuto.mfnrEn;
        memcpy(&attr->stAuto.stMfnrParams, &mfnr_attr.stAuto.stParams, sizeof(attr->stAuto.stMfnrParams));
        memcpy(&attr->stAuto.stMfnrParamSelect, &mfnr_attr.stAuto.stSelect, sizeof(attr->stAuto.stMfnrParamSelect));
        memcpy(&attr->stAuto.stMfnr_dynamic, &mfnr_attr.stAuto.stMfnr_dynamic, sizeof(attr->stAuto.stMfnr_dynamic));
        attr->stManual.mfnrEn = mfnr_attr.stManual.mfnrEn;
        memcpy(&attr->stManual.stMfnrParamSelect, &mfnr_attr.stManual.stSelect, sizeof(attr->stManual.stMfnrParamSelect));
    }


    RkAiqAynrHandleInt* ynr_algo_handle =
        algoHandle<RkAiqAynrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_AYNR);
    if (ynr_algo_handle) {
        rk_aiq_ynr_attrib_v1_t ynr_attr;
        memset(&ynr_attr, 0x00, sizeof(ynr_attr));
        ret = ynr_algo_handle->getAttrib(&ynr_attr);
        attr->eMode = (ANROPMode_t)(ynr_attr.eMode);
        attr->stAuto.ynrEn = ynr_attr.stAuto.ynrEn;
        memcpy(&attr->stAuto.stYnrParams, &ynr_attr.stAuto.stParams, sizeof(attr->stAuto.stYnrParams));
        memcpy(&attr->stAuto.stYnrParamSelect, &ynr_attr.stAuto.stSelect, sizeof(attr->stAuto.stYnrParamSelect));
        attr->stManual.ynrEn = ynr_attr.stManual.ynrEn;
        memcpy(&attr->stManual.stYnrParamSelect, &ynr_attr.stManual.stSelect, sizeof(attr->stManual.stYnrParamSelect));
    }


    RkAiqAcnrHandleInt* uvnr_algo_handle =
        algoHandle<RkAiqAcnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ACNR);
    if (uvnr_algo_handle) {
        rk_aiq_uvnr_attrib_v1_t uvnr_attr;
        memset(&uvnr_attr, 0x00, sizeof(uvnr_attr));
        ret = uvnr_algo_handle->getAttrib(&uvnr_attr);
        attr->eMode = (ANROPMode_t)(uvnr_attr.eMode);
        attr->stAuto.uvnrEn = uvnr_attr.stAuto.uvnrEn;
        memcpy(&attr->stAuto.stUvnrParams, &uvnr_attr.stAuto.stParams, sizeof(attr->stAuto.stUvnrParams));
        memcpy(&attr->stAuto.stUvnrParamSelect, &uvnr_attr.stAuto.stSelect, sizeof(attr->stAuto.stUvnrParamSelect));
        attr->stManual.uvnrEn = uvnr_attr.stManual.uvnrEn;
        memcpy(&attr->stManual.stUvnrParamSelect, &uvnr_attr.stManual.stSelect, sizeof(attr->stManual.stUvnrParamSelect));
    }

#endif

    return ret;
}


XCamReturn
rk_aiq_user_api_anr_SetIQPara(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_nr_IQPara_t *para)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);

#if ANR_NO_SEPERATE_MARCO
    RkAiqAnrHandleInt* algo_handle =
        algoHandle<RkAiqAnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ANR);

    if (algo_handle) {
        ret = algo_handle->setIQPara(para);
    }
#else
    printf("%s:%d\n", __FUNCTION__, __LINE__);
    if(para->module_bits & (1 << ANR_MODULE_BAYERNR)) {
        RkAiqArawnrHandleInt* rawnr_algo_handle =
            algoHandle<RkAiqArawnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);
        if (rawnr_algo_handle) {
            rk_aiq_bayernr_IQPara_V1_t bayernr_para;
            bayernr_para.stBayernrPara = para->stBayernrPara;
            ret = rawnr_algo_handle->setIQPara(&bayernr_para);
        }
    }
    printf("%s:%d\n", __FUNCTION__, __LINE__);
    if(para->module_bits & (1 << ANR_MODULE_MFNR)) {
        RkAiqAmfnrHandleInt* mfnr_algo_handle =
            algoHandle<RkAiqAmfnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_AMFNR);
        if (mfnr_algo_handle) {
            rk_aiq_mfnr_IQPara_V1_t mfnr_para;
            mfnr_para.stMfnrPara = para->stMfnrPara;
            ret = mfnr_algo_handle->setIQPara(&mfnr_para);
        }
    }
    printf("%s:%d\n", __FUNCTION__, __LINE__);
    if(para->module_bits & (1 << ANR_MODULE_YNR)) {
        RkAiqAynrHandleInt* ynr_algo_handle =
            algoHandle<RkAiqAynrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_AYNR);
        if (ynr_algo_handle) {
            rk_aiq_ynr_IQPara_V1_t ynr_para;
            ynr_para.stYnrPara = para->stYnrPara;
            ret = ynr_algo_handle->setIQPara(&ynr_para);
        }
    }
    printf("%s:%d\n", __FUNCTION__, __LINE__);
    if(para->module_bits & (1 << ANR_MODULE_UVNR)) {
        RkAiqAcnrHandleInt* uvnr_algo_handle =
            algoHandle<RkAiqAcnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ACNR);
        if (uvnr_algo_handle) {
            rk_aiq_uvnr_IQPara_v1_t uvnr_para;
            uvnr_para.stUvnrPara = para->stUvnrPara;
            ret = uvnr_algo_handle->setIQPara(&uvnr_para);
        }
    }

    printf("%s:%d\n", __FUNCTION__, __LINE__);
#endif

    return ret;
}

XCamReturn
rk_aiq_user_api_anr_GetIQPara(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_nr_IQPara_t *para)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);

#if ANR_NO_SEPERATE_MARCO
    RkAiqAnrHandleInt* algo_handle =
        algoHandle<RkAiqAnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ANR);

    if (algo_handle) {
        ret =  algo_handle->getIQPara(para);
    }
#else
    printf("rawnr\n");
    RkAiqArawnrHandleInt* rawnr_algo_handle =
        algoHandle<RkAiqArawnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);
    if (rawnr_algo_handle) {
        printf("rawnr1111\n");
        rk_aiq_bayernr_IQPara_V1_t bayernr_para;
        ret = rawnr_algo_handle->getIQPara(&bayernr_para);
        printf("rawnr2222\n");
        para->stBayernrPara = bayernr_para.stBayernrPara;
    }

    printf("mfnr\n");
    RkAiqAmfnrHandleInt* mfnr_algo_handle =
        algoHandle<RkAiqAmfnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_AMFNR);
    if (mfnr_algo_handle) {
        rk_aiq_mfnr_IQPara_V1_t mfnr_para;
        printf("mfnr 1111\n");
        ret = mfnr_algo_handle->getIQPara(&mfnr_para);
        para->stMfnrPara = mfnr_para.stMfnrPara;
        printf("mfnr 2222\n");
    }

    printf("ynr\n");
    RkAiqAynrHandleInt* ynr_algo_handle =
        algoHandle<RkAiqAynrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_AYNR);
    if (ynr_algo_handle) {
        rk_aiq_ynr_IQPara_V1_t ynr_para;
        ret = ynr_algo_handle->getIQPara(&ynr_para);
        para->stYnrPara = ynr_para.stYnrPara;
    }

    printf("uvnr\n");
    RkAiqAcnrHandleInt* uvnr_algo_handle =
        algoHandle<RkAiqAcnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ACNR);
    if (uvnr_algo_handle) {
        rk_aiq_uvnr_IQPara_v1_t uvnr_para;
        ret = uvnr_algo_handle->getIQPara(&uvnr_para);
        para->stUvnrPara = uvnr_para.stUvnrPara;
    }

    printf("exit\n");
#endif

    return ret;
}


XCamReturn
rk_aiq_user_api_anr_SetLumaSFStrength(const rk_aiq_sys_ctx_t* sys_ctx, float fPercnt)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);

#if ANR_NO_SEPERATE_MARCO
    RkAiqAnrHandleInt* algo_handle =
        algoHandle<RkAiqAnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ANR);

    if (algo_handle) {
        ret = algo_handle->setLumaSFStrength(fPercnt);
    }
#else
    RkAiqArawnrHandleInt* rawnr_algo_handle =
        algoHandle<RkAiqArawnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);
    if (rawnr_algo_handle) {
        ret = rawnr_algo_handle->setStrength(fPercnt);
    }

    RkAiqAynrHandleInt* ynr_algo_handle =
        algoHandle<RkAiqAynrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_AYNR);
    if (ynr_algo_handle) {
        ret = ynr_algo_handle->setStrength(fPercnt);
    }
#endif

    return ret;
}

XCamReturn
rk_aiq_user_api_anr_SetLumaTFStrength(const rk_aiq_sys_ctx_t* sys_ctx, float fPercnt)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);

#if ANR_NO_SEPERATE_MARCO
    RkAiqAnrHandleInt* algo_handle =
        algoHandle<RkAiqAnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ANR);

    if (algo_handle) {
        ret = algo_handle->setLumaTFStrength(fPercnt);
    }
#else
    RkAiqAmfnrHandleInt* mfnr_algo_handle =
        algoHandle<RkAiqAmfnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_AMFNR);
    if (mfnr_algo_handle) {
        ret = mfnr_algo_handle->setLumaStrength(fPercnt);
    }
#endif

    return ret;
}

XCamReturn
rk_aiq_user_api_anr_GetLumaSFStrength(const rk_aiq_sys_ctx_t* sys_ctx, float *pPercnt)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);

#if ANR_NO_SEPERATE_MARCO
    RkAiqAnrHandleInt* algo_handle =
        algoHandle<RkAiqAnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ANR);

    if (algo_handle) {
        ret =  algo_handle->getLumaSFStrength(pPercnt);
    }
#else
    RkAiqArawnrHandleInt* rawnr_algo_handle =
        algoHandle<RkAiqArawnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);
    if (rawnr_algo_handle) {
        ret = rawnr_algo_handle->getStrength(pPercnt);
    }

#if 0
    RkAiqAynrHandleInt* ynr_algo_handle =
        algoHandle<RkAiqAynrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_AYNR);
    if (ynr_algo_handle) {
        ret = ynr_algo_handle->getStrength(pPercnt);
    }
#endif
#endif

    return ret;
}

XCamReturn
rk_aiq_user_api_anr_GetLumaTFStrength(const rk_aiq_sys_ctx_t* sys_ctx, float *pPercnt)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);

#if ANR_NO_SEPERATE_MARCO
    RkAiqAnrHandleInt* algo_handle =
        algoHandle<RkAiqAnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ANR);

    if (algo_handle) {
        ret = algo_handle->getLumaTFStrength(pPercnt);
    }
#else
    RkAiqAmfnrHandleInt* mfnr_algo_handle =
        algoHandle<RkAiqAmfnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_AMFNR);
    if (mfnr_algo_handle) {
        ret = mfnr_algo_handle->getLumaStrength(pPercnt);
    }
#endif

    return ret;
}

XCamReturn
rk_aiq_user_api_anr_SetChromaSFStrength(const rk_aiq_sys_ctx_t* sys_ctx, float fPercnt)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);

#if ANR_NO_SEPERATE_MARCO
    RkAiqAnrHandleInt* algo_handle =
        algoHandle<RkAiqAnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ANR);

    if (algo_handle) {
        ret = algo_handle->setChromaSFStrength(fPercnt);
    }
#else
    RkAiqAcnrHandleInt* uvnr_algo_handle =
        algoHandle<RkAiqAcnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ACNR);
    if (uvnr_algo_handle) {
        ret = uvnr_algo_handle->setStrength(fPercnt);
    }
#endif

    return ret;
}

XCamReturn
rk_aiq_user_api_anr_SetChromaTFStrength(const rk_aiq_sys_ctx_t* sys_ctx, float fPercnt)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);

#if ANR_NO_SEPERATE_MARCO
    RkAiqAnrHandleInt* algo_handle =
        algoHandle<RkAiqAnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ANR);

    if (algo_handle) {
        ret = algo_handle->setChromaTFStrength(fPercnt);
    }
#else
    RkAiqAmfnrHandleInt* mfnr_algo_handle =
        algoHandle<RkAiqAmfnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_AMFNR);
    if (mfnr_algo_handle) {
        ret = mfnr_algo_handle->setChromaStrength(fPercnt);
    }
#endif

    return ret;
}

XCamReturn
rk_aiq_user_api_anr_GetChromaSFStrength(const rk_aiq_sys_ctx_t* sys_ctx, float *pPercnt)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);

#if ANR_NO_SEPERATE_MARCO
    RkAiqAnrHandleInt* algo_handle =
        algoHandle<RkAiqAnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ANR);

    if (algo_handle) {
        ret = algo_handle->getChromaSFStrength(pPercnt);
    }
#else
    RkAiqAcnrHandleInt* uvnr_algo_handle =
        algoHandle<RkAiqAcnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ACNR);
    if (uvnr_algo_handle) {
        ret = uvnr_algo_handle->getStrength(pPercnt);
    }
#endif

    return ret;
}

XCamReturn
rk_aiq_user_api_anr_GetChromaTFStrength(const rk_aiq_sys_ctx_t* sys_ctx, float *pPercnt)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);

#if ANR_NO_SEPERATE_MARCO
    RkAiqAnrHandleInt* algo_handle =
        algoHandle<RkAiqAnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ANR);

    if (algo_handle) {
        ret = algo_handle->getChromaTFStrength(pPercnt);
    }
#else
    RkAiqAmfnrHandleInt* mfnr_algo_handle =
        algoHandle<RkAiqAmfnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_AMFNR);
    if (mfnr_algo_handle) {
        ret = mfnr_algo_handle->getChromaStrength(pPercnt);
    }
#endif

    return ret;
}

XCamReturn
rk_aiq_user_api_anr_SetRawnrSFStrength(const rk_aiq_sys_ctx_t* sys_ctx, float fPercnt)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);

#if ANR_NO_SEPERATE_MARCO
    RkAiqAnrHandleInt* algo_handle =
        algoHandle<RkAiqAnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ANR);

    if (algo_handle) {
        ret = algo_handle->setRawnrSFStrength(fPercnt);
    }
#else
    RkAiqArawnrHandleInt* rawnr_algo_handle =
        algoHandle<RkAiqArawnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);
    if (rawnr_algo_handle) {
        ret = rawnr_algo_handle->setStrength(fPercnt);
    }
#endif

    return ret;
}

XCamReturn
rk_aiq_user_api_anr_GetRawnrSFStrength(const rk_aiq_sys_ctx_t* sys_ctx, float *pPercnt)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);

#if ANR_NO_SEPERATE_MARCO
    RkAiqAnrHandleInt* algo_handle =
        algoHandle<RkAiqAnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ANR);

    if (algo_handle) {
        ret = algo_handle->getRawnrSFStrength(pPercnt);
    }
#else
    RkAiqArawnrHandleInt* rawnr_algo_handle =
        algoHandle<RkAiqArawnrHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);
    if (rawnr_algo_handle) {
        ret = rawnr_algo_handle->getStrength(pPercnt);
    }
#endif

    return XCAM_RETURN_NO_ERROR;
}

RKAIQ_END_DECLARE
