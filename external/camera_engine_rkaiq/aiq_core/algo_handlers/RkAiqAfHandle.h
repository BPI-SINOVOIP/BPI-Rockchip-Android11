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
#ifndef _RK_AIQ_AF_HANDLE_INT_H_
#define _RK_AIQ_AF_HANDLE_INT_H_

#include <atomic>

#include "RkAiqHandle.h"
#include "af/rk_aiq_uapi_af_int.h"
#include "rk_aiq_api_private.h"
#include "rk_aiq_pool.h"
#include "xcam_mutex.h"

namespace RkCam {

class RkAiqAfHandleInt : virtual public RkAiqHandle {
 public:
    explicit RkAiqAfHandleInt(RkAiqAlgoDesComm* des, RkAiqCore* aiqCore)
        : RkAiqHandle(des, aiqCore), mProcResShared(nullptr) {
        memset(&mCurAtt, 0, sizeof(rk_aiq_af_attrib_t));
        memset(&mNewAtt, 0, sizeof(rk_aiq_af_attrib_t));
        isUpdateAttDone     = false;
        isUpdateZoomPosDone = false;
    };
    virtual ~RkAiqAfHandleInt() { RkAiqHandle::deInit(); };
    virtual XCamReturn updateConfig(bool needSync);
    virtual XCamReturn prepare();
    virtual XCamReturn preProcess();
    virtual XCamReturn processing();
    virtual XCamReturn postProcess();
    virtual XCamReturn genIspResult(RkAiqFullParams* params, RkAiqFullParams* cur_params);
    // TODO add algo specific methords, this is a sample
    XCamReturn setAttrib(rk_aiq_af_attrib_t* att);
    XCamReturn getAttrib(rk_aiq_af_attrib_t* att);
    XCamReturn lock();
    XCamReturn unlock();
    XCamReturn Oneshot();
    XCamReturn ManualTriger();
    XCamReturn Tracking();
    XCamReturn setZoomIndex(int index);
    XCamReturn getZoomIndex(int* index);
    XCamReturn endZoomChg();
    XCamReturn startZoomCalib();
    XCamReturn resetZoom();
    XCamReturn GetSearchPath(rk_aiq_af_sec_path_t* path);
    XCamReturn GetSearchResult(rk_aiq_af_result_t* result);
    XCamReturn GetFocusRange(rk_aiq_af_focusrange* range);
    XCamReturn GetCustomAfRes(rk_tool_customAf_res_t* att);

 protected:
    virtual void init();
    virtual void deInit() { RkAiqHandle::deInit(); };

 private:
    bool getValueFromFile(const char* path, int* pos);

    // TODO
    rk_aiq_af_attrib_t mCurAtt;
    rk_aiq_af_attrib_t mNewAtt;
    mutable std::atomic<bool> isUpdateAttDone;
    mutable std::atomic<bool> isUpdateZoomPosDone;
    int mLastZoomIndex;

    SmartPtr<RkAiqAlgoProcResAfIntShared> mProcResShared;

 private:
    DECLARE_HANDLE_REGISTER_TYPE(RkAiqAfHandleInt);
};

};  // namespace RkCam

#endif
