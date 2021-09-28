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
* \file init_qp.c
*
* \brief
*    This file contain qp initialization functions
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
#include "rc_cntrl_param.h"
#include "var_q_operator.h"
#include "mem_req_and_acq.h"
#include "rc_common.h"
#include "init_qp.h"

typedef struct init_qp_t
{
    /* WORD32 ai4_bpp_for_qp[MAX_MPEG2_QP]; */
    WORD32 i4_max_qp;
    WORD32 i4_num_pels_in_frame;
    WORD32 i4_is_hbr;
} init_qp_t;

#define BPP_Q_FACTOR (16)
#define QP_FOR_ONE_BPP (3) /*(10)*/

#if NON_STEADSTATE_CODE
WORD32 init_qp_num_fill_use_free_memtab(
    init_qp_handle *pps_init_qp, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type)
{
    WORD32 i4_mem_tab_idx = 0;
    static init_qp_t s_init_qp;

    /* Hack for al alloc, during which we dont have any state memory.
      Dereferencing can cause issues */
    if(e_func_type == GET_NUM_MEMTAB || e_func_type == FILL_MEMTAB)
        (*pps_init_qp) = &s_init_qp;

    /*for src rate control state structure*/
    if(e_func_type != GET_NUM_MEMTAB)
    {
        fill_memtab(
            &ps_memtab[i4_mem_tab_idx], sizeof(init_qp_t), MEM_TAB_ALIGNMENT, PERSISTENT, DDR);
        use_or_fill_base(&ps_memtab[0], (void **)pps_init_qp, e_func_type);
    }
    i4_mem_tab_idx++;

    return (i4_mem_tab_idx);
}

/****************************************************************************
Function Name : init_init_qp
Description   :
Inputs        : ps_init_qp

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void init_init_qp(
    init_qp_handle ps_init_qp, WORD32 *pi4_min_max_qp, WORD32 i4_num_pels_in_frame, WORD32 i4_is_hbr)
{
    WORD32 i4_max_qp;
    /* Finding the max qp among I P and B frame */
    i4_max_qp = pi4_min_max_qp[1];
    if(i4_max_qp < pi4_min_max_qp[3])
        i4_max_qp = pi4_min_max_qp[3];
    if(i4_max_qp < pi4_min_max_qp[5])
        i4_max_qp = pi4_min_max_qp[5];

    /*for(i=0;i<i4_max_qp;i++)
    {
        ps_init_qp->ai4_bpp_for_qp[i] = (QP_FOR_ONE_BPP*(1<<BPP_Q_FACTOR))/(i+1);
    }*/
    ps_init_qp->i4_max_qp = i4_max_qp;
    ps_init_qp->i4_num_pels_in_frame = (!i4_num_pels_in_frame) ? 1 : i4_num_pels_in_frame;
    ps_init_qp->i4_is_hbr = i4_is_hbr;
}
#endif /* #if NON_STEADSTATE_CODE */

/* To ensure init_qp for high bit rates is low */
#define QP_FOR_ONE_BPP_HBR (5)

/****************************************************************************
Function Name : get_init_qp_using_pels_bits_per_frame
Description   :
Inputs        : ps_init_qp

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
/* If the remaining pels in frame is zero we would be using the init time pixels for calculating the bits per pixel */
WORD32 get_init_qp_using_pels_bits_per_frame(
    init_qp_handle ps_init_qp,
    picture_type_e e_pic_type,
    WORD32 i4_bits_remaining_in_frame,
    WORD32 i4_rem_pels_in_frame)
{
    WORD32 i4_qp;
    WORD32 i4_qp_for_one_bpp;

    if(ps_init_qp->i4_is_hbr)
    {
        i4_qp_for_one_bpp = QP_FOR_ONE_BPP_HBR;
    }
    else
    {
        i4_qp_for_one_bpp = QP_FOR_ONE_BPP;
    }

    if(!i4_rem_pels_in_frame)
        i4_rem_pels_in_frame = ps_init_qp->i4_num_pels_in_frame;
    if(e_pic_type == P_PIC || e_pic_type == P1_PIC)
        i4_bits_remaining_in_frame = i4_bits_remaining_in_frame * I_TO_P_BIT_RATIO;
    if(e_pic_type >= B_PIC && e_pic_type != P1_PIC)
        i4_bits_remaining_in_frame =
            i4_bits_remaining_in_frame * (I_TO_P_BIT_RATIO * P_TO_B_BIT_RATIO);

    /* Assuming a 1 bpp => Qp = 12, So Qp = 1 => 12 bpp. [bpp halves with every doubling of Qp] */
    /* x bpp =  i4_bits_remaining_in_frame/i4_rem_pels_in_frame
       1 bpp = QP_FOR_ONE_BPP
       QP_FOR_X_BPP = QP_FOR_ONE_BPP/(x) = QP_FOR_ONE_BPP*i4_rem_pels_in_frame/i4_bits_remaining_in_frame */
    X_PROD_Y_DIV_Z(i4_qp_for_one_bpp, i4_rem_pels_in_frame, i4_bits_remaining_in_frame, i4_qp);

    /* Scaling the Qp values based on picture type */
    if(e_pic_type == P_PIC || e_pic_type == P1_PIC)
        i4_qp = ((i4_qp * I_TO_P_RATIO) >> K_Q);

    if(e_pic_type >= B_PIC && e_pic_type != P1_PIC)
    {
        if(!ps_init_qp->i4_is_hbr)
        {
            i4_qp = ((i4_qp * P_TO_B_RATIO * I_TO_P_RATIO) >> (K_Q + K_Q));
        }
        else
        {
            i4_qp = ((i4_qp * P_TO_B_RATIO_HBR * I_TO_P_RATIO) >> (K_Q + K_Q));
        }
    }

    if(i4_qp > ps_init_qp->i4_max_qp)
        i4_qp = ps_init_qp->i4_max_qp;
    else if(i4_qp == 0)
        i4_qp = 1;

    return i4_qp;
}

#if NON_STEADSTATE_CODE
/****************************************************************************
Function Name : change_init_qp_max_qp
Description   :
Inputs        : ps_init_qp

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void change_init_qp_max_qp(init_qp_handle ps_init_qp, WORD32 *pi4_min_max_qp)
{
    WORD32 i4_max_qp;
    /* Finding the max qp among I P and B frame */
    i4_max_qp = pi4_min_max_qp[1];
    if(i4_max_qp < pi4_min_max_qp[3])
        i4_max_qp = pi4_min_max_qp[3];
    if(i4_max_qp < pi4_min_max_qp[5])
        i4_max_qp = pi4_min_max_qp[5];

    ps_init_qp->i4_max_qp = i4_max_qp;
}
#endif /* #if NON_STEADSTATE_CODE */
