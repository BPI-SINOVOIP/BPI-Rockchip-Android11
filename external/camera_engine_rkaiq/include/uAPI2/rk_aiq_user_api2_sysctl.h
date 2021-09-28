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

#ifndef RK_AIQ_USER_API2_SYSCTL_H
#define RK_AIQ_USER_API2_SYSCTL_H

#include "rk_aiq.h"
#include "rk_aiq_algo_des.h"
#include "rk_aiq_user_api_sysctl.h"

RKAIQ_BEGIN_DECLARE

/********************below are verified api********************/

/*!
 * \brief pre-settings before init
 *
 * \param[in] sns_ent_name    active sensor media entity name. This represents the unique camera module
 *                            in system. And the whole active pipeline could be retrieved by this.
 * \param[in] mode            pipleline working mode, just used by \ref
 *                            rk_aiq_uapi_sysctl_init to select iq file
 *                            according to hdr mode. Unused if params \ref
 *                            iq_file is specified.
 * \param[in] force_iq_file   optional, forcely used this iq file. Must be xml.
 *                            Such as -> force_iq_file = "ov5695_TongJu_CHT842-MD.xml";
 *                            Meanwhile, there must has "ov5695_TongJu_CHT842-MD.json" in the same path with xml.
 * \note Optional API, should be called before\ref rk_aiq_uapi_sysctl_init. This
 *       API extends the functionality of \ref rk_aiq_uapi_sysctl_init. And just
 *       used to help select the satisfied iq file when multiple iq files
 *       exsist.
 */
XCamReturn
rk_aiq_uapi2_sysctl_preInit(const char* sns_ent_name,
                           rk_aiq_working_mode_t mode,
                           const char* force_iq_file);

/*!
 * \brief initialze aiq control system context
 * Should call before any other APIs
 *
 * \param[in] sns_ent_name    active sensor media entity name. This represents the unique camera module\n
 *                            in system. And the whole active pipeline could be retrieved by this.
 * \param[in] iq_file_dir     define the search directory of the iq files.
 * \param[in] err_cb          not mandatory. it's used to return system errors to user.
 * \param[in] metas_cb        not mandatory. it's used to return the metas(sensor exposure settings,\n
 *                            isp settings, etc.) for each frame
 * \return return system context if success, or NULL if failure.
 */
rk_aiq_sys_ctx_t*
rk_aiq_uapi2_sysctl_init(const char* sns_ent_name,
                        const char* iq_file_dir,
                        rk_aiq_error_cb err_cb,
                        rk_aiq_metas_cb metas_cb);

/*!
 * \brief deinitialze aiq context
 * Should not be called in started state
 *
 * \param[in] ctx             the context returned by \ref rk_aiq_uapi2_sysctl_init
 */
void
rk_aiq_uapi2_sysctl_deinit( rk_aiq_sys_ctx_t* ctx);

/*!
 * \brief prepare aiq control system before runninig
 * prepare AIQ running enviroment, should be called before \ref rk_aiq_uapi2_sysctl_start.\n
 * And if re-prepared is required after \ref rk_aiq_uapi2_sysctl_start is called,\n
 * should call \ref rk_aiq_uapi2_sysctl_stop firstly.
 *
 * \param[in] ctx             the context returned by \ref rk_aiq_uapi2_sysctl_init
 * \param[in] width           sensor active output width, just used to check internally
 * \param[in] height          sensor active output height, just used to check internally
 * \param[in] mode            pipleline working mode
 * \return return 0 if success
 */
XCamReturn
rk_aiq_uapi2_sysctl_prepare(const rk_aiq_sys_ctx_t* ctx,
                           uint32_t  width, uint32_t  height,
                           rk_aiq_working_mode_t mode);

/*!
 * \brief start aiq control system
 * should be called after \ref rk_aiq_uapi2_sysctl_prepare. After this call,
 * the aiq system repeats getting 3A statistics from ISP driver, running
 * aiq algorimths(AE, AWB, AF, etc.), setting new parameters to drivers.
 *
 * \param[in] ctx             the context returned by \ref rk_aiq_uapi2_sysctl_init
 * \return return 0 if success
 */
XCamReturn
rk_aiq_uapi2_sysctl_start(const rk_aiq_sys_ctx_t* ctx);

/*!
 * \brief stop aiq control system
 *
 * \param[in] ctx             the context returned by \ref rk_aiq_uapi2_sysctl_init
 * \param[in] keep_ext_hw_st  do not change external devices status, like ircut/cpsl
 * \return return 0 if success
 */
XCamReturn
rk_aiq_uapi2_sysctl_stop(const rk_aiq_sys_ctx_t* ctx, bool keep_ext_hw_st);

/*!
 * \brief get aiq version infos
 *
 * \param[in,out] vers         version infos
 * \return return 0 if success
 */
void rk_aiq_uapi2_get_version_info(rk_aiq_ver_info_t* vers);

/*!
 * \brief apply an new iq file when stream on
 *
 * \param[in] iqfile       iqfile which will be applied
 */
XCamReturn
rk_aiq_uapi2_sysctl_updateIq(rk_aiq_sys_ctx_t* sys_ctx, char* iqfile);


int32_t
rk_aiq_uapi2_sysctl_getModuleCtl(const rk_aiq_sys_ctx_t* ctx,
                                rk_aiq_module_id_t mId, bool *mod_en);

XCamReturn
rk_aiq_uapi2_sysctl_setModuleCtl(const rk_aiq_sys_ctx_t* ctx, rk_aiq_module_id_t mId, bool mod_en);


/*!
 * \brief get algo lib enabled status
 *
 * \param[in] ctx             context
 * \param[in] algo_type       algo type defined by RkAiqAlgoDesComm.type
 * \param[in] lib_id          0 for system integrated algos;\n
 *                            returned by call \ref rk_aiq_uapi2_sysctl_regLib for customer algos
 * \return                    return true if enabled , false if disabled or unregistered
 */
bool
rk_aiq_uapi2_sysctl_getAxlibStatus(const rk_aiq_sys_ctx_t* ctx,
                                  const int algo_type,
                                  const int lib_id);

/*!
 * \brief enable or disable algo lib
 * If the \ref lib_id is the same as the current running algo, this interface
 * could be called in any state except for the context uninitialized. Otherwise,
 * it could only be called in prepared or initialized state followed by
 * call \ref rk_aiq_uapi2_sysctl_prepare , and in this case, the old algo which type
 * is \ref algo_type will be replaced by the new algo \ref lib_id.
 *
 * \param[in] ctx             context
 * \param[in] algo_type       algo type defined by RkAiqAlgoDesComm.type
 * \param[in] lib_id          0 for system integrated algos;\n
 *                            returned by call \ref rk_aiq_uapi2_sysctl_regLib for customer algos
 * \param[in] enable          enable or disable algos. enable means running the algo's processes\n
 *                            defined in \ref RkAiqAlgoDesComm; disable means\n
 *                            bypass the algo's prcosses.
 * \return return 0 if success
 */
XCamReturn
rk_aiq_uapi2_sysctl_enableAxlib(const rk_aiq_sys_ctx_t* ctx,
                               const int algo_type,
                               const int lib_id,
                               bool enable);

/*!
 * \brief get enabled algo lib context
 * The returned algo context will be used as the first param of algo private APIs.
 * For those customer's algo lib, this interface should be called after
 * \ref rk_aiq_uapi_sysctl_enableAxlib, or will return NULL.
 *
 * \param[in] ctx             context
 * \param[in] algo_type       algo type defined by RkAiqAlgoDesComm.type
 * \return return 0 if success
 */
const RkAiqAlgoContext*
rk_aiq_uapi2_sysctl_getEnabledAxlibCtx(const rk_aiq_sys_ctx_t* ctx, const int algo_type);

/********************below api continue to use api of CamHwIsp20********************/

XCamReturn
rk_aiq_uapi2_sysctl_getStaticMetas(const char* sns_ent_name, rk_aiq_static_info_t* static_info);

/*!
 * \brief enum static camera infos
 *
 * \param[in] index           which camera info will be enum
 * \param[out] static_info    returned camera infos
 * \return return 0 if success
 */
XCamReturn
rk_aiq_uapi2_sysctl_enumStaticMetas(int index, rk_aiq_static_info_t* static_info);

/*!
 * \brief get sensor entity name from video node
 * \param[in] vd             pp stream video node full path
 * \return return the binded sensor name
 */
const char*
rk_aiq_uapi2_sysctl_getBindedSnsEntNmByVd(const char* vd);

/*!
 * \brief get crop window of isp input
 *
 * \param[in] rect       get cams crop prop
 */
XCamReturn
rk_aiq_uapi2_sysctl_getCrop(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_rect_t *rect);


/*!
 * \brief set compensation light config
 *
 * \param[in] ctx             context
 * \param[out] cfg            cpsl configs
 * \return return 0 if success
 */
XCamReturn
rk_aiq_uapi2_sysctl_setCpsLtCfg(const rk_aiq_sys_ctx_t* ctx,
                               rk_aiq_cpsl_cfg_t* cfg);
/*!
 * \brief get compensation light info
 *
 * \param[in] ctx             context
 * \param[out] info           current cpsl settings
 * \return return 0 if success
 */
XCamReturn
rk_aiq_uapi2_sysctl_getCpsLtInfo(const rk_aiq_sys_ctx_t* ctx,
                                rk_aiq_cpsl_info_t* info);
/*!
 * \brief query compensation light capability
 *
 * \param[in] ctx             context
 * \param[out] cap            cpsl cap info
 * \return return 0 if success
 */
XCamReturn
rk_aiq_uapi2_sysctl_queryCpsLtCap(const rk_aiq_sys_ctx_t* ctx,
                                 rk_aiq_cpsl_cap_t* cap);

/*!
 * \brief set the bypass stream rotation
 *
 * \param[in] ctx             context
 * \param[in] rot             rotation val
 * \return return 0 if success
 */
XCamReturn
rk_aiq_uapi2_sysctl_setSharpFbcRotation(const rk_aiq_sys_ctx_t* ctx, rk_aiq_rotation_t rot);

/*!
 * \brief set multiple cameras working concurrently
 * Notify this AIQ ctx will run with other sensor's AIQ ctx.

 * \param[in] cc        set cams concurrently used or not
 * \note should be called before rk_aiq_uapi_sysctl_start
 */
void
rk_aiq_uapi2_sysctl_setMulCamConc(const rk_aiq_sys_ctx_t* ctx, bool cc);

/*!
 * \brief register mems sensor handler interface
 *
 * \param[in] intf          mems sensor interfaces
 */
XCamReturn
rk_aiq_uapi2_sysctl_regMemsSensorIntf(const rk_aiq_sys_ctx_t* sys_ctx,
                                     const rk_aiq_mems_sensor_intf_t* intf);

/**
 * @brief switch calibDB for different scene
 *
 * @param sys_ctx
 * @param main_scene  main scene name
 * @param sub_scene   sub scenen name
 *
 * @return 0 if no error
 */
int
rk_aiq_uapi2_sysctl_switch_scene(const rk_aiq_sys_ctx_t* sys_ctx,
                                    const char* main_scene,
                                    const char* sub_scene);

XCamReturn
rk_aiq_uapi2_sysctl_tuning(const rk_aiq_sys_ctx_t* sys_ctx, char* param);

char* rk_aiq_uapi2_sysctl_readiq(const rk_aiq_sys_ctx_t* sys_ctx, char* param);

/*problem api*/
#if 0
//HDR mode may not work;
XCamReturn rk_aiq_uapi_sysctl_swWorkingModeDyn(const rk_aiq_sys_ctx_t* ctx, rk_aiq_working_mode_t mode);

//rk_aiq_uapi2_sysctl_setCrop() is not ready yet in hdr mode;
/*!
 * \brief set crop window of isp input
 * This API will affect the isp pipeline resolution.
 *
 * \param[in] rect      set cams crop prop
 * \note Optinal API, should be called before rk_aiq_uapi2_sysctl_prepare
 */
XCamReturn
rk_aiq_uapi2_sysctl_setCrop(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_rect_t rect);
#endif

/*api wait to be verified*/
#if 0
XCamReturn rk_aiq_uapi_sysctl_enqueueRkRawFile(const rk_aiq_sys_ctx_t* ctx, const char *path);
XCamReturn rk_aiq_uapi_sysctl_prepareRkRaw(const rk_aiq_sys_ctx_t* ctx, rk_aiq_raw_prop_t prop);
XCamReturn rk_aiq_uapi_sysctl_enqueueRkRawBuf(const rk_aiq_sys_ctx_t* ctx, void *rawdata, bool sync);
XCamReturn rk_aiq_uapi_sysctl_registRkRawCb(const rk_aiq_sys_ctx_t* ctx, void (*callback)(void*));
XCamReturn rk_aiq_uapi_sysctl_regLib(const rk_aiq_sys_ctx_t* ctx, RkAiqAlgoDesComm* algo_lib_des);
XCamReturn rk_aiq_uapi_sysctl_unRegLib(const rk_aiq_sys_ctx_t* ctx, const int algo_type, const int lib_id);
#endif


#if 0
XCamReturn
rk_aiq_uapi_sysctl_get3AStats(const rk_aiq_sys_ctx_t* ctx,
                              rk_aiq_isp_stats_t *stats);

XCamReturn
rk_aiq_uapi_sysctl_get3AStatsBlk(const rk_aiq_sys_ctx_t* ctx,
                              rk_aiq_isp_stats_t **stats, int timeout_ms);

void
rk_aiq_uapi_sysctl_release3AStatsRef(const rk_aiq_sys_ctx_t* ctx,
                                     rk_aiq_isp_stats_t *stats);

#endif

RKAIQ_END_DECLARE

#endif
