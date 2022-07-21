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
#ifndef _RK_AIQ_YNR_V3_HANDLE_INT_H_
#define _RK_AIQ_YNR_V3_HANDLE_INT_H_

#include "RkAiqHandle.h"
#include "aynr3/rk_aiq_uapi_aynr_int_v3.h"
#include "rk_aiq_api_private.h"
#include "rk_aiq_pool.h"
#include "xcam_mutex.h"

namespace RkCam {

class RkAiqAynrV3HandleInt : virtual public RkAiqHandle {
 public:
    explicit RkAiqAynrV3HandleInt(RkAiqAlgoDesComm* des, RkAiqCore* aiqCore)
        : RkAiqHandle(des, aiqCore) {
        updateAtt      = false;
        updateStrength = false;
        memset(&mCurStrength, 0x00, sizeof(mCurStrength));
        mCurStrength.percent = 1.0;
        memset(&mNewStrength, 0x00, sizeof(mNewStrength));
        mNewStrength.percent = 1.0;
        memset(&mCurAtt, 0x00, sizeof(mCurAtt));
        memset(&mNewAtt, 0x00, sizeof(mNewAtt));
    };
    virtual ~RkAiqAynrV3HandleInt() { RkAiqHandle::deInit(); };
    virtual XCamReturn updateConfig(bool needSync);
    virtual XCamReturn prepare();
    virtual XCamReturn preProcess();
    virtual XCamReturn processing();
    virtual XCamReturn postProcess();
    virtual XCamReturn genIspResult(RkAiqFullParams* params, RkAiqFullParams* cur_params);
    // TODO add algo specific methords, this is a sample
    XCamReturn setAttrib(rk_aiq_ynr_attrib_v3_t* att);
    XCamReturn getAttrib(rk_aiq_ynr_attrib_v3_t* att);
    XCamReturn setStrength(rk_aiq_ynr_strength_v3_t* pStrength);
    XCamReturn getStrength(rk_aiq_ynr_strength_v3_t* pStrength);

 protected:
    virtual void init();
    virtual void deInit() { RkAiqHandle::deInit(); };

 private:
    // TODO
    rk_aiq_ynr_attrib_v3_t mCurAtt;
    rk_aiq_ynr_attrib_v3_t mNewAtt;
    rk_aiq_ynr_strength_v3_t mCurStrength;
    rk_aiq_ynr_strength_v3_t mNewStrength;
    mutable std::atomic<bool> updateStrength;

 private:
    DECLARE_HANDLE_REGISTER_TYPE(RkAiqAynrV3HandleInt);
};

};  // namespace RkCam

#endif
