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
* \file ihevce_mv_pred.h
*
* \brief
*    This file contains function prototypes of MV predcition function
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_MV_PRED_H_
#define _IHEVCE_MV_PRED_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

#define MAX_MVP_LIST_CAND 2
#define MAX_MVP_LIST_CAND_MEM (MAX_MVP_LIST_CAND + 1)

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

void ihevce_mv_pred(
    mv_pred_ctxt_t *ps_ctxt,
    nbr_4x4_t *ps_top_nbr_4x4,
    nbr_4x4_t *ps_left_nbr_4x4,
    nbr_4x4_t *ps_top_left_nbr_4x4,
    WORD32 left_nbr_4x4_strd,
    nbr_avail_flags_t *ps_avail_flags,
    pu_mv_t *ps_col_mv,
    pu_t *ps_pu,
    pu_mv_t *ps_pred_mv,
    UWORD8 (*pu1_is_top_used)[MAX_MVP_LIST_CAND]);

#endif /* _IHEVCE_MV_PRED_H_ */
