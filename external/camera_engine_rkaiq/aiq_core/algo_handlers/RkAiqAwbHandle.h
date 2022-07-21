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
#ifndef _RK_AIQ_AWB_HANDLE_INT_H_
#define _RK_AIQ_AWB_HANDLE_INT_H_

#include "RkAiqHandle.h"
#include "awb/rk_aiq_uapi_awb_int.h"
#include "rk_aiq_api_private.h"
#include "rk_aiq_pool.h"
#include "xcam_mutex.h"

namespace RkCam {

class RkAiqAwbHandleInt : public RkAiqHandle {
    friend class RkAiqAwbV21HandleInt;

 public:
    explicit RkAiqAwbHandleInt(RkAiqAlgoDesComm* des, RkAiqCore* aiqCore)
        : RkAiqHandle(des, aiqCore), mProcResShared(nullptr) {
        memset(&mCurAtt, 0, sizeof(rk_aiq_wb_attrib_t));
        memset(&mNewAtt, 0, sizeof(rk_aiq_wb_attrib_t));
        memset(&mCurWbV20Attr, 0, sizeof(mCurWbV20Attr));
        memset(&mCurWbOpModeAttr, 0, sizeof(mCurWbOpModeAttr));
        mCurWbOpModeAttr.mode = RK_AIQ_WB_MODE_MAX;
        memset(&mCurWbMwbAttr, 0, sizeof(mCurWbMwbAttr));
        memset(&mCurWbAwbAttr, 0, sizeof(mCurWbAwbAttr));
        memset(&mCurWbAwbWbGainAdjustAttr, 0, sizeof(mCurWbAwbWbGainAdjustAttr));
        memset(&mCurWbAwbWbGainOffsetAttr, 0, sizeof(mCurWbAwbWbGainOffsetAttr));
        memset(&mCurWbAwbMultiWindowAttr, 0, sizeof(mCurWbAwbMultiWindowAttr));
        memset(&mNewWbV20Attr, 0, sizeof(mNewWbV20Attr));
        memset(&mNewWbOpModeAttr, 0, sizeof(mNewWbOpModeAttr));
        mNewWbOpModeAttr.mode = RK_AIQ_WB_MODE_MAX;
        memset(&mNewWbMwbAttr, 0, sizeof(mNewWbMwbAttr));
        memset(&mNewWbAwbAttr, 0, sizeof(mNewWbAwbAttr));
        memset(&mNewWbAwbWbGainAdjustAttr, 0, sizeof(mNewWbAwbWbGainAdjustAttr));
        memset(&mNewWbAwbWbGainOffsetAttr, 0, sizeof(mNewWbAwbWbGainOffsetAttr));
        memset(&mNewWbAwbMultiWindowAttr, 0, sizeof(mNewWbAwbMultiWindowAttr));
        updateWbV20Attr             = false;
        updateWbOpModeAttr          = false;
        updateWbMwbAttr             = false;
        updateWbAwbAttr             = false;
        updateWbAwbWbGainAdjustAttr = false;
        updateWbAwbWbGainOffsetAttr = false;
        updateWbAwbMultiWindowAttr  = false;
    };
    virtual ~RkAiqAwbHandleInt() { RkAiqHandle::deInit(); };
    virtual XCamReturn updateConfig(bool needSync);
    virtual XCamReturn prepare();
    virtual XCamReturn preProcess();
    virtual XCamReturn processing();
    virtual XCamReturn postProcess();
    virtual XCamReturn genIspResult(RkAiqFullParams* params, RkAiqFullParams* cur_params);
    // TODO add algo specific methords, this is a sample
    XCamReturn setAttrib(rk_aiq_wb_attrib_t att);
    XCamReturn getAttrib(rk_aiq_wb_attrib_t* att);
    XCamReturn getCct(rk_aiq_wb_cct_t* cct);
    XCamReturn queryWBInfo(rk_aiq_wb_querry_info_t* wb_querry_info);
    XCamReturn lock();
    XCamReturn unlock();
    XCamReturn setWbV20Attrib(rk_aiq_uapiV2_wbV20_attrib_t att);
    XCamReturn getWbV20Attrib(rk_aiq_uapiV2_wbV20_attrib_t* att);
    XCamReturn setWbOpModeAttrib(rk_aiq_uapiV2_wb_opMode_t att);
    XCamReturn getWbOpModeAttrib(rk_aiq_uapiV2_wb_opMode_t* att);
    XCamReturn setMwbAttrib(rk_aiq_wb_mwb_attrib_t att);
    XCamReturn getMwbAttrib(rk_aiq_wb_mwb_attrib_t* att);
    XCamReturn setAwbV20Attrib(rk_aiq_uapiV2_wbV20_awb_attrib_t att);
    XCamReturn getAwbV20Attrib(rk_aiq_uapiV2_wbV20_awb_attrib_t* att);
    XCamReturn setWbAwbWbGainAdjustAttrib(rk_aiq_uapiV2_wb_awb_wbGainAdjust_t att);
    XCamReturn getWbAwbWbGainAdjustAttrib(rk_aiq_uapiV2_wb_awb_wbGainAdjust_t* att);
    XCamReturn setWbAwbWbGainOffsetAttrib(rk_aiq_uapiV2_wb_awb_wbGainOffset_t att);
    XCamReturn getWbAwbWbGainOffsetAttrib(rk_aiq_uapiV2_wb_awb_wbGainOffset_t* att);
    XCamReturn setWbAwbMultiWindowAttrib(rk_aiq_uapiV2_wb_awb_mulWindow_t att);
    XCamReturn getWbAwbMultiWindowAttrib(rk_aiq_uapiV2_wb_awb_mulWindow_t* att);
    XCamReturn getAlgoStat(rk_tool_awb_stat_res_full_t *awb_stat_algo);
    XCamReturn getStrategyResult(rk_tool_awb_strategy_result_t *awb_strategy_result);

 protected:
    virtual void init();
    virtual void deInit() { RkAiqHandle::deInit(); };
    SmartPtr<RkAiqAlgoProcResAwbIntShared> mProcResShared;

 private:
    // TODO
    rk_aiq_wb_attrib_t mCurAtt;
    rk_aiq_wb_attrib_t mNewAtt;
    // v2
    rk_aiq_uapiV2_wbV20_attrib_t mCurWbV20Attr;  // v21 todo
    rk_aiq_uapiV2_wbV20_attrib_t mNewWbV20Attr;
    rk_aiq_uapiV2_wb_opMode_t mCurWbOpModeAttr;
    rk_aiq_uapiV2_wb_opMode_t mNewWbOpModeAttr;
    rk_aiq_wb_mwb_attrib_t mCurWbMwbAttr;
    rk_aiq_wb_mwb_attrib_t mNewWbMwbAttr;
    rk_aiq_uapiV2_wbV20_awb_attrib_t mCurWbAwbAttr;
    rk_aiq_uapiV2_wbV20_awb_attrib_t mNewWbAwbAttr;
    rk_aiq_uapiV2_wb_awb_wbGainAdjust_t mCurWbAwbWbGainAdjustAttr;
    rk_aiq_uapiV2_wb_awb_wbGainAdjust_t mNewWbAwbWbGainAdjustAttr;
    rk_aiq_uapiV2_wb_awb_wbGainOffset_t mCurWbAwbWbGainOffsetAttr;
    rk_aiq_uapiV2_wb_awb_wbGainOffset_t mNewWbAwbWbGainOffsetAttr;
    rk_aiq_uapiV2_wb_awb_mulWindow_t mCurWbAwbMultiWindowAttr;
    rk_aiq_uapiV2_wb_awb_mulWindow_t mNewWbAwbMultiWindowAttr;
    mutable std::atomic<bool> updateWbV20Attr;
    mutable std::atomic<bool> updateWbOpModeAttr;
    mutable std::atomic<bool> updateWbMwbAttr;
    mutable std::atomic<bool> updateWbAwbAttr;
    mutable std::atomic<bool> updateWbAwbWbGainAdjustAttr;
    mutable std::atomic<bool> updateWbAwbWbGainOffsetAttr;
    mutable std::atomic<bool> updateWbAwbMultiWindowAttr;

 private:
    DECLARE_HANDLE_REGISTER_TYPE(RkAiqAwbHandleInt);
};

};  // namespace RkCam

#endif
