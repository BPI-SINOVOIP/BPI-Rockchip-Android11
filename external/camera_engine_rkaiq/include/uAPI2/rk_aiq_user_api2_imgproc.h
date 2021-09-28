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

#ifndef _RK_AIQ_UAPI2_IMGPROC_H_
#define _RK_AIQ_UAPI2_IMGPROC_H_

#include "base/xcam_common.h"
#include "rk_aiq_user_api_imgproc.h"
#include "rk_aiq_user_api2_ae.h"
#include "rk_aiq_user_api2_adegamma.h"
#include "rk_aiq_user_api2_agamma.h"
#include "rk_aiq_user_api2_alsc.h"
#include "rk_aiq_user_api2_ablc.h"
#include "rk_aiq_user_api2_adehaze.h"
#include "rk_aiq_user_api2_atmo.h"
#include "rk_aiq_user_api2_amerge.h"
#include "rk_aiq_user_api2_adrc.h"
#include "rk_aiq_user_api2_abayernr_v2.h"
#include "rk_aiq_user_api2_acnr_v1.h"
#include "rk_aiq_user_api2_aynr_v2.h"
#include "rk_aiq_user_api2_asharp_v3.h"
#include "rk_aiq_user_api2_anr.h"
#include "rk_aiq_user_api2_af.h"
#include "rk_aiq_user_api2_awb.h"
#include "rk_aiq_user_api2_accm.h"
#include "rk_aiq_user_api2_a3dlut.h"

#include "rk_aiq_user_api2_adpcc.h"
#include "rk_aiq_user_api2_acp.h"
#include "rk_aiq_user_api2_adebayer.h"
#include "rk_aiq_user_api2_a3dlut.h"
#include "rk_aiq_user_api2_aldch.h"
#include "rk_aiq_user_api2_afec.h"



RKAIQ_BEGIN_DECLARE
/*
**********************************************************
*                        API of AEC module of V2
**********************************************************
*/

/*
*****************************
*
* Desc: set ae mode
* Argument:
*   mode contains: auto & manual
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setExpMode(const rk_aiq_sys_ctx_t* ctx, opMode_t mode);
XCamReturn rk_aiq_uapi2_getExpMode(const rk_aiq_sys_ctx_t* ctx, opMode_t *mode);

/*
*****************************
*
* Desc: set exposure parameter
* Argument:
*    auto exposure mode:
*      exposure gain will be adjust between [gain->min, gain->max]
*    manual exposure mode:
*      gain->min == gain->max
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setExpGainRange(const rk_aiq_sys_ctx_t* ctx, paRange_t *gain);
XCamReturn rk_aiq_uapi2_getExpGainRange(const rk_aiq_sys_ctx_t* ctx, paRange_t *gain);
/*
*****************************
*
* Desc: set exposure parameter
* Argument:
*    auto exposure mode:
*       exposure time will be adjust between [time->min, time->max]
*    manual exposure mode:
*       exposure time will be set gain->min == gain->max;
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setExpTimeRange(const rk_aiq_sys_ctx_t* ctx, paRange_t *time);
XCamReturn rk_aiq_uapi2_getExpTimeRange(const rk_aiq_sys_ctx_t* ctx, paRange_t *time);

/*
*****************************
*
* Desc: blacklight compensation
* Argument:
*      on:  1  on
*           0  off
*      areaType: blacklight compensation area
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setBLCMode(const rk_aiq_sys_ctx_t* ctx, bool on, aeMeasAreaType_t areaType);

/*
*****************************
*
* Desc: backlight compensation strength,only available in normal mode
* Argument:
*      strength:  [1,100]
*****************************
*/
XCamReturn rk_aiq_uapi2_setBLCStrength(const rk_aiq_sys_ctx_t* ctx, int strength);

/*
*****************************
*
* Desc: highlight compensation
* Argument:
*      on:  1  on
*           0  off
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setHLCMode(const rk_aiq_sys_ctx_t* ctx, bool on);

/*
*****************************
*
* Desc: highlight compensation strength,only available in normal mode
* Argument:
*      strength:  [1,100]
*****************************
*/
XCamReturn rk_aiq_uapi2_setHLCStrength(const rk_aiq_sys_ctx_t* ctx, int strength);

/*
*****************************
*
* Desc: set anti-flicker mode
* Argument:
*    mode
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setAntiFlickerEn(const rk_aiq_sys_ctx_t* ctx, bool on);
XCamReturn rk_aiq_uapi2_getAntiFlickerEn(const rk_aiq_sys_ctx_t* ctx, bool* on);

/*
*****************************
*
* Desc: set anti-flicker mode
* Argument:
*    mode
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setAntiFlickerMode(const rk_aiq_sys_ctx_t* ctx, antiFlickerMode_t mode);
XCamReturn rk_aiq_uapi2_getAntiFlickerMode(const rk_aiq_sys_ctx_t* ctx, antiFlickerMode_t *mode);

/*
**********************************************************
* White balance & Color
**********************************************************
*/

/*
*****************************
*
* Desc: set white balance mode
* Argument:
*   mode:  auto: auto white balance
*          manual: manual white balance
*****************************
*/
XCamReturn rk_aiq_uapi2_setWBMode(const rk_aiq_sys_ctx_t* ctx, opMode_t mode);
XCamReturn rk_aiq_uapi2_getWBMode(const rk_aiq_sys_ctx_t* ctx, opMode_t *mode);


/*
*****************************
*
* Desc: lock/unlock auto white balance
* Argument:
*
*
*****************************
*/
XCamReturn rk_aiq_uapi2_lockAWB(const rk_aiq_sys_ctx_t* ctx);
XCamReturn rk_aiq_uapi2_unlockAWB(const rk_aiq_sys_ctx_t* ctx);

/*
*****************************
*
* Desc: set manual white balance scene mode
* Argument:
*   ct_scene:
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setMWBScene(const rk_aiq_sys_ctx_t* ctx, rk_aiq_wb_scene_t scene);
XCamReturn rk_aiq_uapi2_getMWBScene(const rk_aiq_sys_ctx_t* ctx, rk_aiq_wb_scene_t *scene);


/*
*****************************
*
* Desc: set manual white balance r/b gain
* Argument:
*   ct_scene:
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setMWBGain(const rk_aiq_sys_ctx_t* ctx, rk_aiq_wb_gain_t *gain);
XCamReturn rk_aiq_uapi2_getWBGain(const rk_aiq_sys_ctx_t* ctx, rk_aiq_wb_gain_t *gain);

/*
*****************************
*
* Desc: set manual white balance color temperature
* Argument:
*   ct: color temperature value [2800, 7500]K
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setMWBCT(const rk_aiq_sys_ctx_t* ctx, unsigned int ct);
XCamReturn rk_aiq_uapi2_getWBCT(const rk_aiq_sys_ctx_t* ctx, unsigned int *ct);

/*
*****************************
*
* Desc: set wbgain offset for auto white balance
* Argument:
*   attr: wbgain offset  [-4, 4]
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setAwbGainOffsetAttrib(const rk_aiq_sys_ctx_t* ctx, CalibDbV2_Awb_gain_offset_cfg_t attr);
XCamReturn rk_aiq_uapi2_getAwbGainOffsetAttrib(const rk_aiq_sys_ctx_t* ctx, CalibDbV2_Awb_gain_offset_cfg_t *attr);

/*
*****************************
*
* Desc: set hue adjustment para  for auto white balance
* Argument:
*   attr :  hue adjustment para
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setAwbGainAdjustAttrib(const rk_aiq_sys_ctx_t* ctx, rk_aiq_uapiV2_wb_awb_wbGainAdjust_t attr);
XCamReturn rk_aiq_uapi2_getAwbGainAdjustAttrib(const rk_aiq_sys_ctx_t* ctx, rk_aiq_uapiV2_wb_awb_wbGainAdjust_t *attr);

/*
*****************************
*
* Desc: set multiwindow  para  for auto white balance; only for 1109 1126
* Argument:
*   attr :   set multiwindow  para
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setAwbMultiWindowAttrib(const rk_aiq_sys_ctx_t* sys_ctx, CalibDbV2_Awb_Mul_Win_t attr);
XCamReturn rk_aiq_uapi2_getAwbMultiWindowAttrib(const rk_aiq_sys_ctx_t* sys_ctx, CalibDbV2_Awb_Mul_Win_t *attr);


/*
*****************************
*
* Desc: set all api  para  for auto white balance ; only for 3566 3568
* Argument:
*   attr :   all paras for awb api
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setAwbV21AllAttrib(const rk_aiq_sys_ctx_t* ctx, rk_aiq_uapiV2_wbV21_attrib_t attr);
XCamReturn rk_aiq_uapi2_getAwbV21AllAttrib(const rk_aiq_sys_ctx_t* ctx, rk_aiq_uapiV2_wbV21_attrib_t *attr);


/*
*****************************
*
* Desc: set all api  para  for auto white balance ; only for 1109 1126
* Argument:
*   attr :   all paras for awb api
*
*****************************
*/

XCamReturn rk_aiq_uapi2_setAwbV20AllAttrib(const rk_aiq_sys_ctx_t* ctx, rk_aiq_uapiV2_wbV20_attrib_t attr);
XCamReturn rk_aiq_uapi2_getAwbV20AllAttrib(const rk_aiq_sys_ctx_t* ctx, rk_aiq_uapiV2_wbV20_attrib_t *attr);



/*
*****************************
*
* Desc: set power line frequence
* Argument:
*    freq
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setExpPwrLineFreqMode(const rk_aiq_sys_ctx_t* ctx, expPwrLineFreq_t freq);
XCamReturn rk_aiq_uapi2_getExpPwrLineFreqMode(const rk_aiq_sys_ctx_t* ctx, expPwrLineFreq_t *freq);

/*
*****************************
*
* Desc: Adjust image gamma

*****************************
*/
XCamReturn rk_aiq_uapi2_setGammaCoef(const rk_aiq_sys_ctx_t* ctx, rk_aiq_gamma_attrib_V2_t gammaAttr);

/*
*****************************
*
* Desc: set/get dark area boost strength
*    this function is active for normal mode
* Argument:
*   level: [1, 10]
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setDarkAreaBoostStrth(const rk_aiq_sys_ctx_t* ctx, unsigned int level);
XCamReturn rk_aiq_uapi2_getDarkAreaBoostStrth(const rk_aiq_sys_ctx_t* ctx, unsigned int *level);

/*
*****************************
*
* Desc: set hdr mode
* Argument:
*   mode:
*     auto: auto hdr mode
*     manual??manual hdr mode
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setHDRMergeMode(const rk_aiq_sys_ctx_t* ctx, opMode_t mode);
XCamReturn rk_aiq_uapi2_getHDRMergeMode(const rk_aiq_sys_ctx_t* ctx, opMode_t *mode);

XCamReturn rk_aiq_uapi2_setHDRTmoMode(const rk_aiq_sys_ctx_t* ctx, opMode_t mode);
XCamReturn rk_aiq_uapi2_getHDRTmoMode(const rk_aiq_sys_ctx_t* ctx, opMode_t *mode);
/*
*****************************
*
* Desc: set manual hdr strength
*    this function is active for HDR is manual mode
* Argument:
*   level: [1, 100]
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setMHDRStrth(const rk_aiq_sys_ctx_t* ctx, bool on, unsigned int level);
XCamReturn rk_aiq_uapi2_getMHDRStrth(const rk_aiq_sys_ctx_t* ctx, bool *on, unsigned int *level)
;

/*
*****************************
*
* Desc: set/get dark area boost strength
*    this function is active for normal mode
* Argument:
*   level: [1, 10]
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setDarkAreaBoostStrth(const rk_aiq_sys_ctx_t* ctx, unsigned int level);
XCamReturn rk_aiq_uapi2_getDarkAreaBoostStrth(const rk_aiq_sys_ctx_t* ctx, unsigned int *level);

/*
*****************************
*
* Desc: set hdr mode
* Argument:
*   mode:
*     auto: auto hdr mode
*     manual??manual hdr mode
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setHDRMergeMode(const rk_aiq_sys_ctx_t* ctx, opMode_t mode);
XCamReturn rk_aiq_uapi2_getHDRMergeMode(const rk_aiq_sys_ctx_t* ctx, opMode_t *mode);

XCamReturn rk_aiq_uapi2_setHDRTmoMode(const rk_aiq_sys_ctx_t* ctx, opMode_t mode);
XCamReturn rk_aiq_uapi2_getHDRTmoMode(const rk_aiq_sys_ctx_t* ctx, opMode_t *mode);
/*
*****************************
*
* Desc: set manual hdr strength
*    this function is active for HDR is manual mode
* Argument:
*   level: [1, 100]
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setMHDRStrth(const rk_aiq_sys_ctx_t* ctx, bool on, unsigned int level);
XCamReturn rk_aiq_uapi2_getMHDRStrth(const rk_aiq_sys_ctx_t* ctx, bool *on, unsigned int *level);

/*
* Desc: enable/disable dehaze
*
*****************************
*/
XCamReturn rk_aiq_uapi2_enableDhz(const rk_aiq_sys_ctx_t* ctx);
XCamReturn rk_aiq_uapi2_disableDhz(const rk_aiq_sys_ctx_t* ctx);

/*
*****************************
*
* Desc: set manual drc Compress
*     this function is active for DRC is HiLit mode
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setDrcCompress(const rk_aiq_sys_ctx_t* ctx, mDrcCompress_t* pIn);
XCamReturn rk_aiq_uapi2_getDrcCompress(const rk_aiq_sys_ctx_t* ctx, mDrcCompress_t *pOut);


/*
*****************************
*
* Desc: set manual drc Local TMO
*     this function is active for DRC is DRC Gain mode
* Argument:
*   LocalWeit: [0, 1]
*   GlobalContrast: [0, 1]
*   LoLitContrast: [0, 1]
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setDrcLocalTMO(const rk_aiq_sys_ctx_t* ctx, float LocalWeit, float GlobalContrast, float LoLitContrast);
XCamReturn rk_aiq_uapi2_getDrcLocalTMO(const rk_aiq_sys_ctx_t* ctx, float* LocalWeit, float* GlobalContrast, float* LoLitContrast);


/*
*****************************
*
* Desc: set manual drc HiLit
*     this function is active for DRC is HiLit mode
* Argument:
*   Strength: [0, 1]
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setDrcHiLit(const rk_aiq_sys_ctx_t* ctx, float Strength);
XCamReturn rk_aiq_uapi2_getDrcHiLit(const rk_aiq_sys_ctx_t* ctx, float* Strength);


/*
*****************************
*
* Desc: set manual drc Gain
*     this function is active for DRC is DRC Gain mode
* Argument:
*   Gain: [1, 8]
*   Alpha: [0, 1]
*   Clip: [0, 64]

*
*****************************
*/
XCamReturn rk_aiq_uapi2_setDrcGain(const rk_aiq_sys_ctx_t* ctx, float Gain, float Alpha, float Clip);
XCamReturn rk_aiq_uapi2_getDrcGain(const rk_aiq_sys_ctx_t* ctx, float* Gain, float* Alpha, float* Clip);


/*
*****************************
*
* Desc: set drc fuction on/off
*     only valid in non-HDR mode
*
*****************************
*/

XCamReturn rk_aiq_uapi2_enableDrc(const rk_aiq_sys_ctx_t* ctx);
XCamReturn rk_aiq_uapi2_disableDrc(const rk_aiq_sys_ctx_t* ctx);


/*
**********************************************************
* Dehazer
**********************************************************
*/
/*
*****************************
*
* Desc: set/get dehaze mode
* Argument:
*   mode:
*     auto: auto dehaze
*     manual：manual dehaze
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setDhzMode(const rk_aiq_sys_ctx_t* ctx, opMode_t mode);
XCamReturn rk_aiq_uapi2_getDhzMode(const rk_aiq_sys_ctx_t* ctx, opMode_t *mode);

/*
*****************************
*
* Desc: set/get manual dehaze strength
*     this function is active for dehaze is manual mode
* Argument:
*   level: [0, 10]
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setMDhzStrth(const rk_aiq_sys_ctx_t* ctx,  unsigned int level);
XCamReturn rk_aiq_uapi2_getMDhzStrth(const rk_aiq_sys_ctx_t* ctx,  unsigned int* level);

/*
**********************************************************
* Noise reduction
**********************************************************
*/
/*
*****************************
*
* Desc: set noise reduction mode
* Argument:
*   mode:
*     auto: auto noise reduction
*     manual：manual noise reduction
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setNRMode(const rk_aiq_sys_ctx_t* ctx, opMode_t mode);
XCamReturn rk_aiq_uapi2_getNRMode(const rk_aiq_sys_ctx_t* ctx, opMode_t *mode);

/*
*****************************
*
* Desc: set normal noise reduction strength
* Argument:
*   level: [0, 100]
* Normal mode
*****************************
*/
XCamReturn rk_aiq_uapi2_setANRStrth(const rk_aiq_sys_ctx_t* ctx, unsigned int level);
XCamReturn rk_aiq_uapi2_getANRStrth(const rk_aiq_sys_ctx_t* ctx, unsigned int *level);


/*
*****************************
*
* Desc: set manual spatial noise reduction strength
*    this function is active for NR is manual mode
* Argument:
*   level: [0, 100]
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setMSpaNRStrth(const rk_aiq_sys_ctx_t* ctx, bool on, unsigned int level);
XCamReturn rk_aiq_uapi2_getMSpaNRStrth(const rk_aiq_sys_ctx_t* ctx, bool *on, unsigned int *level);

/*
*****************************
*
* Desc: set manual time noise reduction strength
*     this function is active for NR is manual mode
* Argument:
*   level: [0, 100]
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setMTNRStrth(const rk_aiq_sys_ctx_t* ctx, bool on, unsigned int level);

XCamReturn rk_aiq_uapi2_getMTNRStrth(const rk_aiq_sys_ctx_t* ctx, bool *on, unsigned int *level);


/*
**********************************************************
* Focus & Zoom
**********************************************************
*/
/*
*****************************
*
* Desc: set focus mode
* Argument:
*   mode:  auto: auto focus
*          manual: manual focus
*          semi-auto: semi-auto focus
*****************************
*/
XCamReturn rk_aiq_uapi2_setFocusMode(const rk_aiq_sys_ctx_t* ctx, opMode_t mode);
XCamReturn rk_aiq_uapi2_getFocusMode(const rk_aiq_sys_ctx_t* ctx, opMode_t *mode);

/*
*****************************
*
* Desc: set fix mode code
* Argument:
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setFixedModeCode(const rk_aiq_sys_ctx_t* ctx, unsigned short code);
XCamReturn rk_aiq_uapi2_getFixedModeCode(const rk_aiq_sys_ctx_t* ctx, unsigned short *code);

/*
*****************************
*
* Desc: set focus window
* Argument:
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setFocusWin(const rk_aiq_sys_ctx_t* ctx, paRect_t *rect);
XCamReturn rk_aiq_uapi2_getFocusWin(const rk_aiq_sys_ctx_t* ctx, paRect_t *rect);

/*
*****************************
*
* Desc: set/get focus meas config
* Argument:
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setFocusMeasCfg(const rk_aiq_sys_ctx_t* ctx, rk_aiq_af_algo_meas_t* meascfg);
XCamReturn rk_aiq_uapi2_getFocusMeasCfg(const rk_aiq_sys_ctx_t* ctx, rk_aiq_af_algo_meas_t* meascfg);

/*
*****************************
*
* Desc: lock/unlock auto focus
* Argument:
*
*
*****************************
*/
XCamReturn rk_aiq_uapi2_lockFocus(const rk_aiq_sys_ctx_t* ctx);
XCamReturn rk_aiq_uapi2_unlockFocus(const rk_aiq_sys_ctx_t* ctx);

/*
*****************************
*
* Desc: oneshot focus
* Argument:
*
*
*****************************
*/
XCamReturn rk_aiq_uapi2_oneshotFocus(const rk_aiq_sys_ctx_t* ctx);

/*
*****************************
*
* Desc: manual triger focus
* Argument:
*
*
*****************************
*/
XCamReturn rk_aiq_uapi2_manualTrigerFocus(const rk_aiq_sys_ctx_t* ctx);

/*
*****************************
*
* Desc: tracking focus
* Argument:
*
*
*****************************
*/
XCamReturn rk_aiq_uapi2_trackingFocus(const rk_aiq_sys_ctx_t* ctx);

/*
*****************************
*
* Desc: vcm config
* Argument:
*
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setVcmCfg(const rk_aiq_sys_ctx_t* ctx, rk_aiq_lens_vcmcfg* cfg);
XCamReturn rk_aiq_uapi2_getVcmCfg(const rk_aiq_sys_ctx_t* ctx, rk_aiq_lens_vcmcfg* cfg);

/*
*****************************
*
* Desc: af serach path record
* Argument:
*
*
*****************************
*/
XCamReturn rk_aiq_uapi2_getSearchPath(const rk_aiq_sys_ctx_t* ctx, rk_aiq_af_sec_path_t* path);

/*
*****************************
*
* Desc: af serach path record
* Argument:
*
*
*****************************
*/
XCamReturn rk_aiq_uapi2_getSearchResult(const rk_aiq_sys_ctx_t* ctx, rk_aiq_af_result_t* result);

/*
*****************************
*
* Desc: set/get zoom position
* Argument:
*
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setOpZoomPosition(const rk_aiq_sys_ctx_t* ctx, int pos);
XCamReturn rk_aiq_uapi2_getOpZoomPosition(const rk_aiq_sys_ctx_t* ctx, int *pos);

/*
*****************************
*
* Desc: get zoom range
* Argument:
*
*
*****************************
*/
XCamReturn rk_aiq_uapi2_getZoomRange(const rk_aiq_sys_ctx_t* ctx, rk_aiq_af_zoomrange* range);

/*
**********************************************************
* Color Correction
**********************************************************
*/
/*
*****************************
*
* Desc: set/get color correction mode
* Argument:
*   mode:
*     auto: auto color correction
*     manual: manual color correction
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setCCMMode(const rk_aiq_sys_ctx_t* ctx, opMode_t mode);
XCamReturn rk_aiq_uapi2_getCCMMode(const rk_aiq_sys_ctx_t* ctx, opMode_t *mode);

/*
*****************************
*
* Desc: set/get manual color correction matrix
*     this function is active for color correction is manual mode
* Argument:
* mccm:
*                   3x3 matrix
*                   1x3 offset
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setMCcCoef(const rk_aiq_sys_ctx_t* ctx,  rk_aiq_ccm_matrix_t *mccm);
XCamReturn rk_aiq_uapi2_getMCcCoef(const rk_aiq_sys_ctx_t* ctx,  rk_aiq_ccm_matrix_t *mccm);

/*
*****************************
*
* Desc: set/get auto color correction saturation
*     this function is active for color correction is auto mode
* Argument:
*   finalsat : range in [0, 100]
*
*****************************
*/
XCamReturn rk_aiq_uapi2_getACcmSat(const rk_aiq_sys_ctx_t* ctx,  float *finalsat);

/*
*****************************
*
* Desc: get auto color correction used illu name
*     this function is active for color correction is auto mode
* Argument:
*    illumination
*
*****************************
*/
XCamReturn rk_aiq_uapi2_getACcmIlluName(const rk_aiq_sys_ctx_t* ctx,  char *illumination);

/*
*****************************
*
* Desc: get auto color correction used ccm name
*     this function is active for color correction is auto mode
* Argument:
*    ccm_name[2]
*
*****************************
*/
XCamReturn rk_aiq_uapi2_getACcmMatrixName(const rk_aiq_sys_ctx_t* ctx,  char **ccm_name);

/*
**********************************************************
* 3-Dimensional Look Up Table
**********************************************************
*/
/*
*****************************
*
* Desc: set/get 3dlut mode
* Argument:
*   mode:
*     auto: auto 3dlut
*     manual: manual 3dlut
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setLut3dMode(const rk_aiq_sys_ctx_t* ctx, opMode_t mode);
XCamReturn rk_aiq_uapi2_getLut3dMode(const rk_aiq_sys_ctx_t* ctx, opMode_t *mode);

/*
*****************************
*
* Desc: set/get manual 3d Look-up-table
*     this function is active for 3dlut is manual mode
* Argument:
*     mlut:
*           lut_r[729]
*           lut_g[729]
*           lut_b[729]
*
*****************************
*/
XCamReturn rk_aiq_uapi2_setM3dLut(const rk_aiq_sys_ctx_t* ctx,  rk_aiq_lut3d_table_t *mlut);
XCamReturn rk_aiq_uapi2_getM3dLut(const rk_aiq_sys_ctx_t* ctx,  rk_aiq_lut3d_table_t *mlut);

/*
*****************************
*
* Desc: set/get auto 3d Look-up-table strength
*     this function is active for 3d Look-up-table is auto mode
* Argument:
*   alpha : range in [0, 1]
*
*****************************
*/
XCamReturn rk_aiq_uapi2_getA3dLutStrth(const rk_aiq_sys_ctx_t* ctx,  float *alpha);

/*
*****************************
*
* Desc: get auto 3d Look-up-table used lut name
*     this function is active for 3d Look-up-table is auto mode
* Argument:
*    name
*
*****************************
*/
XCamReturn rk_aiq_uapi2_getA3dLutName(const rk_aiq_sys_ctx_t* ctx,  char *name);

/*
*****************************
*
* Desc:
* Argument:
*****************************
*/
XCamReturn rk_aiq_uapi2_setLdchEn(const rk_aiq_sys_ctx_t* ctx, bool en);
/*
*****************************
*
* Desc: the adjustment range of distortion intensity is 0~255
* Argument:
*****************************
*/
XCamReturn rk_aiq_uapi2_setLdchCorrectLevel(const rk_aiq_sys_ctx_t* ctx, int correctLevel);

/*
*****************************
*
* Desc: fec dynamic switch, valid only if aiq hasn't executed the 'prepare' action
* Argument:
*****************************
*/
XCamReturn rk_aiq_uapi2_setFecEn(const rk_aiq_sys_ctx_t* ctx, bool en);

/*
*****************************
*
* Desc: set corrective direction of FEC, valid only if aiq hasn't executed the 'prepare' action
* Argument:
*****************************
*/
XCamReturn rk_aiq_uapi2_setFecCorrectDirection(const rk_aiq_sys_ctx_t* ctx,
        const fec_correct_direction_t direction);
/*
*****************************
*
* Desc: The FEC module is still working in bypass state
* Argument:
*****************************
*/
XCamReturn rk_aiq_uapi2_setFecBypass(const rk_aiq_sys_ctx_t* ctx, bool en);

/*
*****************************
*
* Desc: the adjustment range of distortion intensity is 0~255
* Argument:
*****************************
*/
XCamReturn rk_aiq_uapi2_setFecCorrectLevel(const rk_aiq_sys_ctx_t* ctx, int correctLevel);

/*
*****************************
*
* Desc:
* Argument:
*****************************
*/
XCamReturn rk_aiq_uapi2_setFecCorrectMode(const rk_aiq_sys_ctx_t* ctx,
        const fec_correct_mode_t mode);
RKAIQ_END_DECLARE

#endif
