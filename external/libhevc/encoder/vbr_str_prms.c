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
* \file vbr_str_prms.c
*
* \brief
*    This file contain
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/
/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/
/* System include files */
#include <stdio.h>

/* User include files */
#include "ittiam_datatypes.h"
#include "rc_cntrl_param.h"
#include "var_q_operator.h"
#include "rc_common.h"
#include "vbr_str_prms.h"

/******************************************************************************
  Function Name   : init_vbv_str_prms
  Description     : Initializes and calcuates the number of I frame and P frames
                    in the delay period
  Arguments       :
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
#if NON_STEADSTATE_CODE
void init_vbv_str_prms(
    vbr_str_prms_t *p_vbr_str_prms,
    UWORD32 u4_intra_frm_interval,
    UWORD32 u4_src_ticks,
    UWORD32 u4_tgt_ticks,
    UWORD32 u4_frms_in_delay_period)
{
    p_vbr_str_prms->u4_frms_in_delay_prd = u4_frms_in_delay_period;
    p_vbr_str_prms->u4_src_ticks = u4_src_ticks;
    p_vbr_str_prms->u4_tgt_ticks = u4_tgt_ticks;
    p_vbr_str_prms->u4_intra_frame_int = u4_intra_frm_interval;
}
#endif /* #if NON_STEADSTATE_CODE */

/*********************************************************************************
  Function Name   : change_vbr_str_prms
  Description     : Takes in changes of Intra frame interval, source and target ticks
                    and recalculates the position of the  next I frame
  Arguments       :
  Return Values   : void
  Revision History:
                    Creation
***********************************************************************************/
#if NON_STEADSTATE_CODE
void change_vsp_ifi(vbr_str_prms_t *p_vbr_str_prms, UWORD32 u4_intra_frame_int)
{
    init_vbv_str_prms(
        p_vbr_str_prms,
        u4_intra_frame_int,
        p_vbr_str_prms->u4_src_ticks,
        p_vbr_str_prms->u4_tgt_ticks,
        p_vbr_str_prms->u4_frms_in_delay_prd);
}
/******************************************************************************
  Function Name   : change_vsp_tgt_ticks
  Description     :
  Arguments       : p_vbr_str_prms
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
void change_vsp_tgt_ticks(vbr_str_prms_t *p_vbr_str_prms, UWORD32 u4_tgt_ticks)
{
    UWORD32 u4_rem_intra_per_scaled;
    UWORD32 u4_prev_tgt_ticks = p_vbr_str_prms->u4_tgt_ticks;

    /*
        If the target frame rate is changed, recalculate the position of the next I frame based
        on the new target frame rate

        LIMITATIONS :
        Currently no support is available for dynamic change in source frame rate
    */

    u4_rem_intra_per_scaled =
        ((p_vbr_str_prms->u4_intra_prd_pos_in_tgt_ticks - p_vbr_str_prms->u4_cur_pos_in_src_ticks) /
         u4_prev_tgt_ticks) *
        u4_tgt_ticks;

    p_vbr_str_prms->u4_intra_prd_pos_in_tgt_ticks =
        u4_rem_intra_per_scaled + p_vbr_str_prms->u4_cur_pos_in_src_ticks;
}
/******************************************************************************
  Function Name   : change_vsp_src_ticks
  Description     :
  Arguments       : p_vbr_str_prms
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
void change_vsp_src_ticks(vbr_str_prms_t *p_vbr_str_prms, UWORD32 u4_src_ticks)
{
    init_vbv_str_prms(
        p_vbr_str_prms,
        p_vbr_str_prms->u4_intra_frame_int,
        u4_src_ticks,
        p_vbr_str_prms->u4_tgt_ticks,
        p_vbr_str_prms->u4_frms_in_delay_prd);
}
/******************************************************************************
  Function Name   : change_vsp_fidp
  Description     :
  Arguments       : p_vbr_str_prms
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
void change_vsp_fidp(vbr_str_prms_t *p_vbr_str_prms, UWORD32 u4_frms_in_delay_period)
{
    init_vbv_str_prms(
        p_vbr_str_prms,
        p_vbr_str_prms->u4_intra_frame_int,
        p_vbr_str_prms->u4_src_ticks,
        p_vbr_str_prms->u4_tgt_ticks,
        u4_frms_in_delay_period);
}
#endif /* #if NON_STEADSTATE_CODE */
