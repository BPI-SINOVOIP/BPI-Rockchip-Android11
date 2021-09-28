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
* \file est_sad.c
*
* \brief
*    This file contain sad estimation related functions
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
#include "est_sad.h"

typedef struct est_sad_t
{
    WORD32 i4_use_est_intra_sad;
    UWORD32 au4_prev_frm_sad[MAX_PIC_TYPE]; /* Previous frame SAD */
    UWORD32 u4_n_p_frm_ifi_avg_sad; /* Current (nth) ifi average P frame SAD */
    UWORD32 u4_n_1_p_frm_ifi_avg_sad; /* (n-1)th ifi average P frame SAD */
    UWORD32 u4_n_2_p_frm_ifi_avg_sad; /* (n-2)th ifi average P frame SAD */
    WORD32 i4_num_ifi_encoded; /* number of ifi encoded till now */
    WORD32 i4_num_p_frm_in_cur_ifi; /* number of P frames in the current IFI */
} est_sad_t;

#if NON_STEADSTATE_CODE
WORD32 est_sad_num_fill_use_free_memtab(
    est_sad_t **pps_est_sad, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type)
{
    WORD32 i4_mem_tab_idx = 0;
    static est_sad_t s_est_sad;

    /* Hack for al alloc, during which we dont have any state memory.
      Dereferencing can cause issues */
    if(e_func_type == GET_NUM_MEMTAB || e_func_type == FILL_MEMTAB)
        (*pps_est_sad) = &s_est_sad;

    /*for src rate control state structure*/
    if(e_func_type != GET_NUM_MEMTAB)
    {
        fill_memtab(
            &ps_memtab[i4_mem_tab_idx], sizeof(est_sad_t), MEM_TAB_ALIGNMENT, PERSISTENT, DDR);
        use_or_fill_base(&ps_memtab[0], (void **)pps_est_sad, e_func_type);
    }
    i4_mem_tab_idx++;

    return (i4_mem_tab_idx);
}
#endif /* #if NON_STEADSTATE_CODE */
/****************************************************************************
Function Name : init_est_sad
Description   :
Inputs        : ps_est_sad
                i4_use_est_intra_sad

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void init_est_sad(est_sad_t *ps_est_sad, WORD32 i4_use_est_intra_sad)
{
    WORD32 i;
    ps_est_sad->i4_use_est_intra_sad = i4_use_est_intra_sad;

    for(i = 0; i < MAX_PIC_TYPE; i++)
        ps_est_sad->au4_prev_frm_sad[i] = 0;

    ps_est_sad->u4_n_p_frm_ifi_avg_sad = 0;
    ps_est_sad->u4_n_1_p_frm_ifi_avg_sad = 0;
    ps_est_sad->u4_n_2_p_frm_ifi_avg_sad = 0;
    ps_est_sad->i4_num_ifi_encoded = 0;
    ps_est_sad->i4_num_p_frm_in_cur_ifi = 0;
}
/****************************************************************************
Function Name : reset_est_sad
Description   :
Inputs        : ps_est_sad

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void reset_est_sad(est_sad_t *ps_est_sad)
{
    init_est_sad(ps_est_sad, ps_est_sad->i4_use_est_intra_sad);
}

/****************************************************************************
Function Name : get_est_sad
Description   :
Inputs        : ps_est_sad

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
/*
Get estimated SAD can be called at any point. The various use cases are:
1) When a I frame is getting encoded,
     - get the estimated of P => No issues since we use the last coded P frame value
     - get estimated of I => This call for two cases:
                   => a) if num_ifi_encoded is less than 2
                         then return the previous encoded I frame sad
                   => b) if num_ifi_encoded is more than 2, then we scale
                         the prev I sad by the ratio of (n-1) ifi P to n-2 ifi P
2) When P frame is getting encoded,
    - get the estimated of P =>  No issues since we use the last coded P frame value
    - get the estimated of I => Simillar to I we have two cases. To handle the b) case
                                extra logic had to introduced using
                                u1_is_n_1_p_frm_ifi_avg_sad_usable flag
*/
UWORD32 get_est_sad(est_sad_t *ps_est_sad, picture_type_e e_pic_type)
{
    if(ps_est_sad->i4_use_est_intra_sad)
    {
        UWORD32 u4_estimated_sad;
        if(e_pic_type == P_PIC)
        {
            u4_estimated_sad = ps_est_sad->au4_prev_frm_sad[P_PIC];
        }
        else if(e_pic_type == B_PIC)
        {
            u4_estimated_sad = ps_est_sad->au4_prev_frm_sad[B_PIC];
        }
        else
        {
            if(ps_est_sad->i4_num_ifi_encoded < 2)
            {
                /* Only one IFI has been encoded and so use the previous I frames SAD */
                u4_estimated_sad = ps_est_sad->au4_prev_frm_sad[I_PIC];
            }
            else
            {
                /* Since the n-1 'P' frame IFI would have just accumulated the frame sads
                we average it out here */
                UWORD32 u4_n_1_p_frm_ifi_avg_sad, u4_n_2_p_frm_ifi_avg_sad;
                number_t vq_n_1_p_frm_ifi_avg_sad, vq_n_2_p_frm_ifi_avg_sad;
                number_t vq_prev_frm_sad_i;
                /* If there are frames in the current IFI start using it to estimate the I frame SAD */
                if(ps_est_sad->i4_num_p_frm_in_cur_ifi)
                {
                    u4_n_1_p_frm_ifi_avg_sad =
                        (ps_est_sad->u4_n_p_frm_ifi_avg_sad / ps_est_sad->i4_num_p_frm_in_cur_ifi);
                    u4_n_2_p_frm_ifi_avg_sad = ps_est_sad->u4_n_1_p_frm_ifi_avg_sad;
                }
                else
                {
                    u4_n_1_p_frm_ifi_avg_sad = ps_est_sad->u4_n_1_p_frm_ifi_avg_sad;
                    u4_n_2_p_frm_ifi_avg_sad = ps_est_sad->u4_n_2_p_frm_ifi_avg_sad;
                }
                /* If any of the previous p frame SADs are zeros we just return the previous
                I frame SAD */
                if(u4_n_1_p_frm_ifi_avg_sad && u4_n_2_p_frm_ifi_avg_sad)
                {
                    SET_VAR_Q(vq_prev_frm_sad_i, ps_est_sad->au4_prev_frm_sad[I_PIC], 0);
                    SET_VAR_Q(vq_n_1_p_frm_ifi_avg_sad, u4_n_1_p_frm_ifi_avg_sad, 0);
                    SET_VAR_Q(vq_n_2_p_frm_ifi_avg_sad, u4_n_2_p_frm_ifi_avg_sad, 0);
                    /**************************************************************************
                    Estimated SAD =
                    (n-1)th intra frame interval(ifi) P frame Avg SAD *
                    (prev I frame SAD / (n-2)nd intra frame interval(ifi) P frame Avg SAD)
                    **************************************************************************/
                    mult32_var_q(vq_prev_frm_sad_i, vq_n_1_p_frm_ifi_avg_sad, &vq_prev_frm_sad_i);
                    div32_var_q(vq_prev_frm_sad_i, vq_n_2_p_frm_ifi_avg_sad, &vq_prev_frm_sad_i);
                    number_t_to_word32(vq_prev_frm_sad_i, (WORD32 *)&u4_estimated_sad);
                }
                else
                {
                    u4_estimated_sad = ps_est_sad->au4_prev_frm_sad[I_PIC];
                }
            }
        }
        return u4_estimated_sad;
    }
    else
    {
        return ps_est_sad->au4_prev_frm_sad[e_pic_type];
    }
}
/****************************************************************************
Function Name : update_ppic_sad
Description   :
Inputs        : ps_est_sad

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
WORD32 update_ppic_sad(est_sad_t *ps_est_sad, WORD32 i4_est_sad, WORD32 i4_prev_p_sad)
{
    i4_est_sad = ((ps_est_sad->au4_prev_frm_sad[P_PIC]) * ((i4_est_sad << 4) / i4_prev_p_sad)) >> 4;
    /* printf("i4_est_sad=%d prev_psad=%d\n",i4_est_sad,ps_est_sad->au4_prev_frm_sad[P_PIC]); */
    if(i4_est_sad > (WORD32)ps_est_sad->au4_prev_frm_sad[P_PIC])
    {
        if(4 * i4_est_sad > 5 * i4_prev_p_sad)
            i4_est_sad = (5 * i4_prev_p_sad) >> 2;
        ps_est_sad->au4_prev_frm_sad[P_PIC] = i4_est_sad;
        return 0;
    }
    return 1;
}
/****************************************************************************
Function Name : update_actual_sad
Description   :
Inputs        : ps_est_sad

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void update_actual_sad(est_sad_t *ps_est_sad, UWORD32 u4_actual_sad, picture_type_e e_pic_type)
{
    ps_est_sad->au4_prev_frm_sad[e_pic_type] = u4_actual_sad;

    if(ps_est_sad->i4_use_est_intra_sad)
    {
        if(e_pic_type == I_PIC)
        {
            /* The requirement is to have two IFI before estimating I frame SAD */
            if(ps_est_sad->i4_num_ifi_encoded < 2)
                ps_est_sad->i4_num_ifi_encoded++;

            /* Calculate the average SAD */
            if(ps_est_sad->i4_num_p_frm_in_cur_ifi)
            {
                ps_est_sad->u4_n_p_frm_ifi_avg_sad /= ps_est_sad->i4_num_p_frm_in_cur_ifi;
            }
            else
            {
                ps_est_sad->u4_n_p_frm_ifi_avg_sad = 0;
            }
            /* Push the (n-1)th average SAD to the (n-2)th average SAD  */
            ps_est_sad->u4_n_2_p_frm_ifi_avg_sad = ps_est_sad->u4_n_1_p_frm_ifi_avg_sad;
            /* Push the nth average SAD to the (n-1)th average SAD */
            ps_est_sad->u4_n_1_p_frm_ifi_avg_sad = ps_est_sad->u4_n_p_frm_ifi_avg_sad;
            /* Reset SAD and number of P frames */
            ps_est_sad->u4_n_p_frm_ifi_avg_sad = 0;
            ps_est_sad->i4_num_p_frm_in_cur_ifi = 0;
        }
        else
        {
            ps_est_sad->u4_n_p_frm_ifi_avg_sad += u4_actual_sad;
            ps_est_sad->i4_num_p_frm_in_cur_ifi++;
        }
    }
}
/****************************************************************************
Function Name : update_prev_frame_intra_sad
Description   :
Inputs        : ps_est_sad

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void update_prev_frame_intra_sad(est_sad_t *ps_est_sad, WORD32 i4_intra_frm_sad)
{
    ps_est_sad->au4_prev_frm_sad[I_PIC] = i4_intra_frm_sad;
}
/****************************************************************************
Function Name : get_prev_frame_intra_sad
Description   :
Inputs        : ps_est_sad

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
WORD32 get_prev_frame_intra_sad(est_sad_t *ps_est_sad)
{
    return ps_est_sad->au4_prev_frm_sad[I_PIC];
}
/****************************************************************************
Function Name : update_prev_frame_sad
Description   :
Inputs        : ps_est_sad

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void update_prev_frame_sad(est_sad_t *ps_est_sad, WORD32 i4_frm_sad, picture_type_e e_pic_type)
{
    ps_est_sad->au4_prev_frm_sad[e_pic_type] = i4_frm_sad;
}
/****************************************************************************
Function Name : get_prev_frame_sad
Description   :
Inputs        : ps_est_sad

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
WORD32 get_prev_frame_sad(est_sad_t *ps_est_sad, picture_type_e e_pic_type)
{
    return ps_est_sad->au4_prev_frm_sad[e_pic_type];
}
