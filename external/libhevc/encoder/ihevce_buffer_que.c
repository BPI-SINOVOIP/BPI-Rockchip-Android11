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

/**
******************************************************************************
* @file
*  ihevce_buffer_que.c
*
* @brief
*  This file contains all the functions related to Buffer Queue manager
*
* @author
*  ittiam
*
* @par List of Functions:
*  ihevce_buff_que_get_mem_recs
*  ihevce_buff_que_get_num_mem_recs
*  ihevce_buff_que_init
*  ihevce_buff_que_get_free_buf
*  ihevce_buff_que_get_next_buf
*  ihevce_buff_que_get_next_reorder_buf
*  ihevce_buff_que_set_buf_prod
*  ihevce_buff_que_rel_buf
*  ihevce_buff_que_get_active_bufs
*  ihevce_buff_que_set_reorder_buf
*
******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System Include Files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

/* User Include Files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevce_buffer_que_interface.h"
#include "ihevce_buffer_que_private.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
************************************************************************
* \brief
*    return number of records used by Buffer Que manager.
************************************************************************
*/
WORD32 ihevce_buff_que_get_num_mem_recs(void)
{
    return (NUM_BUFFER_QUE_MEM_RECS);
}

/*!
************************************************************************
* \brief
*    return each record attributes of Buffer Que manager
************************************************************************
*/
WORD32 ihevce_buff_que_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab, WORD32 max_num_bufs_in_que, WORD32 i4_mem_space)
{
    /* Que manager state structure */
    ps_mem_tab[BUFFER_QUE_CTXT].i4_mem_size = sizeof(buf_que_t);
    ps_mem_tab[BUFFER_QUE_CTXT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[BUFFER_QUE_CTXT].i4_mem_alignment = 8;

    /* number of users memory */
    ps_mem_tab[BUFFER_QUE_NUM_USER_MEM].i4_mem_size = (sizeof(WORD32) * max_num_bufs_in_que);
    ps_mem_tab[BUFFER_QUE_NUM_USER_MEM].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[BUFFER_QUE_NUM_USER_MEM].i4_mem_alignment = 8;

    /* Produced status memory */
    ps_mem_tab[BUFFER_QUE_PROD_STS_MEM].i4_mem_size = (sizeof(WORD32) * max_num_bufs_in_que);
    ps_mem_tab[BUFFER_QUE_PROD_STS_MEM].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[BUFFER_QUE_PROD_STS_MEM].i4_mem_alignment = 8;

    /* Encode sequence memory */
    ps_mem_tab[BUFFER_QUE_ENC_SEQ_MEM].i4_mem_size = (sizeof(UWORD32) * max_num_bufs_in_que);
    ps_mem_tab[BUFFER_QUE_ENC_SEQ_MEM].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[BUFFER_QUE_ENC_SEQ_MEM].i4_mem_alignment = 8;

    /* Queued sequence memory */
    ps_mem_tab[BUFFER_QUE_QUED_SEQ_MEM].i4_mem_size = (sizeof(UWORD32) * max_num_bufs_in_que);
    ps_mem_tab[BUFFER_QUE_QUED_SEQ_MEM].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[BUFFER_QUE_QUED_SEQ_MEM].i4_mem_alignment = 8;

    return (NUM_BUFFER_QUE_MEM_RECS);
}

/*!
************************************************************************
* \brief
*    Intialization for Buffer Que manager state structure
************************************************************************
*/
void *ihevce_buff_que_init(iv_mem_rec_t *ps_mem_tab, WORD32 num_bufs_in_que, void **ppv_buff_ptrs)
{
    buf_que_t *ps_buf_que;
    WORD32 i;

    /* que manager state structure */
    ps_buf_que = (buf_que_t *)ps_mem_tab[BUFFER_QUE_CTXT].pv_base;

    /* buffer status memory init */
    ps_buf_que->pi4_num_users = (WORD32 *)ps_mem_tab[BUFFER_QUE_NUM_USER_MEM].pv_base;

    ps_buf_que->pi4_produced_sts = (WORD32 *)ps_mem_tab[BUFFER_QUE_PROD_STS_MEM].pv_base;

    ps_buf_que->pu4_enc_seq = (UWORD32 *)ps_mem_tab[BUFFER_QUE_ENC_SEQ_MEM].pv_base;

    ps_buf_que->pu4_que_seq = (UWORD32 *)ps_mem_tab[BUFFER_QUE_QUED_SEQ_MEM].pv_base;

    /* reset the state structure variables */
    ps_buf_que->i4_num_bufs = num_bufs_in_que;
    ps_buf_que->i4_num_active_bufs = 0;
    ps_buf_que->u4_last_prod = 0;
    ps_buf_que->u4_last_cons = 0;
    ps_buf_que->u4_next_disp_seq = 0;
    ps_buf_que->u4_last_disp_seq = 0;
    ps_buf_que->ppv_buff_ptrs = ppv_buff_ptrs;

    /* init all the buffer status to default values */
    for(i = 0; i < ps_buf_que->i4_num_bufs; i++)
    {
        ps_buf_que->pi4_num_users[i] = 0;
        ps_buf_que->pi4_produced_sts[i] = 0;
        ps_buf_que->pu4_enc_seq[i] = UINT32_MAX;
        ps_buf_que->pu4_que_seq[i] = UINT32_MAX;
    }

    return ((void *)ps_buf_que);
}

/*!
**************************************************************************
* \brief
*    This function gets the next free buffer. This function is called by the
*    Producer to get a free buffer
**************************************************************************
*/
void *ihevce_buff_que_get_free_buf(void *pv_buf_que, WORD32 *pi4_id)
{
    buf_que_t *ps_buf_que;
    WORD32 i;
    WORD32 num_bufs;

    ps_buf_que = (buf_que_t *)pv_buf_que;
    num_bufs = ps_buf_que->i4_num_bufs;

    /* loop unitl a free buffer is found */
    for(i = 0; i < num_bufs; i++)
    {
        if((ps_buf_que->pi4_num_users[i] == 0) && (ps_buf_que->pi4_produced_sts[i] == 0))
        {
            *(pi4_id) = i;
            ps_buf_que->pi4_num_users[i] = 1;
            ps_buf_que->pu4_que_seq[i] = ps_buf_que->u4_last_prod;
            ps_buf_que->u4_last_prod += 1;

            return (ps_buf_que->ppv_buff_ptrs[i]);
        }
    }
    return (NULL);
}

/*!
**************************************************************************
* \brief
*    This function gets the next buffer in Que . This function will be called by
*    consumer to get the next buffer in Queued order.
**************************************************************************
*/
void *ihevce_buff_que_get_next_buf(void *pv_buf_que, WORD32 *pi4_id)
{
    buf_que_t *ps_buf_que;
    WORD32 i;
    UWORD32 next_qued_seq;

    ps_buf_que = (buf_que_t *)pv_buf_que;

    /* get the next queued buffer to be sent */
    next_qued_seq = ps_buf_que->u4_last_cons;

    /* check for matching index */
    for(i = 0; i < ps_buf_que->i4_num_bufs; i++)
    {
        if(next_qued_seq == ps_buf_que->pu4_que_seq[i])
        {
            if(1 == ps_buf_que->pi4_produced_sts[i])
            {
                *(pi4_id) = i;
                ps_buf_que->u4_last_cons += 1;

                return (ps_buf_que->ppv_buff_ptrs[i]);
            }
            else
            {
                break;
            }
        }
    }

    /* Buffer not ready for Consumption */
    return (NULL);
}

/*!
**************************************************************************
* \brief
*    This function gives the buffer curresponding to the id passed
**************************************************************************
*/
void *ihevce_buff_que_get_buf(void *pv_buf_que, WORD32 i4_id)
{
    buf_que_t *ps_buf_que;

    ps_buf_que = (buf_que_t *)pv_buf_que;

    if(i4_id >= ps_buf_que->i4_num_bufs)
        return (NULL);

    return (ps_buf_que->ppv_buff_ptrs[i4_id]);
}

/*!
**************************************************************************
* \brief
*    This function gets the next buffer for in reordered order. This function
*    will be called by consumer to get the next buffer in reordered order
**************************************************************************
*/
void *ihevce_buff_que_get_next_reorder_buf(void *pv_buf_que, WORD32 *pi4_id)
{
    buf_que_t *ps_buf_que;
    WORD32 i;
    UWORD32 next_disp_seq;

    ps_buf_que = (buf_que_t *)pv_buf_que;

    /* get the next reordered buffer to be sent */
    next_disp_seq = ps_buf_que->u4_last_disp_seq;

    /* check for matching index */
    for(i = 0; i < ps_buf_que->i4_num_bufs; i++)
    {
        if(next_disp_seq == ps_buf_que->pu4_enc_seq[i])
        {
            *(pi4_id) = i;
            ps_buf_que->u4_last_disp_seq += 1;

            return (ps_buf_que->ppv_buff_ptrs[i]);
        }
    }

    /* Buffer not ready for Consumption */
    return (NULL);
}

/*!
**************************************************************************
* \brief
*    This function sets the buffer as produced. This function will be called
*    by Producer to say that buffer is ready for consumption.
**************************************************************************
*/
WORD32 ihevce_buff_que_set_buf_prod(void *pv_buf_que, WORD32 buf_id, WORD32 num_users)
{
    buf_que_t *ps_buf_que;

    ps_buf_que = (buf_que_t *)pv_buf_que;

    if(buf_id < ps_buf_que->i4_num_bufs)
    {
        if(ps_buf_que->pi4_produced_sts[buf_id] == 0)
        {
            ps_buf_que->pi4_num_users[buf_id] += num_users;
            ps_buf_que->i4_num_active_bufs += 1;
            ps_buf_que->pi4_produced_sts[buf_id] = 1;

            return 0;
        }
        else
        {
            /* Buffer is already marked as Produced */
            return (-1);
        }
    }
    else
    {
        /* Unable to recognize the Buffer ID */
        return (-1);
    }

    return (-1);
}

/*!
**************************************************************************
* \brief
*    This function decrements number of users. If Number of users are Zero,
*    then active Buffers in list gets decremented and this buffer is marked
*    unused.
**************************************************************************
*/
WORD32 ihevce_buff_que_rel_buf(void *pv_buf_que, WORD32 buf_id)
{
    buf_que_t *ps_buf_que;
    WORD32 i;

    ps_buf_que = (buf_que_t *)pv_buf_que;
    i = buf_id;

    /* check if the buf id is less than max num buffers */
    if(i < ps_buf_que->i4_num_bufs)
    {
        if(ps_buf_que->pi4_produced_sts[i] > 0)
        {
            /* decrease the number of users */
            ps_buf_que->pi4_num_users[i] -= 1;

            if(ps_buf_que->pi4_num_users[i] == 0)
            {
                if(0 == ps_buf_que->i4_num_active_bufs)
                {
                    return (-1);
                }

                ps_buf_que->i4_num_active_bufs -= 1;
                ps_buf_que->pi4_produced_sts[i] = 0;
            }
            return 0;
        }
        else
        {
            /* Illeagal release of Buffer, No one is using it */
            return (-1);
        }
    }

    /* Unable to recognize the Buffer ID */
    return (-1);
}

/*!
**************************************************************************
* \brief
*    This function gets number of active buffers.
**************************************************************************
*/
WORD32 ihevce_buff_que_get_active_bufs(void *pv_buf_que)
{
    buf_que_t *ps_buf_que;

    ps_buf_que = (buf_que_t *)pv_buf_que;
    return (ps_buf_que->i4_num_active_bufs);
}

/*!
**************************************************************************
* \brief
*    This function sets the reorder number for given buffer.
*    this will set the order for the consumer who is consuming in reorder order
**************************************************************************
*/
WORD32 ihevce_buff_que_set_reorder_buf(void *pv_buf_que, WORD32 buf_id)
{
    buf_que_t *ps_buf_que;

    ps_buf_que = (buf_que_t *)pv_buf_que;

    if(buf_id < ps_buf_que->i4_num_bufs)
    {
        WORD32 next_disp_seq = ps_buf_que->u4_next_disp_seq;

        /* increment the seq number */
        ps_buf_que->u4_next_disp_seq++;

        /* set the reorder number to the corresponding id */
        ps_buf_que->pu4_enc_seq[buf_id] = next_disp_seq;

        return 0;
    }
    else
    {
        /* invalid buffer id */
        return (-1);
    }

    return (-1);
}
