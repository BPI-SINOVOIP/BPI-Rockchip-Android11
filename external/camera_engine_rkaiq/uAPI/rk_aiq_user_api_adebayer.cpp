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
#include "rk_aiq_user_api_adebayer.h"

#include "RkAiqCamGroupHandleInt.h"
#include "algo_handlers/RkAiqAdebayerHandle.h"

RKAIQ_BEGIN_DECLARE

#ifdef RK_SIMULATOR_HW
#define CHECK_USER_API_ENABLE
#endif

XCamReturn
rk_aiq_user_api_adebayer_SetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, adebayer_attrib_t attr)
{
    CHECK_USER_API_ENABLE2(sys_ctx);
    CHECK_USER_API_ENABLE(RK_AIQ_ALGO_TYPE_ADEBAYER);
    RKAIQ_API_SMART_LOCK(sys_ctx);

    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
    #ifdef RKAIQ_ENABLE_CAMGROUP
        RkAiqCamGroupAdebayerHandleInt* algo_handle =
            camgroupAlgoHandle<RkAiqCamGroupAdebayerHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ADEBAYER);

        if (algo_handle) {
            return algo_handle->setAttrib(attr);
        } else {
            XCamReturn ret = XCAM_RETURN_ERROR_FAILED;
            const rk_aiq_camgroup_ctx_t* camgroup_ctx = (rk_aiq_camgroup_ctx_t *)sys_ctx;
            for (auto camCtx : camgroup_ctx->cam_ctxs_array) {
                if (!camCtx)
                    continue;

                RkAiqAdebayerHandleInt* singleCam_algo_handle =
                    algoHandle<RkAiqAdebayerHandleInt>(camCtx, RK_AIQ_ALGO_TYPE_ADEBAYER);
                if (singleCam_algo_handle)
                    ret = singleCam_algo_handle->setAttrib(attr);
            }
            return ret;
        }
    #else
        return XCAM_RETURN_ERROR_FAILED;
    #endif
    } else {
        RkAiqAdebayerHandleInt* algo_handle =
            algoHandle<RkAiqAdebayerHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ADEBAYER);

        if (algo_handle) {
            return algo_handle->setAttrib(attr);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api_adebayer_GetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, adebayer_attrib_t *attr)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
    #ifdef RKAIQ_ENABLE_CAMGROUP
        RkAiqCamGroupAdebayerHandleInt* algo_handle =
            camgroupAlgoHandle<RkAiqCamGroupAdebayerHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ADEBAYER);

        if (algo_handle) {
            return algo_handle->getAttrib(attr);
        } else {
            XCamReturn ret = XCAM_RETURN_ERROR_FAILED;
            const rk_aiq_camgroup_ctx_t* camgroup_ctx = (rk_aiq_camgroup_ctx_t *)sys_ctx;
            for (auto camCtx : camgroup_ctx->cam_ctxs_array) {
                if (!camCtx)
                    continue;

                RkAiqAdebayerHandleInt* singleCam_algo_handle =
                    algoHandle<RkAiqAdebayerHandleInt>(camCtx, RK_AIQ_ALGO_TYPE_ADEBAYER);
                if (singleCam_algo_handle)
                    ret = singleCam_algo_handle->getAttrib(attr);
            }
            return ret;
        }
    #else
        return XCAM_RETURN_ERROR_FAILED;
    #endif
    } else {
        RkAiqAdebayerHandleInt* algo_handle =
            algoHandle<RkAiqAdebayerHandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ADEBAYER);

        if (algo_handle) {
            return algo_handle->getAttrib(attr);
        }
    }

    return XCAM_RETURN_ERROR_FAILED;
}

RKAIQ_END_DECLARE
