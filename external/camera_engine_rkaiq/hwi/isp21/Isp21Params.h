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

#ifndef _CAM_HW_ISP21_PARAMS_H_
#define _CAM_HW_ISP21_PARAMS_H_

#include "rk_aiq_pool.h"
#include "isp20/Isp20Params.h"
#include "rkisp21-config.h"

namespace RkCam {

class Isp21Params : public Isp20Params {
public:
    explicit Isp21Params() : Isp20Params() {};
    virtual ~Isp21Params() {};
protected:
    virtual bool convert3aResultsToIspCfg(SmartPtr<cam3aResult> &result, void* isp_cfg_p);
private:
    XCAM_DEAD_COPY(Isp21Params);
    template<class T>
    void convertAiqAwbGainToIsp21Params(T& isp_cfg,
                                        const rk_aiq_wb_gain_t& awb_gain, const rk_aiq_isp_blc_v21_t &blc,
                                        bool awb_gain_update);
    void convertAiqBlcToIsp21Params(struct isp21_isp_params_cfg& isp_cfg,
                                    rk_aiq_isp_blc_v21_t &blc);
    void convertAiqAdehazeToIsp21Params(struct isp21_isp_params_cfg& isp_cfg,
                                        const rk_aiq_isp_dehaze_v21_t& dhaze);
    void convertAiqCcmToIsp21Params(struct isp21_isp_params_cfg& isp_cfg,
                                    const rk_aiq_ccm_cfg_t& ccm);
    void convertAiqAgicToIsp21Params(struct isp21_isp_params_cfg& isp_cfg,
                                    const rk_aiq_isp_gic_v21_t& gic_cfg);
    void convertAiqAwbToIsp21Params(struct isp21_isp_params_cfg& isp_cfg,
                                    const rk_aiq_awb_stat_cfg_v201_t& awb_meas,
                                    bool awb_cfg_udpate);
    void convertAiqRawnrToIsp21Params(struct isp21_isp_params_cfg& isp_cfg,
                                      rk_aiq_isp_baynr_v21_t& rawnr);
    void convertAiqTnrToIsp21Params(struct isp21_isp_params_cfg& isp_cfg,
                                    rk_aiq_isp_bay3d_v21_t& tnr);
    void convertAiqUvnrToIsp21Params(struct isp21_isp_params_cfg& isp_cfg,
                                     rk_aiq_isp_cnr_v21_t& uvnr);
    void convertAiqYnrToIsp21Params(struct isp21_isp_params_cfg& isp_cfg,
                                    rk_aiq_isp_ynr_v21_t& ynr);
    void convertAiqSharpenToIsp21Params(struct isp21_isp_params_cfg& isp_cfg,
                                        rk_aiq_isp_sharp_v21_t& sharp);
    void convertAiqDrcToIsp21Params(struct isp21_isp_params_cfg& isp_cfg,
                                    rk_aiq_isp_drc_v21_t& adrc_data);
};

};
#endif
