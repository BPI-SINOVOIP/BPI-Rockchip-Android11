/*
 *rk_aiq_types_alsc_algo_prvt.h
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

#ifndef _RK_AIQ_TYPES_ALSC_ALGO_PRVT_H_
#define _RK_AIQ_TYPES_ALSC_ALGO_PRVT_H_

#include "rk_aiq_types.h"
#include "rk_aiq_types_alsc_algo_int.h"
#include "alsc_head.h"
#include "xcam_log.h"
#include "xcam_common.h"
#include "list.h"
#include "RkAiqCalibDbV2Helper.h"

RKAIQ_BEGIN_DECLARE
typedef const CalibDbV2_LscTableProfile_t* pLscTableProfile_t;

typedef struct lsc_matrix
{
    Cam17x17UShortMatrix_t  LscMatrix[CAM_4CH_COLOR_COMPONENT_MAX];
} lsc_matrix_t;

/** @brief store LSC recent/last results*/
typedef struct alsc_rest_s {
    uint32_t caseIndex;
    float fVignetting;
    List dominateIlluList;//to record domain illuminant
    int estimateIlluCaseIdx;
    uint32_t resIdx;
    pLscTableProfile_t pLscProfile1;
    pLscTableProfile_t pLscProfile2;
    lsc_matrix_t undampedLscMatrixTable;
    lsc_matrix_t dampedLscMatrixTable;
} alsc_rest_t;

typedef struct illu_node_s {
    void*        p_next;       /**< for adding to a list */
    unsigned int value;
} illu_node_t;

typedef struct alsc_illu_case_resolution {
    resolution_t resolution;
    pLscTableProfile_t *lsc_table_group;
    int lsc_table_count;
} alsc_illu_case_resolution_t;

/**
 * @brief alsc illumination case is different in <enum CalibDb_Used_For_Case_e>,
 *        <resolution>, <color temperature>.
 */
typedef struct alsc_illu_case {
    const CalibDbV2_AlscCof_ill_t *alsc_cof;
    alsc_illu_case_resolution_t *res_group;
    uint32_t res_count;
    uint32_t current_res_idx;
} alsc_illu_case_t;

typedef alsc_illu_case_t* pIlluCase_t;
/** @brief depends on enum  CalibDb_Used_For_Case_e */
typedef struct alsc_mode_data_s
{
    //TODO: actually is const point and should add const
    pIlluCase_t     *illu_case;
    uint32_t        illu_case_count;
} alsc_mode_data_t;

typedef struct alsc_grad_s
{
    resolution_t    resolution;
    uint16_t        LscXGradTbl[LSC_GRAD_TBL_SIZE];
    uint16_t        LscYGradTbl[LSC_GRAD_TBL_SIZE];
} alsc_grad_t;

typedef struct alsc_context_s {
    const CalibDbV2_LSC_t   *calibLscV2;

    alsc_illu_case_t        *illu_case;
    uint32_t                illu_case_count;

    alsc_mode_data_t        alsc_mode[USED_FOR_CASE_MAX];

    alsc_grad_t             *res_grad;
    uint32_t                res_grad_count;

    resolution_t  cur_res;
    alsc_sw_info_t alscSwInfo;
    alsc_rest_t alscRest;
    rk_aiq_lsc_cfg_t lscHwConf; //hw para
    unsigned int count;

    //ctrl & api
    rk_aiq_lsc_attrib_t mCurAtt;
    rk_aiq_lsc_attrib_t mNewAtt;
    bool updateAtt;

    //in some cases, the scene does not change, so it doesn't need to calculate in every frame;
    bool auto_mode_need_run_algo;
} alsc_context_t ;

typedef alsc_context_t* alsc_handle_t ;

typedef struct _RkAiqAlgoContext {
    alsc_handle_t alsc_para;
} RkAiqAlgoContext;


RKAIQ_END_DECLARE

#endif

