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

#ifndef _RK_AIQ_USER_API_ABAYER2DNRV2_H_
#define _RK_AIQ_USER_API_ABAYER2DNRV2_H_

#include "abayer2dnr2/rk_aiq_uapi_abayer2dnr_int_v2.h"

typedef struct rk_aiq_sys_ctx_s rk_aiq_sys_ctx_t;

RKAIQ_BEGIN_DECLARE

XCamReturn
rk_aiq_user_api_abayer2dnrV2_SetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_bayer2dnr_attrib_v2_t* attr);

XCamReturn
rk_aiq_user_api_abayer2dnrV2_GetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_bayer2dnr_attrib_v2_t* attr);

XCamReturn
rk_aiq_user_api_abayer2dnrV2_SetStrength(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_bayer2dnr_strength_v2_t *pStrength);

XCamReturn
rk_aiq_user_api_abayer2dnrV2_GetStrength(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_bayer2dnr_strength_v2_t *pStrength);






RKAIQ_END_DECLARE

#endif
