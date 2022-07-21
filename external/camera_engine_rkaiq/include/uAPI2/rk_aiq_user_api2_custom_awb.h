/*
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

#ifndef _RK_AIQ_USER_API_CUSTOM_AWB_H_
#define _RK_AIQ_USER_API_CUSTOM_AWB_H_

#include "rk_aiq_types.h" /* common structs */
#include "rk_aiq_user_api2_sysctl.h" /* rk_aiq_sys_ctx_t */
#include "awb/rk_aiq_types_awb_stat_v201.h" /* include awb structs*/

RKAIQ_BEGIN_DECLARE

/* all awb stats */
typedef struct rk_aiq_customAwb_stats_s
{
    rk_aiq_awb_stat_wp_res_light_v201_t light[RK_AIQ_AWB_MAX_WHITEREGIONS_NUM];
    int WpNo2[RK_AIQ_AWB_MAX_WHITEREGIONS_NUM];
    rk_aiq_awb_stat_blk_res_v201_t   blockResult[RK_AIQ_AWB_GRID_NUM_TOTAL];
    rk_aiq_awb_stat_wp_res_light_v201_t multiwindowLightResult[4];
    rk_aiq_awb_stat_wp_res_v201_t excWpRangeResult[RK_AIQ_AWB_STAT_WP_RANGE_NUM_V201];
    unsigned int WpNoHist[RK_AIQ_AWB_WP_HIST_BIN_NUM];
    struct rk_aiq_customAwb_stats_s* next;
} rk_aiq_customAwb_stats_t;

typedef struct rk_aiq_customAwb_hw_cfg_s {
    bool awbEnable;
    bool lscBypEnable;
    uint8_t frameChoose;
    unsigned short windowSet[4];
    unsigned char lightNum;
    unsigned short maxR;
    unsigned short minR;
    unsigned short maxG;
    unsigned short minG;
    unsigned short maxB;
    unsigned short minB;
    unsigned short maxY;
    unsigned short minY;
    bool uvDetectionEnable;
    rk_aiq_awb_uv_range_para_t   uvRange_param[RK_AIQ_AWB_MAX_WHITEREGIONS_NUM];
    bool xyDetectionEnable;
    rk_aiq_rgb2xy_para_t      rgb2xy_param;
    rk_aiq_awb_xy_range_para_t    xyRange_param[RK_AIQ_AWB_MAX_WHITEREGIONS_NUM];
    bool threeDyuvEnable;
    unsigned short threeDyuvIllu[RK_AIQ_AWB_YUV_LS_PARA_NUM];
    short   icrgb2RYuv_matrix[12];
    rk_aiq_awb_rt3dyuv_range_para_t     ic3Dyuv2Range_param[RK_AIQ_AWB_YUV_LS_PARA_NUM];
    bool multiwindow_en;
    unsigned short multiwindow[RK_AIQ_AWB_MULTIWINDOW_NUM_V201][4];
    rk_aiq_awb_exc_range_v201_t excludeWpRange[RK_AIQ_AWB_EXCLUDE_WP_RANGE_NUM];
    bool wpDiffWeiEnable;
    unsigned char wpDiffwei_y[RK_AIQ_AWBWP_WEIGHT_CURVE_DOT_NUM];
    unsigned char wpDiffwei_w[RK_AIQ_AWBWP_WEIGHT_CURVE_DOT_NUM];
    rk_aiq_awb_xy_type_v201_t xyRangeTypeForWpHist;
    bool blkWeightEnable;
    unsigned char blkWeight[RK_AIQ_AWB_GRID_NUM_TOTAL];
    rk_aiq_awb_blk_stat_mode_v201_t blkMeasureMode;
    rk_aiq_awb_xy_type_v201_t xyRangeTypeForBlkStatistics;
    rk_aiq_awb_blk_stat_realwp_ill_e illIdxForBlkStatistics;
} rk_aiq_customAwb_hw_cfg_t;

typedef struct rk_aiq_customAwb_single_hw_cfg_t {
    unsigned short windowSet[4];
    bool multiwindow_en;
    unsigned short multiwindow[RK_AIQ_AWB_MULTIWINDOW_NUM_V201][4];
    bool blkWeightEnable;
    unsigned char blkWeight[RK_AIQ_AWB_GRID_NUM_TOTAL];
} rk_aiq_customAwb_single_hw_cfg_t;


/* different awb result for each camera */
typedef struct rk_aiq_customeAwb_single_results_s
{
    rk_aiq_wb_gain_t awb_gain_algo;//for each camera
    rk_aiq_customAwb_single_hw_cfg_t  awbHwConfig;//for each camera
    struct rk_aiq_customeAwb_single_results_s *next;
} rk_aiq_customeAwb_single_results_t;


/* full awb results */
typedef struct rk_aiq_customeAwb_results_s
{
    bool  IsConverged; //true: converged; false: not converged
    rk_aiq_wb_gain_t awb_gain_algo;
    float awb_smooth_factor;
    rk_aiq_customAwb_hw_cfg_t  awbHwConfig;
    rk_aiq_customeAwb_single_results_t *next;//defalut vaue is nullptr,which means all cameras with the same cfg;
} rk_aiq_customeAwb_results_t;


typedef struct rk_aiq_customeAwb_cbs_s
{
    /* ctx is the rk_aiq_sys_ctx_t which is corresponded to
     * camera, could be mapped to camera id.
     */
    int32_t (*pfn_awb_init)(void* ctx);
    int32_t (*pfn_awb_run)(void* ctx, const rk_aiq_customAwb_stats_t* pstAwbInfo,
                          rk_aiq_customeAwb_results_t* pstAwbResult);
    /* not used now */
    int32_t (*pfn_awb_ctrl)(void* ctx, uint32_t u32Cmd, void *pValue);
    int32_t (*pfn_awb_exit)(void* ctx);
} rk_aiq_customeAwb_cbs_t;

/*!
 * \brief register custom Awb algo
 *
 * \param[in] ctx             context
 * \param[in] cbs             custom Awb callbacks
 * \note should be called after rk_aiq_uapi_sysctl_init
 */
XCamReturn
rk_aiq_uapi2_customAWB_register(const rk_aiq_sys_ctx_t* ctx, rk_aiq_customeAwb_cbs_t* cbs);

/*!
 * \brief enable/disable custom Awb algo
 *
 * \param[in] ctx             context
 * \param[in] enable          enable/diable custom Awb
 * \note should be called after rk_aiq_uapi_customAWB_register. If custom Awb was enabled,
 *       Rk awb will be stopped, vice versa.
 */
XCamReturn
rk_aiq_uapi2_customAWB_enable(const rk_aiq_sys_ctx_t* ctx, bool enable);

/*!
 * \brief unregister custom Awb algo
 *
 * \param[in] ctx             context
 * \note should be called after rk_aiq_uapi_customAWB_register.
 */
XCamReturn
rk_aiq_uapi2_customAWB_unRegister(const rk_aiq_sys_ctx_t* ctx);

RKAIQ_END_DECLARE

#endif
