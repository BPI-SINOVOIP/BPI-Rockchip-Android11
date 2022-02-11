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

#include "CamHwIsp21.h"
//#include "isp20/Isp20PollThread.h"
#ifdef ANDROID_OS
#include <cutils/properties.h>
#endif

namespace RkCam {

CamHwIsp21::CamHwIsp21()
    : CamHwIsp20()
{
    mNoReadBack = true;
#ifndef ANDROID_OS
    char* valueStr = getenv("normal_no_read_back");
    if (valueStr) {
        mNoReadBack = atoi(valueStr) > 0 ? true : false;
    }
#else
    char property_value[PROPERTY_VALUE_MAX] = {'\0'};

    property_get("persist.vendor.rkisp_no_read_back", property_value, "-1");
    int val = atoi(property_value);
    if (val != -1)
        mNoReadBack = atoi(property_value) > 0 ? true : false;
#endif
}

CamHwIsp21::~CamHwIsp21()
{}

XCamReturn
CamHwIsp21::init(const char* sns_ent_name)
{
    xcam_mem_clear (_full_active_isp21_params);

    XCamReturn ret = CamHwIsp20::init(sns_ent_name);

    //SmartPtr<Isp20PollThread> isp20Pollthread =
    //    mPollthread.dynamic_cast_ptr<Isp20PollThread>();
    //isp20Pollthread->set_need_luma_rd_info(false);
    return ret;
}

XCamReturn
CamHwIsp21::stop()
{

    XCamReturn ret = CamHwIsp20::stop();
    xcam_mem_clear (_full_active_isp21_params);

    return ret;
}

bool CamHwIsp21::isOnlineByWorkingMode()
{
    return false;
}

XCamReturn
CamHwIsp21::dispatchResult(cam3aResultList& list)
{
    cam3aResultList isp_result_list;
    for (auto& result : list) {
        switch (result->getType()) {
        case RESULT_TYPE_FLASH_PARAM:
        case RESULT_TYPE_CPSL_PARAM:
        case RESULT_TYPE_IRIS_PARAM:
        case RESULT_TYPE_FOCUS_PARAM:
        case RESULT_TYPE_EXPOSURE:
            CamHwIsp20::dispatchResult(result);
            break;
        default:
            isp_result_list.push_back(result);
            break;
        }
    }

    if (isp_result_list.size() > 0) {
        handleIsp3aReslut(isp_result_list);
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
CamHwIsp21::dispatchResult(SmartPtr<cam3aResult> result)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    if (!result.ptr())
        return XCAM_RETURN_ERROR_PARAM;

    LOGD("%s enter, msg type(0x%x)", __FUNCTION__, result->getType());
    switch (result->getType())
    {
    case RESULT_TYPE_FLASH_PARAM:
    case RESULT_TYPE_CPSL_PARAM:
    case RESULT_TYPE_IRIS_PARAM:
    case RESULT_TYPE_FOCUS_PARAM:
    case RESULT_TYPE_EXPOSURE:
        return CamHwIsp20::dispatchResult(result);
    default:
        handleIsp3aReslut(result);
        break;
    }
    return ret;
    LOGD("%s exit", __FUNCTION__);
}

void
CamHwIsp21::gen_full_isp_params(const struct isp21_isp_params_cfg* update_params,
                                struct isp21_isp_params_cfg* full_params,
                                uint64_t* module_en_update_partial,
                                uint64_t* module_cfg_update_partial)
{
    XCAM_ASSERT (update_params);
    XCAM_ASSERT (full_params);
    int i = 0;

    ENTER_CAMHW_FUNCTION();
    for (; i <= RK_ISP2X_MAX_ID; i++)
        if (update_params->module_en_update & (1LL << i)) {
            if ((full_params->module_ens & (1LL << i)) !=
                    (update_params->module_ens & (1LL << i)))
                *module_en_update_partial |= 1LL << i;
            full_params->module_en_update |= 1LL << i;
            // clear old bit value
            full_params->module_ens &= ~(1LL << i);
            // set new bit value
            full_params->module_ens |= update_params->module_ens & (1LL << i);
        }

    for (i = 0; i <= RK_ISP2X_MAX_ID; i++) {
        if (update_params->module_cfg_update & (1LL << i)) {

#define CHECK_UPDATE_PARAMS(dst, src) \
            if (memcmp(&dst, &src, sizeof(dst)) == 0 && \
                full_params->frame_id > ISP_PARAMS_EFFECT_DELAY_CNT) { \
                continue; \
            } \
            *module_cfg_update_partial |= 1LL << i; \
            dst = src; \

            full_params->module_cfg_update |= 1LL << i;
            switch (i) {
            case RK_ISP2X_RAWAE_BIG1_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawae0, update_params->meas.rawae0);
                break;
            case RK_ISP2X_RAWAE_BIG2_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawae1, update_params->meas.rawae1);
                break;
            case RK_ISP2X_RAWAE_BIG3_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawae2, update_params->meas.rawae2);
                break;
            case RK_ISP2X_RAWAE_LITE_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawae3, update_params->meas.rawae3);
                break;
            case RK_ISP2X_RAWHIST_BIG1_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawhist0, update_params->meas.rawhist0);
                break;
            case RK_ISP2X_RAWHIST_BIG2_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawhist1, update_params->meas.rawhist1);
                break;
            case RK_ISP2X_RAWHIST_BIG3_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawhist2, update_params->meas.rawhist2);
                break;
            case RK_ISP2X_RAWHIST_LITE_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawhist3, update_params->meas.rawhist3);
                break;
            case RK_ISP2X_YUVAE_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.yuvae, update_params->meas.yuvae);
                break;
            case RK_ISP2X_SIHST_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.sihst, update_params->meas.sihst);
                break;
            case RK_ISP2X_SIAWB_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.siawb, update_params->meas.siawb);
                break;
            case RK_ISP2X_RAWAWB_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawawb, update_params->meas.rawawb);
                break;
            case RK_ISP2X_AWB_GAIN_ID:
                CHECK_UPDATE_PARAMS(full_params->others.awb_gain_cfg, update_params->others.awb_gain_cfg);
                break;
            case RK_ISP2X_RAWAF_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawaf, update_params->meas.rawaf);
                break;
            case RK_ISP2X_HDRMGE_ID:
                CHECK_UPDATE_PARAMS(full_params->others.hdrmge_cfg, update_params->others.hdrmge_cfg);
                break;
            /* case RK_ISP2X_HDRTMO_ID: */
            /*     full_params->others.hdrtmo_cfg = update_params->others.hdrtmo_cfg; */
            /*     break; */
            case RK_ISP2X_CTK_ID:
                CHECK_UPDATE_PARAMS(full_params->others.ccm_cfg, update_params->others.ccm_cfg);
                break;
            case RK_ISP2X_LSC_ID:
                CHECK_UPDATE_PARAMS(full_params->others.lsc_cfg, update_params->others.lsc_cfg);
                break;
            case RK_ISP2X_GOC_ID:
                CHECK_UPDATE_PARAMS(full_params->others.gammaout_cfg, update_params->others.gammaout_cfg);
                break;
            case RK_ISP2X_3DLUT_ID:
                CHECK_UPDATE_PARAMS(full_params->others.isp3dlut_cfg, update_params->others.isp3dlut_cfg);
                break;
            case RK_ISP2X_DPCC_ID:
                CHECK_UPDATE_PARAMS(full_params->others.dpcc_cfg, update_params->others.dpcc_cfg);
                break;
            case RK_ISP2X_BLS_ID:
                CHECK_UPDATE_PARAMS(full_params->others.bls_cfg, update_params->others.bls_cfg);
                break;
            case RK_ISP2X_DEBAYER_ID:
                CHECK_UPDATE_PARAMS(full_params->others.debayer_cfg, update_params->others.debayer_cfg);
                break;
            case RK_ISP2X_DHAZ_ID:
                CHECK_UPDATE_PARAMS(full_params->others.dhaz_cfg, update_params->others.dhaz_cfg);
                break;
            /* case RK_ISP2X_RAWNR_ID: */
            /*     full_params->others.rawnr_cfg = update_params->others.rawnr_cfg; */
            /*     break; */
            /* case RK_ISP2X_GAIN_ID: */
            /*     full_params->others.gain_cfg = update_params->others.gain_cfg; */
            /*     break; */
            case RK_ISP2X_LDCH_ID:
                CHECK_UPDATE_PARAMS(full_params->others.ldch_cfg, update_params->others.ldch_cfg);
                break;
            case RK_ISP2X_GIC_ID:
                CHECK_UPDATE_PARAMS(full_params->others.gic_cfg, update_params->others.gic_cfg);
                break;
            case RK_ISP2X_CPROC_ID:
                CHECK_UPDATE_PARAMS(full_params->others.cproc_cfg, update_params->others.cproc_cfg);
                break;
            case Rk_ISP21_BAYNR_ID:
                CHECK_UPDATE_PARAMS(full_params->others.baynr_cfg, update_params->others.baynr_cfg);
                break;
            case Rk_ISP21_BAY3D_ID:
                CHECK_UPDATE_PARAMS(full_params->others.bay3d_cfg, update_params->others.bay3d_cfg);
                break;
            case Rk_ISP21_YNR_ID:
                CHECK_UPDATE_PARAMS(full_params->others.ynr_cfg, update_params->others.ynr_cfg);
                break;
            case Rk_ISP21_CNR_ID:
                CHECK_UPDATE_PARAMS(full_params->others.cnr_cfg, update_params->others.cnr_cfg);
                break;
            case Rk_ISP21_SHARP_ID:
                CHECK_UPDATE_PARAMS(full_params->others.sharp_cfg, update_params->others.sharp_cfg);
                break;
            case Rk_ISP21_DRC_ID:
                CHECK_UPDATE_PARAMS(full_params->others.drc_cfg, update_params->others.drc_cfg);
                break;
            case RK_ISP2X_SDG_ID:
                CHECK_UPDATE_PARAMS(full_params->others.sdg_cfg, update_params->others.sdg_cfg);
                break;
            default:
                break;
            }
        }
    }
    EXIT_CAMHW_FUNCTION();
}

/*
 * some module(HDR/TNR) parameters are related to the next frame exposure
 * and can only be easily obtained at the hwi layer,
 * so these parameters are calculated at hwi and the result is overwritten.
 */
XCamReturn
CamHwIsp21::overrideExpRatioToAiqResults(const sint32_t frameId,
        int module_id,
        cam3aResultList &results,
        int hdr_mode)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    SmartPtr<RkAiqExpParamsProxy> curFrameExpParam;
    SmartPtr<RkAiqExpParamsProxy> nextFrameExpParam;
    SmartPtr<BaseSensorHw> mSensorSubdev = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();

    if (mSensorSubdev.ptr()) {
        if (mSensorSubdev->getEffectiveExpParams(curFrameExpParam, frameId) < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "exp-sync: module_id: 0x%x, rx id: %d\n",
                            module_id,
                            frameId);
            return ret;
        }

        if (mSensorSubdev->getEffectiveExpParams(nextFrameExpParam, frameId + 1) < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "exp-sync: module_id: 0x%x, rx id: %d\n",
                            module_id,
                            frameId + 1);
            return ret;
        }
    }

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "exp-sync: module_id: 0x%x, rx id: %d\n"
                    "curFrame(%d): lexp: %f-%f, mexp: %f-%f, sexp: %f-%f\n"
                    "nextFrame(%d): lexp: %f-%f, mexp: %f-%f, sexp: %f-%f\n",
                    module_id,
                    frameId,
                    frameId,
                    curFrameExpParam->data()->aecExpInfo.HdrExp[2].exp_real_params.analog_gain,
                    curFrameExpParam->data()->aecExpInfo.HdrExp[2].exp_real_params.integration_time,
                    curFrameExpParam->data()->aecExpInfo.HdrExp[1].exp_real_params.analog_gain,
                    curFrameExpParam->data()->aecExpInfo.HdrExp[1].exp_real_params.integration_time,
                    curFrameExpParam->data()->aecExpInfo.HdrExp[0].exp_real_params.analog_gain,
                    curFrameExpParam->data()->aecExpInfo.HdrExp[0].exp_real_params.integration_time,
                    frameId + 1,
                    nextFrameExpParam->data()->aecExpInfo.HdrExp[2].exp_real_params.analog_gain,
                    nextFrameExpParam->data()->aecExpInfo.HdrExp[2].exp_real_params.integration_time,
                    nextFrameExpParam->data()->aecExpInfo.HdrExp[1].exp_real_params.analog_gain,
                    nextFrameExpParam->data()->aecExpInfo.HdrExp[1].exp_real_params.integration_time,
                    nextFrameExpParam->data()->aecExpInfo.HdrExp[0].exp_real_params.analog_gain,
                    nextFrameExpParam->data()->aecExpInfo.HdrExp[0].exp_real_params.integration_time);

    //calc expo
    float nextLExpo = 0;
    float nextMExpo = 0;
    float nextSExpo = 0;
    float nextRatioLS = 1.0;
    if(hdr_mode == RK_AIQ_WORKING_MODE_NORMAL) {
        nextLExpo = nextFrameExpParam->data()->aecExpInfo.LinearExp.exp_real_params.analog_gain * \
                    nextFrameExpParam->data()->aecExpInfo.LinearExp.exp_real_params.integration_time;
        nextMExpo = nextLExpo;
        nextSExpo = nextLExpo;
    }
    else if(hdr_mode >= RK_AIQ_WORKING_MODE_ISP_HDR2) {
        nextLExpo = nextFrameExpParam->data()->aecExpInfo.HdrExp[1].exp_real_params.analog_gain * \
                    nextFrameExpParam->data()->aecExpInfo.HdrExp[1].exp_real_params.integration_time;
        nextMExpo = nextLExpo;
        nextSExpo = nextFrameExpParam->data()->aecExpInfo.HdrExp[0].exp_real_params.analog_gain * \
                    nextFrameExpParam->data()->aecExpInfo.HdrExp[0].exp_real_params.integration_time;
    }
    else {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get wrong hdr mode\n");
        return ret;
    }

    nextRatioLS = nextLExpo / nextSExpo;
    nextRatioLS = nextRatioLS >= 1 ? nextRatioLS : 1;

    switch (module_id) {
    case Rk_ISP21_DRC_ID:
    {
        SmartPtr<cam3aResult> drc_res = get_3a_module_result(results, RESULT_TYPE_DRC_PARAM);
        SmartPtr<RkAiqIspDrcParamsProxyV21> drcParamsProxy;
        if (drc_res.ptr()) {
            drcParamsProxy = drc_res.dynamic_cast_ptr<RkAiqIspDrcParamsProxyV21>();
        } else {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get drc params from 3a result failed!\n");
            return ret;
        }
        rk_aiq_isp_drc_v21_t& drc = drcParamsProxy->data()->result;

        if(!drc.bTmoEn)
            break;

        if (drc.LongFrameMode)
            nextRatioLS = 1.0;

        //get sw_drc_compres_scl
        float MFHDR_LOG_Q_BITS = 11;
        float adrc_gain = drc.DrcProcRes.sw_drc_adrc_gain;
        float log_ratio2 = log(nextRatioLS * adrc_gain) / log(2.0f) + 12;
        float offsetbits_int = (float)(drc.DrcProcRes.sw_drc_offset_pow2);
        float offsetbits = offsetbits_int * pow(2, MFHDR_LOG_Q_BITS);
        float hdrbits = log_ratio2 * pow(2, MFHDR_LOG_Q_BITS);
        float hdrvalidbits = hdrbits - offsetbits;
        float compres_scl = (12 * pow(2, MFHDR_LOG_Q_BITS * 2)) / hdrvalidbits;
        drc.DrcProcRes.sw_drc_compres_scl = (int)(compres_scl);

        //get sw_drc_min_ogain
        if(drc.DrcProcRes.sw_drc_min_ogain == 1)
            drc.DrcProcRes.sw_drc_min_ogain = 1 << 15;
        else {
            float sw_drc_min_ogain = 1 / (nextRatioLS * adrc_gain);
            drc.DrcProcRes.sw_drc_min_ogain = (int)(sw_drc_min_ogain * pow(2, 15) + 0.5);
        }

        //get sw_drc_compres_y
        if(drc.CompressMode == COMPRESS_AUTO) {
            float curveparam, curveparam2, curveparam3, tmp;
            float luma2[17] = { 0, 1024, 2048, 3072, 4096, 5120, 6144, 7168, 8192, 10240, 12288, 14336, 16384, 18432, 20480, 22528, 24576 };
            float curveTable[17];
            float ISP_RAW_BIT = 12;
            float dstbits = ISP_RAW_BIT * pow(2, MFHDR_LOG_Q_BITS);
            float validbits = dstbits - offsetbits;
            for(int i = 0; i < ISP21_DRC_Y_NUM; ++i)
            {
                curveparam = (float)(validbits - 0) / (hdrvalidbits - validbits + pow(2, -6));
                curveparam2 = validbits * (1 + curveparam);
                curveparam3 = hdrvalidbits * curveparam;
                tmp = luma2[i] * hdrvalidbits / 24576;
                curveTable[i] = (tmp * curveparam2 / (tmp + curveparam3));
                drc.DrcProcRes.sw_drc_compres_y[i] = (int)(curveTable[i]) ;
            }
        }

        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "nextRatioLS:%f sw_drc_compres_scl:%d sw_drc_min_ogain:%d\n",
                        nextRatioLS, drc.DrcProcRes.sw_drc_compres_scl, drc.DrcProcRes.sw_drc_min_ogain);
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "CompressMode:%d\n", drc.CompressMode);
        for(int i = 0; i < ISP21_DRC_Y_NUM; ++i)
            LOGD_CAMHW_SUBM(ISP20HW_SUBM, "sw_drc_compres_y[%d]:%d\n", i, drc.DrcProcRes.sw_drc_compres_y[i]);

        break;
    }
    case RK_ISP2X_HDRMGE_ID:
    {
        SmartPtr<cam3aResult> merge_res = get_3a_module_result(results, RESULT_TYPE_MERGE_PARAM);
        SmartPtr<RkAiqIspMergeParamsProxy> mergeParamsProxy;

        if (merge_res.ptr()) {
            mergeParamsProxy = merge_res.dynamic_cast_ptr<RkAiqIspMergeParamsProxy>();
        } else {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get drc params from 3a result failed!\n");
            return ret;
        }
        RkAiqAmergeProcResult_t& merge = mergeParamsProxy->data()->result;

        if(merge.Res.sw_hdrmge_mode == 0)
            break;

        //get sw_hdrmge_gain0
        merge.Res.sw_hdrmge_gain0 = (int)(64 * nextRatioLS);
        if(nextRatioLS == 1)
            merge.Res.sw_hdrmge_gain0_inv = (int)(4096 * (1 / nextRatioLS) - 1);
        else
            merge.Res.sw_hdrmge_gain0_inv = (int)(4096 * (1 / nextRatioLS));

        //get sw_hdrmge_gain1
        merge.Res.sw_hdrmge_gain1 = 0x40;
        merge.Res.sw_hdrmge_gain1_inv = 0xfff;

        //get sw_hdrmge_gain2
        merge.Res.sw_hdrmge_gain2 = 0x40;

        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "sw_hdrmge_gain0:%d sw_hdrmge_gain0_inv:%d\n",
                        merge.Res.sw_hdrmge_gain0, merge.Res.sw_hdrmge_gain0_inv);

        break;
    }
    default:
        LOGW_CAMHW_SUBM(ISP20HW_SUBM, "unkown module id: 0x%x!\n", module_id);
        break;
    }

    return ret;
}

XCamReturn
CamHwIsp21::setIspConfig()
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    ENTER_CAMHW_FUNCTION();

    SmartPtr<V4l2Buffer> v4l2buf;
    uint32_t frameId = -1;

    {
        SmartLock locker (_isp_params_cfg_mutex);
        while (_effecting_ispparam_map.size() > 4)
            _effecting_ispparam_map.erase(_effecting_ispparam_map.begin());
    }
    if (mIspParamsDev.ptr()) {
        ret = mIspParamsDev->get_buffer(v4l2buf);
        if (ret) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "Can not get isp params buffer\n");
            return XCAM_RETURN_ERROR_PARAM;
        }
    } else
        return XCAM_RETURN_BYPASS;

    cam3aResultList ready_results;
    ret = mParamsAssembler->deQueOne(ready_results, frameId);
    if (ret != XCAM_RETURN_NO_ERROR) {
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "deque isp ready parameter failed\n");
        mIspParamsDev->return_buffer_to_pool (v4l2buf);
        return XCAM_RETURN_ERROR_PARAM;
    }

    LOGD_ANALYZER("----------%s, start config id(%d)'s isp params", __FUNCTION__, frameId);

    struct isp21_isp_params_cfg update_params;

    update_params.module_en_update = 0;
    update_params.module_ens = 0;
    update_params.module_cfg_update = 0;
    if (_state == CAM_HW_STATE_STOPPED || _state == CAM_HW_STATE_PREPARED || _state == CAM_HW_STATE_PAUSED) {
        // update all ens
        _full_active_isp21_params.module_en_update = ~0;
        // just re-config the enabled moddules
        _full_active_isp21_params.module_cfg_update = _full_active_isp21_params.module_ens;
    } else {
        _full_active_isp21_params.module_en_update = 0;
        // use module_ens to store module status, so we can use it to restore
        // the init params for re-start and re-prepare
        /* _full_active_isp21_params.module_ens = 0; */
        _full_active_isp21_params.module_cfg_update = 0;
    }

    ret = overrideExpRatioToAiqResults(frameId, Rk_ISP21_DRC_ID, ready_results, _hdr_mode);
    if (ret < 0) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "DRC convertExpRatioToAiqResults error!\n");
    }

    ret = overrideExpRatioToAiqResults(frameId, RK_ISP2X_HDRMGE_ID, ready_results, _hdr_mode);
    if (ret < 0) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "Merge convertExpRatioToAiqResults error!\n");
    }

    if (frameId >= 0) {
        SmartPtr<cam3aResult> awb_res = get_3a_module_result(ready_results, RESULT_TYPE_AWB_PARAM);
        SmartPtr<RkAiqIspAwbParamsProxyV21> awbParams;
        if (awb_res.ptr()) {
            awbParams = awb_res.dynamic_cast_ptr<RkAiqIspAwbParamsProxyV21>();
            {
                SmartLock locker (_isp_params_cfg_mutex);
                _effecting_ispparam_map[frameId].awb_cfg_v201 = awbParams->data()->result;

            }
        } else {
            /* use the latest */
            SmartLock locker (_isp_params_cfg_mutex);
            if (!_effecting_ispparam_map.empty()) {
                _effecting_ispparam_map[frameId].awb_cfg_v201 = (_effecting_ispparam_map.rbegin())->second.awb_cfg_v201;
                LOGW_CAMHW_SUBM(ISP20HW_SUBM, "use frame %d awb params for frame %d !\n",
                                frameId, (_effecting_ispparam_map.rbegin())->first);
            } else {
                LOGW_CAMHW_SUBM(ISP20HW_SUBM, "get awb params from 3a result failed for frame %d !\n", frameId);
            }
        }
    }

    // TODO: merge_isp_results would cause the compile warning: reference to ‘merge_isp_results’ is ambiguous
    // now use Isp21Params::merge_isp_results instead
    if (Isp21Params::merge_isp_results(ready_results, &update_params) != XCAM_RETURN_NO_ERROR)
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "ISP parameter translation error\n");

    uint64_t module_en_update_partial = 0;
    uint64_t module_cfg_update_partial = 0;
    gen_full_isp_params(&update_params, &_full_active_isp21_params,
                        &module_en_update_partial, &module_cfg_update_partial);

    if (_state == CAM_HW_STATE_STOPPED)
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "ispparam ens 0x%llx, en_up 0x%llx, cfg_up 0x%llx",
                        _full_active_isp21_params.module_ens,
                        _full_active_isp21_params.module_en_update,
                        _full_active_isp21_params.module_cfg_update);
#ifdef RUNTIME_MODULE_DEBUG
    _full_active_isp21_params.module_en_update &= ~g_disable_isp_modules_en;
    _full_active_isp21_params.module_ens |= g_disable_isp_modules_en;
    _full_active_isp21_params.module_cfg_update &= ~g_disable_isp_modules_cfg_update;
    module_en_update_partial = _full_active_isp21_params.module_en_update;
    module_cfg_update_partial = _full_active_isp21_params.module_cfg_update;
#endif

    {
        SmartLock locker (_isp_params_cfg_mutex);
        if (frameId < 0) {
            _effecting_ispparam_map[0].isp_params_v21 = _full_active_isp21_params;
        } else {
            _effecting_ispparam_map[frameId].isp_params_v21 = _full_active_isp21_params;
        }
    }

    if (v4l2buf.ptr()) {
        struct isp21_isp_params_cfg* isp_params;
        int buf_index = v4l2buf->get_buf().index;

        isp_params = (struct isp21_isp_params_cfg*)v4l2buf->get_buf().m.userptr;
        *isp_params = _full_active_isp21_params;
        isp_params->module_en_update = module_en_update_partial;
        isp_params->module_cfg_update = module_cfg_update_partial;
        //TODO: isp driver has bug now, lsc cfg_up should be set along with
        //en_up
        if (isp_params->module_cfg_update & ISP2X_MODULE_LSC)
            isp_params->module_en_update |= ISP2X_MODULE_LSC;
        isp_params->frame_id = frameId;

#if 0 //TODO: isp21 params has no exposure info field
        SmartPtr<SensorHw> mSensorSubdev = mSensorDev.dynamic_cast_ptr<SensorHw>();
        if (mSensorSubdev.ptr()) {
            memset(&isp_params->exposure, 0, sizeof(isp_params->exposure));
            SmartPtr<RkAiqExpParamsProxy> expParam;

            if (mSensorSubdev->getEffectiveExpParams(expParam, frameId) < 0) {
                LOGE_CAMHW_SUBM(ISP20HW_SUBM, "frame_id(%d), get exposure failed!!!\n", frameId);
            } else {
                if (RK_AIQ_HDR_GET_WORKING_MODE(_hdr_mode) == RK_AIQ_WORKING_MODE_NORMAL) {
                    isp_params->exposure.linear_exp.analog_gain_code_global = \
                            expParam->data()->aecExpInfo.LinearExp.exp_sensor_params.analog_gain_code_global;
                    isp_params->exposure.linear_exp.coarse_integration_time = \
                            expParam->data()->aecExpInfo.LinearExp.exp_sensor_params.coarse_integration_time;
                } else {
                    isp_params->exposure.hdr_exp[0].analog_gain_code_global = \
                            expParam->data()->aecExpInfo.HdrExp[0].exp_sensor_params.analog_gain_code_global;
                    isp_params->exposure.hdr_exp[0].coarse_integration_time = \
                            expParam->data()->aecExpInfo.HdrExp[0].exp_sensor_params.coarse_integration_time;
                    isp_params->exposure.hdr_exp[1].analog_gain_code_global = \
                            expParam->data()->aecExpInfo.HdrExp[1].exp_sensor_params.analog_gain_code_global;
                    isp_params->exposure.hdr_exp[1].coarse_integration_time = \
                            expParam->data()->aecExpInfo.HdrExp[1].exp_sensor_params.coarse_integration_time;
                    isp_params->exposure.hdr_exp[2].analog_gain_code_global = \
                            expParam->data()->aecExpInfo.HdrExp[2].exp_sensor_params.analog_gain_code_global;
                    isp_params->exposure.hdr_exp[2].coarse_integration_time = \
                            expParam->data()->aecExpInfo.HdrExp[2].exp_sensor_params.coarse_integration_time;
                }
            }
        }
#endif
        if (mIspParamsDev->queue_buffer (v4l2buf) != 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "RKISP1: failed to ioctl VIDIOC_QBUF for index %d, %d %s.\n",
                            buf_index, errno, strerror(errno));
            mIspParamsDev->return_buffer_to_pool (v4l2buf);
            return XCAM_RETURN_ERROR_IOCTL;
        }

        ispModuleEns = _full_active_isp21_params.module_ens;
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "ispparam ens 0x%llx, en_up 0x%llx, cfg_up 0x%llx",
                        _full_active_isp21_params.module_ens,
                        isp_params->module_en_update,
                        isp_params->module_cfg_update);

        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "device(%s) queue buffer index %d, queue cnt %d, check exit status again[exit: %d]",
                        XCAM_STR (mIspParamsDev->get_device_name()),
                        buf_index, mIspParamsDev->get_queued_bufcnt(), _is_exit);
        if (_is_exit)
            return XCAM_RETURN_BYPASS;
    } else
        return XCAM_RETURN_BYPASS;

    EXIT_CAMHW_FUNCTION();
    return ret;
}


};
