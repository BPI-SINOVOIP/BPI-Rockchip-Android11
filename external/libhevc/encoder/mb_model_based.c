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
* \file mb_model_based.c
*
* \brief
*    This file contain mb level API functions
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
/* User include files */
#include "ittiam_datatypes.h"
#include "rc_common.h"
#include "rc_cntrl_param.h"
#include "var_q_operator.h"
#include "mem_req_and_acq.h"
#include "mb_model_based.h"

typedef struct mb_rate_control_t
{
    /* Frame Qp */
    UWORD8 u1_frm_qp;
    /* Estimated average activity for the current frame (updated with the previous
    frame activity since it is independent of picture type whether it is I or P) */
    WORD32 i4_avg_activity;
} mb_rate_control_t;

WORD32 mbrc_num_fill_use_free_memtab(
    mb_rate_control_t **pps_mb_rate_control, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type)
{
    WORD32 i4_mem_tab_idx = 0;
    static mb_rate_control_t s_mb_rate_control_temp;

    /* Hack for al alloc, during which we dont have any state memory.
      Dereferencing can cause issues */
    if(e_func_type == GET_NUM_MEMTAB || e_func_type == FILL_MEMTAB)
        (*pps_mb_rate_control) = &s_mb_rate_control_temp;

    /*for src rate control state structure*/
    if(e_func_type != GET_NUM_MEMTAB)
    {
        fill_memtab(
            &ps_memtab[i4_mem_tab_idx],
            sizeof(mb_rate_control_t),
            MEM_TAB_ALIGNMENT,
            PERSISTENT,
            DDR);
        use_or_fill_base(&ps_memtab[0], (void **)pps_mb_rate_control, e_func_type);
    }
    i4_mem_tab_idx++;

    return (i4_mem_tab_idx);
}

/********************************************************************************
                         MB LEVEL API FUNCTIONS
********************************************************************************/
/******************************************************************************
  Function Name   : init_mb_level_rc
  Description     : Initialise the mb model and the average activity to default values
  Arguments       :
  Return Values   : void
  Revision History:
                    13 03 2008   KJN  Creation
*********************************************************************************/
void init_mb_level_rc(mb_rate_control_t *ps_mb_rate_control)
{
    /* Set values to default */
    ps_mb_rate_control->i4_avg_activity = 0;
}
/******************************************************************************
  Function Name   : mb_init_frame_level
  Description     : Initialise the mb state with frame level decisions
  Arguments       : u1_frame_qp - Frame level qp
  Return Values   : void
  Revision History:
                    13 03 2008   KJN  Creation
*********************************************************************************/
void mb_init_frame_level(mb_rate_control_t *ps_mb_rate_control, UWORD8 u1_frame_qp)
{
    /* Update frame level QP */
    ps_mb_rate_control->u1_frm_qp = u1_frame_qp;
}
/******************************************************************************
  Function Name   : reset_mb_activity
  Description     : Reset the mb activity - Whenever there is SCD
                    the mb activity is reset
  Arguments       :
  Return Values   : void
  Revision History:
                    13 03 2008   KJN  Creation
*********************************************************************************/
void reset_mb_activity(mb_rate_control_t *ps_mb_rate_control)
{
    ps_mb_rate_control->i4_avg_activity = 0;
}

/******************************************************************************
  Function Name   : get_mb_qp
  Description     : Calculates the mb level qp
  Arguments       : i4_cur_mb_activity - current frame mb activity
                    pi4_mb_qp - Array of 2 values for before and after mb activity
                                modulation
  Return Values   : void

  Revision History:
                    13 03 2008   KJN  Creation
*********************************************************************************/
void get_mb_qp(mb_rate_control_t *ps_mb_rate_control, WORD32 i4_cur_mb_activity, WORD32 *pi4_mb_qp)
{
    WORD32 i4_qp;
    /* Initialise the mb level qp with the frame level qp */
    i4_qp = ps_mb_rate_control->u1_frm_qp;

    /* Store the model based QP - This is used for updating the rate control model */
    pi4_mb_qp[0] = i4_qp;

    /* Modulate the Qp based on the activity */
    if((ps_mb_rate_control->i4_avg_activity) && (i4_qp < 100))
    {
        i4_qp = ((((2 * i4_cur_mb_activity)) + ps_mb_rate_control->i4_avg_activity) * i4_qp +
                 ((i4_cur_mb_activity + 2 * ps_mb_rate_control->i4_avg_activity) >> 1)) /
                (i4_cur_mb_activity + 2 * ps_mb_rate_control->i4_avg_activity);

        if(i4_qp > ((3 * ps_mb_rate_control->u1_frm_qp) >> 1))
            i4_qp = ((3 * ps_mb_rate_control->u1_frm_qp) >> 1);
    }

    /* Store the qp modulated by mb activity - This is used for encoding the MB */
    pi4_mb_qp[1] = i4_qp;
}
/******************************************************************************
  Function Name   : get_frm_level_qp
  Description     : Returns the stored frame level QP
  Arguments       :
  Revision History:
                    13 03 2008   KJN  Creation
*********************************************************************************/
UWORD8 get_frm_level_qp(mb_rate_control_t *ps_mb_rate_control)
{
    return (ps_mb_rate_control->u1_frm_qp);
}
/******************************************************************************
  Function Name   : mb_update_frame_level
  Description     : Update the frame level info collected
  Arguments       : i4_avg_activity - Average activity fot frame
  Return Values   :
  Revision History:
                    13 03 2008   KJN  Creation
*********************************************************************************/
void mb_update_frame_level(mb_rate_control_t *ps_mb_rate_control, WORD32 i4_avg_activity)
{
    /*****************************************************************************
                    Update the Average Activity
    *****************************************************************************/
    ps_mb_rate_control->i4_avg_activity = i4_avg_activity;
}
