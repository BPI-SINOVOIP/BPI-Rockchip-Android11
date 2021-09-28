/*
* rk_aiq_awb_algo_v201.h

* for rockchip v2.0.0
*
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
/* for rockchip v2.0.0*/
#ifndef __RK_AIQ_AWB_ALGO_V201_H__
#define __RK_AIQ_AWB_ALGO_V201_H__

#include "rk_aiq_types_awb_stat_v201.h"
#include "rk_aiq_types_awb_algo_prvt.h"

/*
* v1.0.0
* -add pre_wbgain cfg & sync to cmodel v2.49 for awbV201
* -fix the bug cause by time share
*/
XCamReturn AwbInitV201(awb_contex_t** para, const CamCalibDbV2Context_t* calib);
XCamReturn AwbPrepareV201(awb_contex_t *par);
XCamReturn AwbReconfigV201(awb_contex_t *para);
XCamReturn AwbPreProcV201(rk_aiq_awb_stat_res_v201_t awb_measure_result, awb_contex_t* para);
XCamReturn AwbProcessingV201(awb_contex_t* para);
XCamReturn AwbReleaseV201(awb_contex_t* para);

#endif
