/*
* rk_aiq_types_camgroup_aynr_algo_prvt.h

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

#ifndef __RK_AIQ_CAMGROUP_AYNR_ALGO_PRVT_H__
#define __RK_AIQ_CAMGROUP_AYNR_ALGO_PRVT_H__

#include "aynr2/rk_aiq_types_aynr_algo_prvt_v2.h"
#include "aynr3/rk_aiq_types_aynr_algo_prvt_v3.h"

typedef enum CalibDbV2_CamGroup_Aynr_Method_e {
    CalibDbV2_CAMGROUP_AYNR_METHOD_MIN = 0,
    CalibDbV2_CAMGROUP_AYNR_METHOD_MEAN,/*config each camera with same para, para is got by mean stas of all cameras*/
    CalibDbV2_CAMGROUP_AYNR_METHOD_MAX,   /*config each camera with diff para, each para is got by stats of tha camera*/
} CalibDbV2_CamGroup_Aynr_Method_t;


typedef struct CalibDbV2_CamGroup_Aynr_s {
    CalibDbV2_CamGroup_Aynr_Method_t  groupMethod;
    // add more para for surround view
} CalibDbV2_CamGroup_Aynr_t;

typedef enum aynr_hardware_version_e
{
    AYNR_HARDWARE_MIN = 0,
    AYNR_HARDWARE_V1,
    AYNR_HARDWARE_V2,
    AYNR_HARDWARE_V3,
    AYNR_HARDWARE_MAX
} aynr_hardware_version_t;


typedef struct CamGroup_Aynr_Contex_s {
    union {
        Aynr_Context_V2_t* aynr_contex_v2;
        Aynr_Context_V3_t* aynr_contex_v3;
    };
    CalibDbV2_CamGroup_Aynr_t group_CalibV2;
    int camera_Num;

} CamGroup_Aynr_Contex_t;



#endif

