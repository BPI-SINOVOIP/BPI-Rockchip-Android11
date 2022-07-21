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
#include "rk_aiq_user_api_again_v2.h"

#include "algo_handlers/RkAiqAgainV2Handle.h"

RKAIQ_BEGIN_DECLARE

#ifdef RK_SIMULATOR_HW
#define CHECK_USER_API_ENABLE
#endif

XCamReturn
rk_aiq_user_api_againV2_SetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_gain_attrib_v2_t* attr)
{
    CHECK_USER_API_ENABLE2(sys_ctx);
    CHECK_USER_API_ENABLE(RK_AIQ_ALGO_TYPE_ARAWNR);
    RkAiqAgainV2HandleInt* algo_handle =
        algoHandle<RkAiqAgainV2HandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);

    if (algo_handle) {
        return algo_handle->setAttrib(attr);
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api_againV2_GetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_gain_attrib_v2_t* attr)
{
    RkAiqAgainV2HandleInt* algo_handle =
        algoHandle<RkAiqAgainV2HandleInt>(sys_ctx, RK_AIQ_ALGO_TYPE_ARAWNR);

    if (algo_handle) {
        return algo_handle->getAttrib(attr);
    }

    return XCAM_RETURN_NO_ERROR;
}

RKAIQ_END_DECLARE
