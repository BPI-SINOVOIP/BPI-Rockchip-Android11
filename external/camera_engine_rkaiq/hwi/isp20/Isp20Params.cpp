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

#include <cstring>
#include "Isp20Params.h"
#include "isp21/rkisp21-config.h"

namespace RkCam {

uint32_t IspParamsAssembler::MAX_PENDING_PARAMS = 10;

IspParamsAssembler::IspParamsAssembler (const char* name)
    : mLatestReadyFrmId(-1)
    , mReadyMask(0)
    , mName(name)
    , mReadyNums(0)
    , mCondNum(0)
    , started(false)
{
}

IspParamsAssembler::~IspParamsAssembler ()
{
}

void
IspParamsAssembler::rmReadyCondition(uint32_t cond)
{
    SmartLock locker (mParamsMutex);
    LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: enter \n",
                    __FUNCTION__, __LINE__, mName.c_str());
    if (mCondMaskMap.find(cond) != mCondMaskMap.end()) {
        mReadyMask &= ~mCondMaskMap[cond];
    }
    LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: exit \n",
                    __FUNCTION__, __LINE__, mName.c_str());
}

void
IspParamsAssembler::addReadyCondition(uint32_t cond)
{
    SmartLock locker (mParamsMutex);
    LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: enter \n",
                    __FUNCTION__, __LINE__, mName.c_str());

    if (mCondMaskMap.find(cond) == mCondMaskMap.end()) {
        if (mCondNum > 63) {
            LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: max condintion num exceed 32",
                            mName.c_str());
            return;
        }

        mCondMaskMap[cond] = 1 << mCondNum;
        mReadyMask |= mCondMaskMap[cond];
        mCondNum++;
        LOGI_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: map cond %s 0x%x -> 0x%llx, mask: 0x%llx",
                        mName.c_str(), Cam3aResultType2Str[cond], cond, mCondMaskMap[cond], mReadyMask);
    } else {
        LOGI_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: map cond %s 0x%x -> 0x%llx already added",
                        mName.c_str(), Cam3aResultType2Str[cond], cond, mCondMaskMap[cond]);
    }

    LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: exit \n",
                    __FUNCTION__, __LINE__, mName.c_str());
}

XCamReturn
IspParamsAssembler::queue(SmartPtr<cam3aResult>& result)
{
    SmartLock locker (mParamsMutex);
    return queue_locked(result);
}

XCamReturn
IspParamsAssembler::queue_locked(SmartPtr<cam3aResult>& result)
{
    LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: enter \n",
                    __FUNCTION__, __LINE__, mName.c_str());

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    if (!result.ptr()) {
        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: null result", mName.c_str());
        return ret;
    }

    sint32_t frame_id = result->getId();
    int type = result->getType();

    if (!started) {
        LOGI_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: intial params type %s , result_id[%d] !",
                        mName.c_str(), Cam3aResultType2Str[type], frame_id);
        if (frame_id != 0)
            LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: intial params type %s , result_id[%d] != 0",
                            mName.c_str(), Cam3aResultType2Str[type], frame_id);
        mInitParamsList.push_back(result);

        return XCAM_RETURN_NO_ERROR;
    }

#if 0 // allow non-mandatory params
    if (mCondMaskMap.find(type) == mCondMaskMap.end()) {
        LOGI_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: result type: 0x%x is not required, skip ",
                        mName.c_str(), type);
        for (auto cond_it : mCondMaskMap)
            LOGI_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: -->need type: 0x%x", mName.c_str(), cond_it.first);
        return ret;
    }
#endif
    // exception case 1 : wrong result frame_id
    if (frame_id != -1 && (frame_id <= mLatestReadyFrmId)) {
        // merged to the oldest one
        bool found = false;
        for (const auto& iter : mParamsMap) {
            if (!(iter.second.flags & mCondMaskMap[type])) {
                frame_id = iter.first;
                found = true;
                break;
            }
        }
        if (!found) {
            if (!mParamsMap.empty())
                frame_id = (mParamsMap.rbegin())->first + 1;
            else {
                frame_id = mLatestReadyFrmId + 1;
                LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: type %s, mLatestReadyFrmId %d, "
                                "can't find a proper unready params, impossible case",
                                mName.c_str(), Cam3aResultType2Str[type], mLatestReadyFrmId);
            }
        }
        LOGI_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: type %s , delayed result_id[%d], merged to %d",
                        mName.c_str(), Cam3aResultType2Str[type], result->getId(), frame_id);
        result->setId(frame_id);
    } else if (frame_id != 0 && mLatestReadyFrmId == -1) {
        LOGW_CAMHW_SUBM(ISP20PARAM_SUBM,
                        "Wrong initial id %d set to 0, last %d", frame_id,
                        mLatestReadyFrmId);
        frame_id = 0;
        result->setId(0);
    }

    mParamsMap[frame_id].params.push_back(result);
    mParamsMap[frame_id].flags |= mCondMaskMap[type];

    LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s, new params: frame: %d, type:%s, flag: 0x%llx",
                    mName.c_str(), frame_id, Cam3aResultType2Str[type], mCondMaskMap[type]);

    bool ready =
        (mReadyMask == mParamsMap[frame_id].flags) ? true : false;

    LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s, frame: %d, flags: 0x%llx, mask: 0x%llx, ready status: %d !",
                    mName.c_str(), frame_id, mParamsMap[frame_id].flags, mReadyMask, ready);

    mParamsMap[frame_id].ready = ready;

    if (ready) {
        mReadyNums++;
        if (frame_id > mLatestReadyFrmId)
            mLatestReadyFrmId = frame_id;
        else {
            // impossible case
            LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%s, wrong ready params, latest %d <= new %d, drop it !",
                            mName.c_str(), mLatestReadyFrmId, frame_id);
            mParamsMap.erase(mParamsMap.find(frame_id));
            return ret;
        }
        LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s, frame: %d params ready, mReadyNums: %d !",
                        mName.c_str(), frame_id, mReadyNums);
    }

    bool overflow = false;
    if (mParamsMap.size() > MAX_PENDING_PARAMS) {
        LOGW_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: pending params overflow, max is %d",
                        mName.c_str(), MAX_PENDING_PARAMS);
        overflow = true;
    }
    bool ready_disorder = false;
    if (mReadyNums > 0 && !(mParamsMap.begin())->second.ready) {
        ready_disorder = true;
        LOGW_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: ready params disordered",
                        mName.c_str());
    }
    if (overflow || ready_disorder) {
        // exception case 2 : current ready one is not the first one in
        // mParamsMap, this means some conditions frame_id may be NOT
        // continuous, should check the AIQCORE and isp driver,
        // so far we merge all disordered to one.
        std::map<int, params_t>::iterator it = mParamsMap.begin();
        cam3aResultList merge_list;
        sint32_t merge_id = 0;
        for (it = mParamsMap.begin(); it != mParamsMap.end();) {
            if (!(it->second.ready)) {
                LOGW_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: ready disorderd, NOT ready id(flags:0x%x) %d < ready %d !",
                                mName.c_str(), it->second.flags, it->first, frame_id);
                // print missing params
                std::string missing_conds;
                for (auto cond : mCondMaskMap) {
                    if (!(cond.second & it->second.flags)) {
                        missing_conds.append(Cam3aResultType2Str[cond.first]);
                        missing_conds.append(",");
                    }
                }
                if (!missing_conds.empty())
                    LOGW_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: [%d] missing conditions: %s !",
                                    mName.c_str(), it->first, missing_conds.c_str());
                // forced to ready
                merge_list.insert(merge_list.end(), it->second.params.begin(), it->second.params.end());
                merge_id = it->first;
                it = mParamsMap.erase(it);
            } else
                break;
        }

        if (merge_list.size() > 0) {
            mReadyNums++;
            if (merge_id > mLatestReadyFrmId)
                mLatestReadyFrmId = merge_id;
            mParamsMap[merge_id].params.clear();
            mParamsMap[merge_id].params.assign(merge_list.begin(), merge_list.end());
            LOGW_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: merge all pending disorderd to frame %d !",
                            mName.c_str(), merge_id);
            mParamsMap[merge_id].flags = mReadyMask;
            mParamsMap[merge_id].ready = true;
        }
    }

    LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: exit \n",
                    __FUNCTION__, __LINE__, mName.c_str());

    return ret;

}

XCamReturn
IspParamsAssembler::queue(cam3aResultList& results)
{
    LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: enter \n", __FUNCTION__, __LINE__, mName.c_str());

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    SmartLock locker (mParamsMutex);

    for (auto result : results)
        queue_locked(result);

    LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: exit \n",
                    __FUNCTION__, __LINE__, mName.c_str());

    return ret;
}

void
IspParamsAssembler::forceReady(sint32_t frame_id)
{
    SmartLock locker (mParamsMutex);

    if (mParamsMap.find(frame_id) != mParamsMap.end()) {
        if (!mParamsMap[frame_id].ready) {
            // print missing params
            std::string missing_conds;
            for (auto cond : mCondMaskMap) {
                if (!(cond.second & mParamsMap[frame_id].flags)) {
                    missing_conds.append(Cam3aResultType2Str[cond.first]);
                    missing_conds.append(",");
                }
            }
            if (!missing_conds.empty())
                LOGW_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: %s: [%d] missing conditions: %s !",
                                mName.c_str(), __func__, frame_id, missing_conds.c_str());
            LOGW_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:%s: [%d] params forced to ready",
                            mName.c_str(), __func__, frame_id);
            mReadyNums++;
            if (frame_id > mLatestReadyFrmId)
                mLatestReadyFrmId = frame_id;
            mParamsMap[frame_id].flags = mReadyMask;
            mParamsMap[frame_id].ready = true;
        } else
            LOGW_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:%s: [%d] params is already ready",
                            mName.c_str(), __func__, frame_id);
    } else {
        LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: %s: [%d] params does not exist, the next is %d",
                        mName.c_str(), __func__, frame_id,
                        mParamsMap.empty() ? -1 :  (mParamsMap.begin())->first);
    }
}

bool
IspParamsAssembler::ready()
{
    LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: enter \n",
                    __FUNCTION__, __LINE__, mName.c_str());
    SmartLock locker (mParamsMutex);
    LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: ready params num %d", mName.c_str(), mReadyNums);
    return mReadyNums > 0 ? true : false;
}

XCamReturn
IspParamsAssembler::deQueOne(cam3aResultList& results, uint32_t& frame_id)
{
    LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: enter \n",
                    __FUNCTION__, __LINE__, mName.c_str());

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    SmartLock locker (mParamsMutex);
    if (mReadyNums > 0) {
        // get next params id, the first one in map
        std::map<int, params_t>::iterator it = mParamsMap.begin();

        if (it == mParamsMap.end()) {
            LOGI_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: mParamsMap is empty !", mName.c_str());
            return XCAM_RETURN_ERROR_PARAM;
        } else {
            LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: deque frame %d params, ready %d",
                            mName.c_str(), it->first, it->second.ready);
            results = it->second.params;
            frame_id = it->first;
            mParamsMap.erase(it);
            mReadyNums--;
        }
    } else {
        LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s: no ready params", mName.c_str());
        return XCAM_RETURN_ERROR_PARAM;
    }
    LOG1_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: exit \n",
                    __FUNCTION__, __LINE__, mName.c_str());

    return ret;
}

void
IspParamsAssembler::reset()
{
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: enter \n",
                    __FUNCTION__, __LINE__, mName.c_str());
    SmartLock locker (mParamsMutex);
    reset_locked();
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: exit \n",
                    __FUNCTION__, __LINE__, mName.c_str());
}

void
IspParamsAssembler::reset_locked()
{
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: enter \n",
                    __FUNCTION__, __LINE__, mName.c_str());
    mParamsMap.clear();
    mLatestReadyFrmId = -1;
    mReadyMask = 0;
    mReadyNums = 0;
    mCondNum = 0;
    mCondMaskMap.clear();
    mInitParamsList.clear();
    started = false;
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) %s: exit \n",
                    __FUNCTION__, __LINE__, mName.c_str());
}

XCamReturn
IspParamsAssembler::start()
{
    SmartLock locker (mParamsMutex);
    if (started)
        return XCAM_RETURN_NO_ERROR;

    started = true;

    for (auto result : mInitParamsList)
        queue_locked(result);

    mInitParamsList.clear();

    return XCAM_RETURN_NO_ERROR;
}

void
IspParamsAssembler::stop() {
    SmartLock locker (mParamsMutex);
    if (!started)
        return;
    started = false;
    reset_locked();
}

template<class T>
void
Isp20Params::convertAiqAeToIsp20Params
(
    T& isp_cfg,
    const rk_aiq_isp_aec_meas_t& aec_meas
)
{
    /* ae update */
    if(/*aec_meas.ae_meas_en*/1) {
        isp_cfg.module_ens |= 1LL << RK_ISP2X_RAWAE_LITE_ID;
        isp_cfg.module_ens |= 1LL << RK_ISP2X_RAWAE_BIG1_ID;
        isp_cfg.module_ens |= 1LL << RK_ISP2X_RAWAE_BIG2_ID;
        isp_cfg.module_ens |= 1LL << RK_ISP2X_RAWAE_BIG3_ID;
        isp_cfg.module_ens |= 1LL << RK_ISP2X_YUVAE_ID;
        if(/*aec_meas.ae_meas_update*/1) {
            isp_cfg.module_en_update |= 1LL << RK_ISP2X_RAWAE_LITE_ID;
            isp_cfg.module_cfg_update |= 1LL << RK_ISP2X_RAWAE_LITE_ID;

            isp_cfg.module_en_update |= 1LL << RK_ISP2X_RAWAE_BIG1_ID;
            isp_cfg.module_cfg_update |= 1LL << RK_ISP2X_RAWAE_BIG1_ID;

            isp_cfg.module_en_update |= 1LL << RK_ISP2X_RAWAE_BIG2_ID;
            isp_cfg.module_cfg_update |= 1LL << RK_ISP2X_RAWAE_BIG2_ID;

            isp_cfg.module_en_update |= 1LL << RK_ISP2X_RAWAE_BIG3_ID;
            isp_cfg.module_cfg_update |= 1LL << RK_ISP2X_RAWAE_BIG3_ID;

            isp_cfg.module_en_update |= 1LL << RK_ISP2X_YUVAE_ID;
            isp_cfg.module_cfg_update |= 1LL << RK_ISP2X_YUVAE_ID;
        } else {
            return;
        }
    } else {
        return;
    }

    memcpy(&isp_cfg.meas.rawae3, &aec_meas.rawae3, sizeof(aec_meas.rawae3));
    memcpy(&isp_cfg.meas.rawae1, &aec_meas.rawae1, sizeof(aec_meas.rawae1));
    memcpy(&isp_cfg.meas.rawae2, &aec_meas.rawae2, sizeof(aec_meas.rawae2));
    memcpy(&isp_cfg.meas.rawae0, &aec_meas.rawae0, sizeof(aec_meas.rawae0));
    memcpy(&isp_cfg.meas.yuvae, &aec_meas.yuvae, sizeof(aec_meas.yuvae));

    /*
     *     LOGD_CAMHW_SUBM(ISP20PARAM_SUBM,"xuhf-debug: hist_meas-isp_cfg size: [%dx%d]-[%dx%d]-[%dx%d]-[%dx%d]\n",
     *                     sizeof(aec_meas.rawae3),
     *                     sizeof(isp_cfg.meas.rawae3),
     *                     sizeof(aec_meas.rawae1),
     *                     sizeof(isp_cfg.meas.rawae1),
     *                     sizeof(aec_meas.rawae2),
     *                     sizeof(isp_cfg.meas.rawae2),
     *                     sizeof(aec_meas.rawae0),
     *                     sizeof(isp_cfg.meas.rawae0));
     *
     *     LOGD_CAMHW_SUBM(ISP20PARAM_SUBM,"xuhf-debug: aec_meas: win size: [%dx%d]-[%dx%d]-[%dx%d]-[%dx%d]\n",
     *            aec_meas.rawae0.win.h_size,
     *            aec_meas.rawae0.win.v_size,
     *            aec_meas.rawae1.win.h_size,
     *            aec_meas.rawae1.win.v_size,
     *            aec_meas.rawae2.win.h_size,
     *            aec_meas.rawae2.win.v_size,
     *            aec_meas.rawae3.win.h_size,
     *            aec_meas.rawae3.win.v_size);
     *
     *     LOGD_CAMHW_SUBM(ISP20PARAM_SUBM,"xuhf-debug: isp_cfg: win size: [%dx%d]-[%dx%d]-[%dx%d]-[%dx%d]\n",
     *            isp_cfg.meas.rawae0.win.h_size,
     *            isp_cfg.meas.rawae0.win.v_size,
     *            isp_cfg.meas.rawae1.win.h_size,
     *            isp_cfg.meas.rawae1.win.v_size,
     *            isp_cfg.meas.rawae2.win.h_size,
     *            isp_cfg.meas.rawae2.win.v_size,
     *            isp_cfg.meas.rawae3.win.h_size,
     *            isp_cfg.meas.rawae3.win.v_size);
     */
}

template<class T>
void
Isp20Params::convertAiqHistToIsp20Params
(
    T& isp_cfg,
    const rk_aiq_isp_hist_meas_t& hist_meas
)
{
    /* hist update */
    if(/*hist_meas.hist_meas_en*/1) {
        isp_cfg.module_ens |= 1LL << RK_ISP2X_RAWHIST_LITE_ID;
        isp_cfg.module_ens |= 1LL << RK_ISP2X_RAWHIST_BIG1_ID;
        isp_cfg.module_ens |= 1LL << RK_ISP2X_RAWHIST_BIG2_ID;
        isp_cfg.module_ens |= 1LL << RK_ISP2X_RAWHIST_BIG3_ID;
        isp_cfg.module_ens |= 1LL << RK_ISP2X_SIHST_ID;

        if(/*hist_meas.hist_meas_update*/1) {
            isp_cfg.module_en_update |= 1LL << RK_ISP2X_RAWHIST_LITE_ID;
            isp_cfg.module_cfg_update |= 1LL << RK_ISP2X_RAWHIST_LITE_ID;

            isp_cfg.module_en_update |= 1LL << RK_ISP2X_RAWHIST_BIG1_ID;
            isp_cfg.module_cfg_update |= 1LL << RK_ISP2X_RAWHIST_BIG1_ID;

            isp_cfg.module_en_update |= 1LL << RK_ISP2X_RAWHIST_BIG2_ID;
            isp_cfg.module_cfg_update |= 1LL << RK_ISP2X_RAWHIST_BIG2_ID;

            isp_cfg.module_en_update |= 1LL << RK_ISP2X_RAWHIST_BIG3_ID;
            isp_cfg.module_cfg_update |= 1LL << RK_ISP2X_RAWHIST_BIG3_ID;

            isp_cfg.module_en_update |= 1LL << RK_ISP2X_SIHST_ID;
            isp_cfg.module_cfg_update |= 1LL << RK_ISP2X_SIHST_ID;

        } else {
            return;
        }
    } else {
        return;
    }

    memcpy(&isp_cfg.meas.rawhist3, &hist_meas.rawhist3, sizeof(hist_meas.rawhist3));
    memcpy(&isp_cfg.meas.rawhist1, &hist_meas.rawhist1, sizeof(hist_meas.rawhist1));
    memcpy(&isp_cfg.meas.rawhist2, &hist_meas.rawhist2, sizeof(hist_meas.rawhist2));
    memcpy(&isp_cfg.meas.rawhist0, &hist_meas.rawhist0, sizeof(hist_meas.rawhist0));
    memcpy(&isp_cfg.meas.sihst, &hist_meas.sihist, sizeof(hist_meas.sihist));

    /*
     *     LOGD_CAMHW_SUBM(ISP20PARAM_SUBM,"xuhf-debug: hist_meas-isp_cfg size: [%dx%d]-[%dx%d]-[%dx%d]-[%dx%d]\n",
     *                     sizeof(hist_meas.rawhist3),
     *                     sizeof(isp_cfg.meas.rawhist3),
     *                     sizeof(hist_meas.rawhist1),
     *                     sizeof(isp_cfg.meas.rawhist1),
     *                     sizeof(hist_meas.rawhist2),
     *                     sizeof(isp_cfg.meas.rawhist2),
     *                     sizeof(hist_meas.rawhist0),
     *                     sizeof(isp_cfg.meas.rawhist0));
     *
     *     LOGD_CAMHW_SUBM(ISP20PARAM_SUBM,"xuhf-debug: hist_meas: hist win size: [%dx%d]-[%dx%d]-[%dx%d]-[%dx%d]\n",
     *                     hist_meas.rawhist0.win.h_size,
     *                     hist_meas.rawhist0.win.v_size,
     *                     hist_meas.rawhist1.win.h_size,
     *                     hist_meas.rawhist1.win.v_size,
     *                     hist_meas.rawhist2.win.h_size,
     *                     hist_meas.rawhist2.win.v_size,
     *                     hist_meas.rawhist3.win.h_size,
     *                     hist_meas.rawhist3.win.v_size);
     *
     *     LOGD_CAMHW_SUBM(ISP20PARAM_SUBM,"xuhf-debug: isp_cfg: hist win size: [%dx%d]-[%dx%d]-[%dx%d]-[%dx%d]\n",
     *                     isp_cfg.meas.rawhist0.win.h_size,
     *                     isp_cfg.meas.rawhist0.win.v_size,
     *                     isp_cfg.meas.rawhist1.win.h_size,
     *                     isp_cfg.meas.rawhist1.win.v_size,
     *                     isp_cfg.meas.rawhist2.win.h_size,
     *                     isp_cfg.meas.rawhist2.win.v_size,
     *                     isp_cfg.meas.rawhist3.win.h_size,
     *                     isp_cfg.meas.rawhist3.win.v_size);
     */
}

template<class T>
void
Isp20Params::convertAiqAwbToIsp20Params(T& isp_cfg,
                                        const rk_aiq_awb_stat_cfg_v200_t& awb_meas, bool awb_cfg_udpate)
{
    if(awb_cfg_udpate) {
        if(awb_meas.awbEnable) {
            isp_cfg.module_ens |= ISP2X_MODULE_RAWAWB;
            isp_cfg.module_cfg_update |= ISP2X_MODULE_RAWAWB;
            isp_cfg.module_en_update |= ISP2X_MODULE_RAWAWB;
        }
    } else {
        return;
    }

    struct isp2x_rawawb_meas_cfg * awb_cfg_v200 = &isp_cfg.meas.rawawb;
    awb_cfg_v200->rawawb_sel                        =    awb_meas.frameChoose;
    awb_cfg_v200->sw_rawawb_xy_en                   =    awb_meas.xyDetectionEnable;
    awb_cfg_v200->sw_rawawb_uv_en                   =    awb_meas.uvDetectionEnable;
    awb_cfg_v200->sw_rawlsc_bypass_en               =    awb_meas.lscBypEnable;
    awb_cfg_v200->sw_rawawb_3dyuv_ls_idx0           =     awb_meas.threeDyuvIllu[0];
    awb_cfg_v200->sw_rawawb_3dyuv_ls_idx1           =     awb_meas.threeDyuvIllu[1];
    awb_cfg_v200->sw_rawawb_3dyuv_ls_idx2           =     awb_meas.threeDyuvIllu[2];
    awb_cfg_v200->sw_rawawb_3dyuv_ls_idx3           =     awb_meas.threeDyuvIllu[3];
    awb_cfg_v200->sw_rawawb_blk_measure_mode        =     awb_meas.blkMeasureMode;
    awb_cfg_v200->sw_rawawb_store_wp_th0            =     awb_meas.blkMeasWpTh[0];
    awb_cfg_v200->sw_rawawb_store_wp_th1            =     awb_meas.blkMeasWpTh[1];
    awb_cfg_v200->sw_rawawb_store_wp_th2            =     awb_meas.blkMeasWpTh[2];

    awb_cfg_v200->sw_rawawb_light_num               =    awb_meas.lightNum;
    awb_cfg_v200->sw_rawawb_h_offs                  =    awb_meas.windowSet[0];
    awb_cfg_v200->sw_rawawb_v_offs                  =    awb_meas.windowSet[1];
    awb_cfg_v200->sw_rawawb_h_size                  =    awb_meas.windowSet[2];
    awb_cfg_v200->sw_rawawb_v_size                  =    awb_meas.windowSet[3];
    switch(awb_meas.dsMode) {
    case RK_AIQ_AWB_DS_4X4:
        awb_cfg_v200->sw_rawawb_wind_size = 0;
        break;
    default:
        awb_cfg_v200->sw_rawawb_wind_size = 1;
    }
    awb_cfg_v200->sw_rawawb_r_max                   =    awb_meas.maxR;
    awb_cfg_v200->sw_rawawb_g_max                   =    awb_meas.maxG;
    awb_cfg_v200->sw_rawawb_b_max                   =    awb_meas.maxB;
    awb_cfg_v200->sw_rawawb_y_max                   =    awb_meas.maxY;
    awb_cfg_v200->sw_rawawb_r_min                   =    awb_meas.minR;
    awb_cfg_v200->sw_rawawb_g_min                   =    awb_meas.minG;
    awb_cfg_v200->sw_rawawb_b_min                   =    awb_meas.minB;
    awb_cfg_v200->sw_rawawb_y_min                   =    awb_meas.minY;
    awb_cfg_v200->sw_rawawb_c_range                 =     awb_meas.rgb2yuv_c_range;
    awb_cfg_v200->sw_rawawb_y_range                 =    awb_meas.rgb2yuv_y_range;
    awb_cfg_v200->sw_rawawb_coeff_y_r               =    awb_meas.rgb2yuv_matrix[0];
    awb_cfg_v200->sw_rawawb_coeff_y_g               =    awb_meas.rgb2yuv_matrix[1];
    awb_cfg_v200->sw_rawawb_coeff_y_b               =    awb_meas.rgb2yuv_matrix[2];
    awb_cfg_v200->sw_rawawb_coeff_u_r               =    awb_meas.rgb2yuv_matrix[3];
    awb_cfg_v200->sw_rawawb_coeff_u_g               =    awb_meas.rgb2yuv_matrix[4];
    awb_cfg_v200->sw_rawawb_coeff_u_b               =    awb_meas.rgb2yuv_matrix[5];
    awb_cfg_v200->sw_rawawb_coeff_v_r               =    awb_meas.rgb2yuv_matrix[6];
    awb_cfg_v200->sw_rawawb_coeff_v_g               =    awb_meas.rgb2yuv_matrix[7];
    awb_cfg_v200->sw_rawawb_coeff_v_b               =    awb_meas.rgb2yuv_matrix[8];
    //uv
    awb_cfg_v200->sw_rawawb_vertex0_u_0             =    awb_meas.uvRange_param[0].pu_region[0];
    awb_cfg_v200->sw_rawawb_vertex0_v_0             =    awb_meas.uvRange_param[0].pv_region[0];
    awb_cfg_v200->sw_rawawb_vertex1_u_0             =    awb_meas.uvRange_param[0].pu_region[1];
    awb_cfg_v200->sw_rawawb_vertex1_v_0             =    awb_meas.uvRange_param[0].pv_region[1];
    awb_cfg_v200->sw_rawawb_vertex2_u_0             =    awb_meas.uvRange_param[0].pu_region[2];
    awb_cfg_v200->sw_rawawb_vertex2_v_0             =    awb_meas.uvRange_param[0].pv_region[2];
    awb_cfg_v200->sw_rawawb_vertex3_u_0             =    awb_meas.uvRange_param[0].pu_region[3];
    awb_cfg_v200->sw_rawawb_vertex3_v_0             =    awb_meas.uvRange_param[0].pv_region[3];
    awb_cfg_v200->sw_rawawb_islope01_0              =    awb_meas.uvRange_param[0].slope_inv[0];
    awb_cfg_v200->sw_rawawb_islope12_0              =    awb_meas.uvRange_param[0].slope_inv[1];
    awb_cfg_v200->sw_rawawb_islope23_0              =    awb_meas.uvRange_param[0].slope_inv[2];
    awb_cfg_v200->sw_rawawb_islope30_0              =    awb_meas.uvRange_param[0].slope_inv[3];
    awb_cfg_v200->sw_rawawb_vertex0_u_1             =    awb_meas.uvRange_param[1].pu_region[0];
    awb_cfg_v200->sw_rawawb_vertex0_v_1             =    awb_meas.uvRange_param[1].pv_region[0];
    awb_cfg_v200->sw_rawawb_vertex1_u_1             =    awb_meas.uvRange_param[1].pu_region[1];
    awb_cfg_v200->sw_rawawb_vertex1_v_1             =    awb_meas.uvRange_param[1].pv_region[1];
    awb_cfg_v200->sw_rawawb_vertex2_u_1             =    awb_meas.uvRange_param[1].pu_region[2];
    awb_cfg_v200->sw_rawawb_vertex2_v_1             =    awb_meas.uvRange_param[1].pv_region[2];
    awb_cfg_v200->sw_rawawb_vertex3_u_1             =    awb_meas.uvRange_param[1].pu_region[3];
    awb_cfg_v200->sw_rawawb_vertex3_v_1             =    awb_meas.uvRange_param[1].pv_region[3];
    awb_cfg_v200->sw_rawawb_islope01_1              =    awb_meas.uvRange_param[1].slope_inv[0];
    awb_cfg_v200->sw_rawawb_islope12_1              =    awb_meas.uvRange_param[1].slope_inv[1];
    awb_cfg_v200->sw_rawawb_islope23_1              =    awb_meas.uvRange_param[1].slope_inv[2];
    awb_cfg_v200->sw_rawawb_islope30_1              =    awb_meas.uvRange_param[1].slope_inv[3];
    awb_cfg_v200->sw_rawawb_vertex0_u_2             =    awb_meas.uvRange_param[2].pu_region[0];
    awb_cfg_v200->sw_rawawb_vertex0_v_2             =    awb_meas.uvRange_param[2].pv_region[0];
    awb_cfg_v200->sw_rawawb_vertex1_u_2             =    awb_meas.uvRange_param[2].pu_region[1];
    awb_cfg_v200->sw_rawawb_vertex1_v_2             =    awb_meas.uvRange_param[2].pv_region[1];
    awb_cfg_v200->sw_rawawb_vertex2_u_2             =    awb_meas.uvRange_param[2].pu_region[2];
    awb_cfg_v200->sw_rawawb_vertex2_v_2             =    awb_meas.uvRange_param[2].pv_region[2];
    awb_cfg_v200->sw_rawawb_vertex3_u_2             =    awb_meas.uvRange_param[2].pu_region[3];
    awb_cfg_v200->sw_rawawb_vertex3_v_2             =    awb_meas.uvRange_param[2].pv_region[3];
    awb_cfg_v200->sw_rawawb_islope01_2              =    awb_meas.uvRange_param[2].slope_inv[0];
    awb_cfg_v200->sw_rawawb_islope12_2              =    awb_meas.uvRange_param[2].slope_inv[1];
    awb_cfg_v200->sw_rawawb_islope23_2              =    awb_meas.uvRange_param[2].slope_inv[2];
    awb_cfg_v200->sw_rawawb_islope30_2              =    awb_meas.uvRange_param[2].slope_inv[3];
    awb_cfg_v200->sw_rawawb_vertex0_u_3             =    awb_meas.uvRange_param[3].pu_region[0];
    awb_cfg_v200->sw_rawawb_vertex0_v_3             =    awb_meas.uvRange_param[3].pv_region[0];
    awb_cfg_v200->sw_rawawb_vertex1_u_3             =    awb_meas.uvRange_param[3].pu_region[1];
    awb_cfg_v200->sw_rawawb_vertex1_v_3             =    awb_meas.uvRange_param[3].pv_region[1];
    awb_cfg_v200->sw_rawawb_vertex2_u_3             =    awb_meas.uvRange_param[3].pu_region[2];
    awb_cfg_v200->sw_rawawb_vertex2_v_3             =    awb_meas.uvRange_param[3].pv_region[2];
    awb_cfg_v200->sw_rawawb_vertex3_u_3             =    awb_meas.uvRange_param[3].pu_region[3];
    awb_cfg_v200->sw_rawawb_vertex3_v_3             =    awb_meas.uvRange_param[3].pv_region[3];
    awb_cfg_v200->sw_rawawb_islope01_3              =    awb_meas.uvRange_param[3].slope_inv[0];
    awb_cfg_v200->sw_rawawb_islope12_3              =    awb_meas.uvRange_param[3].slope_inv[1];
    awb_cfg_v200->sw_rawawb_islope23_3              =    awb_meas.uvRange_param[3].slope_inv[2];
    awb_cfg_v200->sw_rawawb_islope30_3              =    awb_meas.uvRange_param[3].slope_inv[3];
    awb_cfg_v200->sw_rawawb_vertex0_u_4             =    awb_meas.uvRange_param[4].pu_region[0];
    awb_cfg_v200->sw_rawawb_vertex0_v_4             =    awb_meas.uvRange_param[4].pv_region[0];
    awb_cfg_v200->sw_rawawb_vertex1_u_4             =    awb_meas.uvRange_param[4].pu_region[1];
    awb_cfg_v200->sw_rawawb_vertex1_v_4             =    awb_meas.uvRange_param[4].pv_region[1];
    awb_cfg_v200->sw_rawawb_vertex2_u_4             =    awb_meas.uvRange_param[4].pu_region[2];
    awb_cfg_v200->sw_rawawb_vertex2_v_4             =    awb_meas.uvRange_param[4].pv_region[2];
    awb_cfg_v200->sw_rawawb_vertex3_u_4             =    awb_meas.uvRange_param[4].pu_region[3];
    awb_cfg_v200->sw_rawawb_vertex3_v_4             =    awb_meas.uvRange_param[4].pv_region[3];
    awb_cfg_v200->sw_rawawb_islope01_4              =    awb_meas.uvRange_param[4].slope_inv[0];
    awb_cfg_v200->sw_rawawb_islope12_4              =    awb_meas.uvRange_param[4].slope_inv[1];
    awb_cfg_v200->sw_rawawb_islope23_4              =    awb_meas.uvRange_param[4].slope_inv[2];
    awb_cfg_v200->sw_rawawb_islope30_4              =    awb_meas.uvRange_param[4].slope_inv[3];
    awb_cfg_v200->sw_rawawb_vertex0_u_5             =    awb_meas.uvRange_param[5].pu_region[0];
    awb_cfg_v200->sw_rawawb_vertex0_v_5             =    awb_meas.uvRange_param[5].pv_region[0];
    awb_cfg_v200->sw_rawawb_vertex1_u_5             =    awb_meas.uvRange_param[5].pu_region[1];
    awb_cfg_v200->sw_rawawb_vertex1_v_5             =    awb_meas.uvRange_param[5].pv_region[1];
    awb_cfg_v200->sw_rawawb_vertex2_u_5             =    awb_meas.uvRange_param[5].pu_region[2];
    awb_cfg_v200->sw_rawawb_vertex2_v_5             =    awb_meas.uvRange_param[5].pv_region[2];
    awb_cfg_v200->sw_rawawb_vertex3_u_5             =    awb_meas.uvRange_param[5].pu_region[3];
    awb_cfg_v200->sw_rawawb_vertex3_v_5             =    awb_meas.uvRange_param[5].pv_region[3];
    awb_cfg_v200->sw_rawawb_islope01_5              =    awb_meas.uvRange_param[5].slope_inv[0];
    awb_cfg_v200->sw_rawawb_islope12_5              =    awb_meas.uvRange_param[5].slope_inv[1];
    awb_cfg_v200->sw_rawawb_islope23_5              =    awb_meas.uvRange_param[5].slope_inv[2];
    awb_cfg_v200->sw_rawawb_islope30_5              =    awb_meas.uvRange_param[5].slope_inv[3];
    awb_cfg_v200->sw_rawawb_vertex0_u_6             =    awb_meas.uvRange_param[6].pu_region[0];
    awb_cfg_v200->sw_rawawb_vertex0_v_6             =    awb_meas.uvRange_param[6].pv_region[0];
    awb_cfg_v200->sw_rawawb_vertex1_u_6             =    awb_meas.uvRange_param[6].pu_region[1];
    awb_cfg_v200->sw_rawawb_vertex1_v_6             =    awb_meas.uvRange_param[6].pv_region[1];
    awb_cfg_v200->sw_rawawb_vertex2_u_6             =    awb_meas.uvRange_param[6].pu_region[2];
    awb_cfg_v200->sw_rawawb_vertex2_v_6             =    awb_meas.uvRange_param[6].pv_region[2];
    awb_cfg_v200->sw_rawawb_vertex3_u_6             =    awb_meas.uvRange_param[6].pu_region[3];
    awb_cfg_v200->sw_rawawb_vertex3_v_6             =    awb_meas.uvRange_param[6].pv_region[3];
    awb_cfg_v200->sw_rawawb_islope01_6              =    awb_meas.uvRange_param[6].slope_inv[0];
    awb_cfg_v200->sw_rawawb_islope12_6              =    awb_meas.uvRange_param[6].slope_inv[1];
    awb_cfg_v200->sw_rawawb_islope23_6              =    awb_meas.uvRange_param[6].slope_inv[2];
    awb_cfg_v200->sw_rawawb_islope30_6              =    awb_meas.uvRange_param[6].slope_inv[3];
    //yuv
    awb_cfg_v200->sw_rawawb_b_uv_0                  =    awb_meas.yuvRange_param[0].b_uv;
    awb_cfg_v200->sw_rawawb_slope_ydis_0            =    awb_meas.yuvRange_param[0].slope_ydis;
    awb_cfg_v200->sw_rawawb_b_ydis_0                =    awb_meas.yuvRange_param[0].b_ydis;
    awb_cfg_v200->sw_rawawb_slope_vtcuv_0           =    awb_meas.yuvRange_param[0].slope_inv_neg_uv;
    awb_cfg_v200->sw_rawawb_inv_dslope_0            =    awb_meas.yuvRange_param[0].slope_factor_uv;
    awb_cfg_v200->sw_rawawb_b_uv_1                  =    awb_meas.yuvRange_param[1].b_uv;
    awb_cfg_v200->sw_rawawb_slope_ydis_1            =    awb_meas.yuvRange_param[1].slope_ydis;
    awb_cfg_v200->sw_rawawb_b_ydis_1                =    awb_meas.yuvRange_param[1].b_ydis;
    awb_cfg_v200->sw_rawawb_slope_vtcuv_1           =    awb_meas.yuvRange_param[1].slope_inv_neg_uv;
    awb_cfg_v200->sw_rawawb_inv_dslope_1            =    awb_meas.yuvRange_param[1].slope_factor_uv;
    awb_cfg_v200->sw_rawawb_b_uv_2                  =    awb_meas.yuvRange_param[2].b_uv;
    awb_cfg_v200->sw_rawawb_slope_ydis_2            =    awb_meas.yuvRange_param[2].slope_ydis;
    awb_cfg_v200->sw_rawawb_b_ydis_2                =    awb_meas.yuvRange_param[2].b_ydis;
    awb_cfg_v200->sw_rawawb_slope_vtcuv_2           =    awb_meas.yuvRange_param[2].slope_inv_neg_uv;
    awb_cfg_v200->sw_rawawb_inv_dslope_2            =    awb_meas.yuvRange_param[2].slope_factor_uv;
    awb_cfg_v200->sw_rawawb_b_uv_3                  =    awb_meas.yuvRange_param[3].b_uv;
    awb_cfg_v200->sw_rawawb_slope_ydis_3            =    awb_meas.yuvRange_param[3].slope_ydis;
    awb_cfg_v200->sw_rawawb_b_ydis_3                =    awb_meas.yuvRange_param[3].b_ydis;
    awb_cfg_v200->sw_rawawb_slope_vtcuv_3           =    awb_meas.yuvRange_param[3].slope_inv_neg_uv;
    awb_cfg_v200->sw_rawawb_inv_dslope_3            =    awb_meas.yuvRange_param[3].slope_factor_uv;
    awb_cfg_v200->sw_rawawb_ref_u                   =    awb_meas.yuvRange_param[0].ref_u;
    awb_cfg_v200->sw_rawawb_ref_v_0                 =    awb_meas.yuvRange_param[0].ref_v;
    awb_cfg_v200->sw_rawawb_ref_v_1                 =    awb_meas.yuvRange_param[1].ref_v;
    awb_cfg_v200->sw_rawawb_ref_v_2                 =    awb_meas.yuvRange_param[2].ref_v;
    awb_cfg_v200->sw_rawawb_ref_v_3                 =    awb_meas.yuvRange_param[3].ref_v;
    awb_cfg_v200->sw_rawawb_dis0_0                  =    awb_meas.yuvRange_param[0].dis[0];
    awb_cfg_v200->sw_rawawb_dis1_0                  =    awb_meas.yuvRange_param[0].dis[1];
    awb_cfg_v200->sw_rawawb_dis2_0                  =    awb_meas.yuvRange_param[0].dis[2];
    awb_cfg_v200->sw_rawawb_dis3_0                  =    awb_meas.yuvRange_param[0].dis[3];
    awb_cfg_v200->sw_rawawb_dis4_0                  =    awb_meas.yuvRange_param[0].dis[4];
    awb_cfg_v200->sw_rawawb_dis5_0                  =    awb_meas.yuvRange_param[0].dis[5];
    awb_cfg_v200->sw_rawawb_th0_0                   =    awb_meas.yuvRange_param[0].th[0];
    awb_cfg_v200->sw_rawawb_th1_0                   =    awb_meas.yuvRange_param[0].th[1];
    awb_cfg_v200->sw_rawawb_th2_0                   =    awb_meas.yuvRange_param[0].th[2];
    awb_cfg_v200->sw_rawawb_th3_0                   =    awb_meas.yuvRange_param[0].th[3];
    awb_cfg_v200->sw_rawawb_th4_0                   =    awb_meas.yuvRange_param[0].th[4];
    awb_cfg_v200->sw_rawawb_th5_0                   =    awb_meas.yuvRange_param[0].th[5];
    awb_cfg_v200->sw_rawawb_dis0_1                  =    awb_meas.yuvRange_param[1].dis[0];
    awb_cfg_v200->sw_rawawb_dis1_1                  =    awb_meas.yuvRange_param[1].dis[1];
    awb_cfg_v200->sw_rawawb_dis2_1                  =    awb_meas.yuvRange_param[1].dis[2];
    awb_cfg_v200->sw_rawawb_dis3_1                  =    awb_meas.yuvRange_param[1].dis[3];
    awb_cfg_v200->sw_rawawb_dis4_1                  =    awb_meas.yuvRange_param[1].dis[4];
    awb_cfg_v200->sw_rawawb_dis5_1                  =    awb_meas.yuvRange_param[1].dis[5];
    awb_cfg_v200->sw_rawawb_th0_1                   =    awb_meas.yuvRange_param[1].th[0];
    awb_cfg_v200->sw_rawawb_th1_1                   =    awb_meas.yuvRange_param[1].th[1];
    awb_cfg_v200->sw_rawawb_th2_1                   =    awb_meas.yuvRange_param[1].th[2];
    awb_cfg_v200->sw_rawawb_th3_1                   =    awb_meas.yuvRange_param[1].th[3];
    awb_cfg_v200->sw_rawawb_th4_1                   =    awb_meas.yuvRange_param[1].th[4];
    awb_cfg_v200->sw_rawawb_th5_1                   =    awb_meas.yuvRange_param[1].th[5];
    awb_cfg_v200->sw_rawawb_dis0_2                  =    awb_meas.yuvRange_param[2].dis[0];
    awb_cfg_v200->sw_rawawb_dis1_2                  =    awb_meas.yuvRange_param[2].dis[1];
    awb_cfg_v200->sw_rawawb_dis2_2                  =    awb_meas.yuvRange_param[2].dis[2];
    awb_cfg_v200->sw_rawawb_dis3_2                  =    awb_meas.yuvRange_param[2].dis[3];
    awb_cfg_v200->sw_rawawb_dis4_2                  =    awb_meas.yuvRange_param[2].dis[4];
    awb_cfg_v200->sw_rawawb_dis5_2                  =    awb_meas.yuvRange_param[2].dis[5];
    awb_cfg_v200->sw_rawawb_th0_2                   =    awb_meas.yuvRange_param[2].th[0];
    awb_cfg_v200->sw_rawawb_th1_2                   =    awb_meas.yuvRange_param[2].th[1];
    awb_cfg_v200->sw_rawawb_th2_2                   =    awb_meas.yuvRange_param[2].th[2];
    awb_cfg_v200->sw_rawawb_th3_2                   =    awb_meas.yuvRange_param[2].th[3];
    awb_cfg_v200->sw_rawawb_th4_2                   =    awb_meas.yuvRange_param[2].th[4];
    awb_cfg_v200->sw_rawawb_th5_2                   =    awb_meas.yuvRange_param[2].th[5];
    awb_cfg_v200->sw_rawawb_dis0_3                  =    awb_meas.yuvRange_param[3].dis[0];
    awb_cfg_v200->sw_rawawb_dis1_3                  =    awb_meas.yuvRange_param[3].dis[1];
    awb_cfg_v200->sw_rawawb_dis2_3                  =    awb_meas.yuvRange_param[3].dis[2];
    awb_cfg_v200->sw_rawawb_dis3_3                  =    awb_meas.yuvRange_param[3].dis[3];
    awb_cfg_v200->sw_rawawb_dis4_3                  =    awb_meas.yuvRange_param[3].dis[4];
    awb_cfg_v200->sw_rawawb_dis5_3                  =    awb_meas.yuvRange_param[3].dis[5];
    awb_cfg_v200->sw_rawawb_th0_3                   =    awb_meas.yuvRange_param[3].th[0];
    awb_cfg_v200->sw_rawawb_th1_3                   =    awb_meas.yuvRange_param[3].th[1];
    awb_cfg_v200->sw_rawawb_th2_3                   =    awb_meas.yuvRange_param[3].th[2];
    awb_cfg_v200->sw_rawawb_th3_3                   =    awb_meas.yuvRange_param[3].th[3];
    awb_cfg_v200->sw_rawawb_th4_3                   =    awb_meas.yuvRange_param[3].th[4];
    awb_cfg_v200->sw_rawawb_th5_3                   =    awb_meas.yuvRange_param[3].th[5];
    //xy
    awb_cfg_v200->sw_rawawb_wt0                     =    awb_meas.rgb2xy_param.pseudoLuminanceWeight[0];
    awb_cfg_v200->sw_rawawb_wt1                     =    awb_meas.rgb2xy_param.pseudoLuminanceWeight[1];
    awb_cfg_v200->sw_rawawb_wt2                     =    awb_meas.rgb2xy_param.pseudoLuminanceWeight[2];
    awb_cfg_v200->sw_rawawb_mat0_x                  =    awb_meas.rgb2xy_param.rotationMat[0];
    awb_cfg_v200->sw_rawawb_mat1_x                  =    awb_meas.rgb2xy_param.rotationMat[1];
    awb_cfg_v200->sw_rawawb_mat2_x                  =    awb_meas.rgb2xy_param.rotationMat[2];
    awb_cfg_v200->sw_rawawb_mat0_y                  =    awb_meas.rgb2xy_param.rotationMat[3];
    awb_cfg_v200->sw_rawawb_mat1_y                  =    awb_meas.rgb2xy_param.rotationMat[4];
    awb_cfg_v200->sw_rawawb_mat2_y                  =    awb_meas.rgb2xy_param.rotationMat[5];
    awb_cfg_v200->sw_rawawb_nor_x0_0                =    awb_meas.xyRange_param[0].NorrangeX[0];
    awb_cfg_v200->sw_rawawb_nor_x1_0                =    awb_meas.xyRange_param[0].NorrangeX[1];
    awb_cfg_v200->sw_rawawb_nor_y0_0                =    awb_meas.xyRange_param[0].NorrangeY[0];
    awb_cfg_v200->sw_rawawb_nor_y1_0                =    awb_meas.xyRange_param[0].NorrangeY[1];
    awb_cfg_v200->sw_rawawb_big_x0_0                =    awb_meas.xyRange_param[0].SperangeX[0];
    awb_cfg_v200->sw_rawawb_big_x1_0                =    awb_meas.xyRange_param[0].SperangeX[1];
    awb_cfg_v200->sw_rawawb_big_y0_0                =    awb_meas.xyRange_param[0].SperangeY[0];
    awb_cfg_v200->sw_rawawb_big_y1_0                =    awb_meas.xyRange_param[0].SperangeY[1];
    awb_cfg_v200->sw_rawawb_sma_x0_0                =    awb_meas.xyRange_param[0].SmalrangeX[0];
    awb_cfg_v200->sw_rawawb_sma_x1_0                =    awb_meas.xyRange_param[0].SmalrangeX[1];
    awb_cfg_v200->sw_rawawb_sma_y0_0                =    awb_meas.xyRange_param[0].SmalrangeY[0];
    awb_cfg_v200->sw_rawawb_sma_y1_0                =    awb_meas.xyRange_param[0].SmalrangeY[1];
    awb_cfg_v200->sw_rawawb_nor_x0_1                =    awb_meas.xyRange_param[1].NorrangeX[0];
    awb_cfg_v200->sw_rawawb_nor_x1_1                =    awb_meas.xyRange_param[1].NorrangeX[1];
    awb_cfg_v200->sw_rawawb_nor_y0_1                =    awb_meas.xyRange_param[1].NorrangeY[0];
    awb_cfg_v200->sw_rawawb_nor_y1_1                =    awb_meas.xyRange_param[1].NorrangeY[1];
    awb_cfg_v200->sw_rawawb_big_x0_1                =    awb_meas.xyRange_param[1].SperangeX[0];
    awb_cfg_v200->sw_rawawb_big_x1_1                =    awb_meas.xyRange_param[1].SperangeX[1];
    awb_cfg_v200->sw_rawawb_big_y0_1                =    awb_meas.xyRange_param[1].SperangeY[0];
    awb_cfg_v200->sw_rawawb_big_y1_1                =    awb_meas.xyRange_param[1].SperangeY[1];
    awb_cfg_v200->sw_rawawb_sma_x0_1                =    awb_meas.xyRange_param[1].SmalrangeX[0];
    awb_cfg_v200->sw_rawawb_sma_x1_1                =    awb_meas.xyRange_param[1].SmalrangeX[1];
    awb_cfg_v200->sw_rawawb_sma_y0_1                =    awb_meas.xyRange_param[1].SmalrangeY[0];
    awb_cfg_v200->sw_rawawb_sma_y1_1                =    awb_meas.xyRange_param[1].SmalrangeY[1];
    awb_cfg_v200->sw_rawawb_nor_x0_2                =    awb_meas.xyRange_param[2].NorrangeX[0];
    awb_cfg_v200->sw_rawawb_nor_x1_2                =    awb_meas.xyRange_param[2].NorrangeX[1];
    awb_cfg_v200->sw_rawawb_nor_y0_2                =    awb_meas.xyRange_param[2].NorrangeY[0];
    awb_cfg_v200->sw_rawawb_nor_y1_2                =    awb_meas.xyRange_param[2].NorrangeY[1];
    awb_cfg_v200->sw_rawawb_big_x0_2                =    awb_meas.xyRange_param[2].SperangeX[0];
    awb_cfg_v200->sw_rawawb_big_x1_2                =    awb_meas.xyRange_param[2].SperangeX[1];
    awb_cfg_v200->sw_rawawb_big_y0_2                =    awb_meas.xyRange_param[2].SperangeY[0];
    awb_cfg_v200->sw_rawawb_big_y1_2                =    awb_meas.xyRange_param[2].SperangeY[1];
    awb_cfg_v200->sw_rawawb_sma_x0_2                =    awb_meas.xyRange_param[2].SmalrangeX[0];
    awb_cfg_v200->sw_rawawb_sma_x1_2                =    awb_meas.xyRange_param[2].SmalrangeX[1];
    awb_cfg_v200->sw_rawawb_sma_y0_2                =    awb_meas.xyRange_param[2].SmalrangeY[0];
    awb_cfg_v200->sw_rawawb_sma_y1_2                =    awb_meas.xyRange_param[2].SmalrangeY[1];
    awb_cfg_v200->sw_rawawb_nor_x0_3                =    awb_meas.xyRange_param[3].NorrangeX[0];
    awb_cfg_v200->sw_rawawb_nor_x1_3                =    awb_meas.xyRange_param[3].NorrangeX[1];
    awb_cfg_v200->sw_rawawb_nor_y0_3                =    awb_meas.xyRange_param[3].NorrangeY[0];
    awb_cfg_v200->sw_rawawb_nor_y1_3                =    awb_meas.xyRange_param[3].NorrangeY[1];
    awb_cfg_v200->sw_rawawb_big_x0_3                =    awb_meas.xyRange_param[3].SperangeX[0];
    awb_cfg_v200->sw_rawawb_big_x1_3                =    awb_meas.xyRange_param[3].SperangeX[1];
    awb_cfg_v200->sw_rawawb_big_y0_3                =    awb_meas.xyRange_param[3].SperangeY[0];
    awb_cfg_v200->sw_rawawb_big_y1_3                =    awb_meas.xyRange_param[3].SperangeY[1];
    awb_cfg_v200->sw_rawawb_sma_x0_3                =    awb_meas.xyRange_param[3].SmalrangeX[0];
    awb_cfg_v200->sw_rawawb_sma_x1_3                =    awb_meas.xyRange_param[3].SmalrangeX[1];
    awb_cfg_v200->sw_rawawb_sma_y0_3                =    awb_meas.xyRange_param[3].SmalrangeY[0];
    awb_cfg_v200->sw_rawawb_sma_y1_3                =    awb_meas.xyRange_param[3].SmalrangeY[1];
    awb_cfg_v200->sw_rawawb_nor_x0_4                =    awb_meas.xyRange_param[4].NorrangeX[0];
    awb_cfg_v200->sw_rawawb_nor_x1_4                =    awb_meas.xyRange_param[4].NorrangeX[1];
    awb_cfg_v200->sw_rawawb_nor_y0_4                =    awb_meas.xyRange_param[4].NorrangeY[0];
    awb_cfg_v200->sw_rawawb_nor_y1_4                =    awb_meas.xyRange_param[4].NorrangeY[1];
    awb_cfg_v200->sw_rawawb_big_x0_4                =    awb_meas.xyRange_param[4].SperangeX[0];
    awb_cfg_v200->sw_rawawb_big_x1_4                =    awb_meas.xyRange_param[4].SperangeX[1];
    awb_cfg_v200->sw_rawawb_big_y0_4                =    awb_meas.xyRange_param[4].SperangeY[0];
    awb_cfg_v200->sw_rawawb_big_y1_4                =    awb_meas.xyRange_param[4].SperangeY[1];
    awb_cfg_v200->sw_rawawb_sma_x0_4                =    awb_meas.xyRange_param[4].SmalrangeX[0];
    awb_cfg_v200->sw_rawawb_sma_x1_4                =    awb_meas.xyRange_param[4].SmalrangeX[1];
    awb_cfg_v200->sw_rawawb_sma_y0_4                =    awb_meas.xyRange_param[4].SmalrangeY[0];
    awb_cfg_v200->sw_rawawb_sma_y1_4                =    awb_meas.xyRange_param[4].SmalrangeY[1];
    awb_cfg_v200->sw_rawawb_nor_x0_5                =    awb_meas.xyRange_param[5].NorrangeX[0];
    awb_cfg_v200->sw_rawawb_nor_x1_5                =    awb_meas.xyRange_param[5].NorrangeX[1];
    awb_cfg_v200->sw_rawawb_nor_y0_5                =    awb_meas.xyRange_param[5].NorrangeY[0];
    awb_cfg_v200->sw_rawawb_nor_y1_5                =    awb_meas.xyRange_param[5].NorrangeY[1];
    awb_cfg_v200->sw_rawawb_big_x0_5                =    awb_meas.xyRange_param[5].SperangeX[0];
    awb_cfg_v200->sw_rawawb_big_x1_5                =    awb_meas.xyRange_param[5].SperangeX[1];
    awb_cfg_v200->sw_rawawb_big_y0_5                =    awb_meas.xyRange_param[5].SperangeY[0];
    awb_cfg_v200->sw_rawawb_big_y1_5                =    awb_meas.xyRange_param[5].SperangeY[1];
    awb_cfg_v200->sw_rawawb_sma_x0_5                =    awb_meas.xyRange_param[5].SmalrangeX[0];
    awb_cfg_v200->sw_rawawb_sma_x1_5                =    awb_meas.xyRange_param[5].SmalrangeX[1];
    awb_cfg_v200->sw_rawawb_sma_y0_5                =    awb_meas.xyRange_param[5].SmalrangeY[0];
    awb_cfg_v200->sw_rawawb_sma_y1_5                =    awb_meas.xyRange_param[5].SmalrangeY[1];
    awb_cfg_v200->sw_rawawb_nor_x0_6                =    awb_meas.xyRange_param[6].NorrangeX[0];
    awb_cfg_v200->sw_rawawb_nor_x1_6                =    awb_meas.xyRange_param[6].NorrangeX[1];
    awb_cfg_v200->sw_rawawb_nor_y0_6                =    awb_meas.xyRange_param[6].NorrangeY[0];
    awb_cfg_v200->sw_rawawb_nor_y1_6                =    awb_meas.xyRange_param[6].NorrangeY[1];
    awb_cfg_v200->sw_rawawb_big_x0_6                =    awb_meas.xyRange_param[6].SperangeX[0];
    awb_cfg_v200->sw_rawawb_big_x1_6                =    awb_meas.xyRange_param[6].SperangeX[1];
    awb_cfg_v200->sw_rawawb_big_y0_6                =    awb_meas.xyRange_param[6].SperangeY[0];
    awb_cfg_v200->sw_rawawb_big_y1_6                =    awb_meas.xyRange_param[6].SperangeY[1];
    awb_cfg_v200->sw_rawawb_sma_x0_6                =    awb_meas.xyRange_param[6].SmalrangeX[0];
    awb_cfg_v200->sw_rawawb_sma_x1_6                =    awb_meas.xyRange_param[6].SmalrangeX[1];
    awb_cfg_v200->sw_rawawb_sma_y0_6                =    awb_meas.xyRange_param[6].SmalrangeY[0];
    awb_cfg_v200->sw_rawawb_sma_y1_6                =    awb_meas.xyRange_param[6].SmalrangeY[1];
    //multiwindow
    awb_cfg_v200->sw_rawawb_multiwindow_en          =      awb_meas.multiwindow_en;
    awb_cfg_v200->sw_rawawb_multiwindow0_h_offs     =      awb_meas.multiwindow[0][0];
    awb_cfg_v200->sw_rawawb_multiwindow0_v_offs     =      awb_meas.multiwindow[0][1];
    awb_cfg_v200->sw_rawawb_multiwindow0_h_size     =      awb_meas.multiwindow[0][2];
    awb_cfg_v200->sw_rawawb_multiwindow0_v_size     =      awb_meas.multiwindow[0][3];
    awb_cfg_v200->sw_rawawb_multiwindow1_h_offs     =      awb_meas.multiwindow[1][0];
    awb_cfg_v200->sw_rawawb_multiwindow1_v_offs     =      awb_meas.multiwindow[1][1];
    awb_cfg_v200->sw_rawawb_multiwindow1_h_size     =      awb_meas.multiwindow[1][2];
    awb_cfg_v200->sw_rawawb_multiwindow1_v_size     =      awb_meas.multiwindow[1][3];
    awb_cfg_v200->sw_rawawb_multiwindow2_h_offs     =      awb_meas.multiwindow[2][0];
    awb_cfg_v200->sw_rawawb_multiwindow2_v_offs     =      awb_meas.multiwindow[2][1];
    awb_cfg_v200->sw_rawawb_multiwindow2_h_size     =      awb_meas.multiwindow[2][2];
    awb_cfg_v200->sw_rawawb_multiwindow2_v_size     =      awb_meas.multiwindow[2][3];
    awb_cfg_v200->sw_rawawb_multiwindow3_h_offs     =      awb_meas.multiwindow[3][0];
    awb_cfg_v200->sw_rawawb_multiwindow3_v_offs     =      awb_meas.multiwindow[3][1];
    awb_cfg_v200->sw_rawawb_multiwindow3_h_size     =      awb_meas.multiwindow[3][2];
    awb_cfg_v200->sw_rawawb_multiwindow3_v_size     =      awb_meas.multiwindow[3][3];
    awb_cfg_v200->sw_rawawb_multiwindow4_h_offs     =      awb_meas.multiwindow[4][0];
    awb_cfg_v200->sw_rawawb_multiwindow4_v_offs     =      awb_meas.multiwindow[4][1];
    awb_cfg_v200->sw_rawawb_multiwindow4_h_size     =      awb_meas.multiwindow[4][2];
    awb_cfg_v200->sw_rawawb_multiwindow4_v_size     =      awb_meas.multiwindow[4][3];
    awb_cfg_v200->sw_rawawb_multiwindow5_h_offs     =      awb_meas.multiwindow[5][0];
    awb_cfg_v200->sw_rawawb_multiwindow5_v_offs     =      awb_meas.multiwindow[5][1];
    awb_cfg_v200->sw_rawawb_multiwindow5_h_size     =      awb_meas.multiwindow[5][2];
    awb_cfg_v200->sw_rawawb_multiwindow5_v_size     =      awb_meas.multiwindow[5][3];
    awb_cfg_v200->sw_rawawb_multiwindow6_h_offs     =      awb_meas.multiwindow[6][0];
    awb_cfg_v200->sw_rawawb_multiwindow6_v_offs     =      awb_meas.multiwindow[6][1];
    awb_cfg_v200->sw_rawawb_multiwindow6_h_size     =      awb_meas.multiwindow[6][2];
    awb_cfg_v200->sw_rawawb_multiwindow6_v_size     =      awb_meas.multiwindow[6][3];
    awb_cfg_v200->sw_rawawb_multiwindow7_h_offs     =      awb_meas.multiwindow[7][0];
    awb_cfg_v200->sw_rawawb_multiwindow7_v_offs     =      awb_meas.multiwindow[7][1];
    awb_cfg_v200->sw_rawawb_multiwindow7_h_size     =      awb_meas.multiwindow[7][2];
    awb_cfg_v200->sw_rawawb_multiwindow7_v_size     =      awb_meas.multiwindow[7][3];
    //exc range

    awb_cfg_v200->sw_rawawb_exc_wp_region0_excen    =     awb_meas.excludeWpRange[0].excludeEnable;
    awb_cfg_v200->sw_rawawb_exc_wp_region0_measen   =     awb_meas.excludeWpRange[0].measureEnable;
    switch(awb_meas.excludeWpRange[0].domain) {
    case RK_AIQ_AWB_EXC_RANGE_DOMAIN_UV:
        awb_cfg_v200->sw_rawawb_exc_wp_region0_domain  =     0;
        break;
    default:
        awb_cfg_v200->sw_rawawb_exc_wp_region0_domain  =     1;
    }
    awb_cfg_v200->sw_rawawb_exc_wp_region0_xu0      =     awb_meas.excludeWpRange[0].xu[0];
    awb_cfg_v200->sw_rawawb_exc_wp_region0_xu1      =     awb_meas.excludeWpRange[0].xu[1];
    awb_cfg_v200->sw_rawawb_exc_wp_region0_yv0      =     awb_meas.excludeWpRange[0].yv[0];
    awb_cfg_v200->sw_rawawb_exc_wp_region0_yv1      =     awb_meas.excludeWpRange[0].yv[1];
    awb_cfg_v200->sw_rawawb_exc_wp_region1_excen    =     awb_meas.excludeWpRange[1].excludeEnable;
    awb_cfg_v200->sw_rawawb_exc_wp_region1_measen   =     awb_meas.excludeWpRange[1].measureEnable;
    awb_cfg_v200->sw_rawawb_exc_wp_region1_domain   =     awb_meas.excludeWpRange[1].domain;
    switch(awb_meas.excludeWpRange[1].domain) {
    case RK_AIQ_AWB_EXC_RANGE_DOMAIN_UV:
        awb_cfg_v200->sw_rawawb_exc_wp_region1_domain  =     0;
        break;
    default:
        awb_cfg_v200->sw_rawawb_exc_wp_region1_domain  =     1;
    }
    awb_cfg_v200->sw_rawawb_exc_wp_region1_xu0      =     awb_meas.excludeWpRange[1].xu[0];
    awb_cfg_v200->sw_rawawb_exc_wp_region1_xu1      =     awb_meas.excludeWpRange[1].xu[1];
    awb_cfg_v200->sw_rawawb_exc_wp_region1_yv0      =     awb_meas.excludeWpRange[1].yv[0];
    awb_cfg_v200->sw_rawawb_exc_wp_region1_yv1      =     awb_meas.excludeWpRange[1].yv[1];
    awb_cfg_v200->sw_rawawb_exc_wp_region2_excen    =     awb_meas.excludeWpRange[2].excludeEnable;
    awb_cfg_v200->sw_rawawb_exc_wp_region2_measen   =     awb_meas.excludeWpRange[2].measureEnable;
    switch(awb_meas.excludeWpRange[2].domain) {
    case RK_AIQ_AWB_EXC_RANGE_DOMAIN_UV:
        awb_cfg_v200->sw_rawawb_exc_wp_region2_domain  =     0;
        break;
    default:
        awb_cfg_v200->sw_rawawb_exc_wp_region2_domain  =     1;
    }
    awb_cfg_v200->sw_rawawb_exc_wp_region2_xu0      =     awb_meas.excludeWpRange[2].xu[0];
    awb_cfg_v200->sw_rawawb_exc_wp_region2_xu1      =     awb_meas.excludeWpRange[2].xu[1];
    awb_cfg_v200->sw_rawawb_exc_wp_region2_yv0      =     awb_meas.excludeWpRange[2].yv[0];
    awb_cfg_v200->sw_rawawb_exc_wp_region2_yv1      =     awb_meas.excludeWpRange[2].yv[1];
    awb_cfg_v200->sw_rawawb_exc_wp_region3_excen    =     awb_meas.excludeWpRange[3].excludeEnable;
    awb_cfg_v200->sw_rawawb_exc_wp_region3_measen   =     awb_meas.excludeWpRange[3].measureEnable;
    awb_cfg_v200->sw_rawawb_exc_wp_region3_domain   =     awb_meas.excludeWpRange[3].domain;
    switch(awb_meas.excludeWpRange[3].domain) {
    case RK_AIQ_AWB_EXC_RANGE_DOMAIN_UV:
        awb_cfg_v200->sw_rawawb_exc_wp_region3_domain  =     0;
        break;
    default:
        awb_cfg_v200->sw_rawawb_exc_wp_region3_domain  =     1;
    }
    awb_cfg_v200->sw_rawawb_exc_wp_region3_xu0      =     awb_meas.excludeWpRange[3].xu[0];
    awb_cfg_v200->sw_rawawb_exc_wp_region3_xu1      =     awb_meas.excludeWpRange[3].xu[1];
    awb_cfg_v200->sw_rawawb_exc_wp_region3_yv0      =     awb_meas.excludeWpRange[3].yv[0];
    awb_cfg_v200->sw_rawawb_exc_wp_region3_yv1      =     awb_meas.excludeWpRange[3].yv[1];
    awb_cfg_v200->sw_rawawb_exc_wp_region4_excen    =     awb_meas.excludeWpRange[4].excludeEnable;
    awb_cfg_v200->sw_rawawb_exc_wp_region4_measen   =     awb_meas.excludeWpRange[4].measureEnable;
    switch(awb_meas.excludeWpRange[4].domain) {
    case RK_AIQ_AWB_EXC_RANGE_DOMAIN_UV:
        awb_cfg_v200->sw_rawawb_exc_wp_region4_domain  =     0;
        break;
    default:
        awb_cfg_v200->sw_rawawb_exc_wp_region4_domain  =     1;
    }
    awb_cfg_v200->sw_rawawb_exc_wp_region4_xu0      =     awb_meas.excludeWpRange[4].xu[0];
    awb_cfg_v200->sw_rawawb_exc_wp_region4_xu1      =     awb_meas.excludeWpRange[4].xu[1];
    awb_cfg_v200->sw_rawawb_exc_wp_region4_yv0      =     awb_meas.excludeWpRange[4].yv[0];
    awb_cfg_v200->sw_rawawb_exc_wp_region4_yv1      =     awb_meas.excludeWpRange[4].yv[1];
    awb_cfg_v200->sw_rawawb_exc_wp_region5_excen    =     awb_meas.excludeWpRange[5].excludeEnable;
    awb_cfg_v200->sw_rawawb_exc_wp_region5_measen   =     awb_meas.excludeWpRange[5].measureEnable;
    switch(awb_meas.excludeWpRange[5].domain) {
    case RK_AIQ_AWB_EXC_RANGE_DOMAIN_UV:
        awb_cfg_v200->sw_rawawb_exc_wp_region5_domain  =     0;
        break;
    default:
        awb_cfg_v200->sw_rawawb_exc_wp_region5_domain  =     1;
    }
    awb_cfg_v200->sw_rawawb_exc_wp_region5_xu0      =     awb_meas.excludeWpRange[5].xu[0];
    awb_cfg_v200->sw_rawawb_exc_wp_region5_xu1      =     awb_meas.excludeWpRange[5].xu[1];
    awb_cfg_v200->sw_rawawb_exc_wp_region5_yv0      =     awb_meas.excludeWpRange[5].yv[0];
    awb_cfg_v200->sw_rawawb_exc_wp_region5_yv1      =     awb_meas.excludeWpRange[5].yv[1];
    awb_cfg_v200->sw_rawawb_exc_wp_region6_excen    =     awb_meas.excludeWpRange[6].excludeEnable;
    awb_cfg_v200->sw_rawawb_exc_wp_region6_measen   =     awb_meas.excludeWpRange[6].measureEnable;
    switch(awb_meas.excludeWpRange[6].domain) {
    case RK_AIQ_AWB_EXC_RANGE_DOMAIN_UV:
        awb_cfg_v200->sw_rawawb_exc_wp_region6_domain  =     0;
        break;
    default:
        awb_cfg_v200->sw_rawawb_exc_wp_region6_domain  =     1;
    }
    awb_cfg_v200->sw_rawawb_exc_wp_region6_xu0      =     awb_meas.excludeWpRange[6].xu[0];
    awb_cfg_v200->sw_rawawb_exc_wp_region6_xu1      =     awb_meas.excludeWpRange[6].xu[1];
    awb_cfg_v200->sw_rawawb_exc_wp_region6_yv0      =     awb_meas.excludeWpRange[6].yv[0];
    awb_cfg_v200->sw_rawawb_exc_wp_region6_yv1      =     awb_meas.excludeWpRange[6].yv[1];

    awb_cfg_v200->sw_rawawb_store_wp_flag_ls_idx0   =     awb_meas.storeWpFlagIllu[0];
    awb_cfg_v200->sw_rawawb_store_wp_flag_ls_idx1   =     awb_meas.storeWpFlagIllu[1];
    awb_cfg_v200->sw_rawawb_store_wp_flag_ls_idx2   =     awb_meas.storeWpFlagIllu[2];
}

template<class T>
void Isp20Params::convertAiqMergeToIsp20Params(T& isp_cfg,
        const rk_aiq_isp_merge_t& amerge_data)
{
    // TODO: could be always on ? isp driver would do the right thing
    if(amerge_data.Res.sw_hdrmge_mode)
    {
        isp_cfg.module_en_update |= 1LL << RK_ISP2X_HDRMGE_ID;
        isp_cfg.module_ens |= 1LL << RK_ISP2X_HDRMGE_ID;
        isp_cfg.module_cfg_update |= 1LL << RK_ISP2X_HDRMGE_ID;
    }
    else
    {
        isp_cfg.module_en_update |= 1LL << RK_ISP2X_HDRMGE_ID;
        isp_cfg.module_ens &= ~(1LL << RK_ISP2X_HDRMGE_ID);
        isp_cfg.module_cfg_update &= ~(1LL << RK_ISP2X_HDRMGE_ID);
    }

    isp_cfg.others.hdrmge_cfg.mode         = amerge_data.Res.sw_hdrmge_mode;
    isp_cfg.others.hdrmge_cfg.gain0_inv    = amerge_data.Res.sw_hdrmge_gain0_inv;
    isp_cfg.others.hdrmge_cfg.gain0         = amerge_data.Res.sw_hdrmge_gain0;
    isp_cfg.others.hdrmge_cfg.gain1_inv    = amerge_data.Res.sw_hdrmge_gain1_inv;
    isp_cfg.others.hdrmge_cfg.gain1        = amerge_data.Res.sw_hdrmge_gain1;
    isp_cfg.others.hdrmge_cfg.gain2        = amerge_data.Res.sw_hdrmge_gain2;
    isp_cfg.others.hdrmge_cfg.lm_dif_0p15  = amerge_data.Res.sw_hdrmge_lm_dif_0p15;
    isp_cfg.others.hdrmge_cfg.lm_dif_0p9   = amerge_data.Res.sw_hdrmge_lm_dif_0p9;
    isp_cfg.others.hdrmge_cfg.ms_diff_0p15 = amerge_data.Res.sw_hdrmge_ms_dif_0p15;
    isp_cfg.others.hdrmge_cfg.ms_dif_0p8   = amerge_data.Res.sw_hdrmge_ms_dif_0p8;
    for(int i = 0; i < 17; i++)
    {
        isp_cfg.others.hdrmge_cfg.curve.curve_0[i] = amerge_data.Res.sw_hdrmge_l0_y[i];
        isp_cfg.others.hdrmge_cfg.curve.curve_1[i] = amerge_data.Res.sw_hdrmge_l1_y[i];
        isp_cfg.others.hdrmge_cfg.e_y[i]           = amerge_data.Res.sw_hdrmge_e_y[i];
    }

#if 0
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: gain0_inv %d", __LINE__, isp_cfg.others.hdrmge_cfg.gain0_inv);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: gain0 %d", __LINE__, isp_cfg.others.hdrmge_cfg.gain0);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: gain1_inv %d", __LINE__, isp_cfg.others.hdrmge_cfg.gain1_inv);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: gain1 %d", __LINE__, isp_cfg.others.hdrmge_cfg.gain1);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: gain2 %d", __LINE__, isp_cfg.others.hdrmge_cfg.gain2);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: lm_dif_0p15 %d", __LINE__, isp_cfg.others.hdrmge_cfg.lm_dif_0p15);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: lm_dif_0p9 %d", __LINE__, isp_cfg.others.hdrmge_cfg.lm_dif_0p9);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: ms_diff_0p15 %d", __LINE__, isp_cfg.others.hdrmge_cfg.ms_diff_0p15);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: ms_dif_0p8 %d", __LINE__, isp_cfg.others.hdrmge_cfg.ms_dif_0p8);
    for(int i = 0 ; i < 17; i++)
    {
        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: curve_0[%d] %d", __LINE__, i, isp_cfg.others.hdrmge_cfg.curve.curve_0[i]);
        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: curve_1[%d] %d", __LINE__, i, isp_cfg.others.hdrmge_cfg.curve.curve_1[i]);
        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: e_y[%d] %d", __LINE__, i, isp_cfg.others.hdrmge_cfg.e_y[i]);
    }

#endif
}

template<class T>
void Isp20Params::convertAiqTmoToIsp20Params(T& isp_cfg,
        const rk_aiq_isp_tmo_t& atmo_data)
{
    if(atmo_data.bTmoEn)
    {
        isp_cfg.module_en_update |= 1LL << RK_ISP2X_HDRTMO_ID;
        isp_cfg.module_ens |= 1LL << RK_ISP2X_HDRTMO_ID;
        isp_cfg.module_cfg_update |= 1LL << RK_ISP2X_HDRTMO_ID;
    }
    else
    {
        isp_cfg.module_en_update |= 1LL << RK_ISP2X_HDRTMO_ID;
        isp_cfg.module_ens &= ~(1LL << RK_ISP2X_HDRTMO_ID);
        isp_cfg.module_cfg_update &= ~(1LL << RK_ISP2X_HDRTMO_ID);
    }

    isp_cfg.others.hdrtmo_cfg.cnt_vsize     = atmo_data.Res.sw_hdrtmo_cnt_vsize;
    isp_cfg.others.hdrtmo_cfg.gain_ld_off2  = atmo_data.Res.sw_hdrtmo_gain_ld_off2;
    isp_cfg.others.hdrtmo_cfg.gain_ld_off1  = atmo_data.Res.sw_hdrtmo_gain_ld_off1;
    isp_cfg.others.hdrtmo_cfg.big_en        = atmo_data.Res.sw_hdrtmo_big_en;
    isp_cfg.others.hdrtmo_cfg.nobig_en      = atmo_data.Res.sw_hdrtmo_nobig_en;
    isp_cfg.others.hdrtmo_cfg.newhst_en     = atmo_data.Res.sw_hdrtmo_newhist_en;
    isp_cfg.others.hdrtmo_cfg.cnt_mode      = atmo_data.Res.sw_hdrtmo_cnt_mode;
    isp_cfg.others.hdrtmo_cfg.expl_lgratio  = atmo_data.Res.sw_hdrtmo_expl_lgratio;
    isp_cfg.others.hdrtmo_cfg.lgscl_ratio   = atmo_data.Res.sw_hdrtmo_lgscl_ratio;
    isp_cfg.others.hdrtmo_cfg.cfg_alpha     = atmo_data.Res.sw_hdrtmo_cfg_alpha;
    isp_cfg.others.hdrtmo_cfg.set_gainoff   = atmo_data.Res.sw_hdrtmo_set_gainoff;
    isp_cfg.others.hdrtmo_cfg.set_palpha    = atmo_data.Res.sw_hdrtmo_set_palpha;
    isp_cfg.others.hdrtmo_cfg.set_lgmax     = atmo_data.Res.sw_hdrtmo_set_lgmax;
    isp_cfg.others.hdrtmo_cfg.set_lgmin     = atmo_data.Res.sw_hdrtmo_set_lgmin;
    isp_cfg.others.hdrtmo_cfg.set_weightkey = atmo_data.Res.sw_hdrtmo_set_weightkey;
    isp_cfg.others.hdrtmo_cfg.set_lgmean    = atmo_data.Res.sw_hdrtmo_set_lgmean;
    isp_cfg.others.hdrtmo_cfg.set_lgrange1  = atmo_data.Res.sw_hdrtmo_set_lgrange1;
    isp_cfg.others.hdrtmo_cfg.set_lgrange0  = atmo_data.Res.sw_hdrtmo_set_lgrange0;
    isp_cfg.others.hdrtmo_cfg.set_lgavgmax  = atmo_data.Res.sw_hdrtmo_set_lgavgmax;
    isp_cfg.others.hdrtmo_cfg.clipgap1_i    = atmo_data.Res.sw_hdrtmo_clipgap1;
    isp_cfg.others.hdrtmo_cfg.clipgap0_i    = atmo_data.Res.sw_hdrtmo_clipgap0;
    isp_cfg.others.hdrtmo_cfg.clipratio1    = atmo_data.Res.sw_hdrtmo_clipratio1;
    isp_cfg.others.hdrtmo_cfg.clipratio0    = atmo_data.Res.sw_hdrtmo_clipratio0;
    isp_cfg.others.hdrtmo_cfg.ratiol        = atmo_data.Res.sw_hdrtmo_ratiol;
    isp_cfg.others.hdrtmo_cfg.lgscl_inv     = atmo_data.Res.sw_hdrtmo_lgscl_inv;
    isp_cfg.others.hdrtmo_cfg.lgscl         = atmo_data.Res.sw_hdrtmo_lgscl;
    isp_cfg.others.hdrtmo_cfg.lgmax         = atmo_data.Res.sw_hdrtmo_lgmax;
    isp_cfg.others.hdrtmo_cfg.hist_low      = atmo_data.Res.sw_hdrtmo_hist_low;
    isp_cfg.others.hdrtmo_cfg.hist_min      = atmo_data.Res.sw_hdrtmo_hist_min;
    isp_cfg.others.hdrtmo_cfg.hist_shift    = atmo_data.Res.sw_hdrtmo_hist_shift;
    isp_cfg.others.hdrtmo_cfg.hist_0p3      = atmo_data.Res.sw_hdrtmo_hist_0p3;
    isp_cfg.others.hdrtmo_cfg.hist_high     = atmo_data.Res.sw_hdrtmo_hist_high;
    isp_cfg.others.hdrtmo_cfg.palpha_lwscl  = atmo_data.Res.sw_hdrtmo_palpha_lwscl;
    isp_cfg.others.hdrtmo_cfg.palpha_lw0p5  = atmo_data.Res.sw_hdrtmo_palpha_lw0p5;
    isp_cfg.others.hdrtmo_cfg.palpha_0p18   = atmo_data.Res.sw_hdrtmo_palpha_0p18;
    isp_cfg.others.hdrtmo_cfg.maxgain       = atmo_data.Res.sw_hdrtmo_maxgain;
    isp_cfg.others.hdrtmo_cfg.maxpalpha     = atmo_data.Res.sw_hdrtmo_maxpalpha;

    //tmo predict
    isp_cfg.others.hdrtmo_cfg.predict.global_tmo = atmo_data.isHdrGlobalTmo;
    isp_cfg.others.hdrtmo_cfg.predict.scene_stable = atmo_data.Predict.Scenestable;
    isp_cfg.others.hdrtmo_cfg.predict.k_rolgmean = atmo_data.Predict.K_Rolgmean;
    isp_cfg.others.hdrtmo_cfg.predict.iir = atmo_data.Predict.iir;
    isp_cfg.others.hdrtmo_cfg.predict.iir_max = atmo_data.Predict.iir_max;
    isp_cfg.others.hdrtmo_cfg.predict.global_tmo_strength = atmo_data.Predict.global_tmo_strength;
#if 0
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: cnt_vsize %d", __LINE__, isp_cfg.others.hdrtmo_cfg.cnt_vsize);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: gain_ld_off2 %d", __LINE__, isp_cfg.others.hdrtmo_cfg.gain_ld_off2);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: gain_ld_off1 %d", __LINE__, isp_cfg.others.hdrtmo_cfg.gain_ld_off1);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: big_en %d", __LINE__, isp_cfg.others.hdrtmo_cfg.big_en);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: nobig_en %d", __LINE__, isp_cfg.others.hdrtmo_cfg.nobig_en);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: newhst_en %d", __LINE__, isp_cfg.others.hdrtmo_cfg.newhst_en);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: cnt_mode %d", __LINE__, isp_cfg.others.hdrtmo_cfg.cnt_mode);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: expl_lgratio %d", __LINE__, isp_cfg.others.hdrtmo_cfg.expl_lgratio);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: lgscl_ratio %d", __LINE__, isp_cfg.others.hdrtmo_cfg.lgscl_ratio);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: cfg_alpha %d", __LINE__, isp_cfg.others.hdrtmo_cfg.cfg_alpha);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: set_gainoff %d", __LINE__, isp_cfg.others.hdrtmo_cfg.set_gainoff);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: set_palpha %d", __LINE__, isp_cfg.others.hdrtmo_cfg.set_palpha);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: set_lgmax %d", __LINE__, isp_cfg.others.hdrtmo_cfg.set_lgmax);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: set_lgmin %d", __LINE__, isp_cfg.others.hdrtmo_cfg.set_lgmin);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: set_weightkey %d", __LINE__, isp_cfg.others.hdrtmo_cfg.set_weightkey);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: set_lgmean %d", __LINE__, isp_cfg.others.hdrtmo_cfg.set_lgmean);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: set_lgrange1 %d", __LINE__, isp_cfg.others.hdrtmo_cfg.set_lgrange1);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: set_lgrange0 %d", __LINE__, isp_cfg.others.hdrtmo_cfg.set_lgrange0);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: set_lgavgmax %d", __LINE__, isp_cfg.others.hdrtmo_cfg.set_lgavgmax);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: clipgap1_i %d", __LINE__, isp_cfg.others.hdrtmo_cfg.clipgap1_i);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: clipgap0_i %d", __LINE__, isp_cfg.others.hdrtmo_cfg.clipgap0_i);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: clipratio1 %d", __LINE__, isp_cfg.others.hdrtmo_cfg.clipratio1);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: clipratio0 %d", __LINE__, isp_cfg.others.hdrtmo_cfg.clipratio0);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: ratiol %d", __LINE__, isp_cfg.others.hdrtmo_cfg.ratiol);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: lgscl_inv %d", __LINE__, isp_cfg.others.hdrtmo_cfg.lgscl_inv);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: lgscl %d", __LINE__, isp_cfg.others.hdrtmo_cfg.lgscl);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: lgmax %d", __LINE__, isp_cfg.others.hdrtmo_cfg.lgmax);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: hist_low %d", __LINE__, isp_cfg.others.hdrtmo_cfg.hist_low);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: hist_min %d", __LINE__, isp_cfg.others.hdrtmo_cfg.hist_min);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: hist_shift %d", __LINE__, isp_cfg.others.hdrtmo_cfg.hist_shift);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: hist_0p3 %d", __LINE__, isp_cfg.others.hdrtmo_cfg.hist_0p3);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: hist_high %d", __LINE__, isp_cfg.others.hdrtmo_cfg.hist_high);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: palpha_lwscl %d", __LINE__, isp_cfg.others.hdrtmo_cfg.palpha_lwscl);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: palpha_lw0p5 %d", __LINE__, isp_cfg.others.hdrtmo_cfg.palpha_lw0p5);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: palpha_0p18 %d", __LINE__, isp_cfg.others.hdrtmo_cfg.palpha_0p18);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: maxgain %d", __LINE__, isp_cfg.others.hdrtmo_cfg.maxgain);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%d: maxpalpha %d", __LINE__, isp_cfg.others.hdrtmo_cfg.maxpalpha);
#endif
}

template<class T>
void
Isp20Params::convertAiqAfToIsp20Params(T& isp_cfg,
                                       const rk_aiq_isp_af_meas_t& af_data, bool af_cfg_udpate)
{
    if (!af_cfg_udpate)
        return;

    if (af_data.contrast_af_en)
        isp_cfg.module_ens |= ISP2X_MODULE_RAWAF;
    isp_cfg.module_en_update |= ISP2X_MODULE_RAWAF;
    isp_cfg.module_cfg_update |= ISP2X_MODULE_RAWAF;
    isp_cfg.meas.rawaf.rawaf_sel = af_data.rawaf_sel;
    isp_cfg.meas.rawaf.gamma_en = af_data.gamma_flt_en;
    isp_cfg.meas.rawaf.gaus_en = af_data.gaus_flt_en;
    isp_cfg.meas.rawaf.afm_thres = af_data.afm_thres;
    isp_cfg.meas.rawaf.gaus_coe_h0 = af_data.gaus_h0;
    isp_cfg.meas.rawaf.gaus_coe_h1 = af_data.gaus_h1;
    isp_cfg.meas.rawaf.gaus_coe_h2 = af_data.gaus_h2;
    memcpy(isp_cfg.meas.rawaf.lum_var_shift,
           af_data.lum_var_shift, ISP2X_RAWAF_WIN_NUM * sizeof(unsigned char));
    memcpy(isp_cfg.meas.rawaf.afm_var_shift,
           af_data.afm_var_shift, ISP2X_RAWAF_WIN_NUM * sizeof(unsigned char));
    memcpy(isp_cfg.meas.rawaf.line_en,
           af_data.line_en, ISP2X_RAWAF_LINE_NUM * sizeof(unsigned char));
    memcpy(isp_cfg.meas.rawaf.line_num,
           af_data.line_num, ISP2X_RAWAF_LINE_NUM * sizeof(unsigned char));
    memcpy(isp_cfg.meas.rawaf.gamma_y,
           af_data.gamma_y, ISP2X_RAWAF_GAMMA_NUM * sizeof(unsigned short));

    isp_cfg.meas.rawaf.num_afm_win = af_data.window_num;
    isp_cfg.meas.rawaf.win[0].h_offs = af_data.wina_h_offs;
    isp_cfg.meas.rawaf.win[0].v_offs = af_data.wina_v_offs;
    isp_cfg.meas.rawaf.win[0].h_size = af_data.wina_h_size;
    isp_cfg.meas.rawaf.win[0].v_size = af_data.wina_v_size;
    isp_cfg.meas.rawaf.win[1].h_offs = af_data.winb_h_offs;
    isp_cfg.meas.rawaf.win[1].v_offs = af_data.winb_v_offs;
    isp_cfg.meas.rawaf.win[1].h_size = af_data.winb_h_size;
    isp_cfg.meas.rawaf.win[1].v_size = af_data.winb_v_size;
}


#define ISP2X_WBGAIN_FIXSCALE_BIT  8
#define ISP2X_BLC_BIT_MAX 12

template<class T>
void
Isp20Params::convertAiqAwbGainToIsp20Params(T& isp_cfg,
        const rk_aiq_wb_gain_t& awb_gain, const rk_aiq_isp_blc_t &blc, bool awb_gain_update)
{

    if(awb_gain_update) {
        isp_cfg.module_ens |= 1LL << RK_ISP2X_AWB_GAIN_ID;
        isp_cfg.module_cfg_update |= 1LL << RK_ISP2X_AWB_GAIN_ID;
        isp_cfg.module_en_update |= 1LL << RK_ISP2X_AWB_GAIN_ID;
    } else {
        return;
    }

    struct isp2x_awb_gain_cfg *  cfg = &isp_cfg.others.awb_gain_cfg;
    uint16_t max_wb_gain = (1 << (ISP2X_WBGAIN_FIXSCALE_BIT + 2)) - 1;
    rk_aiq_wb_gain_t awb_gain1 = awb_gain;
    if(blc.enable) {
        awb_gain1.bgain *= (float)((1 << ISP2X_BLC_BIT_MAX) - 1) / ((1 << ISP2X_BLC_BIT_MAX) - 1 - blc.blc_b);
        awb_gain1.gbgain *= (float)((1 << ISP2X_BLC_BIT_MAX) - 1) / ((1 << ISP2X_BLC_BIT_MAX) - 1 - blc.blc_gb);
        awb_gain1.rgain *= (float)((1 << ISP2X_BLC_BIT_MAX) - 1) / ((1 << ISP2X_BLC_BIT_MAX) - 1 - blc.blc_r);
        awb_gain1.grgain *= (float)((1 << ISP2X_BLC_BIT_MAX) - 1) / ((1 << ISP2X_BLC_BIT_MAX) - 1 - blc.blc_gr);
    }
    // rescale
    float max_value  = awb_gain1.bgain > awb_gain1.gbgain ? awb_gain1.bgain : awb_gain1.gbgain;
    max_value = max_value > awb_gain1.rgain ? max_value : awb_gain1.rgain;
    float max_wb_gain_f = (float)max_wb_gain / (1 << (ISP2X_WBGAIN_FIXSCALE_BIT));
    if (max_value  > max_wb_gain_f ) {
        float scale = max_value / max_wb_gain_f;
        awb_gain1.bgain /= scale;
        awb_gain1.gbgain /= scale;
        awb_gain1.grgain /= scale;
        awb_gain1.rgain /= scale;
        LOGD_CAMHW("%s: scale %f, awbgain(r,g,g,b):[%f,%f,%f,%f]", __FUNCTION__, scale, awb_gain1.rgain, awb_gain1.grgain, awb_gain1.gbgain, awb_gain1.bgain);
    }
    //fix point
    //LOGE_CAMHW_SUBM(ISP20PARAM_SUBM,"max_wb_gain:%d\n",max_wb_gain);
    uint16_t R = (uint16_t)(0.5 + awb_gain1.rgain * (1 << ISP2X_WBGAIN_FIXSCALE_BIT));
    uint16_t B = (uint16_t)(0.5 + awb_gain1.bgain * (1 << ISP2X_WBGAIN_FIXSCALE_BIT));
    uint16_t Gr = (uint16_t)(0.5 + awb_gain1.grgain * (1 << ISP2X_WBGAIN_FIXSCALE_BIT));
    uint16_t Gb = (uint16_t)(0.5 + awb_gain1.gbgain * (1 << ISP2X_WBGAIN_FIXSCALE_BIT));
    cfg->gain_red       = R > max_wb_gain ? max_wb_gain : R;
    cfg->gain_blue      = B > max_wb_gain ? max_wb_gain : B;
    cfg->gain_green_r   = Gr > max_wb_gain ? max_wb_gain : Gr ;
    cfg->gain_green_b   = Gb > max_wb_gain ? max_wb_gain : Gb;
}

template<class T>
void Isp20Params::convertAiqAgammaToIsp20Params(T& isp_cfg,
        const AgammaProcRes_t& gamma_out_cfg)
{
    if(gamma_out_cfg.gamma_en) {
        isp_cfg.module_ens |= ISP2X_MODULE_GOC;
        isp_cfg.module_en_update |= ISP2X_MODULE_GOC;
        isp_cfg.module_cfg_update |= ISP2X_MODULE_GOC;
    } else {
        isp_cfg.module_ens &= ~ISP2X_MODULE_GOC;
        isp_cfg.module_en_update |= ISP2X_MODULE_GOC;
        return;
    }


    struct isp2x_gammaout_cfg* cfg = &isp_cfg.others.gammaout_cfg;
    cfg->offset = gamma_out_cfg.offset;
    cfg->equ_segm = gamma_out_cfg.equ_segm;
    for (int i = 0; i < 45; i++)
    {
        cfg->gamma_y[i] = gamma_out_cfg.gamma_y[i];
    }
}

template<class T>
void Isp20Params::convertAiqAdegammaToIsp20Params(T& isp_cfg,
        const AdegammaProcRes_t& degamma_cfg)
{
    if(degamma_cfg.degamma_en) {
        isp_cfg.module_ens |= ISP2X_MODULE_SDG;
        isp_cfg.module_en_update |= ISP2X_MODULE_SDG;
        isp_cfg.module_cfg_update |= ISP2X_MODULE_SDG;
    } else {
        isp_cfg.module_ens &= ~ISP2X_MODULE_SDG;
        isp_cfg.module_en_update |= ISP2X_MODULE_SDG;
        return;
    }

    struct isp2x_sdg_cfg* cfg = &isp_cfg.others.sdg_cfg;
    cfg->xa_pnts.gamma_dx0 = degamma_cfg.degamma_X_d0;
    cfg->xa_pnts.gamma_dx1 = degamma_cfg.degamma_X_d1;
    for (int i = 0; i < 17; i++) {
        cfg->curve_r.gamma_y[i] = degamma_cfg.degamma_tableR[i];
        cfg->curve_g.gamma_y[i] = degamma_cfg.degamma_tableG[i];
        cfg->curve_b.gamma_y[i] = degamma_cfg.degamma_tableB[i];
    }

#if 0
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) DEGAMMA_DX0:%d DEGAMMA_DX0:%d\n", __FUNCTION__, __LINE__, cfg->xa_pnts.gamma_dx0, cfg->xa_pnts.gamma_dx1);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "DEGAMMA_R_Y:%d %d %d %d %d %d %d %d\n", cfg->curve_r.gamma_y[0], cfg->curve_r.gamma_y[1],
                    cfg->curve_r.gamma_y[2], cfg->curve_r.gamma_y[3], cfg->curve_r.gamma_y[4], cfg->curve_r.gamma_y[5], cfg->curve_r.gamma_y[6], cfg->curve_r.gamma_y[7]);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "DEGAMMA_G_Y:%d %d %d %d %d %d %d %d\n", cfg->curve_g.gamma_y[0], cfg->curve_g.gamma_y[1],
                    cfg->curve_g.gamma_y[2], cfg->curve_g.gamma_y[3], cfg->curve_g.gamma_y[4], cfg->curve_g.gamma_y[5], cfg->curve_g.gamma_y[6], cfg->curve_g.gamma_y[7]);
    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "DEGAMMA_B_Y:%d %d %d %d %d %d %d %d\n", cfg->curve_b.gamma_y[0], cfg->curve_b.gamma_y[1],
                    cfg->curve_b.gamma_y[2], cfg->curve_b.gamma_y[3], cfg->curve_b.gamma_y[4], cfg->curve_b.gamma_y[5], cfg->curve_b.gamma_y[6], cfg->curve_b.gamma_y[7]);
#endif

}

template<class T>
void Isp20Params::convertAiqAdehazeToIsp20Params(T& isp_cfg,
        const rk_aiq_isp_dehaze_t& dhaze                     )
{
    int i;

    int rawWidth = 1920;
    int rawHeight = 1080;

    if(dhaze.enable) {
        isp_cfg.module_ens |= ISP2X_MODULE_DHAZ;
        isp_cfg.module_en_update |= ISP2X_MODULE_DHAZ;
        isp_cfg.module_cfg_update |= ISP2X_MODULE_DHAZ;
    }
    struct isp2x_dhaz_cfg *  cfg = &isp_cfg.others.dhaz_cfg;

    cfg->enhance_en     = dhaze.enhance_en;
    cfg->hist_chn   = dhaze.hist_chn;
    cfg->hpara_en   = dhaze.hpara_en;
    cfg->hist_en    = dhaze.hist_en;
    cfg->dc_en  = dhaze.dc_en;
    cfg->big_en     = dhaze.big_en;
    cfg->nobig_en   = dhaze.nobig_en;
    cfg->yblk_th    = dhaze.yblk_th;
    cfg->yhist_th   = dhaze.yhist_th;
    cfg->dc_max_th  = dhaze.dc_max_th;
    cfg->dc_min_th  = dhaze.dc_min_th;
    cfg->wt_max     = dhaze.wt_max;
    cfg->bright_max     = dhaze.bright_max;
    cfg->bright_min     = dhaze.bright_min;
    cfg->tmax_base  = dhaze.tmax_base;
    cfg->dark_th    = dhaze.dark_th;
    cfg->air_max    = dhaze.air_max;
    cfg->air_min    = dhaze.air_min;
    cfg->tmax_max   = dhaze.tmax_max;
    cfg->tmax_off   = dhaze.tmax_off;
    cfg->hist_k     = dhaze.hist_k;
    cfg->hist_th_off    = dhaze.hist_th_off;
    cfg->hist_min   = dhaze.hist_min;
    cfg->hist_gratio    = dhaze.hist_gratio;
    cfg->hist_scale     = dhaze.hist_scale;
    cfg->enhance_value  = dhaze.enhance_value;
    cfg->iir_wt_sigma   = dhaze.iir_wt_sigma;
    cfg->iir_sigma  = dhaze.iir_sigma;
    cfg->stab_fnum  = dhaze.stab_fnum;
    cfg->iir_tmax_sigma     = dhaze.iir_tmax_sigma;
    cfg->iir_air_sigma  = dhaze.iir_air_sigma;
    cfg->cfg_wt     = dhaze.cfg_wt;
    cfg->cfg_air    = dhaze.cfg_air;
    cfg->cfg_alpha  = dhaze.cfg_alpha;
    cfg->cfg_gratio     = dhaze.cfg_gratio;
    cfg->cfg_tmax   = dhaze.cfg_tmax;
    cfg->dc_weitcur     = dhaze.dc_weitcur;
    cfg->dc_thed    = dhaze.dc_thed;
    cfg->sw_dhaz_dc_bf_h0   = dhaze.sw_dhaz_dc_bf_h0;
    cfg->sw_dhaz_dc_bf_h1   = dhaze.sw_dhaz_dc_bf_h1;
    cfg->sw_dhaz_dc_bf_h2   = dhaze.sw_dhaz_dc_bf_h2;
    cfg->sw_dhaz_dc_bf_h3   = dhaze.sw_dhaz_dc_bf_h3;
    cfg->sw_dhaz_dc_bf_h4   = dhaze.sw_dhaz_dc_bf_h4;
    cfg->sw_dhaz_dc_bf_h5   = dhaze.sw_dhaz_dc_bf_h5;
    cfg->air_weitcur    = dhaze.air_weitcur;
    cfg->air_thed   = dhaze.air_thed;
    cfg->air_bf_h0  = dhaze.air_bf_h0;
    cfg->air_bf_h1  = dhaze.air_bf_h1;
    cfg->air_bf_h2  = dhaze.air_bf_h2;
    cfg->gaus_h0    = dhaze.gaus_h0;
    cfg->gaus_h1    = dhaze.gaus_h1;
    cfg->gaus_h2    = dhaze.gaus_h2;

    for(int i = 0; i < 6; i++) {
        cfg->conv_t0[i]   = dhaze.conv_t0[i];
        cfg->conv_t1[i]   = dhaze.conv_t1[i];
        cfg->conv_t2[i]   = dhaze.conv_t2[i];
    }



#if 0
    // cfg->dehaze_en      = int(dhaze.dehaze_en[0]);  //0~1  , (1bit) dehaze_en
    cfg->dc_en    = int(dhaze.dehaze_en[1]);  //0~1  , (1bit) dc_en
    cfg->hist_en          = int(dhaze.dehaze_en[2]);  //0~1  , (1bit) hist_en
    cfg->hist_chn         = int(dhaze.dehaze_en[3]);  //0~1  , (1bit) hist_channel
    cfg->big_en           = int(dhaze.dehaze_en[4]);  //0~1  , (1bit) gain_en
    cfg->dc_min_th    = int(dhaze.dehaze_self_adp[0]); //0~255, (8bit) dc_min_th
    cfg->dc_max_th    = int(dhaze.dehaze_self_adp[1]               );  //0~255, (8bit) dc_max_th
    cfg->yhist_th    = int(dhaze.dehaze_self_adp[2]                );  //0~255, (8bit) yhist_th
    cfg->yblk_th    = int(dhaze.dehaze_self_adp[3] * ((rawWidth + 15) / 16) * ((rawHeight + 15) / 16)); //default:28,(9bit) yblk_th
    cfg->dark_th    = int(dhaze.dehaze_self_adp[4]             );  //0~255, (8bit) dark_th
    cfg->bright_min   = int(dhaze.dehaze_self_adp[5]               );  //0~255, (8bit) bright_min
    cfg->bright_max   = int(dhaze.dehaze_self_adp[6]               );  //0~255, (8bit) bright_max
    cfg->wt_max   = int(dhaze.dehaze_range_adj[0] * 256          ); //0~255, (9bit) wt_max
    cfg->air_min   = int(dhaze.dehaze_range_adj[2]             );  //0~255, (8bit) air_min
    cfg->air_max   = int(dhaze.dehaze_range_adj[1]             );  //0~256, (8bit) air_max
    cfg->tmax_base   = int(dhaze.dehaze_range_adj[3]               );  //0~255, (8bit) tmax_base
    cfg->tmax_off   = int(dhaze.dehaze_range_adj[4] * 1024           ); //0~1024,(11bit) tmax_off
    cfg->tmax_max   = int(dhaze.dehaze_range_adj[5] * 1024           ); //0~1024,(11bit) tmax_max
    cfg->hist_gratio   = int(dhaze.dehaze_hist_para[0] * 8           ); //       (8bit) hist_gratio
    cfg->hist_th_off   = int(dhaze.dehaze_hist_para[1]             );  //       (8bit) hist_th_off
    cfg->hist_k   = int(dhaze.dehaze_hist_para[2] * 4 + 0.5    );  //0~7    (5bit),3bit+2bit, hist_k
    cfg->hist_min   = int(dhaze.dehaze_hist_para[3] * 256      );  //       (9bit) hist_min
    cfg->enhance_en       = int(dhaze.dehaze_enhance[0]                );  //0~1  , (1bit) enhance_en
    cfg->enhance_value    = int(dhaze.dehaze_enhance[1] * 1024 + 0.5   );  //       (14bit),4bit + 10bit, enhance_value
    cfg->hpara_en     = int(dhaze.dehaze_enhance[2]                );  //0~1  , (1bit) sw_hist_para_en
    cfg->hist_scale       = int(dhaze.dehaze_enhance[3] *  256 + 0.5   );  //       (13bit),5bit + 8bit, sw_hist_scale
    cfg->stab_fnum = int(dhaze.dehaze_iir_control[0]           );  //1~31,  (5bit) stab_fnum
    cfg->iir_sigma = int(dhaze.dehaze_iir_control[1]           );  //0~255, (8bit) sigma
    cfg->iir_wt_sigma = int(dhaze.dehaze_iir_control[2] * 8 + 0.5      ); //       (11bit),8bit+3bit, wt_sigma
    cfg->iir_air_sigma = int(dhaze.dehaze_iir_control[3]           );  //       (8bit) air_sigma
    cfg->iir_tmax_sigma = int(dhaze.dehaze_iir_control[4] * 1024 + 0.5);   //       (11bit) tmax_sigma
    cfg->cfg_alpha = int(MIN(dhaze.dehaze_user_config[0] * 256, 255)); //0~255, (8bit) cfg_alpha
    cfg->cfg_wt = int(dhaze.dehaze_user_config[1] * 256         ); //0~256, (9bit) cfg_wt
    cfg->cfg_air = int(dhaze.dehaze_user_config[2]             );  //0~255, (8bit) cfg_air
    cfg->cfg_tmax = int(dhaze.dehaze_user_config[3] * 1024       ); //0~1024,(11bit) cfg_tmax
    cfg->cfg_gratio = int(dhaze.dehaze_user_config[4] * 256      ); //       (13bit),5bit+8bit, cfg_gratio
    cfg->dc_thed      = int(dhaze.dehaze_bi_para[0]                );  //0~255, (8bit) dc_thed
    cfg->dc_weitcur       = int(dhaze.dehaze_bi_para[1] * 256 + 0.5    );  //0~256, (9bit) dc_weitcur
    cfg->air_thed     = int(dhaze.dehaze_bi_para[2]                );  //0~255, (8bit) air_thed
    cfg->air_weitcur      = int(dhaze.dehaze_bi_para[3] * 256 + 0.5    );  //0~256, (9bit) air_weitcur


    cfg->sw_dhaz_dc_bf_h0   = int(dhaze.dehaze_dc_bf_h[12]);//h0~h5  从大到小
    cfg->sw_dhaz_dc_bf_h1   = int(dhaze.dehaze_dc_bf_h[7]);
    cfg->sw_dhaz_dc_bf_h2   = int(dhaze.dehaze_dc_bf_h[6]);
    cfg->sw_dhaz_dc_bf_h3   = int(dhaze.dehaze_dc_bf_h[2]);
    cfg->sw_dhaz_dc_bf_h4   = int(dhaze.dehaze_dc_bf_h[1]);
    cfg->sw_dhaz_dc_bf_h5   = int(dhaze.dehaze_dc_bf_h[0]);


    cfg->air_bf_h0  = int(dhaze.dehaze_air_bf_h[4]);//h0~h2  从大到小
    cfg->air_bf_h1  = int(dhaze.dehaze_air_bf_h[1]);
    cfg->air_bf_h2  = int(dhaze.dehaze_air_bf_h[0]);

    cfg->gaus_h0    = int(dhaze.dehaze_gaus_h[4]);//h0~h2  从大到小
    cfg->gaus_h1    = int(dhaze.dehaze_gaus_h[1]);
    cfg->gaus_h2    = int(dhaze.dehaze_gaus_h[0]);

    for (int i = 0; i < 6; i++)
    {
        cfg->conv_t0[i]     = int(dhaze.dehaze_hist_t0[i]);
        cfg->conv_t1[i]     = int(dhaze.dehaze_hist_t1[i]);
        cfg->conv_t2[i]     = int(dhaze.dehaze_hist_t2[i]);
    }
#endif
}


template<class T>
void
Isp20Params::convertAiqBlcToIsp20Params(T& isp_cfg, rk_aiq_isp_blc_t &blc)
{
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) enter \n", __FUNCTION__, __LINE__);

    if(blc.enable) {
        isp_cfg.module_ens |= ISP2X_MODULE_BLS;
    }
    isp_cfg.module_en_update |= ISP2X_MODULE_BLS;
    isp_cfg.module_cfg_update |= ISP2X_MODULE_BLS;

    isp_cfg.others.bls_cfg.enable_auto = 0;
    isp_cfg.others.bls_cfg.en_windows = 0;

    isp_cfg.others.bls_cfg.bls_window1.h_offs = 0;
    isp_cfg.others.bls_cfg.bls_window1.v_offs = 0;
    isp_cfg.others.bls_cfg.bls_window1.h_size = 0;
    isp_cfg.others.bls_cfg.bls_window1.v_size = 0;

    isp_cfg.others.bls_cfg.bls_window2.h_offs = 0;
    isp_cfg.others.bls_cfg.bls_window2.v_offs = 0;
    isp_cfg.others.bls_cfg.bls_window2.h_size = 0;
    isp_cfg.others.bls_cfg.bls_window2.v_size = 0;

    isp_cfg.others.bls_cfg.bls_samples = 0;

    isp_cfg.others.bls_cfg.fixed_val.r = blc.blc_gr;
    isp_cfg.others.bls_cfg.fixed_val.gr = blc.blc_gr;
    isp_cfg.others.bls_cfg.fixed_val.gb = blc.blc_gr;
    isp_cfg.others.bls_cfg.fixed_val.b = blc.blc_gr;

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) exit \n", __FUNCTION__, __LINE__);
}


template<class T>
void
Isp20Params::convertAiqDpccToIsp20Params(T& isp_cfg, rk_aiq_isp_dpcc_t &dpcc)
{
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) enter \n", __FUNCTION__, __LINE__);

    struct isp2x_dpcc_cfg * pDpccCfg = &isp_cfg.others.dpcc_cfg;
    rk_aiq_isp_dpcc_t *pDpccRst = &dpcc;

    if(pDpccRst->stBasic.enable) {
        isp_cfg.module_ens |= ISP2X_MODULE_DPCC;
    }
    isp_cfg.module_en_update |= ISP2X_MODULE_DPCC;
    isp_cfg.module_cfg_update |= ISP2X_MODULE_DPCC;

    //mode 0x0000
    pDpccCfg->stage1_enable = pDpccRst->stBasic.stage1_enable;
    pDpccCfg->grayscale_mode = pDpccRst->stBasic.grayscale_mode;
    //pDpccCfg->enable = pDpccRst->stBasic.enable;

    //output_mode 0x0004
    pDpccCfg->sw_rk_out_sel = pDpccRst->stBasic.sw_rk_out_sel;
    pDpccCfg->sw_dpcc_output_sel = pDpccRst->stBasic.sw_dpcc_output_sel;
    pDpccCfg->stage1_rb_3x3 = pDpccRst->stBasic.stage1_rb_3x3;
    pDpccCfg->stage1_g_3x3 = pDpccRst->stBasic.stage1_g_3x3;
    pDpccCfg->stage1_incl_rb_center = pDpccRst->stBasic.stage1_incl_rb_center;
    pDpccCfg->stage1_incl_green_center = pDpccRst->stBasic.stage1_incl_green_center;

    //set_use 0x0008
    pDpccCfg->stage1_use_fix_set = pDpccRst->stBasic.stage1_use_fix_set;
    pDpccCfg->stage1_use_set_3 = pDpccRst->stBasic.stage1_use_set_3;
    pDpccCfg->stage1_use_set_2 = pDpccRst->stBasic.stage1_use_set_2;
    pDpccCfg->stage1_use_set_1 = pDpccRst->stBasic.stage1_use_set_1;

    //methods_set_1 0x000c
    pDpccCfg->sw_rk_red_blue1_en = pDpccRst->stBasic.sw_rk_red_blue1_en;
    pDpccCfg->rg_red_blue1_enable = pDpccRst->stBasic.rg_red_blue1_enable;
    pDpccCfg->rnd_red_blue1_enable = pDpccRst->stBasic.rnd_red_blue1_enable;
    pDpccCfg->ro_red_blue1_enable = pDpccRst->stBasic.ro_red_blue1_enable;
    pDpccCfg->lc_red_blue1_enable = pDpccRst->stBasic.lc_red_blue1_enable;
    pDpccCfg->pg_red_blue1_enable = pDpccRst->stBasic.pg_red_blue1_enable;
    pDpccCfg->sw_rk_green1_en = pDpccRst->stBasic.sw_rk_green1_en;
    pDpccCfg->rg_green1_enable = pDpccRst->stBasic.rg_green1_enable;
    pDpccCfg->rnd_green1_enable = pDpccRst->stBasic.rnd_green1_enable;
    pDpccCfg->ro_green1_enable = pDpccRst->stBasic.ro_green1_enable;
    pDpccCfg->lc_green1_enable = pDpccRst->stBasic.lc_green1_enable;
    pDpccCfg->pg_green1_enable = pDpccRst->stBasic.pg_green1_enable;

    //methods_set_2 0x0010
    pDpccCfg->sw_rk_red_blue2_en = pDpccRst->stBasic.sw_rk_red_blue2_en;
    pDpccCfg->rg_red_blue2_enable = pDpccRst->stBasic.rg_red_blue2_enable;
    pDpccCfg->rnd_red_blue2_enable = pDpccRst->stBasic.rnd_red_blue2_enable;
    pDpccCfg->ro_red_blue2_enable = pDpccRst->stBasic.ro_red_blue2_enable;
    pDpccCfg->lc_red_blue2_enable = pDpccRst->stBasic.lc_red_blue2_enable;
    pDpccCfg->pg_red_blue2_enable = pDpccRst->stBasic.pg_red_blue2_enable;
    pDpccCfg->sw_rk_green2_en = pDpccRst->stBasic.sw_rk_green2_en;
    pDpccCfg->rg_green2_enable = pDpccRst->stBasic.rg_green2_enable;
    pDpccCfg->rnd_green2_enable = pDpccRst->stBasic.rnd_green2_enable;
    pDpccCfg->ro_green2_enable = pDpccRst->stBasic.ro_green2_enable;
    pDpccCfg->lc_green2_enable = pDpccRst->stBasic.lc_green2_enable;
    pDpccCfg->pg_green2_enable = pDpccRst->stBasic.pg_green2_enable;

    //methods_set_3 0x0014
    pDpccCfg->sw_rk_red_blue3_en = pDpccRst->stBasic.sw_rk_red_blue3_en;
    pDpccCfg->rg_red_blue3_enable = pDpccRst->stBasic.rg_red_blue3_enable;
    pDpccCfg->rnd_red_blue3_enable = pDpccRst->stBasic.rnd_red_blue3_enable;
    pDpccCfg->ro_red_blue3_enable = pDpccRst->stBasic.ro_red_blue3_enable;
    pDpccCfg->lc_red_blue3_enable = pDpccRst->stBasic.lc_red_blue3_enable;
    pDpccCfg->pg_red_blue3_enable = pDpccRst->stBasic.pg_red_blue3_enable;
    pDpccCfg->sw_rk_green3_en = pDpccRst->stBasic.sw_rk_green3_en;
    pDpccCfg->rg_green3_enable = pDpccRst->stBasic.rg_green3_enable;
    pDpccCfg->rnd_green3_enable = pDpccRst->stBasic.rnd_green3_enable;
    pDpccCfg->ro_green3_enable = pDpccRst->stBasic.ro_green3_enable;
    pDpccCfg->lc_green3_enable = pDpccRst->stBasic.lc_green3_enable;
    pDpccCfg->pg_green3_enable = pDpccRst->stBasic.pg_green3_enable;

    //line_thresh_1 0x0018
    pDpccCfg->sw_mindis1_rb = pDpccRst->stBasic.sw_mindis1_rb;
    pDpccCfg->sw_mindis1_g = pDpccRst->stBasic.sw_mindis1_g;
    pDpccCfg->line_thr_1_rb = pDpccRst->stBasic.line_thr_1_rb;
    pDpccCfg->line_thr_1_g = pDpccRst->stBasic.line_thr_1_g;

    //line_mad_fac_1 0x001c
    pDpccCfg->sw_dis_scale_min1 = pDpccRst->stBasic.sw_dis_scale_min1;
    pDpccCfg->sw_dis_scale_max1 = pDpccRst->stBasic.sw_dis_scale_max1;
    pDpccCfg->line_mad_fac_1_rb = pDpccRst->stBasic.line_mad_fac_1_rb;
    pDpccCfg->line_mad_fac_1_g = pDpccRst->stBasic.line_mad_fac_1_g;

    //pg_fac_1 0x0020
    pDpccCfg->pg_fac_1_rb = pDpccRst->stBasic.pg_fac_1_rb;
    pDpccCfg->pg_fac_1_g = pDpccRst->stBasic.pg_fac_1_g;

    //rnd_thresh_1 0x0024
    pDpccCfg->rnd_thr_1_rb = pDpccRst->stBasic.rnd_thr_1_rb;
    pDpccCfg->rnd_thr_1_g = pDpccRst->stBasic.rnd_thr_1_g;

    //rg_fac_1 0x0028
    pDpccCfg->rg_fac_1_rb = pDpccRst->stBasic.rg_fac_1_rb;
    pDpccCfg->rg_fac_1_g = pDpccRst->stBasic.rg_fac_1_g;


    //line_thresh_2 0x002c
    pDpccCfg->sw_mindis2_rb = pDpccRst->stBasic.sw_mindis2_rb;
    pDpccCfg->sw_mindis2_g = pDpccRst->stBasic.sw_mindis2_g;
    pDpccCfg->line_thr_2_rb = pDpccRst->stBasic.line_thr_2_rb;
    pDpccCfg->line_thr_2_g = pDpccRst->stBasic.line_thr_2_g;

    //line_mad_fac_2 0x0030
    pDpccCfg->sw_dis_scale_min2 = pDpccRst->stBasic.sw_dis_scale_min2;
    pDpccCfg->sw_dis_scale_max2 = pDpccRst->stBasic.sw_dis_scale_max2;
    pDpccCfg->line_mad_fac_2_rb = pDpccRst->stBasic.line_mad_fac_2_rb;
    pDpccCfg->line_mad_fac_2_g = pDpccRst->stBasic.line_mad_fac_2_g;

    //pg_fac_2 0x0034
    pDpccCfg->pg_fac_2_rb = pDpccRst->stBasic.pg_fac_2_rb;
    pDpccCfg->pg_fac_2_g = pDpccRst->stBasic.pg_fac_2_g;

    //rnd_thresh_2 0x0038
    pDpccCfg->rnd_thr_2_rb = pDpccRst->stBasic.rnd_thr_2_rb;
    pDpccCfg->rnd_thr_2_g = pDpccRst->stBasic.rnd_thr_2_g;

    //rg_fac_2 0x003c
    pDpccCfg->rg_fac_2_rb = pDpccRst->stBasic.rg_fac_2_rb;
    pDpccCfg->rg_fac_2_g = pDpccRst->stBasic.rg_fac_2_g;


    //line_thresh_3 0x0040
    pDpccCfg->sw_mindis3_rb = pDpccRst->stBasic.sw_mindis3_rb;
    pDpccCfg->sw_mindis3_g = pDpccRst->stBasic.sw_mindis3_g;
    pDpccCfg->line_thr_3_rb = pDpccRst->stBasic.line_thr_3_rb;
    pDpccCfg->line_thr_3_g = pDpccRst->stBasic.line_thr_3_g;

    //line_mad_fac_3 0x0044
    pDpccCfg->sw_dis_scale_min3 = pDpccRst->stBasic.sw_dis_scale_min3;
    pDpccCfg->sw_dis_scale_max3 = pDpccRst->stBasic.sw_dis_scale_max3;
    pDpccCfg->line_mad_fac_3_rb = pDpccRst->stBasic.line_mad_fac_3_rb;
    pDpccCfg->line_mad_fac_3_g = pDpccRst->stBasic.line_mad_fac_3_g;

    //pg_fac_3 0x0048
    pDpccCfg->pg_fac_3_rb = pDpccRst->stBasic.pg_fac_3_rb;
    pDpccCfg->pg_fac_3_g = pDpccRst->stBasic.pg_fac_3_g;

    //rnd_thresh_3 0x004c
    pDpccCfg->rnd_thr_3_rb = pDpccRst->stBasic.rnd_thr_3_rb;
    pDpccCfg->rnd_thr_3_g = pDpccRst->stBasic.rnd_thr_3_g;

    //rg_fac_3 0x0050
    pDpccCfg->rg_fac_3_rb = pDpccRst->stBasic.rg_fac_3_rb;
    pDpccCfg->rg_fac_3_g = pDpccRst->stBasic.rg_fac_3_g;

    //ro_limits 0x0054
    pDpccCfg->ro_lim_3_rb = pDpccRst->stBasic.ro_lim_3_rb;
    pDpccCfg->ro_lim_3_g = pDpccRst->stBasic.ro_lim_3_g;
    pDpccCfg->ro_lim_2_rb = pDpccRst->stBasic.ro_lim_2_rb;;
    pDpccCfg->ro_lim_2_g = pDpccRst->stBasic.ro_lim_2_g;
    pDpccCfg->ro_lim_1_rb = pDpccRst->stBasic.ro_lim_1_rb;
    pDpccCfg->ro_lim_1_g = pDpccRst->stBasic.ro_lim_1_g;

    //rnd_offs 0x0058
    pDpccCfg->rnd_offs_3_rb = pDpccRst->stBasic.rnd_offs_3_rb;
    pDpccCfg->rnd_offs_3_g = pDpccRst->stBasic.rnd_offs_3_g;
    pDpccCfg->rnd_offs_2_rb = pDpccRst->stBasic.rnd_offs_2_rb;
    pDpccCfg->rnd_offs_2_g = pDpccRst->stBasic.rnd_offs_2_g;
    pDpccCfg->rnd_offs_1_rb = pDpccRst->stBasic.rnd_offs_1_rb;
    pDpccCfg->rnd_offs_1_g = pDpccRst->stBasic.rnd_offs_1_g;

    //bpt_ctrl 0x005c
    pDpccCfg->bpt_rb_3x3 = pDpccRst->stBpt.bpt_rb_3x3;
    pDpccCfg->bpt_g_3x3 = pDpccRst->stBpt.bpt_g_3x3;
    pDpccCfg->bpt_incl_rb_center = pDpccRst->stBpt.bpt_incl_rb_center;
    pDpccCfg->bpt_incl_green_center = pDpccRst->stBpt.bpt_incl_green_center;
    pDpccCfg->bpt_use_fix_set = pDpccRst->stBpt.bpt_use_fix_set;
    pDpccCfg->bpt_use_set_3 = pDpccRst->stBpt.bpt_use_set_3;
    pDpccCfg->bpt_use_set_2 = pDpccRst->stBpt.bpt_use_set_2;
    pDpccCfg->bpt_use_set_1 = pDpccRst->stBpt.bpt_use_set_1;
    pDpccCfg->bpt_cor_en = pDpccRst->stBpt.bpt_cor_en;
    pDpccCfg->bpt_det_en = pDpccRst->stBpt.bpt_det_en;

    //bpt_number 0x0060
    pDpccCfg->bp_number = pDpccRst->stBpt.bp_number;

    //bpt_addr 0x0064
    pDpccCfg->bp_table_addr = pDpccRst->stBpt.bp_table_addr;

    //bpt_data 0x0068
    pDpccCfg->bpt_v_addr = pDpccRst->stBpt.bpt_v_addr;
    pDpccCfg->bpt_h_addr = pDpccRst->stBpt.bpt_h_addr;

    //bp_cnt 0x006c
    pDpccCfg->bp_cnt = pDpccRst->stBpt.bp_cnt;

    //pdaf_en 0x0070
    pDpccCfg->sw_pdaf_en = pDpccRst->stPdaf.sw_pdaf_en;

    //pdaf_point_en 0x0074
    for(int i = 0; i < ISP2X_DPCC_PDAF_POINT_NUM; i++) {
        pDpccCfg->pdaf_point_en[i] = pDpccRst->stPdaf.pdaf_point_en[i];
    }

    //pdaf_offset 0x0078
    pDpccCfg->pdaf_offsety = pDpccRst->stPdaf.pdaf_offsety;
    pDpccCfg->pdaf_offsetx = pDpccRst->stPdaf.pdaf_offsetx;

    //pdaf_wrap 0x007c
    pDpccCfg->pdaf_wrapy = pDpccRst->stPdaf.pdaf_wrapy;
    pDpccCfg->pdaf_wrapx = pDpccRst->stPdaf.pdaf_wrapx;

    //pdaf_scope 0x0080
    pDpccCfg->pdaf_wrapy_num = pDpccRst->stPdaf.pdaf_wrapy_num;
    pDpccCfg->pdaf_wrapx_num = pDpccRst->stPdaf.pdaf_wrapx_num;

    //pdaf_point_0 0x0084
    for(int i = 0; i < ISP2X_DPCC_PDAF_POINT_NUM; i++) {
        pDpccCfg->point[i].x = pDpccRst->stPdaf.point[i].x;
        pDpccCfg->point[i].y = pDpccRst->stPdaf.point[i].y;
    }

    //pdaf_forward_med 0x00a4
    pDpccCfg->pdaf_forward_med = pDpccRst->stPdaf.pdaf_forward_med;


    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) exit \n", __FUNCTION__, __LINE__);
}


template<class T>
void
Isp20Params::convertAiqLscToIsp20Params(T& isp_cfg,
                                        const rk_aiq_lsc_cfg_t& lsc)
{

    if(lsc.lsc_en) {
        isp_cfg.module_ens |= ISP2X_MODULE_LSC;
    }
    isp_cfg.module_en_update |= ISP2X_MODULE_LSC;
    isp_cfg.module_cfg_update |= ISP2X_MODULE_LSC;

    struct isp2x_lsc_cfg *  cfg = &isp_cfg.others.lsc_cfg;
    memcpy(cfg->x_size_tbl, lsc.x_size_tbl, sizeof(lsc.x_size_tbl));
    memcpy(cfg->y_size_tbl, lsc.y_size_tbl, sizeof(lsc.y_size_tbl));
    memcpy(cfg->x_grad_tbl, lsc.x_grad_tbl, sizeof(lsc.x_grad_tbl));
    memcpy(cfg->y_grad_tbl, lsc.y_grad_tbl, sizeof(lsc.y_grad_tbl));

    memcpy(cfg->r_data_tbl, lsc.r_data_tbl, sizeof(lsc.r_data_tbl));
    memcpy(cfg->gr_data_tbl, lsc.gr_data_tbl, sizeof(lsc.gr_data_tbl));
    memcpy(cfg->gb_data_tbl, lsc.gb_data_tbl, sizeof(lsc.gb_data_tbl));
    memcpy(cfg->b_data_tbl, lsc.b_data_tbl, sizeof(lsc.b_data_tbl));
}

template<class T>
void Isp20Params::convertAiqCcmToIsp20Params(T& isp_cfg,
        const rk_aiq_ccm_cfg_t& ccm)
{
    if(ccm.ccmEnable) {
        isp_cfg.module_ens |= ISP2X_MODULE_CCM;
    }
    isp_cfg.module_en_update |= ISP2X_MODULE_CCM;
    isp_cfg.module_cfg_update |= ISP2X_MODULE_CCM;

    struct isp2x_ccm_cfg *  cfg = &isp_cfg.others.ccm_cfg;
    const float *coeff = ccm.matrix;
    const float *offset = ccm.offs;

    cfg->coeff0_r =  (coeff[0] - 1) > 0 ? (short)((coeff[0] - 1) * 128 + 0.5) : (short)((coeff[0] - 1) * 128 - 0.5); //check -128?
    cfg->coeff1_r =  coeff[1] > 0 ? (short)(coeff[1] * 128 + 0.5) : (short)(coeff[1] * 128 - 0.5);
    cfg->coeff2_r =  coeff[2] > 0 ? (short)(coeff[2] * 128 + 0.5) : (short)(coeff[2] * 128 - 0.5);
    cfg->coeff0_g =  coeff[3] > 0 ? (short)(coeff[3] * 128 + 0.5) : (short)(coeff[3] * 128 - 0.5);
    cfg->coeff1_g =  (coeff[4] - 1) > 0 ? (short)((coeff[4] - 1) * 128 + 0.5) : (short)((coeff[4] - 1) * 128 - 0.5);
    cfg->coeff2_g =  coeff[5] > 0 ? (short)(coeff[5] * 128 + 0.5) : (short)(coeff[5] * 128 - 0.5);
    cfg->coeff0_b =  coeff[6] > 0 ? (short)(coeff[6] * 128 + 0.5) : (short)(coeff[6] * 128 - 0.5);
    cfg->coeff1_b =  coeff[7] > 0 ? (short)(coeff[7] * 128 + 0.5) : (short)(coeff[7] * 128 - 0.5);
    cfg->coeff2_b =  (coeff[8] - 1) > 0 ? (short)((coeff[8] - 1) * 128 + 0.5) : (short)((coeff[8] - 1) * 128 - 0.5);

    cfg->offset_r = offset[0] > 0 ? (short)(offset[0] + 0.5) : (short)(offset[0] - 0.5);// for 12bit
    cfg->offset_g = offset[1] > 0 ? (short)(offset[1] + 0.5) : (int)(offset[1] - 0.5);
    cfg->offset_b = offset[2] > 0 ? (short)(offset[2] + 0.5) : (short)(offset[2] - 0.5);

    cfg->coeff0_y = (u16 )ccm.rgb2y_para[0];
    cfg->coeff1_y = (u16 )ccm.rgb2y_para[1];
    cfg->coeff2_y = (u16 )ccm.rgb2y_para[2];
    cfg->bound_bit = (u8)ccm.bound_bit;//check

    for( int i = 0; i < 17; i++)
    {
        cfg->alp_y[i] = (u16)(ccm.alp_y[i]);
    }


}

template<class T>
void Isp20Params::convertAiqA3dlutToIsp20Params(T& isp_cfg,
        const rk_aiq_lut3d_cfg_t& lut3d_cfg)
{
    if(lut3d_cfg.enable) {
        isp_cfg.module_ens |= ISP2X_MODULE_3DLUT;
    }
    isp_cfg.module_en_update |= ISP2X_MODULE_3DLUT;
    isp_cfg.module_cfg_update |= ISP2X_MODULE_3DLUT;

    struct isp2x_3dlut_cfg* cfg = &isp_cfg.others.isp3dlut_cfg;
    cfg->bypass_en = lut3d_cfg.bypass_en;
    cfg->actual_size = lut3d_cfg.lut3d_lut_wsize;
    memcpy(cfg->lut_r, lut3d_cfg.look_up_table_r, sizeof(cfg->lut_r));
    memcpy(cfg->lut_g, lut3d_cfg.look_up_table_g, sizeof(cfg->lut_g));
    memcpy(cfg->lut_b, lut3d_cfg.look_up_table_b, sizeof(cfg->lut_b));
}

template<class T>
void Isp20Params::convertAiqRawnrToIsp20Params(T& isp_cfg,
        rk_aiq_isp_rawnr_t& rawnr)
{

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) enter \n", __FUNCTION__, __LINE__);

    struct isp2x_rawnr_cfg * pRawnrCfg = &isp_cfg.others.rawnr_cfg;
    if(rawnr.rawnr_en) {

        isp_cfg.module_ens |= ISP2X_MODULE_RAWNR;
    } else {
        isp_cfg.module_ens &= ~ISP2X_MODULE_RAWNR;
    }
    isp_cfg.module_en_update |= ISP2X_MODULE_RAWNR;
    isp_cfg.module_cfg_update |= ISP2X_MODULE_RAWNR;

    int rawbit = 12;//rawBit;
    float tmp;

    //(0x0004)
    pRawnrCfg->gauss_en = rawnr.gauss_en;
    pRawnrCfg->log_bypass = rawnr.log_bypass;

    //(0x0008 - 0x0010)
    pRawnrCfg->filtpar0 = rawnr.filtpar0;
    pRawnrCfg->filtpar1 = rawnr.filtpar1;
    pRawnrCfg->filtpar2 = rawnr.filtpar2;

    //(0x0014 - 0x0001c)
    pRawnrCfg->dgain0 = rawnr.dgain0;
    pRawnrCfg->dgain1 = rawnr.dgain1;
    pRawnrCfg->dgain2 = rawnr.dgain2;

    //(0x0020 - 0x0002c)
    for(int i = 0; i < ISP2X_RAWNR_LUMA_RATION_NUM; i++) {
        pRawnrCfg->luration[i] = rawnr.luration[i];
    }

    //(0x0030 - 0x0003c)
    for(int i = 0; i < ISP2X_RAWNR_LUMA_RATION_NUM; i++) {
        pRawnrCfg->lulevel[i] = rawnr.lulevel[i];
    }

    //(0x0040)
    pRawnrCfg->gauss = rawnr.gauss;

    //(0x0044)
    pRawnrCfg->sigma = rawnr.sigma;

    //(0x0048)
    pRawnrCfg->pix_diff = rawnr.pix_diff;

    //(0x004c)
    pRawnrCfg->thld_diff = rawnr.thld_diff;

    //(0x0050)
    pRawnrCfg->gas_weig_scl1 = rawnr.gas_weig_scl1;
    pRawnrCfg->gas_weig_scl2 = rawnr.gas_weig_scl2;
    pRawnrCfg->thld_chanelw = rawnr.thld_chanelw;

    //(0x0054)
    pRawnrCfg->lamda = rawnr.lamda;

    //(0x0058 - 0x0005c)
    pRawnrCfg->fixw0 = rawnr.fixw0;
    pRawnrCfg->fixw1 = rawnr.fixw1;
    pRawnrCfg->fixw2 = rawnr.fixw2;
    pRawnrCfg->fixw3 = rawnr.fixw3;

    //(0x0060 - 0x00068)
    pRawnrCfg->wlamda0 = rawnr.wlamda0;
    pRawnrCfg->wlamda1 = rawnr.wlamda1;
    pRawnrCfg->wlamda2 = rawnr.wlamda2;

    //(0x006c)
    pRawnrCfg->rgain_filp = rawnr.rgain_filp;
    pRawnrCfg->bgain_filp = rawnr.bgain_filp;

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) exit \n", __FUNCTION__, __LINE__);
}

template<typename T>
void Isp20Params::convertAiqTnrToIsp20Params(T &pp_cfg,
        rk_aiq_isp_tnr_t& tnr)
{
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) enter \n", __FUNCTION__, __LINE__);
    int i = 0;

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "tnr_en %d", tnr.tnr_en);

    if(tnr.tnr_en) {
        pp_cfg.head.module_ens |= ISPP_MODULE_TNR;
    } else {
        //pp_cfg.head.module_init_ens &= ~ISPP_MODULE_TNR_3TO1;
        pp_cfg.head.module_ens &= ~ISPP_MODULE_TNR;
    }

    pp_cfg.head.module_en_update |= ISPP_MODULE_TNR;
    pp_cfg.head.module_cfg_update |= ISPP_MODULE_TNR;

    struct rkispp_tnr_config  * pTnrCfg = &pp_cfg.tnr_cfg;

    //0x0080
    if (tnr.mode > 0) {
        pp_cfg.head.module_ens |= ISPP_MODULE_TNR_3TO1;
    } else {
        pp_cfg.head.module_ens |= ISPP_MODULE_TNR;
    }

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "mode:%d  pp_cfg:0x%x\n", tnr.mode, pp_cfg.head.module_ens);

    /* pTnrCfg->mode = tnr.mode; */
    pTnrCfg->opty_en = tnr.opty_en;
    pTnrCfg->optc_en = tnr.optc_en;
    pTnrCfg->gain_en = tnr.gain_en;

    //0x0088
    pTnrCfg->pk0_y = tnr.pk0_y;
    pTnrCfg->pk1_y = tnr.pk1_y;
    pTnrCfg->pk0_c = tnr.pk0_c;
    pTnrCfg->pk1_c = tnr.pk1_c;

    //0x008c
    pTnrCfg->glb_gain_cur = tnr.glb_gain_cur;
    pTnrCfg->glb_gain_nxt = tnr.glb_gain_nxt;

    //0x0090
    pTnrCfg->glb_gain_cur_div = tnr.glb_gain_cur_div;
    pTnrCfg->glb_gain_cur_sqrt = tnr.glb_gain_cur_sqrt;

    //0x0094 - 0x0098
    for(i = 0; i < TNR_SIGMA_CURVE_SIZE - 1; i++) {
        pTnrCfg->sigma_x[i] = tnr.sigma_x[i];
    }



    //0x009c - 0x00bc
    for(i = 0; i < TNR_SIGMA_CURVE_SIZE; i++) {
        pTnrCfg->sigma_y[i] = tnr.sigma_y[i];
    }

    //0x00c4 - 0x00cc
    for(i = 0; i < TNR_LUMA_CURVE_SIZE; i++) {
        pTnrCfg->luma_curve[i] = tnr.luma_curve[i];
    }

    //0x00d0
    pTnrCfg->txt_th0_y = tnr.txt_th0_y;
    pTnrCfg->txt_th1_y = tnr.txt_th1_y;

    //0x00d4
    pTnrCfg->txt_th0_c = tnr.txt_th0_c;
    pTnrCfg->txt_th1_c = tnr.txt_th1_c;

    //0x00d8
    pTnrCfg->txt_thy_dlt = tnr.txt_thy_dlt;
    pTnrCfg->txt_thc_dlt = tnr.txt_thc_dlt;

    //0x00dc - 0x00ec
    for(i = 0; i < TNR_GFCOEF6_SIZE; i++) {
        pTnrCfg->gfcoef_y0[i] = tnr.gfcoef_y0[i];
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        pTnrCfg->gfcoef_y1[i] = tnr.gfcoef_y1[i];
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        pTnrCfg->gfcoef_y2[i] = tnr.gfcoef_y2[i];
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        pTnrCfg->gfcoef_y3[i] = tnr.gfcoef_y3[i];
    }

    //0x00f0 - 0x0100
    for(i = 0; i < TNR_GFCOEF6_SIZE; i++) {
        pTnrCfg->gfcoef_yg0[i] = tnr.gfcoef_yg0[i];
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        pTnrCfg->gfcoef_yg1[i] = tnr.gfcoef_yg1[i];
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        pTnrCfg->gfcoef_yg2[i] = tnr.gfcoef_yg2[i];
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        pTnrCfg->gfcoef_yg3[i] = tnr.gfcoef_yg3[i];
    }

    //0x0104 - 0x0110
    for(i = 0; i < TNR_GFCOEF6_SIZE; i++) {
        pTnrCfg->gfcoef_yl0[i] = tnr.gfcoef_yl0[i];
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        pTnrCfg->gfcoef_yl1[i] = tnr.gfcoef_yl1[i];
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        pTnrCfg->gfcoef_yl2[i] = tnr.gfcoef_yl2[i];
    }

    //0x0114 - 0x0120
    for(i = 0; i < TNR_GFCOEF6_SIZE; i++) {
        pTnrCfg->gfcoef_cg0[i] = tnr.gfcoef_cg0[i];
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        pTnrCfg->gfcoef_cg1[i] = tnr.gfcoef_cg1[i];
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        pTnrCfg->gfcoef_cg2[i] = tnr.gfcoef_cg2[i];
    }

    //0x0124 - 0x012c
    for(i = 0; i < TNR_GFCOEF6_SIZE; i++) {
        pTnrCfg->gfcoef_cl0[i] = tnr.gfcoef_cl0[i];
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        pTnrCfg->gfcoef_cl1[i] = tnr.gfcoef_cl1[i];
    }

    //0x0130 - 0x0134
    for(i = 0; i < TNR_SCALE_YG_SIZE; i++) {
        pTnrCfg->scale_yg[i] = tnr.scale_yg[i];
    }

    //0x0138 - 0x013c
    for(i = 0; i < TNR_SCALE_YL_SIZE; i++) {
        pTnrCfg->scale_yl[i] = tnr.scale_yl[i];
    }

    //0x0140 - 0x0148
    for(i = 0; i < TNR_SCALE_CG_SIZE; i++) {
        pTnrCfg->scale_cg[i] = tnr.scale_cg[i];
        pTnrCfg->scale_y2cg[i] = tnr.scale_y2cg[i];
    }

    //0x014c - 0x0154
    for(i = 0; i < TNR_SCALE_CL_SIZE; i++) {
        pTnrCfg->scale_cl[i] = tnr.scale_cl[i];
    }
    for(i = 0; i < TNR_SCALE_Y2CL_SIZE; i++) {
        pTnrCfg->scale_y2cl[i] = tnr.scale_y2cl[i];
    }
    //0x0158
    for(i = 0; i < TNR_WEIGHT_Y_SIZE; i++) {
        pTnrCfg->weight_y[i] = tnr.weight_y[i];
    }

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) exit \n", __FUNCTION__, __LINE__);
}

template<typename T>
void Isp20Params::convertAiqUvnrToIsp20Params(T &pp_cfg,
        rk_aiq_isp_uvnr_t& uvnr)
{
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) enter \n", __FUNCTION__, __LINE__);

    int i = 0;
    struct rkispp_nr_config  * pNrCfg = &pp_cfg.nr_cfg;

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "uvnr_en %d", uvnr.uvnr_en);
    if(uvnr.uvnr_en) {
        pp_cfg.head.module_ens |= ISPP_MODULE_NR;
        //pp_cfg.head.module_init_ens |= ISPP_MODULE_NR;
    } else {
        // NR bit used by ynr and uvnr together, so couldn't be
        // disabled if it was enabled
        if (!(pp_cfg.head.module_ens & ISPP_MODULE_NR))
            pp_cfg.head.module_ens &= ~ISPP_MODULE_NR;
    }

    pp_cfg.head.module_en_update |= ISPP_MODULE_NR;
    pp_cfg.head.module_cfg_update |= ISPP_MODULE_NR;

    //0x0080
    pNrCfg->uvnr_step1_en = uvnr.uvnr_step1_en;
    pNrCfg->uvnr_step2_en = uvnr.uvnr_step2_en;
    pNrCfg->nr_gain_en = uvnr.nr_gain_en;
    pNrCfg->uvnr_nobig_en = uvnr.uvnr_nobig_en;
    pNrCfg->uvnr_big_en = uvnr.uvnr_big_en;


    //0x0084
    pNrCfg->uvnr_gain_1sigma = uvnr.uvnr_gain_1sigma;

    //0x0088
    pNrCfg->uvnr_gain_offset = uvnr.uvnr_gain_offset;

    //0x008c
    pNrCfg->uvnr_gain_uvgain[0] = uvnr.uvnr_gain_uvgain[0];
    pNrCfg->uvnr_gain_uvgain[1] = uvnr.uvnr_gain_uvgain[1];
    pNrCfg->uvnr_gain_t2gen = uvnr.uvnr_gain_t2gen;
    // no need set
    pNrCfg->uvnr_gain_iso = uvnr.uvnr_gain_iso;

    //0x0090
    pNrCfg->uvnr_t1gen_m3alpha = uvnr.uvnr_t1gen_m3alpha;

    //0x0094
    pNrCfg->uvnr_t1flt_mode = uvnr.uvnr_t1flt_mode;

    //0x0098
    pNrCfg->uvnr_t1flt_msigma = uvnr.uvnr_t1flt_msigma;

    //0x009c
    pNrCfg->uvnr_t1flt_wtp = uvnr.uvnr_t1flt_wtp;

    //0x00a0-0x00a4
    for(i = 0; i < NR_UVNR_T1FLT_WTQ_SIZE; i++) {
        pNrCfg->uvnr_t1flt_wtq[i] = uvnr.uvnr_t1flt_wtq[i];
    }

    //0x00a8
    pNrCfg->uvnr_t2gen_m3alpha = uvnr.uvnr_t2gen_m3alpha;

    //0x00ac
    pNrCfg->uvnr_t2gen_msigma = uvnr.uvnr_t2gen_msigma;

    //0x00b0
    pNrCfg->uvnr_t2gen_wtp = uvnr.uvnr_t2gen_wtp;

    //0x00b4
    for(i = 0; i < NR_UVNR_T2GEN_WTQ_SIZE; i++) {
        pNrCfg->uvnr_t2gen_wtq[i] = uvnr.uvnr_t2gen_wtq[i];
    }

    //0x00b8
    pNrCfg->uvnr_t2flt_msigma = uvnr.uvnr_t2flt_msigma;

    //0x00bc
    pNrCfg->uvnr_t2flt_wtp = uvnr.uvnr_t2flt_wtp;
    for(i = 0; i < NR_UVNR_T2FLT_WT_SIZE; i++) {
        pNrCfg->uvnr_t2flt_wt[i] = uvnr.uvnr_t2flt_wt[i];
    }


    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) exit \n", __FUNCTION__, __LINE__);
}


template<typename T>
void Isp20Params::convertAiqYnrToIsp20Params(T &pp_cfg,
        rk_aiq_isp_ynr_t& ynr)
{
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) enter \n", __FUNCTION__, __LINE__);

    int i = 0;
    struct rkispp_nr_config  * pNrCfg = &pp_cfg.nr_cfg;

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "ynr_en %d", ynr.ynr_en);
    if(ynr.ynr_en) {
        pp_cfg.head.module_ens |= ISPP_MODULE_NR;
        //pp_cfg.head.module_init_ens |= ISPP_MODULE_NR;
    } else {
        // NR bit used by ynr and uvnr together, so couldn't be
        // disabled if it was enabled
        if (!(pp_cfg.head.module_ens & ISPP_MODULE_NR))
            pp_cfg.head.module_ens &= ~ISPP_MODULE_NR;
    }

    pp_cfg.head.module_en_update |= ISPP_MODULE_NR;
    pp_cfg.head.module_cfg_update |= ISPP_MODULE_NR;

    //0x0104 - 0x0108
    for(i = 0; i < NR_YNR_SGM_DX_SIZE; i++) {
        pNrCfg->ynr_sgm_dx[i] = ynr.ynr_sgm_dx[i];
    }

    //0x010c - 0x012c
    for(i = 0; i < NR_YNR_SGM_Y_SIZE; i++) {
        pNrCfg->ynr_lsgm_y[i] = ynr.ynr_lsgm_y[i];
    }


    //0x0130
    for(i = 0; i < NR_YNR_CI_SIZE; i++) {
        pNrCfg->ynr_lci[i] = ynr.ynr_lci[i];
    }

    //0x0134
    for(i = 0; i < NR_YNR_LGAIN_MIN_SIZE; i++) {
        pNrCfg->ynr_lgain_min[i] = ynr.ynr_lgain_min[i];
    }

    //0x0138
    pNrCfg->ynr_lgain_max = ynr.ynr_lgain_max;


    //0x013c
    pNrCfg->ynr_lmerge_bound = ynr.ynr_lmerge_bound;
    pNrCfg->ynr_lmerge_ratio = ynr.ynr_lmerge_ratio;

    //0x0140
    for(i = 0; i < NR_YNR_LWEIT_FLT_SIZE; i++) {
        pNrCfg->ynr_lweit_flt[i] = ynr.ynr_lweit_flt[i];
    }

    //0x0144 - 0x0164
    for(i = 0; i < NR_YNR_SGM_Y_SIZE; i++) {
        pNrCfg->ynr_hsgm_y[i] = ynr.ynr_hsgm_y[i];
    }

    //0x0168
    for(i = 0; i < NR_YNR_CI_SIZE; i++) {
        pNrCfg->ynr_hlci[i] = ynr.ynr_hlci[i];
    }

    //0x016c
    for(i = 0; i < NR_YNR_CI_SIZE; i++) {
        pNrCfg->ynr_lhci[i] = ynr.ynr_lhci[i];
    }

    //0x0170
    for(i = 0; i < NR_YNR_CI_SIZE; i++) {
        pNrCfg->ynr_hhci[i] = ynr.ynr_hhci[i];
    }

    //0x0174
    for(i = 0; i < NR_YNR_HGAIN_SGM_SIZE; i++) {
        pNrCfg->ynr_hgain_sgm[i] = ynr.ynr_hgain_sgm[i];
    }

    //0x0178 - 0x0188
    for(i = 0; i < NR_YNR_HWEIT_D_SIZE; i++) {
        pNrCfg->ynr_hweit_d[i] = ynr.ynr_hweit_d[i];
    }


    //0x018c - 0x01a0
    for(i = 0; i < NR_YNR_HGRAD_Y_SIZE; i++) {
        pNrCfg->ynr_hgrad_y[i] = ynr.ynr_hgrad_y[i];
    }

    //0x01a4 -0x01a8
    for(i = 0; i < NR_YNR_HWEIT_SIZE; i++) {
        pNrCfg->ynr_hweit[i] = ynr.ynr_hweit[i];
    }

    //0x01b0
    pNrCfg->ynr_hmax_adjust = ynr.ynr_hmax_adjust;

    //0x01b4
    pNrCfg->ynr_hstrength = ynr.ynr_hstrength;

    //0x01b8
    pNrCfg->ynr_lweit_cmp[0] = ynr.ynr_lweit_cmp[0];
    pNrCfg->ynr_lweit_cmp[1] = ynr.ynr_lweit_cmp[1];


    //0x01bc
    pNrCfg->ynr_lmaxgain_lv4 = ynr.ynr_lmaxgain_lv4;

    //0x01c0 - 0x01e0
    for(i = 0; i < NR_YNR_HSTV_Y_SIZE; i++) {
        pNrCfg->ynr_hstv_y[i] = ynr.ynr_hstv_y[i];
    }

    //0x01e4  - 0x01e8
    for(i = 0; i < NR_YNR_ST_SCALE_SIZE; i++) {
        pNrCfg->ynr_st_scale[i] = ynr.ynr_st_scale[i];
    }

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) exit \n", __FUNCTION__, __LINE__);

}

template<typename T>
void Isp20Params::convertAiqSharpenToIsp20Params(T &pp_cfg,
        rk_aiq_isp_sharpen_t& sharp, rk_aiq_isp_edgeflt_t& edgeflt)
{
    int i = 0;
    struct rkispp_sharp_config  * pSharpCfg = &pp_cfg.shp_cfg;
    RKAsharp_Sharp_HW_Fix_t *pSharpV1 = &sharp.stSharpFixV1;

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "sharp_en %d edgeflt_en %d", pSharpV1->sharp_en, edgeflt.edgeflt_en);

    if(pSharpV1->sharp_en && edgeflt.edgeflt_en) {
        pp_cfg.head.module_ens |= ISPP_MODULE_SHP;
        //pp_cfg.head.module_init_ens |= ISPP_MODULE_SHP;
    } else {
        pp_cfg.head.module_ens &=  ~ISPP_MODULE_SHP;
    }

    pp_cfg.head.module_en_update |= ISPP_MODULE_SHP;
    pp_cfg.head.module_cfg_update |= ISPP_MODULE_SHP;
#if 1
    //0x0080
    pSharpCfg->alpha_adp_en = edgeflt.alpha_adp_en;
    pSharpCfg->yin_flt_en = pSharpV1->yin_flt_en;
    pSharpCfg->edge_avg_en = pSharpV1->edge_avg_en;

    //0x0084
    pSharpCfg->hbf_ratio = pSharpV1->hbf_ratio;
    pSharpCfg->ehf_th = pSharpV1->ehf_th;
    pSharpCfg->pbf_ratio = pSharpV1->pbf_ratio;

    //0x0088
    pSharpCfg->edge_thed = edgeflt.edge_thed;
    pSharpCfg->dir_min = edgeflt.dir_min;
    pSharpCfg->smoth_th4 = edgeflt.smoth_th4;

    //0x008c
    pSharpCfg->l_alpha = edgeflt.l_alpha;
    pSharpCfg->g_alpha = edgeflt.g_alpha;


    //0x0090
    for(i = 0; i < SHP_PBF_KERNEL_SIZE; i++) {
        pSharpCfg->pbf_k[i] = pSharpV1->pbf_k[i];
    }

    //0x0094 - 0x0098
    for(i = 0; i < SHP_MRF_KERNEL_SIZE; i++) {
        pSharpCfg->mrf_k[i] = pSharpV1->mrf_k[i];
    }

    //0x009c -0x00a4
    for(i = 0; i < SHP_MBF_KERNEL_SIZE; i++) {
        pSharpCfg->mbf_k[i] = pSharpV1->mbf_k[i];
    }

    //0x00a8 -0x00ac
    for(i = 0; i < SHP_HRF_KERNEL_SIZE; i++) {
        pSharpCfg->hrf_k[i] = pSharpV1->hrf_k[i];
    }

    //0x00b0
    for(i = 0; i < SHP_HBF_KERNEL_SIZE; i++) {
        pSharpCfg->hbf_k[i] = pSharpV1->hbf_k[i];
    }

    //0x00b4
    for(i = 0; i < SHP_EDGE_COEF_SIZE; i++) {
        pSharpCfg->eg_coef[i] = edgeflt.eg_coef[i];
    }

    //0x00b8
    for(i = 0; i < SHP_EDGE_SMOTH_SIZE; i++) {
        pSharpCfg->eg_smoth[i] = edgeflt.eg_smoth[i];
    }

    //0x00bc - 0x00c0
    for(i = 0; i < SHP_EDGE_GAUS_SIZE; i++) {
        pSharpCfg->eg_gaus[i] = edgeflt.eg_gaus[i];
    }

    //0x00c4 - 0x00c8
    for(i = 0; i < SHP_DOG_KERNEL_SIZE; i++) {
        pSharpCfg->dog_k[i] = edgeflt.dog_k[i];
    }

    //0x00cc - 0x00d0
    for(i = 0; i < 6; i++) {
        pSharpCfg->lum_point[i] = pSharpV1->lum_point[i];
    }

    //0x00d4
    pSharpCfg->pbf_shf_bits = pSharpV1->pbf_shf_bits;
    pSharpCfg->mbf_shf_bits = pSharpV1->mbf_shf_bits;
    pSharpCfg->hbf_shf_bits = pSharpV1->hbf_shf_bits;


    //0x00d8 - 0x00dc
    for(i = 0; i < 8; i++) {
        pSharpCfg->pbf_sigma[i] = pSharpV1->pbf_sigma[i];
    }

    //0x00e0 - 0x00e4
    for(i = 0; i < 8; i++) {
        pSharpCfg->lum_clp_m[i] = pSharpV1->lum_clp_m[i];
    }

    //0x00e8 - 0x00ec
    for(i = 0; i < 8; i++) {
        pSharpCfg->lum_min_m[i] = pSharpV1->lum_min_m[i];
    }

    //0x00f0 - 0x00f4
    for(i = 0; i < 8; i++) {
        pSharpCfg->mbf_sigma[i] = pSharpV1->mbf_sigma[i];
    }

    //0x00f8 - 0x00fc
    for(i = 0; i < 8; i++) {
        pSharpCfg->lum_clp_h[i] = pSharpV1->lum_clp_h[i];
    }

    //0x0100 - 0x0104
    for(i = 0; i < 8; i++) {
        pSharpCfg->hbf_sigma[i] = pSharpV1->hbf_sigma[i];
    }

    //0x0108 - 0x010c
    for(i = 0; i < 8; i++) {
        pSharpCfg->edge_lum_thed[i] = edgeflt.edge_lum_thed[i];
    }

    //0x0110 - 0x0114
    for(i = 0; i < 8; i++) {
        pSharpCfg->clamp_pos[i] = edgeflt.clamp_pos[i];
    }

    //0x0118 - 0x011c
    for(i = 0; i < 8; i++) {
        pSharpCfg->clamp_neg[i] = edgeflt.clamp_neg[i];
    }

    //0x0120 - 0x0124
    for(i = 0; i < 8; i++) {
        pSharpCfg->detail_alpha[i] = edgeflt.detail_alpha[i];
    }

    //0x0128
    pSharpCfg->rfl_ratio = pSharpV1->rfl_ratio;
    pSharpCfg->rfh_ratio = pSharpV1->rfh_ratio;
    //0x012C
    pSharpCfg->m_ratio = pSharpV1->m_ratio;
    pSharpCfg->h_ratio = pSharpV1->h_ratio;
#endif
}

template<class T>
void
Isp20Params::convertAiqGainToIsp20Params(T& isp_cfg,
        rk_aiq_isp_gain_t& gain)
{
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) enter \n", __FUNCTION__, __LINE__);

    int i = 0;
    struct isp2x_gain_cfg * pGainCfg = &isp_cfg.others.gain_cfg;

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "gain table en %d \n", gain.gain_table_en);
    if(gain.gain_table_en) {
        isp_cfg.module_ens |= ISP2X_MODULE_GAIN;
        isp_cfg.module_en_update |= ISP2X_MODULE_GAIN;
        isp_cfg.module_cfg_update |= ISP2X_MODULE_GAIN;
    }

#if 0
    pGainCfg->dhaz_en = 0;
    pGainCfg->wdr_en = 0;
    pGainCfg->tmo_en = 0;
    pGainCfg->lsc_en = 0;
    pGainCfg->mge_en = 0;

    if(isp_cfg.module_ens & ISP2X_MODULE_DHAZ) {
        pGainCfg->dhaz_en = 1;
    }

    if(isp_cfg.module_ens & ISP2X_MODULE_WDR) {
        pGainCfg->wdr_en = 1;
    }

    if(isp_cfg.others.hdrmge_cfg.mode) {
        pGainCfg->tmo_en = 1;
        pGainCfg->mge_en = 1;
    }

    if(isp_cfg.module_ens & ISP2X_MODULE_LSC) {
        pGainCfg->lsc_en = 1;
    }


    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:%d gain en: %d %d %d %d %d\n",
                    __FUNCTION__, __LINE__,
                    pGainCfg->dhaz_en, pGainCfg->wdr_en,
                    pGainCfg->tmo_en, pGainCfg->lsc_en,
                    pGainCfg->mge_en);
#endif

    for(i = 0; i < ISP2X_GAIN_HDRMGE_GAIN_NUM; i++) {
        pGainCfg->mge_gain[i] = gain.mge_gain[i];
    }

    for(i = 0; i < ISP2X_GAIN_IDX_NUM; i++) {
        pGainCfg->idx[i] = gain.idx[i];
    }

    for(i = 0; i < ISP2X_GAIN_LUT_NUM; i++) {
        pGainCfg->lut[i] = gain.lut[i];
    }

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s:(%d) exit \n", __FUNCTION__, __LINE__);
}


template<typename T>
void Isp20Params::convertAiqFecToIsp20Params(T &pp_cfg,
        rk_aiq_isp_fec_t& fec)
{
    /* FEC module can't be enable/disable dynamically, the mode should
     * be decided in init params. we'll check if the module_init_ens
     * changed in CamIsp20Hw.cpp
     */

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "fec update params, enable %d usage %d, config %d", fec.fec_en, fec.usage, fec.config);
    if(fec.fec_en) {
        if (fec.usage == ISPP_MODULE_FEC_ST) {
            pp_cfg.head.module_ens |= ISPP_MODULE_FEC_ST;
            pp_cfg.head.module_en_update |= ISPP_MODULE_FEC_ST;
        } else if (fec.usage == ISPP_MODULE_FEC) {
            pp_cfg.head.module_ens |= ISPP_MODULE_FEC;
            pp_cfg.head.module_en_update |= ISPP_MODULE_FEC;
        }

        if (!fec.config) {
            pp_cfg.head.module_cfg_update &= ~ISPP_MODULE_FEC;
        } else {
            struct rkispp_fec_config  *pFecCfg = &pp_cfg.fec_cfg;

            pFecCfg->crop_en = fec.crop_en;
            pFecCfg->crop_width = fec.crop_width;
            pFecCfg->crop_height = fec.crop_height;
            pFecCfg->mesh_density = fec.mesh_density;
            pFecCfg->mesh_size = fec.mesh_size;
            pFecCfg->buf_fd = fec.mesh_buf_fd;
            //pp_cfg.fec_output_buf_index = fec.img_buf_index;
            //pp_cfg.fec_output_buf_size = fec.img_buf_size;

            pp_cfg.head.module_cfg_update |= ISPP_MODULE_FEC;
        }
    } else {
        pp_cfg.head.module_ens &= ~(ISPP_MODULE_FEC_ST | ISPP_MODULE_FEC);
        pp_cfg.head.module_en_update |= (ISPP_MODULE_FEC_ST | ISPP_MODULE_FEC);
    }
}

XCamReturn
Isp20Params::checkIsp20Params(struct isp2x_isp_params_cfg& isp_cfg)
{
    //TODO
    return XCAM_RETURN_NO_ERROR;
}

template<class T>
void
Isp20Params::convertAiqAdemosaicToIsp20Params(T& isp_cfg, rk_aiq_isp_debayer_t &demosaic)
{
    if (demosaic.updatecfg) {
        if (demosaic.enable) {
            isp_cfg.module_ens |= ISP2X_MODULE_DEBAYER;
            isp_cfg.module_en_update |= ISP2X_MODULE_DEBAYER;
            isp_cfg.module_cfg_update |= ISP2X_MODULE_DEBAYER;
        } else {
            isp_cfg.module_ens &= ~ISP2X_MODULE_DEBAYER;
            isp_cfg.module_en_update |= ISP2X_MODULE_DEBAYER;
        }
    } else {
        return;
    }

    isp_cfg.others.debayer_cfg.clip_en = demosaic.clip_en;
    isp_cfg.others.debayer_cfg.filter_c_en = demosaic.filter_c_en;
    isp_cfg.others.debayer_cfg.filter_g_en = demosaic.filter_g_en;
    isp_cfg.others.debayer_cfg.gain_offset = demosaic.gain_offset;
    isp_cfg.others.debayer_cfg.offset = demosaic.offset;
    isp_cfg.others.debayer_cfg.hf_offset = demosaic.hf_offset;
    isp_cfg.others.debayer_cfg.thed0 = demosaic.thed0;
    isp_cfg.others.debayer_cfg.thed1 = demosaic.thed1;
    isp_cfg.others.debayer_cfg.dist_scale = demosaic.dist_scale;
    isp_cfg.others.debayer_cfg.shift_num = demosaic.shift_num;
    isp_cfg.others.debayer_cfg.filter1_coe1 = demosaic.filter1_coe[0];
    isp_cfg.others.debayer_cfg.filter1_coe2 = demosaic.filter1_coe[1];
    isp_cfg.others.debayer_cfg.filter1_coe3 = demosaic.filter1_coe[2];
    isp_cfg.others.debayer_cfg.filter1_coe4 = demosaic.filter1_coe[3];
    isp_cfg.others.debayer_cfg.filter1_coe5 = demosaic.filter1_coe[4];
    isp_cfg.others.debayer_cfg.filter2_coe1 = demosaic.filter2_coe[0];
    isp_cfg.others.debayer_cfg.filter2_coe2 = demosaic.filter2_coe[1];
    isp_cfg.others.debayer_cfg.filter2_coe3 = demosaic.filter2_coe[2];
    isp_cfg.others.debayer_cfg.filter2_coe4 = demosaic.filter2_coe[3];
    isp_cfg.others.debayer_cfg.filter2_coe5 = demosaic.filter2_coe[4];
    isp_cfg.others.debayer_cfg.max_ratio = demosaic.max_ratio;
    isp_cfg.others.debayer_cfg.order_max = demosaic.order_max;
    isp_cfg.others.debayer_cfg.order_min = demosaic.order_min;
}

template<class T>
void
Isp20Params::convertAiqCpToIsp20Params(T& isp_cfg,
                                       const rk_aiq_acp_params_t& cp_cfg)
{
    struct isp2x_cproc_cfg* cproc_cfg = &isp_cfg.others.cproc_cfg;

    // TODO: set range
    /* cproc_cfg->y_in_range = 1; */
    /* cproc_cfg->y_out_range = 1; */
    /* cproc_cfg->c_out_range = 1; */

    if (cp_cfg.enable) {
        isp_cfg.module_ens |= ISP2X_MODULE_CPROC;
        isp_cfg.module_en_update |= ISP2X_MODULE_CPROC;
        isp_cfg.module_cfg_update |= ISP2X_MODULE_CPROC;
    } else {
        isp_cfg.module_ens &= ~ISP2X_MODULE_CPROC;
        isp_cfg.module_en_update |= ISP2X_MODULE_CPROC;
    }

    cproc_cfg->contrast = (uint8_t)(cp_cfg.contrast);
    cproc_cfg->sat = (uint8_t)(cp_cfg.saturation);
    cproc_cfg->brightness = (uint8_t)(cp_cfg.brightness - 128);
    cproc_cfg->hue = (uint8_t)(cp_cfg.hue - 128);
}

template<class T>
void
Isp20Params::convertAiqIeToIsp20Params(T& isp_cfg,
                                       const rk_aiq_isp_ie_t& ie_cfg)
{
    struct isp2x_ie_cfg* ie_config = &isp_cfg.others.ie_cfg;

    isp_cfg.module_ens |= ISP2X_MODULE_IE;
    isp_cfg.module_en_update |= ISP2X_MODULE_IE;
    isp_cfg.module_cfg_update |= ISP2X_MODULE_IE;

    switch (ie_cfg.base.mode) {
    case RK_AIQ_IE_EFFECT_BW:
        ie_config->effect = V4L2_COLORFX_BW;
        break;
    case RK_AIQ_IE_EFFECT_NEGATIVE:
        ie_config->effect = V4L2_COLORFX_NEGATIVE;
        break;
    case RK_AIQ_IE_EFFECT_SEPIA:
        ie_config->effect = V4L2_COLORFX_SEPIA;
        break;
    case RK_AIQ_IE_EFFECT_EMBOSS:
    {
        ie_config->effect = V4L2_COLORFX_EMBOSS;
        ie_config->eff_mat_1 = (uint16_t)(ie_cfg.extra.mode_coeffs[0])
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[1]) << 0x4)
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[2]) << 0x8)
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[3]) << 0xc);
        ie_config->eff_mat_2 = (uint16_t)(ie_cfg.extra.mode_coeffs[4])
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[5]) << 0x4)
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[6]) << 0x8)
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[7]) << 0xc);
        ie_config->eff_mat_3 = (ie_cfg.extra.mode_coeffs[8]);
        /*not used for this effect*/
        ie_config->eff_mat_4 = 0;
        ie_config->eff_mat_5 = 0;
        ie_config->color_sel = 0;
        ie_config->eff_tint = 0;
    }
    break;
    case RK_AIQ_IE_EFFECT_SKETCH:
    {
        ie_config->effect = V4L2_COLORFX_SKETCH;
        ie_config->eff_mat_3 = ((uint16_t)(ie_cfg.extra.mode_coeffs[0]) << 0x4)
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[1]) << 0x8)
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[2]) << 0xc);
        /*not used for this effect*/
        ie_config->eff_mat_4 = (uint16_t)(ie_cfg.extra.mode_coeffs[3])
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[4]) << 0x4)
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[5]) << 0x8)
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[6]) << 0xc);
        ie_config->eff_mat_5 = (uint16_t)(ie_cfg.extra.mode_coeffs[7])
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[8]) << 0x4);

        /*not used for this effect*/
        ie_config->eff_mat_1 = 0;
        ie_config->eff_mat_2 = 0;
        ie_config->color_sel = 0;
        ie_config->eff_tint = 0;
    }
    break;
    case RK_AIQ_IE_EFFECT_SHARPEN:
    {
        /* TODO: can't find related mode in v4l2_colorfx*/
        //ie_config->effect =
        //  V4L2_COLORFX_EMBOSS;
        ie_config->eff_mat_1 = (uint16_t)(ie_cfg.extra.mode_coeffs[0])
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[1]) << 0x4)
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[2]) << 0x8)
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[3]) << 0xc);
        ie_config->eff_mat_2 = (uint16_t)(ie_cfg.extra.mode_coeffs[4])
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[5]) << 0x4)
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[6]) << 0x8)
                               | ((uint16_t)(ie_cfg.extra.mode_coeffs[7]) << 0xc);
        ie_config->eff_mat_3 = (ie_cfg.extra.mode_coeffs[8]);
        /*not used for this effect*/
        ie_config->eff_mat_4 = 0;
        ie_config->eff_mat_5 = 0;
        ie_config->color_sel = 0;
        ie_config->eff_tint = 0;
    }
    break;
    case RK_AIQ_IE_EFFECT_NONE:
    {
        isp_cfg.module_ens &= ~ISP2X_MODULE_IE;
        isp_cfg.module_en_update |= ISP2X_MODULE_IE;
        isp_cfg.module_cfg_update &= ~ISP2X_MODULE_IE;
    }
    break;
    default:
        break;
    }
}

template<class T>
void
Isp20Params::convertAiqAldchToIsp20Params(T& isp_cfg,
        const rk_aiq_isp_ldch_t& ldch_cfg)
{
    struct isp2x_ldch_cfg  *pLdchCfg = &isp_cfg.others.ldch_cfg;

    // TODO: add update flag for ldch
    if (ldch_cfg.ldch_en) {
        isp_cfg.module_ens |= ISP2X_MODULE_LDCH;
        isp_cfg.module_en_update |= ISP2X_MODULE_LDCH;
        isp_cfg.module_cfg_update |= ISP2X_MODULE_LDCH;

        pLdchCfg->hsize = ldch_cfg.lut_h_size;
        pLdchCfg->vsize = ldch_cfg.lut_v_size;
        pLdchCfg->buf_fd = ldch_cfg.lut_mem_fd;
    } else {
        isp_cfg.module_ens &= ~ISP2X_MODULE_LDCH;
        isp_cfg.module_en_update |= ISP2X_MODULE_LDCH;
    }
}

template<class T>
void
Isp20Params::convertAiqGicToIsp20Params(T& isp_cfg,
                                        const rk_aiq_isp_gic_t& gic_cfg)
{
    struct isp2x_gic_cfg *isp_gic_cfg = &isp_cfg.others.gic_cfg;

    if (gic_cfg.gic_en) {
        isp_cfg.module_ens |= ISP2X_MODULE_GIC;
        isp_cfg.module_en_update |= ISP2X_MODULE_GIC;
        isp_cfg.module_cfg_update |= ISP2X_MODULE_GIC;
    } else {
        isp_cfg.module_ens &= ~ISP2X_MODULE_GIC;
        isp_cfg.module_en_update |= ISP2X_MODULE_GIC;
    }

    isp_gic_cfg->edge_open = gic_cfg.ProcResV20.edge_open;
    isp_gic_cfg->regmingradthrdark2 = gic_cfg.ProcResV20.regmingradthrdark2;
    isp_gic_cfg->regmingradthrdark1  = gic_cfg.ProcResV20.regmingradthrdark1;
    isp_gic_cfg->regminbusythre  = gic_cfg.ProcResV20.regminbusythre;
    isp_gic_cfg->regdarkthre  = gic_cfg.ProcResV20.regdarkthre;
    isp_gic_cfg->regmaxcorvboth  = gic_cfg.ProcResV20.regmaxcorvboth;
    isp_gic_cfg->regdarktthrehi  = gic_cfg.ProcResV20.regdarktthrehi;
    isp_gic_cfg->regkgrad2dark  = gic_cfg.ProcResV20.regkgrad2dark;
    isp_gic_cfg->regkgrad1dark  = gic_cfg.ProcResV20.regkgrad1dark;
    isp_gic_cfg->regstrengthglobal_fix  = gic_cfg.ProcResV20.regstrengthglobal_fix;
    isp_gic_cfg->regdarkthrestep  = gic_cfg.ProcResV20.regdarkthrestep;
    isp_gic_cfg->regkgrad2  = gic_cfg.ProcResV20.regkgrad2;
    isp_gic_cfg->regkgrad1  = gic_cfg.ProcResV20.regkgrad1;
    isp_gic_cfg->reggbthre  = gic_cfg.ProcResV20.reggbthre;
    isp_gic_cfg->regmaxcorv  = gic_cfg.ProcResV20.regmaxcorv;
    isp_gic_cfg->regmingradthr2  = gic_cfg.ProcResV20.regmingradthr2;
    isp_gic_cfg->regmingradthr1  = gic_cfg.ProcResV20.regmingradthr1;
    isp_gic_cfg->gr_ratio  = gic_cfg.ProcResV20.gr_ratio;
    isp_gic_cfg->dnhiscale = gic_cfg.ProcResV20.dnhiscale;
    isp_gic_cfg->dnloscale = gic_cfg.ProcResV20.dnloscale;
    isp_gic_cfg->reglumapointsstep  = gic_cfg.ProcResV20.reglumapointsstep;
    isp_gic_cfg->gvaluelimithi = gic_cfg.ProcResV20.gvaluelimithi;
    isp_gic_cfg->gvaluelimitlo = gic_cfg.ProcResV20.gvaluelimitlo;
    isp_gic_cfg->fusionratiohilimt1  = gic_cfg.ProcResV20.fusionratiohilimt1;
    isp_gic_cfg->regstrength_fix  = gic_cfg.ProcResV20.regstrength_fix;
    isp_gic_cfg->noise_cut_en = gic_cfg.ProcResV20.noise_cut_en;
    isp_gic_cfg->noise_coe_a = gic_cfg.ProcResV20.noise_coe_a;
    isp_gic_cfg->noise_coe_b = gic_cfg.ProcResV20.noise_coe_b;
    isp_gic_cfg->diff_clip = gic_cfg.ProcResV20.diff_clip;
    for(int i = 0; i < 15; i++)
        isp_gic_cfg->sigma_y[i]  = gic_cfg.ProcResV20.sigma_y[i];
}

void
Isp20Params::set_working_mode(int mode)
{
    _working_mode = mode;
}

template<typename T>
void Isp20Params::convertAiqOrbToIsp20Params(T &pp_cfg,
        rk_aiq_isp_orb_t& orb)
{
    if(orb.orb_en) {
        pp_cfg.head.module_ens |= ISPP_MODULE_ORB;
        pp_cfg.head.module_en_update |= ISPP_MODULE_ORB;
        pp_cfg.head.module_cfg_update |= ISPP_MODULE_ORB;
        //pp_cfg.head.module_init_ens |= ISPP_MODULE_ORB;

        struct rkispp_orb_config  *pOrbCfg = &pp_cfg.orb_cfg;

        pOrbCfg->limit_value = orb.limit_value;
        pOrbCfg->max_feature = orb.max_feature;
    } else {
        pp_cfg.head.module_ens &= ~ISPP_MODULE_ORB;
    }
}

void Isp20Params::setModuleStatus(rk_aiq_module_id_t mId, bool en)
{
#define _ISP_MODULE_CFG_(id)  \
    {\
        _force_module_flags |= 1LL << id;\
        if(en)\
            _force_isp_module_ens |= 1LL << id;\
        else\
            _force_isp_module_ens &= ~(1LL << id);\
    }

#define _ISPP_MODULE_CFG_(id, mod_en)  \
    {\
        _force_module_flags |= 1LL << id;\
        if(en)\
            _force_ispp_module_ens |= mod_en;\
        else\
            _force_ispp_module_ens &= ~(mod_en);\
    }

    SmartLock locker (_mutex);
    switch (mId) {
    case RK_MODULE_INVAL:
        break;
    case RK_MODULE_MAX:
        break;
    case RK_MODULE_FEC:
        break;
    case RK_MODULE_TNR:
        _ISPP_MODULE_CFG_(RK_ISP2X_PP_TNR_ID, ISPP_MODULE_TNR);
        break;
    case RK_MODULE_NR:
        _ISPP_MODULE_CFG_(RK_ISP2X_PP_NR_ID, ISPP_MODULE_NR);
        break;
    case RK_MODULE_RAWNR:
        _ISP_MODULE_CFG_(RK_ISP2X_RAWNR_ID);
    case RK_MODULE_DPCC:
        _ISP_MODULE_CFG_(RK_ISP2X_DPCC_ID);
        break;
    case RK_MODULE_BLS:
        _ISP_MODULE_CFG_(RK_ISP2X_BLS_ID);
        break;
    case RK_MODULE_LSC:
        _ISP_MODULE_CFG_(RK_ISP2X_LSC_ID);
        break;
    case RK_MODULE_CTK:
        _ISP_MODULE_CFG_(RK_ISP2X_CTK_ID);
        break;
    case RK_MODULE_AWB:
        _ISP_MODULE_CFG_(RK_ISP2X_RAWAWB_ID);
        break;
    case RK_MODULE_GOC:
        _ISP_MODULE_CFG_(RK_ISP2X_GOC_ID);
        break;
    case RK_MODULE_3DLUT:
        _ISP_MODULE_CFG_(RK_ISP2X_3DLUT_ID);
        break;
    case RK_MODULE_LDCH:
        _ISP_MODULE_CFG_(RK_ISP2X_LDCH_ID);
        break;
    case RK_MODULE_GIC:
        _ISP_MODULE_CFG_(RK_ISP2X_GIC_ID);
        break;
    case RK_MODULE_AWB_GAIN:
        _ISP_MODULE_CFG_(RK_ISP2X_GAIN_ID);
        break;
    case RK_MODULE_SHARP:
        _ISPP_MODULE_CFG_(RK_ISP2X_PP_TSHP_ID, ISPP_MODULE_SHP);
        break;
    case RK_MODULE_AE:
        break;
        //case RK_MODULE_DHAZ:
        //    _ISP_MODULE_CFG_(RK_ISP2X_DHAZ_ID);
        //break;
    }
}

void Isp20Params::getModuleStatus(rk_aiq_module_id_t mId, bool& en)
{
    int mod_id = -1;
    switch (mId) {
    case RK_MODULE_INVAL:
        break;
    case RK_MODULE_MAX:
        break;
    case RK_MODULE_TNR:
        mod_id = RK_ISP2X_PP_TNR_ID;
        break;
    case RK_MODULE_RAWNR:
        mod_id = RK_ISP2X_RAWNR_ID;
        break;
    case RK_MODULE_DPCC:
        mod_id = RK_ISP2X_DPCC_ID;
        break;
    case RK_MODULE_BLS:
        mod_id = RK_ISP2X_BLS_ID;
        break;
    case RK_MODULE_LSC:
        mod_id = RK_ISP2X_LSC_ID;
        break;
    case RK_MODULE_CTK:
        mod_id = RK_ISP2X_CTK_ID;
        break;
    case RK_MODULE_AWB:
        mod_id = RK_ISP2X_RAWAWB_ID;
        break;
    case RK_MODULE_GOC:
        mod_id = RK_ISP2X_GOC_ID;
        break;
    case RK_MODULE_NR:
        mod_id = RK_ISP2X_PP_NR_ID;
        break;
    case RK_MODULE_3DLUT:
        mod_id = RK_ISP2X_3DLUT_ID;
        break;
    case RK_MODULE_LDCH:
        mod_id = RK_ISP2X_LDCH_ID;
        break;
    case RK_MODULE_GIC:
        mod_id = RK_ISP2X_GIC_ID;
        break;
    case RK_MODULE_AWB_GAIN:
        mod_id = RK_ISP2X_GAIN_ID;
        break;
    case RK_MODULE_SHARP:
        mod_id = RK_ISP2X_PP_TSHP_ID;
        break;
    case RK_MODULE_AE:
        mod_id = RK_ISP2X_RAWAE_LITE_ID;
        break;
    case RK_MODULE_FEC:
        mod_id = RK_ISP2X_PP_TFEC_ID;
        break;
        //case RK_MODULE_DHAZ:
        //    mod_id = RK_ISP2X_DHAZ_ID;
        break;
    }

    if (mod_id < 0)
        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "input param: module ID is wrong!");
    else
        en = getModuleForceEn(mod_id);
}

bool Isp20Params::getModuleForceFlag(int module_id)
{
    SmartLock locker (_mutex);
    return ((_force_module_flags & (1LL << module_id)) >> module_id);
}

void Isp20Params::setModuleForceFlagInverse(int module_id)
{
    SmartLock locker (_mutex);
    _force_module_flags &= (~(1LL << module_id));
}

bool Isp20Params::getModuleForceEn(int module_id)
{
    SmartLock locker (_mutex);
    if(module_id == RK_ISP2X_PP_TNR_ID)
        return (_force_ispp_module_ens & ISPP_MODULE_TNR) >> 0;
    else if(module_id == RK_ISP2X_PP_NR_ID)
        return (_force_ispp_module_ens & ISPP_MODULE_NR) >> 1;
    else if(module_id == RK_ISP2X_PP_TSHP_ID)
        return (_force_ispp_module_ens & ISPP_MODULE_SHP) >> 2;
    else if(module_id == RK_ISP2X_PP_TFEC_ID)
        return (_force_ispp_module_ens & ISPP_MODULE_FEC) >> 3;
    else
        return ((_force_isp_module_ens & (1LL << module_id)) >> module_id);
}

void Isp20Params::updateIspModuleForceEns(u64 module_ens)
{
    SmartLock locker (_mutex);
    _force_isp_module_ens = module_ens;
}

void Isp20Params::updateIsppModuleForceEns(u32 module_ens)
{
    SmartLock locker (_mutex);
    _force_ispp_module_ens = module_ens;
}

#if 0
void
Isp20Params::forceOverwriteAiqIsppCfg(struct rkispp_params_cfg& pp_cfg,
                                      SmartPtr<RkAiqIspParamsProxy> aiq_meas_results,
                                      SmartPtr<RkAiqIspParamsProxy> aiq_other_results)
{
    rk_aiq_ispp_meas_params_t* ispp_meas_param =
        static_cast<rk_aiq_ispp_meas_params_t*>(aiq_meas_results->data().ptr());
    rk_aiq_ispp_other_params_t* ispp_other_param =
        static_cast<rk_aiq_ispp_other_params_t*>(aiq_other_results->data().ptr());

    for (int i = RK_ISP2X_PP_TNR_ID; i <= RK_ISP2X_PP_MAX_ID; i++) {
        if (getModuleForceFlag(i)) {
            switch (i) {
            case RK_ISP2X_PP_TNR_ID:
                if (!ispp_other_param)
                    break;
                if (getModuleForceEn(RK_ISP2X_PP_TNR_ID)) {
                    if(ispp_other_param->tnr.tnr_en) {
                        pp_cfg.module_ens |= ISPP_MODULE_TNR;
                        pp_cfg.module_en_update |= ISPP_MODULE_TNR;
                        pp_cfg.module_cfg_update |= ISPP_MODULE_TNR;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_PP_TNR_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "algo isn't enabled, so enable module failed!");
                    }
                } else {
                    pp_cfg.module_ens &= ~ISPP_MODULE_TNR;
                    pp_cfg.module_en_update |= ISPP_MODULE_TNR;
                    pp_cfg.module_cfg_update &= ~ISPP_MODULE_TNR;
                }
                break;
            case RK_ISP2X_PP_NR_ID:
                if (!ispp_other_param)
                    break;
                if (getModuleForceEn(RK_ISP2X_PP_NR_ID)) {
                    if( ispp_other_param->tnr.tnr_en) {
                        pp_cfg.module_ens |= ISPP_MODULE_NR;
                        pp_cfg.module_en_update |= ISPP_MODULE_NR;
                        pp_cfg.module_cfg_update |= ISPP_MODULE_NR;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_PP_NR_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "algo isn't enabled, so enable module failed!");
                    }
                } else {
                    pp_cfg.module_ens &= ~ISPP_MODULE_NR;
                    pp_cfg.module_en_update |= ISPP_MODULE_NR;
                    pp_cfg.module_cfg_update &= ~ISPP_MODULE_NR;
                }
                break;
            case RK_ISP2X_PP_TSHP_ID:
                if (!ispp_other_param)
                    break;
                if (getModuleForceEn(RK_ISP2X_PP_TSHP_ID)) {
                    if(ispp_other_param->sharpen.stSharpFixV1.sharp_en ||
                            ispp_other_param->edgeflt.edgeflt_en) {
                        pp_cfg.module_ens |= ISPP_MODULE_SHP;
                        pp_cfg.module_en_update |= ISPP_MODULE_SHP;
                        pp_cfg.module_cfg_update |= ISPP_MODULE_SHP;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_PP_TSHP_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "algo isn't enabled, so enable module failed!");
                    }
                } else {
                    pp_cfg.module_ens &= ~ISPP_MODULE_SHP;
                    pp_cfg.module_en_update |= ISPP_MODULE_SHP;
                    pp_cfg.module_cfg_update &= ~ISPP_MODULE_SHP;
                }
                break;
            }
        }
    }
    updateIsppModuleForceEns(pp_cfg.module_ens);
}

void
Isp20Params::forceOverwriteAiqIspCfg(struct isp2x_isp_params_cfg& isp_cfg,
                                     SmartPtr<RkAiqIspParamsProxy> aiq_results,
                                     SmartPtr<RkAiqIspParamsProxy> aiq_other_results)
{
    rk_aiq_isp_meas_params_v20_t* isp20_meas_result =
        static_cast<rk_aiq_isp_meas_params_v20_t*>(aiq_results->data().ptr());
    rk_aiq_isp_other_params_v20_t* isp20_other_result =
        static_cast<rk_aiq_isp_other_params_v20_t*>(aiq_other_results->data().ptr());
    for (int i = 0; i <= RK_ISP2X_MAX_ID; i++) {
        if (getModuleForceFlag(i)) {
            switch (i) {
            case RK_ISP2X_DPCC_ID:
                if (getModuleForceEn(RK_ISP2X_DPCC_ID)) {
                    if(isp20_meas_result->dpcc.stBasic.enable) {
                        isp_cfg.module_ens |= ISP2X_MODULE_DPCC;
                        isp_cfg.module_en_update |= ISP2X_MODULE_DPCC;
                        isp_cfg.module_cfg_update |= ISP2X_MODULE_DPCC;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_DPCC_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "dpcc algo isn't enabled, so enable module failed!");
                    }
                } else {
                    isp_cfg.module_ens &= ~ISP2X_MODULE_DPCC;
                    isp_cfg.module_en_update |= ISP2X_MODULE_DPCC;
                    isp_cfg.module_cfg_update &= ~ISP2X_MODULE_DPCC;
                }
                break;
            case RK_ISP2X_BLS_ID:
                if (getModuleForceEn(RK_ISP2X_BLS_ID)) {
                    if(isp20_other_result->blc.enable) {
                        isp_cfg.module_ens |= ISP2X_MODULE_BLS;
                        isp_cfg.module_en_update |= ISP2X_MODULE_BLS;
                        isp_cfg.module_cfg_update |= ISP2X_MODULE_BLS;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_BLS_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "bls algo isn't enabled, so enable module failed!");
                    }
                } else {
                    isp_cfg.module_ens &= ~ISP2X_MODULE_BLS;
                    isp_cfg.module_en_update |= ISP2X_MODULE_BLS;
                    isp_cfg.module_cfg_update &= ~ISP2X_MODULE_BLS;
                }
                break;
            case RK_ISP2X_LSC_ID:
                if (getModuleForceEn(RK_ISP2X_LSC_ID)) {
                    if(isp20_meas_result->lsc.lsc_en) {
                        isp_cfg.module_ens |= ISP2X_MODULE_LSC;
                        isp_cfg.module_en_update |= ISP2X_MODULE_LSC;
                        isp_cfg.module_cfg_update |= ISP2X_MODULE_LSC;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_LSC_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "lsc algo isn't enabled, so enable module failed!");
                    }
                } else {
                    isp_cfg.module_ens &= ~ISP2X_MODULE_LSC;
                    isp_cfg.module_en_update |= ISP2X_MODULE_LSC;
                    isp_cfg.module_cfg_update &= ~ISP2X_MODULE_LSC;
                }
                break;
            case RK_ISP2X_CTK_ID:
                if (getModuleForceEn(RK_ISP2X_CTK_ID)) {
                    if(isp20_meas_result->lsc.lsc_en) {
                        isp_cfg.module_ens |= ISP2X_MODULE_CCM;
                        isp_cfg.module_en_update |= ISP2X_MODULE_CCM;
                        isp_cfg.module_cfg_update |= ISP2X_MODULE_CCM;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_CTK_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "ccm algo isn't enabled, so enable module failed!");
                    }
                } else {
                    isp_cfg.module_ens &= ~ISP2X_MODULE_CCM;
                    isp_cfg.module_en_update |= ISP2X_MODULE_CCM;
                    isp_cfg.module_cfg_update &= ~ISP2X_MODULE_CCM;
                }
                break;
            case RK_ISP2X_RAWAWB_ID:
                if (getModuleForceEn(RK_ISP2X_RAWAWB_ID)) {
                    if(isp20_meas_result->awb_cfg.awbEnable) {
                        isp_cfg.module_ens |= ISP2X_MODULE_RAWAWB;
                        isp_cfg.module_en_update |= ISP2X_MODULE_RAWAWB;
                        isp_cfg.module_cfg_update |= ISP2X_MODULE_RAWAWB;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_RAWAWB_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "awb algo isn't enabled, so enable module failed!");
                    }
                } else {
                    isp_cfg.module_ens &= ~ISP2X_MODULE_RAWAWB;
                    isp_cfg.module_en_update |= ISP2X_MODULE_RAWAWB;
                    isp_cfg.module_cfg_update &= ~ISP2X_MODULE_RAWAWB;
                }
                break;
            case RK_ISP2X_GOC_ID:
                if (getModuleForceEn(RK_ISP2X_GOC_ID)) {
                    if(isp20_other_result->agamma.gamma_en) {
                        isp_cfg.module_ens |= ISP2X_MODULE_GOC;
                        isp_cfg.module_en_update |= ISP2X_MODULE_GOC;
                        isp_cfg.module_cfg_update |= ISP2X_MODULE_GOC;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_GOC_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "goc algo isn't enabled, so enable module failed!");
                    }
                } else {
                    isp_cfg.module_ens &= ~ISP2X_MODULE_GOC;
                    isp_cfg.module_en_update |= ISP2X_MODULE_GOC;
                    isp_cfg.module_cfg_update &= ~ISP2X_MODULE_GOC;
                }
                break;
            case RK_ISP2X_RAWNR_ID:
                if (getModuleForceEn(RK_ISP2X_RAWNR_ID)) {
                    if(isp20_other_result->rawnr.rawnr_en) {
                        isp_cfg.module_ens |= ISP2X_MODULE_RAWNR;
                        isp_cfg.module_en_update |= ISP2X_MODULE_RAWNR;
                        isp_cfg.module_cfg_update |= ISP2X_MODULE_RAWNR;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_RAWNR_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "rawnr algo isn't enabled, so enable module failed!");
                    }
                } else {
                    isp_cfg.module_ens &= ~ISP2X_MODULE_RAWNR;
                    isp_cfg.module_en_update |= ISP2X_MODULE_RAWNR;
                    isp_cfg.module_cfg_update &= ~ISP2X_MODULE_RAWNR;
                }
                break;
            case RK_ISP2X_3DLUT_ID:
                if (getModuleForceEn(RK_ISP2X_3DLUT_ID)) {
                    if(isp20_other_result->rawnr.rawnr_en) {
                        isp_cfg.module_ens |= ISP2X_MODULE_3DLUT;
                        isp_cfg.module_en_update |= ISP2X_MODULE_3DLUT;
                        isp_cfg.module_cfg_update |= ISP2X_MODULE_3DLUT;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_3DLUT_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "3dlut algo isn't enabled, so enable module failed!");
                    }
                } else {
                    isp_cfg.module_ens &= ~ISP2X_MODULE_3DLUT;
                    isp_cfg.module_en_update |= ISP2X_MODULE_3DLUT;
                    isp_cfg.module_cfg_update &= ~ISP2X_MODULE_3DLUT;
                }
                break;
            case RK_ISP2X_LDCH_ID:
                if (getModuleForceEn(RK_ISP2X_LDCH_ID)) {
                    if(isp20_other_result->ldch.ldch_en) {
                        isp_cfg.module_ens |= ISP2X_MODULE_LDCH;
                        isp_cfg.module_en_update |= ISP2X_MODULE_LDCH;
                        isp_cfg.module_cfg_update |= ISP2X_MODULE_LDCH;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_LDCH_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "ldch algo isn't enabled, so enable module failed!");
                    }
                } else {
                    isp_cfg.module_ens &= ~ISP2X_MODULE_LDCH;
                    isp_cfg.module_en_update |= ISP2X_MODULE_LDCH;
                    isp_cfg.module_cfg_update &= ~ISP2X_MODULE_LDCH;
                }
                break;
            case RK_ISP2X_GIC_ID:
                if (getModuleForceEn(RK_ISP2X_GIC_ID)) {
                    if(isp20_other_result->gic.gic_en) {
                        isp_cfg.module_ens |= ISP2X_MODULE_GIC;
                        isp_cfg.module_en_update |= ISP2X_MODULE_GIC;
                        isp_cfg.module_cfg_update |= ISP2X_MODULE_GIC;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_GIC_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "gic algo isn't enabled, so enable module failed!");
                    }
                } else {
                    isp_cfg.module_ens &= ~ISP2X_MODULE_GIC;
                    isp_cfg.module_en_update |= ISP2X_MODULE_GIC;
                    isp_cfg.module_cfg_update &= ~ISP2X_MODULE_GIC;
                }
                break;
            case RK_ISP2X_GAIN_ID:
                if (getModuleForceEn(RK_ISP2X_GAIN_ID)) {
                    if(isp20_other_result->gain_config.gain_table_en) {
                        isp_cfg.module_ens |= ISP2X_MODULE_GAIN;
                        isp_cfg.module_en_update |= ISP2X_MODULE_GAIN;
                        isp_cfg.module_cfg_update |= ISP2X_MODULE_GAIN;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_GAIN_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "gain algo isn't enabled, so enable module failed!");
                    }
                } else {
                    isp_cfg.module_ens &= ~ISP2X_MODULE_GAIN;
                    isp_cfg.module_en_update |= ISP2X_MODULE_GAIN;
                    isp_cfg.module_cfg_update &= ~ISP2X_MODULE_GAIN;
                }
                break;
            case RK_ISP2X_DHAZ_ID:
                if (getModuleForceEn(RK_ISP2X_DHAZ_ID)) {
                    if(isp20_other_result->adhaz.enable) {
                        isp_cfg.module_ens |= ISP2X_MODULE_DHAZ;
                        isp_cfg.module_en_update |= ISP2X_MODULE_DHAZ;
                        isp_cfg.module_cfg_update |= ISP2X_MODULE_DHAZ;
                    } else {
                        setModuleForceFlagInverse(RK_ISP2X_DHAZ_ID);
                        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "dehaze algo isn't enabled, so enable module failed!");
                    }
                } else {
                    isp_cfg.module_ens &= ~ISP2X_MODULE_DHAZ;
                    isp_cfg.module_en_update |= ISP2X_MODULE_DHAZ;
                    isp_cfg.module_cfg_update &= ~ISP2X_MODULE_DHAZ;
                }
                break;
            }
        }
    }
    updateIspModuleForceEns(isp_cfg.module_ens);
}
#endif

void
Isp20Params::hdrtmoGetLumaInfo(rk_aiq_luma_params_t * Next, rk_aiq_luma_params_t *Cur,
                               s32 frameNum, int PixelNumBlock, float blc, float *luma)
{
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "PixelNumBlock:%d blc:%f\n", PixelNumBlock, blc);

    float nextSLuma[16] ;
    float curSLuma[16] ;
    float nextMLuma[16] ;
    float curMLuma[16] ;
    float nextLLuma[16];
    float curLLuma[16];

    if (frameNum == 1)
    {
        for(int i = 0; i < ISP2X_MIPI_LUMA_MEAN_MAX; i++) {
            nextLLuma[i] = 0;
            curLLuma[i] = 0;
            nextMLuma[i] = 0;
            curMLuma[i] = 0;
            nextSLuma[i] = (float)Next->luma[0][i];
            nextSLuma[i] /= (float)PixelNumBlock;
            nextSLuma[i] -= blc;
            curSLuma[i] = (float)Cur->luma[0][i];
            curSLuma[i] /= (float)PixelNumBlock;
            curSLuma[i] -= blc;
        }
    } else if (frameNum == 2) {
        for(int i = 0; i < ISP2X_MIPI_LUMA_MEAN_MAX; i++) {
            nextSLuma[i] = (float)Next->luma[1][i];
            nextSLuma[i] /= (float)PixelNumBlock;
            nextSLuma[i] -= blc;
            curSLuma[i] = (float)Cur->luma[1][i];
            curSLuma[i] /= (float)PixelNumBlock;
            curSLuma[i] -= blc;
            nextMLuma[i] = 0;
            curMLuma[i] = 0;
            nextLLuma[i] = (float)Next->luma[0][i];
            nextLLuma[i] /= (float)PixelNumBlock;
            nextLLuma[i] -= blc;
            curLLuma[i] = (float)Cur->luma[0][i];
            curLLuma[i] /= (float)PixelNumBlock;
            curLLuma[i] -= blc;
        }
    } else if (frameNum == 3) {

        for(int i = 0; i < ISP2X_MIPI_LUMA_MEAN_MAX; i++) {
            nextSLuma[i] = (float)Next->luma[2][i];
            nextSLuma[i] /= (float)PixelNumBlock;
            nextSLuma[i] -= blc;
            curSLuma[i] = (float)Cur->luma[2][i];
            curSLuma[i] /= (float)PixelNumBlock;
            curSLuma[i] -= blc;
            nextMLuma[i] = (float)Next->luma[1][i];
            nextMLuma[i] /= (float)PixelNumBlock;
            nextMLuma[i] -= blc;
            curMLuma[i] = (float)Cur->luma[1][i];
            curMLuma[i] /= (float)PixelNumBlock;
            curMLuma[i] -= blc;
            nextLLuma[i] = (float)Next->luma[0][i];
            nextLLuma[i] /= (float)PixelNumBlock;
            nextLLuma[i] -= blc;
            curLLuma[i] = (float)Cur->luma[0][i];
            curLLuma[i] /= (float)PixelNumBlock;
            curLLuma[i] -= blc;
        }
    }

    for(int i = 0; i < ISP2X_MIPI_LUMA_MEAN_MAX; i++) {
        luma[i] = curSLuma[i];
        luma[i + 16] = curMLuma[i];
        luma[i + 32] = curLLuma[i];
        luma[i + 48] = nextSLuma[i];
        luma[i + 64] = nextMLuma[i];
        luma[i + 80] = nextLLuma[i];
    }
}

void
Isp20Params::hdrtmoGetAeInfo(RKAiqAecExpInfo_t* Next, RKAiqAecExpInfo_t* Cur, s32 frameNum, float* expo)
{
    float nextLExpo = 0;
    float curLExpo = 0;
    float nextMExpo = 0;
    float curMExpo = 0;
    float nextSExpo = 0;
    float curSExpo = 0;

    if (frameNum == 1)
    {
        nextLExpo = 0;
        curLExpo = 0;
        nextMExpo = 0;
        curMExpo = 0;
        nextSExpo = Next->LinearExp.exp_real_params.analog_gain * \
                    Next->LinearExp.exp_real_params.integration_time;
        curSExpo = Cur->LinearExp.exp_real_params.analog_gain * \
                   Cur->LinearExp.exp_real_params.integration_time;
    } else if (frameNum == 2) {
        nextLExpo = Next->HdrExp[1].exp_real_params.analog_gain * \
                    Next->HdrExp[1].exp_real_params.integration_time;
        curLExpo = Cur->HdrExp[1].exp_real_params.analog_gain * \
                   Cur->HdrExp[1].exp_real_params.integration_time;
        nextMExpo = nextLExpo;
        curMExpo = curLExpo;
        nextSExpo = Next->HdrExp[0].exp_real_params.analog_gain * \
                    Next->HdrExp[0].exp_real_params.integration_time;
        curSExpo = Cur->HdrExp[0].exp_real_params.analog_gain * \
                   Cur->HdrExp[0].exp_real_params.integration_time;
    } else if (frameNum == 3) {
        nextLExpo = Next->HdrExp[2].exp_real_params.analog_gain * \
                    Next->HdrExp[2].exp_real_params.integration_time;
        curLExpo = Cur->HdrExp[2].exp_real_params.analog_gain * \
                   Cur->HdrExp[2].exp_real_params.integration_time;
        nextMExpo = Next->HdrExp[1].exp_real_params.analog_gain * \
                    Next->HdrExp[1].exp_real_params.integration_time;
        curMExpo = Cur->HdrExp[1].exp_real_params.analog_gain * \
                   Cur->HdrExp[1].exp_real_params.integration_time;
        nextSExpo = Next->HdrExp[0].exp_real_params.analog_gain * \
                    Next->HdrExp[0].exp_real_params.integration_time;
        curSExpo = Cur->HdrExp[0].exp_real_params.analog_gain * \
                   Cur->HdrExp[0].exp_real_params.integration_time;
    }

    expo[0] = curSExpo;
    expo[1] = curMExpo;
    expo[2] = curLExpo;
    expo[3] = nextSExpo;
    expo[4] = nextMExpo;
    expo[5] = nextLExpo;

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "Cur Expo: S:%f M:%f L:%f\n", curSExpo, curMExpo, curLExpo);
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "Next Expo: S:%f M:%f L:%f\n", nextSExpo, nextMExpo, nextLExpo);

}

bool
Isp20Params::hdrtmoSceneStable(sint32_t frameId, int IIRMAX, int IIR, int SetWeight, s32 frameNum, float *LumaDeviation, float StableThr)
{
    bool SceneStable = true;
    float LumaDeviationL = 0;
    float LumaDeviationM = 0;
    float LumaDeviationS = 0;
    float LumaDeviationLinear = 0;
    float LumaDeviationFinnal = 0;

    //set default value when secne change or flow restart
    if(AntiTmoFlicker.preFrameNum != frameNum || frameId == 0) {
        AntiTmoFlicker.preFrameNum = 0;
        AntiTmoFlicker.FirstChange = false;
        AntiTmoFlicker.FirstChangeNum = 0;
        AntiTmoFlicker.FirstChangeDone = false;
        AntiTmoFlicker.FirstChangeDoneNum = 0;
    }

    //get LumaDeviationFinnal value
    if(frameNum == 1) {
        LumaDeviationLinear = LumaDeviation[0];
        LumaDeviationFinnal = LumaDeviationLinear;
    }
    else if(frameNum == 2) {
        LumaDeviationS = LumaDeviation[0];
        LumaDeviationL = LumaDeviation[1];

        if(LumaDeviationL > 0)
            LumaDeviationFinnal = LumaDeviationL;
        else if(LumaDeviationL == 0 && LumaDeviationS > 0)
            LumaDeviationFinnal = LumaDeviationS;

    }
    else if(frameNum == 3) {
        LumaDeviationS = LumaDeviation[0];
        LumaDeviationM = LumaDeviation[1];
        LumaDeviationL = LumaDeviation[2];

        if(LumaDeviationM > 0)
            LumaDeviationFinnal = LumaDeviationM;
        else if(LumaDeviationM == 0 && LumaDeviationL > 0)
            LumaDeviationFinnal = LumaDeviationL;
        else if(LumaDeviationM == 0 && LumaDeviationL == 0 && LumaDeviationS == 0)
            LumaDeviationFinnal = LumaDeviationS;

    }
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "frameId:%ld LumaDeviationLinear:%f LumaDeviationS:%f LumaDeviationM:%f LumaDeviationL:%f\n",
                    frameId, LumaDeviationLinear, LumaDeviationS, LumaDeviationM, LumaDeviationL);

    //skip first N frame for starting
    if(AntiTmoFlicker.FirstChange == false) {
        if(LumaDeviationFinnal) {
            AntiTmoFlicker.FirstChange = true;
            AntiTmoFlicker.FirstChangeNum = frameId;
        }
    }

    if(AntiTmoFlicker.FirstChangeDone == false && AntiTmoFlicker.FirstChange == true) {
        if(LumaDeviationFinnal == 0) {
            AntiTmoFlicker.FirstChangeDone = true;
            AntiTmoFlicker.FirstChangeDoneNum = frameId;
        }
    }

    //detect stable
    if(AntiTmoFlicker.FirstChangeDoneNum && AntiTmoFlicker.FirstChangeNum) {
        if(LumaDeviationFinnal <= StableThr)
            SceneStable = true;
        else
            SceneStable = false;
    }
    else
        SceneStable = true;

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "preFrameNum:%d frameNum:%d FirstChange:%d FirstChangeNum:%d FirstChangeDone:%d FirstChangeDoneNum:%d\n",
                    AntiTmoFlicker.preFrameNum, frameNum, AntiTmoFlicker.FirstChange, AntiTmoFlicker.FirstChangeNum,
                    AntiTmoFlicker.FirstChangeDone, AntiTmoFlicker.FirstChangeDoneNum);
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "LumaDeviationFinnal:%f StableThr:%f SceneStable:%d \n", LumaDeviationFinnal, StableThr, SceneStable);

    //store framrnum
    AntiTmoFlicker.preFrameNum = frameNum;

    return SceneStable;
}

s32
Isp20Params::hdrtmoPredictK(float* luma, float* expo, s32 frameNum, PredictKPara_t *TmoPara)
{
    int PredictK = 0;
    float PredictKfloat = 0;

    float curSExpo = expo[0];
    float curMExpo = expo[1];
    float curLExpo = expo[2];
    float nextSExpo = expo[3];
    float nextMExpo = expo[4];
    float nextLExpo = expo[5];

    float nextLLuma[16];
    float curLLuma[16];
    float nextSLuma[16];
    float curSLuma[16];
    float nextMLuma[16];
    float curMLuma[16];

    for(int i = 0; i < ISP2X_MIPI_LUMA_MEAN_MAX; i++)
    {
        curSLuma[i] = luma[i];
        curMLuma[i] = luma[i + 16];
        curLLuma[i] = luma[i + 32];
        nextSLuma[i] = luma[i + 48];
        nextMLuma[i] = luma[i + 64];
        nextLLuma[i] = luma[i + 80];
    }

    float correction_factor = TmoPara->correction_factor;
    float ratio = 1;
    float offset = TmoPara->correction_offset;
    float LongExpoRatio = 1;
    float ShortExpoRatio = 1;
    float MiddleExpoRatio = 1;
    float MiddleLumaChange = 1;
    float LongLumaChange = 1;
    float ShortLumaChange = 1;
    float EnvLvChange = 0;

    //get expo change
    if(frameNum == 3 || frameNum == 2) {
        if(nextLExpo != 0 && curLExpo != 0)
            LongExpoRatio = nextLExpo / curLExpo;
        else
            LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "Wrong Long frame expo!!!");
    }

    if(frameNum == 3) {
        if(nextMExpo != 0 && curMExpo != 0)
            ShortExpoRatio = nextMExpo / curMExpo;
        else
            LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "Wrong Short frame expo!!!");
    }

    if(nextSExpo != 0 && curSExpo != 0)
        ShortExpoRatio = nextSExpo / curSExpo;
    else
        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "Wrong Short frame expo!!!");

    float nextLMeanLuma = 0;
    float curLMeanLuma = 0;
    float curMMeanLuma = 0;
    float nextMMeanLuma = 0;
    float nextSMeanLuma = 0;
    float curSMeanLuma = 0;
    for(int i = 0; i < ISP2X_MIPI_LUMA_MEAN_MAX; i++)
    {
        nextLMeanLuma += nextLLuma[i];
        curLMeanLuma += curLLuma[i];
        nextMMeanLuma += nextMLuma[i];
        curMMeanLuma += curMLuma[i];
        nextSMeanLuma += nextSLuma[i];
        curSMeanLuma += curSLuma[i];
    }
    nextLMeanLuma /= ISP2X_MIPI_LUMA_MEAN_MAX;
    curLMeanLuma /= ISP2X_MIPI_LUMA_MEAN_MAX;
    nextMMeanLuma /= ISP2X_MIPI_LUMA_MEAN_MAX;
    curMMeanLuma /= ISP2X_MIPI_LUMA_MEAN_MAX;
    nextSMeanLuma /= ISP2X_MIPI_LUMA_MEAN_MAX;
    curSMeanLuma /= ISP2X_MIPI_LUMA_MEAN_MAX;

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "nextLLuma:%f curLLuma:%f\n", nextLMeanLuma, curLMeanLuma);
    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "nextSLuma:%f curSLuma:%f\n", nextSMeanLuma, curSMeanLuma);

    //get luma change
    if(frameNum == 3 || frameNum == 2) {
        if(nextLMeanLuma > 0 && curLMeanLuma > 0)
            LongLumaChange = nextLMeanLuma / curLMeanLuma;
        else if(nextLMeanLuma <= 0 && curLMeanLuma > 0)
        {
            nextLMeanLuma = 1;
            LongLumaChange = nextLMeanLuma / curLMeanLuma;
        }
        else if(nextLMeanLuma > 0 && curLMeanLuma <= 0)
        {
            curLMeanLuma = 1;
            LongLumaChange = nextLMeanLuma / curLMeanLuma;
        }
        else {
            curLMeanLuma = 1;
            nextLMeanLuma = 1;
            LongLumaChange = nextLMeanLuma / curLMeanLuma;
        }
    }

    if(frameNum == 3) {
        if(nextMMeanLuma > 0 && curMMeanLuma > 0)
            MiddleLumaChange = nextMMeanLuma / curMMeanLuma;
        else if(nextMMeanLuma <= 0 && curMMeanLuma > 0)
        {
            nextMMeanLuma = 1;
            MiddleLumaChange = nextMMeanLuma / curMMeanLuma;
        }
        else if(nextMMeanLuma > 0 && curMMeanLuma <= 0)
        {
            curMMeanLuma = 1;
            MiddleLumaChange = nextMMeanLuma / curMMeanLuma;
        }
        else {
            curMMeanLuma = 1;
            nextMMeanLuma = 1;
            MiddleLumaChange = nextMMeanLuma / curMMeanLuma;
        }
    }

    if(nextSMeanLuma > 0 && curSMeanLuma > 0)
        ShortLumaChange = nextSMeanLuma / curSMeanLuma;
    else if(nextSMeanLuma <= 0 && curSMeanLuma > 0)
    {
        nextSMeanLuma = 1;
        ShortLumaChange = nextSMeanLuma / curSMeanLuma;
    }
    else if(nextSMeanLuma > 0 && curSMeanLuma <= 0)
    {
        curSMeanLuma = 1;
        ShortLumaChange = nextSMeanLuma / curSMeanLuma;
    }
    else {
        curSMeanLuma = 1;
        nextSMeanLuma = 1;
        ShortLumaChange = nextSMeanLuma / curSMeanLuma;
    }

    //cal predictK
    if (frameNum == 1)
    {
        LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "nextLuma:%f curLuma:%f LumaChange:%f\n", nextSMeanLuma, curSMeanLuma, ShortLumaChange);
        ratio = ShortLumaChange;

        EnvLvChange = nextSMeanLuma / nextSExpo - curSMeanLuma / curSExpo;
        EnvLvChange = EnvLvChange >= 0 ? EnvLvChange : (-EnvLvChange);
        EnvLvChange /= curSMeanLuma / curSExpo;
        LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "nextEnvLv:%f curEnvLv:%f EnvLvChange:%f\n", nextSMeanLuma / nextSExpo,
                        curSMeanLuma / curSExpo, EnvLvChange);
    }
    else if (frameNum == 2)
    {
        LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "nextLLuma:%f curLLuma:%f LongLumaChange:%f\n", nextLMeanLuma, curLMeanLuma, LongLumaChange);
        LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "nextSLuma:%f curSLuma:%f ShortLumaChange:%f\n", nextSMeanLuma, curSMeanLuma, ShortLumaChange);
        LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "LongPercent:%f UseLongLowTh:%f UseLongUpTh:%f\n", 1, TmoPara->UseLongLowTh, TmoPara->UseLongUpTh);

        if(LongLumaChange > TmoPara->UseLongLowTh || LongLumaChange < TmoPara->UseLongUpTh)
            ratio = LongLumaChange;
        else
            ratio = ShortLumaChange;

        EnvLvChange = nextLMeanLuma / nextLExpo - curLMeanLuma / curLExpo;
        EnvLvChange = EnvLvChange >= 0 ? EnvLvChange : (-EnvLvChange);
        EnvLvChange /= curLMeanLuma / curLExpo;
        LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "nextEnvLv:%f curEnvLv:%f EnvLvChange:%f\n", nextLMeanLuma / nextLExpo,
                        curLMeanLuma / curLExpo, EnvLvChange);

    }
    else if (frameNum == 3)
    {
        LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "nextLLuma:%f curLLuma:%f LongLumaChange:%f\n", nextLMeanLuma, curLMeanLuma, LongLumaChange);
        LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "nextMLuma:%f curMLuma:%f MiddleLumaChange:%f\n", nextMMeanLuma, curMMeanLuma, MiddleLumaChange);
        LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "nextSLuma:%f curSLuma:%f ShortLumaChange:%f\n", nextSMeanLuma, curSMeanLuma, ShortLumaChange);
        LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "LongPercent:%f UseLongLowTh:%f UseLongUpTh:%f\n", TmoPara->Hdr3xLongPercent,
                        TmoPara->UseLongLowTh, TmoPara->UseLongUpTh);

        float LongLumaChangeNew = TmoPara->Hdr3xLongPercent * LongLumaChange + (1 - TmoPara->Hdr3xLongPercent) * MiddleLumaChange;
        if(LongLumaChangeNew > TmoPara->UseLongLowTh || LongLumaChangeNew < TmoPara->UseLongUpTh)
            ratio = LongLumaChangeNew;
        else
            ratio = ShortLumaChange;

        EnvLvChange = nextMMeanLuma / nextMExpo - curMMeanLuma / curMExpo;
        EnvLvChange = EnvLvChange >= 0 ? EnvLvChange : (-EnvLvChange);
        EnvLvChange /= curMMeanLuma / curMExpo;
        LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "nextEnvLv:%f curEnvLv:%f EnvLvChange:%f\n", nextMMeanLuma / nextMExpo,
                        curMMeanLuma / curMExpo, EnvLvChange);
    }

    if(ratio >= 1)
        PredictKfloat = log(correction_factor * ratio + offset) / log(2);
    else if(ratio < 1 && ratio > 0)
    {
        float tmp = ratio / correction_factor - offset;
        tmp = tmp >= 1 ? 1 : tmp <= 0 ? 0.00001 : tmp;
        PredictKfloat = log(tmp) / log(2);
    }
    else
        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "Wrong luma change!!!");

    //add EnvLv judge
    if(EnvLvChange > 0.005) {
        float tmp = curLMeanLuma - nextLMeanLuma;
        tmp = tmp >= 0 ? tmp : (-tmp);
        if(tmp < 1)
            PredictKfloat = 0;
    }
    else
        PredictKfloat = 0;

    PredictKfloat *= 2048;
    PredictK = (int)PredictKfloat;

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "ratio:%f EnvLvChange:%f PredictKfloat:%f PredictK:%d\n",
                    ratio, EnvLvChange, PredictKfloat, PredictK);
    return PredictK;
}

bool Isp20Params::convert3aResultsToIspCfg(SmartPtr<cam3aResult> &result,
        void* isp_cfg_p)
{
    struct isp2x_isp_params_cfg& isp_cfg = *(struct isp2x_isp_params_cfg*)isp_cfg_p;

    if (result.ptr() == NULL) {
        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "3A result empty");
        return false;
    }

    int32_t type = result->getType();
    // LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%s, module (0x%x) convert params!\n", __FUNCTION__, type);
    switch (type)
    {
    case RESULT_TYPE_AEC_PARAM:
    {
        SmartPtr<RkAiqIspAecParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspAecParamsProxy>();
        if (params.ptr()) {
            convertAiqAeToIsp20Params(isp_cfg, params->data()->result);
        }
    }
    break;
    case RESULT_TYPE_HIST_PARAM:
    {
        SmartPtr<RkAiqIspHistParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspHistParamsProxy>();
        if (params.ptr())
            convertAiqHistToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    case RESULT_TYPE_AWB_PARAM:
    {
        SmartPtr<RkAiqIspAwbParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspAwbParamsProxy>();
        if (params.ptr())
            convertAiqAwbToIsp20Params(isp_cfg, params->data()->result, true);
    }
    break;
    case RESULT_TYPE_AWBGAIN_PARAM:
    {
        SmartPtr<RkAiqIspAwbGainParamsProxy> awb_gain = result.dynamic_cast_ptr<RkAiqIspAwbGainParamsProxy>();
        if (awb_gain.ptr() && mBlcResult.ptr()) {
            SmartPtr<RkAiqIspBlcParamsProxy> blc = mBlcResult.dynamic_cast_ptr<RkAiqIspBlcParamsProxy>();
            convertAiqAwbGainToIsp20Params(isp_cfg,
                                           awb_gain->data()->result, blc->data()->result, true);

        } else
            LOGE("don't get %s params, convert awbgain params failed!",
                 awb_gain.ptr() ? "blc" : "awb_gain");
    }
    break;
    case RESULT_TYPE_AF_PARAM:
    {
        SmartPtr<RkAiqIspAfParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspAfParamsProxy>();
        if (params.ptr())
            convertAiqAfToIsp20Params(isp_cfg, params->data()->result, true);
    }
    break;
    case RESULT_TYPE_DPCC_PARAM:
    {
        SmartPtr<RkAiqIspDpccParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspDpccParamsProxy>();
        if (params.ptr())
            convertAiqDpccToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    case RESULT_TYPE_MERGE_PARAM:
    {
        SmartPtr<RkAiqIspMergeParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspMergeParamsProxy>();
        if (params.ptr()) {
            convertAiqMergeToIsp20Params(isp_cfg, params->data()->result);
        }
    }
    break;
    case RESULT_TYPE_TMO_PARAM:
    {
        SmartPtr<RkAiqIspTmoParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspTmoParamsProxy>();
        if (params.ptr()) {
            convertAiqTmoToIsp20Params(isp_cfg, params->data()->result);
        }
    }
    break;
    case RESULT_TYPE_CCM_PARAM:
    {
        SmartPtr<RkAiqIspCcmParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspCcmParamsProxy>();
        if (params.ptr())
            convertAiqCcmToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    case RESULT_TYPE_LSC_PARAM:
    {
        SmartPtr<RkAiqIspLscParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspLscParamsProxy>();
        if (params.ptr())
            convertAiqLscToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    case RESULT_TYPE_BLC_PARAM:
    {
        SmartPtr<RkAiqIspBlcParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspBlcParamsProxy>();
        if (params.ptr())
            convertAiqBlcToIsp20Params(isp_cfg, params->data()->result);
    }
    case RESULT_TYPE_RAWNR_PARAM:
    {
        SmartPtr<RkAiqIspRawnrParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspRawnrParamsProxy>();
        if (params.ptr())
            convertAiqRawnrToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    case RESULT_TYPE_GIC_PARAM:
    {
        SmartPtr<RkAiqIspGicParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspGicParamsProxy>();
        if (params.ptr())
            convertAiqGicToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    case RESULT_TYPE_DEBAYER_PARAM:
    {
        SmartPtr<RkAiqIspDebayerParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspDebayerParamsProxy>();
        if (params.ptr())
            convertAiqAdemosaicToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    case RESULT_TYPE_LDCH_PARAM:
    {
        SmartPtr<RkAiqIspLdchParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspLdchParamsProxy>();
        if (params.ptr())
            convertAiqAldchToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    case RESULT_TYPE_LUT3D_PARAM:
    {
        SmartPtr<RkAiqIspLut3dParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspLut3dParamsProxy>();
        if (params.ptr())
            convertAiqA3dlutToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    case RESULT_TYPE_DEHAZE_PARAM:
    {
        SmartPtr<RkAiqIspDehazeParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspDehazeParamsProxy>();
        if (params.ptr())
            convertAiqAdehazeToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    case RESULT_TYPE_AGAMMA_PARAM:
    {
        SmartPtr<RkAiqIspAgammaParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspAgammaParamsProxy>();
        if (params.ptr())
            convertAiqAgammaToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    case RESULT_TYPE_ADEGAMMA_PARAM:
    {
        SmartPtr<RkAiqIspAdegammaParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspAdegammaParamsProxy>();
        if (params.ptr())
            convertAiqAdegammaToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    case RESULT_TYPE_WDR_PARAM:
#if 0
    {
        SmartPtr<RkAiqIspWdrParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspWdrParamsProxy>();
        if (params.ptr())
            convertAiqWdrToIsp20Params(isp_cfg, params->data()->result);
    }
#endif
    break;
    case RESULT_TYPE_CSM_PARAM:
#if 0
    {
        SmartPtr<RkAiqIspCsmParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspCsmParamsProxy>();
        if (params.ptr())
            convertAiqToIsp20Params(isp_cfg, params->data()->result);
    }
#endif
    break;
    case RESULT_TYPE_CGC_PARAM:
        break;
    case RESULT_TYPE_CONV422_PARAM:
        break;
    case RESULT_TYPE_YUVCONV_PARAM:
        break;
    case RESULT_TYPE_GAIN_PARAM:
    {
        SmartPtr<RkAiqIspGainParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspGainParamsProxy>();
        if (params.ptr())
            convertAiqGainToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    case RESULT_TYPE_CP_PARAM:
    {
        SmartPtr<RkAiqIspCpParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspCpParamsProxy>();
        if (params.ptr())
            convertAiqCpToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    case RESULT_TYPE_IE_PARAM:
    {
        SmartPtr<RkAiqIspIeParamsProxy> params = result.dynamic_cast_ptr<RkAiqIspIeParamsProxy>();
        if (params.ptr())
            convertAiqIeToIsp20Params(isp_cfg, params->data()->result);
    }
    break;
    default:
        LOGE("unknown param type: 0x%x!", type);
        return false;
    }

    /*
     * cam3aResultList &list = _cam3aConfig[result->getFrameId()];
     * list.push_back(result);
     */

    return true;
}

XCamReturn Isp20Params::merge_isp_results(cam3aResultList &results, void* isp_cfg)
{
    if (results.empty())
        return XCAM_RETURN_ERROR_PARAM;

    mBlcResult = get_3a_result(results, RESULT_TYPE_BLC_PARAM);
    if (!mBlcResult.ptr())
        LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "get blc params failed!\n");

    LOGD_CAMHW_SUBM(ISP20PARAM_SUBM, "%s, isp cam3a results size: %d\n", __FUNCTION__, results.size());
    for (cam3aResultList::iterator iter = results.begin ();
            iter != results.end (); iter++)
    {
        SmartPtr<cam3aResult> &cam3a_result = *iter;

        convert3aResultsToIspCfg(cam3a_result, isp_cfg);
    }
    results.clear();
    mBlcResult.release();
    return XCAM_RETURN_NO_ERROR;
}

template<>
XCamReturn Isp20Params::merge_results<struct rkispp_params_nrcfg>(cam3aResultList &results, struct rkispp_params_nrcfg &pp_cfg)
{
    if (results.empty())
        return XCAM_RETURN_ERROR_PARAM;

    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%s, pp cam3a results size: %d\n", __FUNCTION__, results.size());

    SmartPtr<RkAiqIspSharpenParamsProxy> sharpen = nullptr;
    SmartPtr<RkAiqIspEdgefltParamsProxy> edgeflt = nullptr;

    for (cam3aResultList::iterator iter = results.begin ();
            iter != results.end ();)
    {
        SmartPtr<cam3aResult> &cam3a_result = *iter;

        if (cam3a_result->getType() == RESULT_TYPE_SHARPEN_PARAM || \
                cam3a_result->getType() == RESULT_TYPE_EDGEFLT_PARAM) {
            if (cam3a_result->getType() == RESULT_TYPE_SHARPEN_PARAM)
                sharpen = cam3a_result.dynamic_cast_ptr<RkAiqIspSharpenParamsProxy>();
            else if (cam3a_result->getType() == RESULT_TYPE_EDGEFLT_PARAM)
                edgeflt = cam3a_result.dynamic_cast_ptr<RkAiqIspEdgefltParamsProxy>();
            if (sharpen.ptr() && edgeflt.ptr())
                convertAiqSharpenToIsp20Params(pp_cfg, sharpen->data()->result, edgeflt->data()->result);

            iter = results.erase (iter);
            continue;
        }
        if (cam3a_result->getType() == RESULT_TYPE_UVNR_PARAM) {
            SmartPtr<RkAiqIspUvnrParamsProxy> uvnr = cam3a_result.dynamic_cast_ptr<RkAiqIspUvnrParamsProxy>();
            convertAiqUvnrToIsp20Params(pp_cfg, uvnr->data()->result);
            iter = results.erase (iter);
            continue;
        }
        if (cam3a_result->getType() == RESULT_TYPE_YNR_PARAM) {
            SmartPtr<RkAiqIspYnrParamsProxy> ynr = cam3a_result.dynamic_cast_ptr<RkAiqIspYnrParamsProxy>();
            convertAiqYnrToIsp20Params(pp_cfg, ynr->data()->result);
            iter = results.erase (iter);
            continue;
        }
        if (cam3a_result->getType() == RESULT_TYPE_ORB_PARAM) {
            SmartPtr<RkAiqIspOrbParamsProxy> orb = cam3a_result.dynamic_cast_ptr<RkAiqIspOrbParamsProxy>();
            convertAiqOrbToIsp20Params(pp_cfg, orb->data()->result);
            iter = results.erase (iter);
            continue;
        }
        ++iter;
    }
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn Isp20Params::get_tnr_cfg_params(cam3aResultList &results, struct rkispp_params_tnrcfg &tnr_cfg)
{
    if (results.empty())
        return XCAM_RETURN_ERROR_PARAM;

    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%s, pp cam3a results size: %d\n", __FUNCTION__, results.size());
    SmartPtr<cam3aResult> cam3a_result = get_3a_result(results, RESULT_TYPE_TNR_PARAM);
    if (cam3a_result.ptr()) {
        SmartPtr<RkAiqIspTnrParamsProxy> tnr = nullptr;
        tnr = cam3a_result.dynamic_cast_ptr<RkAiqIspTnrParamsProxy>();
        if (tnr.ptr())
            convertAiqTnrToIsp20Params(tnr_cfg, tnr->data()->result);
    }
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn Isp20Params::get_fec_cfg_params(cam3aResultList &results, struct rkispp_params_feccfg &fec_cfg)
{
    if (results.empty())
        return XCAM_RETURN_ERROR_PARAM;

    LOGE_CAMHW_SUBM(ISP20PARAM_SUBM, "%s, pp cam3a results size: %d\n", __FUNCTION__, results.size());
    SmartPtr<cam3aResult> cam3a_result = get_3a_result(results, RESULT_TYPE_FEC_PARAM);
    if (cam3a_result.ptr()) {
        SmartPtr<RkAiqIspFecParamsProxy> fec = nullptr;
        fec = cam3a_result.dynamic_cast_ptr<RkAiqIspFecParamsProxy>();
        if (fec.ptr()) {
            convertAiqFecToIsp20Params(fec_cfg, fec->data()->result);
        }
    }
    return XCAM_RETURN_NO_ERROR;
}

SmartPtr<cam3aResult>
Isp20Params::get_3a_result (cam3aResultList &results, int32_t type)
{
    cam3aResultList::iterator i_res = results.begin();
    SmartPtr<cam3aResult> res;

    for ( ; i_res !=  results.end(); ++i_res) {
        if (type == (*i_res)->getType ()) {
            res = (*i_res);
            break;
        }
    }

    return res;
}

}; //namspace RkCam
//TODO: to solve template ld compile issue, add isp21 source file here now.
#include "isp21/Isp21Params.cpp"
