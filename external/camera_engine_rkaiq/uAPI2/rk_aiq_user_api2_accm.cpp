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
#include "uAPI2/rk_aiq_user_api2_accm.h"

#include "RkAiqCamGroupHandleInt.h"
#include "algo_handlers/RkAiqAccmHandle.h"

#ifdef RK_SIMULATOR_HW
#define CHECK_USER_API_ENABLE
#endif

XCamReturn  rk_aiq_user_api2_accm_SetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_ccm_attrib_t attr)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    CHECK_USER_API_ENABLE2(sys_ctx);
    CHECK_USER_API_ENABLE(RK_AIQ_ALGO_TYPE_ACCM);
    RKAIQ_API_SMART_LOCK(sys_ctx);

    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
    #ifdef RKAIQ_ENABLE_CAMGROUP
        RkAiqCamGroupAccmHandleInt* algo_handle =
            camgroupAlgoHandle<RkAiqCamGroupAccmHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ACCM);

        if (algo_handle) {
            return algo_handle->setAttrib(attr);
        } else {
            const rk_aiq_camgroup_ctx_t* camgroup_ctx = (rk_aiq_camgroup_ctx_t *)sys_ctx;
            for (auto camCtx : camgroup_ctx->cam_ctxs_array) {
                if (!camCtx)
                    continue;

                RkAiqAccmHandleInt* singleCam_algo_handle =
                    algoHandle<RkAiqAccmHandleInt>(camCtx, RK_AIQ_ALGO_TYPE_ACCM);
                if (singleCam_algo_handle)
                    ret = singleCam_algo_handle->setAttrib(attr);
            }
        }
    #else
        return XCAM_RETURN_ERROR_FAILED;
    #endif
    } else {
        RkAiqAccmHandleInt* algo_handle =
            algoHandle<RkAiqAccmHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ACCM);

        if (algo_handle) {
            return algo_handle->setAttrib(attr);
        }
    }

    return (ret);
}

XCamReturn  rk_aiq_user_api2_accm_GetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_ccm_attrib_t *attr)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
    #ifdef RKAIQ_ENABLE_CAMGROUP
        RkAiqCamGroupAccmHandleInt* algo_handle =
            camgroupAlgoHandle<RkAiqCamGroupAccmHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ACCM);

        if (algo_handle) {
            return algo_handle->getAttrib(attr);
        } else {
            const rk_aiq_camgroup_ctx_t* camgroup_ctx = (rk_aiq_camgroup_ctx_t *)sys_ctx;
            for (auto camCtx : camgroup_ctx->cam_ctxs_array) {
                if (!camCtx)
                    continue;

                RkAiqAccmHandleInt* singleCam_algo_handle =
                    algoHandle<RkAiqAccmHandleInt>(camCtx, RK_AIQ_ALGO_TYPE_ACCM);
                if (singleCam_algo_handle)
                    ret = singleCam_algo_handle->getAttrib(attr);
            }
        }
    #else
        return XCAM_RETURN_ERROR_FAILED;
    #endif
    } else {
        RkAiqAccmHandleInt* algo_handle =
            algoHandle<RkAiqAccmHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ACCM);

        if (algo_handle) {
            return algo_handle->getAttrib(attr);
        }
    }

    return (ret);
}

XCamReturn
rk_aiq_user_api2_accm_QueryCcmInfo(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_ccm_querry_info_t *ccm_querry_info )
{
    RKAIQ_API_SMART_LOCK(sys_ctx);

    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
    #ifdef RKAIQ_ENABLE_CAMGROUP
        RkAiqCamGroupAccmHandleInt* algo_handle =
            camgroupAlgoHandle<RkAiqCamGroupAccmHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ACCM);

        if (algo_handle) {
            return algo_handle->queryCcmInfo(ccm_querry_info);
        } else {
            const rk_aiq_camgroup_ctx_t* camgroup_ctx = (rk_aiq_camgroup_ctx_t *)sys_ctx;
            for (auto camCtx : camgroup_ctx->cam_ctxs_array) {
                if (!camCtx)
                    continue;

                RkAiqAccmHandleInt* singleCam_algo_handle =
                    algoHandle<RkAiqAccmHandleInt>(camCtx, RK_AIQ_ALGO_TYPE_ACCM);
                if (singleCam_algo_handle)
                    return singleCam_algo_handle->queryCcmInfo(ccm_querry_info);
            }
        }
    #else
        return XCAM_RETURN_ERROR_FAILED;
    #endif
    } else {
        RkAiqAccmHandleInt* algo_handle =
            algoHandle<RkAiqAccmHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ACCM);

        if (algo_handle) {
            return algo_handle->queryCcmInfo(ccm_querry_info);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

