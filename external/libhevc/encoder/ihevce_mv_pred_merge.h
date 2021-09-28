/******************************************************************************
 *
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
*/
/*!
******************************************************************************
* \file ihevce_mv_pred_merge.h
*
* \brief
*    This file contains function prototypes of MV Merge candidates list
*    derivation functions and corresponding structure and macrso definations
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_MV_PRED_MERGE_H_
#define _IHEVCE_MV_PRED_MERGE_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define MAX_NUM_MERGE_CAND MAX_MERGE_CANDIDATES
#define MAX_NUM_MV_NBR 5

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
typedef enum
{
    NBR_A0 = 0,
    NBR_A1 = 1,
    NBR_B0 = 2,
    NBR_B1 = 3,
    NBR_B2 = 4,

    /* should be last */
    MAX_NUM_NBRS
} MV_MERGE_NBRS_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

typedef struct
{
    /* Merge candidate motion vectors and refernce idx */
    pu_mv_t mv;

    /* Pred_l0 mode for the candidate */
    UWORD8 u1_pred_flag_l0;

    /* Pred_l1 mode for the candidate */
    UWORD8 u1_pred_flag_l1;

} merge_cand_list_t;

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

void ihevce_collocated_mvp(
    mv_pred_ctxt_t *ps_mv_ctxt,
    pu_t *ps_pu,
    mv_t *ps_mv_col,
    WORD32 *pu4_avail_col_flag,
    WORD32 use_pu_ref_idx,
    WORD32 x_col,
    WORD32 y_col);

WORD32 ihevce_mv_pred_merge(
    mv_pred_ctxt_t *ps_ctxt,
    nbr_4x4_t *ps_top_nbr_4x4,
    nbr_4x4_t *ps_left_nbr_4x4,
    nbr_4x4_t *ps_top_left_nbr_4x4,
    WORD32 left_nbr_4x4_strd,
    nbr_avail_flags_t *ps_avail_flags,
    pu_mv_t *ps_col_mv,
    pu_t *ps_pu,
    PART_SIZE_E part_mode,
    WORD32 part_idx,
    WORD32 single_mcl_flag,
    merge_cand_list_t *ps_merge_cand_list,
    UWORD8 *pu1_is_top_used);

#endif /* _IHEVCE_MV_PRED_MERGE_H_ */
