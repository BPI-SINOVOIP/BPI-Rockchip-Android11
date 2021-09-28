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

#ifndef _RK_AIQ_USER_API2_AWB_H_
#define _RK_AIQ_USER_API2_AWB_H_
#include "awb/rk_aiq_uapi_awb_int.h"

typedef struct rk_aiq_sys_ctx_s rk_aiq_sys_ctx_t;

RKAIQ_BEGIN_DECLARE

XCamReturn
rk_aiq_user_api2_awbV20_SetAllAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_uapiV2_wbV20_attrib_t attr);
XCamReturn
rk_aiq_user_api2_awbV20_GetAllAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_uapiV2_wbV20_attrib_t *attr);
XCamReturn
rk_aiq_user_api2_awbV21_SetAllAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_uapiV2_wbV21_attrib_t attr);
XCamReturn
rk_aiq_user_api2_awbV21_GetAllAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_uapiV2_wbV21_attrib_t *attr);
XCamReturn
rk_aiq_user_api2_awb_GetCCT(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_wb_cct_t *cct);
XCamReturn
rk_aiq_user_api2_awb_QueryWBInfo(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_wb_querry_info_t *wb_querry_info);
XCamReturn
rk_aiq_user_api2_awb_Lock(const rk_aiq_sys_ctx_t* sys_ctx);
XCamReturn
rk_aiq_user_api2_awb_Unlock(const rk_aiq_sys_ctx_t* sys_ctx);
XCamReturn
rk_aiq_user_api2_awb_SetWpModeAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_wb_op_mode_t attr);
XCamReturn
rk_aiq_user_api2_awb_GetWpModeAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_wb_op_mode_t *attr);
XCamReturn
rk_aiq_user_api2_awb_SetMwbAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_wb_mwb_attrib_t attr);
XCamReturn
rk_aiq_user_api2_awb_GetMwbAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_wb_mwb_attrib_t *attr);
XCamReturn
rk_aiq_user_api2_awbV20_SetAwbAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_uapiV2_wbV20_awb_attrib_t attr);
XCamReturn
rk_aiq_user_api2_awbV20_GetAwbAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_uapiV2_wbV20_awb_attrib_t *attr);
XCamReturn
rk_aiq_user_api2_awb_SetWbGainAdjustAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_uapiV2_wb_awb_wbGainAdjust_t attr);
XCamReturn
rk_aiq_user_api2_awb_GetWbGainAdjustAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_uapiV2_wb_awb_wbGainAdjust_t *attr);
XCamReturn
rk_aiq_user_api2_awb_SetWbGainOffsetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, CalibDbV2_Awb_gain_offset_cfg_t attr);
XCamReturn
rk_aiq_user_api2_awb_GetWbGainOffsetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, CalibDbV2_Awb_gain_offset_cfg_t *attr);
XCamReturn
rk_aiq_user_api2_awb_SetMultiWindowAttrib(const rk_aiq_sys_ctx_t* sys_ctx, CalibDbV2_Awb_Mul_Win_t attr);
XCamReturn
rk_aiq_user_api2_awb_GetMultiWindowAttrib(const rk_aiq_sys_ctx_t* sys_ctx, CalibDbV2_Awb_Mul_Win_t *attr);



RKAIQ_END_DECLARE

#endif
