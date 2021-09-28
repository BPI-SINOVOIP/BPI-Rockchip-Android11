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

#include "rk_aiq_user_api2_sysctl.h"

XCamReturn
rk_aiq_uapi2_sysctl_preInit(const char* sns_ent_name,
                           rk_aiq_working_mode_t mode,
                           const char* force_iq_file)
{
    return rk_aiq_uapi_sysctl_preInit(sns_ent_name, mode, force_iq_file);
}

rk_aiq_sys_ctx_t*
rk_aiq_uapi2_sysctl_init(const char* sns_ent_name,
                        const char* config_file_dir,
                        rk_aiq_error_cb err_cb,
                        rk_aiq_metas_cb metas_cb)
{
    return rk_aiq_uapi_sysctl_init(sns_ent_name, config_file_dir, err_cb, metas_cb);
}

void rk_aiq_uapi2_sysctl_deinit(rk_aiq_sys_ctx_t* ctx)
{
    rk_aiq_uapi_sysctl_deinit(ctx);
}

XCamReturn
rk_aiq_uapi2_sysctl_prepare(const rk_aiq_sys_ctx_t* ctx,
                           uint32_t  width, uint32_t  height,
                           rk_aiq_working_mode_t mode)
{
    return rk_aiq_uapi_sysctl_prepare(ctx, width, height, mode);
}

XCamReturn
rk_aiq_uapi2_sysctl_start(const rk_aiq_sys_ctx_t* ctx)
{
    return rk_aiq_uapi_sysctl_start(ctx);
}

XCamReturn
rk_aiq_uapi2_sysctl_stop(const rk_aiq_sys_ctx_t* ctx, bool keep_ext_hw_st)
{
    return rk_aiq_uapi_sysctl_stop(ctx, keep_ext_hw_st);
}

void rk_aiq_uapi2_get_version_info(rk_aiq_ver_info_t* vers)
{
    rk_aiq_uapi_get_version_info(vers);
}

XCamReturn
rk_aiq_uapi2_sysctl_updateIq(rk_aiq_sys_ctx_t* sys_ctx, char* iqfile)
{
    return rk_aiq_uapi_sysctl_updateIq(sys_ctx, iqfile);
}

int32_t
rk_aiq_uapi2_sysctl_getModuleCtl(const rk_aiq_sys_ctx_t* ctx,
                                rk_aiq_module_id_t mId, bool *mod_en)
{
    return rk_aiq_uapi_sysctl_getModuleCtl(ctx, mId, mod_en);
}

XCamReturn
rk_aiq_uapi2_sysctl_setModuleCtl(const rk_aiq_sys_ctx_t* ctx, rk_aiq_module_id_t mId, bool mod_en)
{
    return rk_aiq_uapi_sysctl_setModuleCtl(ctx, mId, mod_en);
}

XCamReturn
rk_aiq_uapi2_sysctl_enableAxlib(const rk_aiq_sys_ctx_t* ctx,
                               const int algo_type,
                               const int lib_id,
                               bool enable)
{
    return rk_aiq_uapi_sysctl_enableAxlib(ctx, algo_type, lib_id, enable);
}

bool
rk_aiq_uapi2_sysctl_getAxlibStatus(const rk_aiq_sys_ctx_t* ctx,
                                  const int algo_type,
                                  const int lib_id)
{
    return rk_aiq_uapi_sysctl_getAxlibStatus(ctx, algo_type, lib_id);
}

const RkAiqAlgoContext*
rk_aiq_uapi2_sysctl_getEnabledAxlibCtx(const rk_aiq_sys_ctx_t* ctx, const int algo_type)
{
    return rk_aiq_uapi_sysctl_getEnabledAxlibCtx(ctx, algo_type);
}

XCamReturn
rk_aiq_uapi2_sysctl_getStaticMetas(const char* sns_ent_name, rk_aiq_static_info_t* static_info)
{
    return rk_aiq_uapi_sysctl_getStaticMetas(sns_ent_name, static_info);
}

XCamReturn
rk_aiq_uapi2_sysctl_enumStaticMetas(int index, rk_aiq_static_info_t* static_info)
{
    return rk_aiq_uapi_sysctl_enumStaticMetas(index, static_info);
}

const char*
rk_aiq_uapi2_sysctl_getBindedSnsEntNmByVd(const char* vd)
{
    return rk_aiq_uapi_sysctl_getBindedSnsEntNmByVd(vd);
}

XCamReturn
rk_aiq_uapi2_sysctl_getCrop(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_rect_t *rect)
{
    return rk_aiq_uapi_sysctl_getCrop(sys_ctx, rect);
}

#if 0
XCamReturn
rk_aiq_uapi2_sysctl_setCrop(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_rect_t rect)
{
    return rk_aiq_uapi_sysctl_setCrop(sys_ctx, rect);
}
#endif

XCamReturn
rk_aiq_uapi2_sysctl_setCpsLtCfg(const rk_aiq_sys_ctx_t* ctx, rk_aiq_cpsl_cfg_t* cfg)
{
    return rk_aiq_uapi_sysctl_setCpsLtCfg(ctx, cfg);
}

XCamReturn
rk_aiq_uapi2_sysctl_getCpsLtInfo(const rk_aiq_sys_ctx_t* ctx, rk_aiq_cpsl_info_t* info)
{
    return rk_aiq_uapi_sysctl_getCpsLtInfo(ctx, info);
}

XCamReturn
rk_aiq_uapi2_sysctl_queryCpsLtCap(const rk_aiq_sys_ctx_t* ctx, rk_aiq_cpsl_cap_t* cap)
{
    return rk_aiq_uapi_sysctl_queryCpsLtCap(ctx, cap);
}

XCamReturn
rk_aiq_uapi2_sysctl_setSharpFbcRotation(const rk_aiq_sys_ctx_t* ctx, rk_aiq_rotation_t rot)
{
    return rk_aiq_uapi_sysctl_setSharpFbcRotation(ctx, rot);
}

void
rk_aiq_uapi2_sysctl_setMulCamConc(const rk_aiq_sys_ctx_t* ctx, bool cc)
{
    rk_aiq_uapi_sysctl_setMulCamConc(ctx, cc);
}

XCamReturn
rk_aiq_uapi2_sysctl_regMemsSensorIntf(const rk_aiq_sys_ctx_t* sys_ctx,
                                     const rk_aiq_mems_sensor_intf_t* intf)
{
    return rk_aiq_uapi_sysctl_regMemsSensorIntf(sys_ctx, intf);
}

int
rk_aiq_uapi2_sysctl_switch_scene(const rk_aiq_sys_ctx_t* sys_ctx,
                                const char* main_scene, const char* sub_scene)
{
    return rk_aiq_uapi_sysctl_switch_scene(sys_ctx, main_scene, sub_scene);
}

XCamReturn
rk_aiq_uapi2_sysctl_tuning(const rk_aiq_sys_ctx_t* sys_ctx, char* param)
{
    return rk_aiq_uapi_sysctl_tuning(sys_ctx, param);
}

char* rk_aiq_uapi2_sysctl_readiq(const rk_aiq_sys_ctx_t* sys_ctx, char* param)
{
    return rk_aiq_uapi_sysctl_readiq(sys_ctx, param);
}

