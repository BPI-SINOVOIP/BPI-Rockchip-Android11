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
* \file rc_sad_acc.c
*
* \brief
*    This file contain sad accumulator related functions
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
#include <string.h>

/* User include files */
#include "ittiam_datatypes.h"
#include "mem_req_and_acq.h"
#include "rc_common.h"
#include "rc_cntrl_param.h"
#include "var_q_operator.h"
#include "trace_support.h"
#include "rc_sad_acc.h"

/* State structure for sad accumulator */
typedef struct
{
    WORD32 ai4_sad[MAX_PIC_TYPE];

} sad_acc_t;

#if NON_STEADSTATE_CODE
WORD32 sad_acc_num_fill_use_free_memtab(
    sad_acc_handle *pps_sad_acc_handle, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type)
{
    WORD32 i4_mem_tab_idx = 0;
    sad_acc_t **pps_sad_acc = (sad_acc_t **)pps_sad_acc_handle;
    static sad_acc_t s_sad_acc;

    /* Hack for al alloc, during which we dont have any state memory.
      Dereferencing can cause issues */
    if(e_func_type == GET_NUM_MEMTAB || e_func_type == FILL_MEMTAB)
        (*pps_sad_acc) = &s_sad_acc;

    /*for src rate control state structure*/
    if(e_func_type != GET_NUM_MEMTAB)
    {
        fill_memtab(
            &ps_memtab[i4_mem_tab_idx], sizeof(sad_acc_t), MEM_TAB_ALIGNMENT, PERSISTENT, DDR);
        use_or_fill_base(&ps_memtab[0], (void **)pps_sad_acc, e_func_type);
    }
    i4_mem_tab_idx++;

    return (i4_mem_tab_idx);
}
/******************************************************************************
  Function Name   : init_sad_acc
  Description     :
  Arguments       : ps_sad_acc_handle
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
void init_sad_acc(sad_acc_handle ps_sad_acc_handle)
{
    sad_acc_t *ps_sad_acc = (sad_acc_t *)ps_sad_acc_handle;
    WORD32 i;
    /* Initialize the array */
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        ps_sad_acc->ai4_sad[i] = -1;
    }
}
#endif /* #if NON_STEADSTATE_CODE */
/******************************************************************************
  Function Name   : sad_acc_put_sad
  Description     :
  Arguments       : ps_sad_acc_handle
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
void sad_acc_put_sad(
    sad_acc_handle ps_sad_acc_handle,
    WORD32 i4_cur_intra_sad,
    WORD32 i4_cur_sad,
    WORD32 i4_cur_pic_type)
{
    sad_acc_t *ps_sad_acc = (sad_acc_t *)ps_sad_acc_handle;
    ps_sad_acc->ai4_sad[I_PIC] = i4_cur_intra_sad;
    ps_sad_acc->ai4_sad[i4_cur_pic_type] = i4_cur_sad;
}
/******************************************************************************
  Function Name   : sad_acc_get_sad
  Description     :
  Arguments       : ps_sad_acc_handle
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
void sad_acc_get_sad(sad_acc_handle ps_sad_acc_handle, WORD32 *pi4_sad)
{
    sad_acc_t *ps_sad_acc = (sad_acc_t *)ps_sad_acc_handle;
    WORD32 i;
    /* Initialize the array */
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        pi4_sad[i] = ps_sad_acc->ai4_sad[i];
    }
}
