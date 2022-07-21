/*
 * Copyright (c) 2019-2022 Rockchip Eletronics Co., Ltd.
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
 */
#include "uAPI2/rk_aiq_user_api2_abayer2dnr_v2.h"

#include "RkAiqCamGroupHandleInt.h"
#include "algo_handlers/RkAiqAbayer2dnrV2Handle.h"

RKAIQ_BEGIN_DECLARE

#ifdef RK_SIMULATOR_HW
#define CHECK_USER_API_ENABLE
#endif

XCamReturn
rk_aiq_user_api2_abayer2dnrV2_SetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_bayer2dnr_attrib_v2_t* attr)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    CHECK_USER_API_ENABLE2(sys_ctx);
    CHECK_USER_API_ENABLE(RK_AIQ_ALGO_TYPE_ARAWNR);
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
#ifdef RKAIQ_ENABLE_CAMGROUP

        RkAiqCamGroupAbayer2dnrV2HandleInt* algo_handle =
            camgroupAlgoHandle<RkAiqCamGroupAbayer2dnrV2HandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);

        if (algo_handle) {
            LOGD_ANR("%s:%d !!!!!!!!!!!!!group!!!!!!!!\n", __FUNCTION__, __LINE__);
            return algo_handle->setAttrib(attr);
        } else {
            const rk_aiq_camgroup_ctx_t* camgroup_ctx = (rk_aiq_camgroup_ctx_t *)sys_ctx;
            for (auto camCtx : camgroup_ctx->cam_ctxs_array) {
                if (!camCtx)
                    continue;

                LOGD_ANR("%s:%d !!!!!!!!!!!!!multi single!!!!!!!!\n", __FUNCTION__, __LINE__);
                RkAiqAbayer2dnrV2HandleInt* singleCam_algo_handle =
                    algoHandle<RkAiqAbayer2dnrV2HandleInt>(camCtx, RK_AIQ_ALGO_TYPE_ARAWNR);
                if (singleCam_algo_handle)
                    ret = singleCam_algo_handle->setAttrib(attr);
            }
        }
#else
        return XCAM_RETURN_ERROR_FAILED;
#endif

    } else {
        RkAiqAbayer2dnrV2HandleInt* algo_handle =
            algoHandle<RkAiqAbayer2dnrV2HandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);
        LOGD_ANR("%s:%d !!!!!!!!!!!!! single!!!!!!!!\n", __FUNCTION__, __LINE__);
        if (algo_handle) {
            return algo_handle->setAttrib(attr);
        }
    }

    return ret;
}

XCamReturn
rk_aiq_user_api2_abayer2dnrV2_GetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_bayer2dnr_attrib_v2_t* attr)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
#ifdef RKAIQ_ENABLE_CAMGROUP

        RkAiqCamGroupAbayer2dnrV2HandleInt* algo_handle =
            camgroupAlgoHandle<RkAiqCamGroupAbayer2dnrV2HandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);

        if (algo_handle) {
            LOGD_ANR("%s:%d !!!!!!!!!!!!!group!!!!!!!!\n", __FUNCTION__, __LINE__);
            return algo_handle->getAttrib(attr);
        } else {
            const rk_aiq_camgroup_ctx_t* camgroup_ctx = (rk_aiq_camgroup_ctx_t *)sys_ctx;
            for (auto camCtx : camgroup_ctx->cam_ctxs_array) {
                if (!camCtx)
                    continue;
                LOGD_ANR("%s:%d !!!!!!!!!!!!!multi single!!!!!!!!\n", __FUNCTION__, __LINE__);
                RkAiqAbayer2dnrV2HandleInt* singleCam_algo_handle =
                    algoHandle<RkAiqAbayer2dnrV2HandleInt>(camCtx, RK_AIQ_ALGO_TYPE_ARAWNR);
                if (singleCam_algo_handle)
                    ret = singleCam_algo_handle->getAttrib(attr);
            }
        }
#else
        return XCAM_RETURN_ERROR_FAILED;
#endif

    } else {
        RkAiqAbayer2dnrV2HandleInt* algo_handle =
            algoHandle<RkAiqAbayer2dnrV2HandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);
        LOGD_ANR("%s:%d !!!!!!!!!!!!!single!!!!!!!!\n", __FUNCTION__, __LINE__);
        if (algo_handle) {
            return algo_handle->getAttrib(attr);
        }
    }


    return ret;
}

XCamReturn
rk_aiq_user_api2_abayer2dnrV2_SetStrength(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_bayer2dnr_strength_v2_t *pStrength)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);

    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
#ifdef RKAIQ_ENABLE_CAMGROUP

        RkAiqCamGroupAbayer2dnrV2HandleInt* algo_handle =
            camgroupAlgoHandle<RkAiqCamGroupAbayer2dnrV2HandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);

        if (algo_handle) {
            LOGD_ANR("%s:%d !!!!!!!!!!!!!group!!!!!!!!\n", __FUNCTION__, __LINE__);
            return algo_handle->setStrength(pStrength);
        } else {
            LOGD_ANR("%s:%d !!!!!!!!!!!!!multi single!!!!!!!!\n", __FUNCTION__, __LINE__);
            const rk_aiq_camgroup_ctx_t* camgroup_ctx = (rk_aiq_camgroup_ctx_t *)sys_ctx;
            for (auto camCtx : camgroup_ctx->cam_ctxs_array) {
                if (!camCtx)
                    continue;

                RkAiqAbayer2dnrV2HandleInt* singleCam_algo_handle =
                    algoHandle<RkAiqAbayer2dnrV2HandleInt>(camCtx, RK_AIQ_ALGO_TYPE_ARAWNR);
                if (singleCam_algo_handle)
                    ret = singleCam_algo_handle->setStrength(pStrength);
            }
        }
#else
        return XCAM_RETURN_ERROR_FAILED;
#endif

    } else {
        LOGD_ANR("%s:%d !!!!!!!!!!!!!single!!!!!!!!\n", __FUNCTION__, __LINE__);
        RkAiqAbayer2dnrV2HandleInt* algo_handle =
            algoHandle<RkAiqAbayer2dnrV2HandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);

        if (algo_handle) {
            return algo_handle->setStrength(pStrength);
        }
    }

    return ret;
}


XCamReturn
rk_aiq_user_api2_abayer2dnrV2_GetStrength(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_bayer2dnr_strength_v2_t *pStrength)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RKAIQ_API_SMART_LOCK(sys_ctx);


    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
#ifdef RKAIQ_ENABLE_CAMGROUP

        RkAiqCamGroupAbayer2dnrV2HandleInt* algo_handle =
            camgroupAlgoHandle<RkAiqCamGroupAbayer2dnrV2HandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);

        if (algo_handle) {
            LOGD_ANR("%s:%d !!!!!!!!!!!!!group!!!!!!!!\n", __FUNCTION__, __LINE__);
            return algo_handle->getStrength(pStrength);
        } else {
            const rk_aiq_camgroup_ctx_t* camgroup_ctx = (rk_aiq_camgroup_ctx_t *)sys_ctx;
            for (auto camCtx : camgroup_ctx->cam_ctxs_array) {
                if (!camCtx)
                    continue;
                LOGD_ANR("%s:%d !!!!!!!!!!!!!multi single!!!!!!!!\n", __FUNCTION__, __LINE__);
                RkAiqAbayer2dnrV2HandleInt* singleCam_algo_handle =
                    algoHandle<RkAiqAbayer2dnrV2HandleInt>(camCtx, RK_AIQ_ALGO_TYPE_ARAWNR);
                if (singleCam_algo_handle)
                    ret = singleCam_algo_handle->getStrength(pStrength);
            }
        }
#else
        return XCAM_RETURN_ERROR_FAILED;
#endif

    } else {
        RkAiqAbayer2dnrV2HandleInt* algo_handle =
            algoHandle<RkAiqAbayer2dnrV2HandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);
        LOGD_ANR("%s:%d !!!!!!!!!!!!!single!!!!!!!!\n", __FUNCTION__, __LINE__);
        if (algo_handle) {
            return algo_handle->getStrength(pStrength);
        }
    }

    return ret;
}



RKAIQ_END_DECLARE
