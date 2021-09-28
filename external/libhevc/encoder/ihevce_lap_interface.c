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
*  ihevce_lap_interface.c
*
* @brief
*  This file contains function definitions related to look-ahead processing
*
* @author
*  ittiam
*
* @par List of Functions:
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
#include <assert.h>

/* User Include Files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevce_api.h"

#include "rc_cntrl_param.h"
#include "rc_frame_info_collector.h"
#include "rc_look_ahead_params.h"

#include "ihevc_defs.h"
#include "ihevc_macros.h"
#include "ihevc_debug.h"
#include "ihevc_structs.h"
#include "ihevc_platform_macros.h"
#include "ihevc_deblk.h"
#include "ihevc_itrans_recon.h"
#include "ihevc_chroma_itrans_recon.h"
#include "ihevc_chroma_intra_pred.h"
#include "ihevc_intra_pred.h"
#include "ihevc_inter_pred.h"
#include "ihevc_mem_fns.h"
#include "ihevc_padding.h"
#include "ihevc_weighted_pred.h"
#include "ihevc_sao.h"
#include "ihevc_resi_trans.h"
#include "ihevc_quant_iquant_ssd.h"
#include "ihevc_cabac_tables.h"

#include "ihevce_defs.h"
#include "ihevce_api.h"
#include "ihevce_hle_interface.h"
#include "ihevce_hle_q_func.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_lap_interface.h"
#include "ihevce_lap_structs.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_function_selector.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_enc_structs.h"
#include "ihevce_rc_enc_structs.h"
#include "ihevce_rc_interface.h"
#include "ihevce_buffer_que_interface.h"

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/
WORD32 gau1_order_insert_pic_type[MAX_TEMPORAL_LAYERS][8] = {
    { P_PIC, B_PIC, P_PIC, B_PIC, P_PIC, B_PIC, P_PIC, B_PIC },
    { P_PIC, B_PIC, B1_PIC, B1_PIC, P_PIC, B_PIC, B1_PIC, B1_PIC },
    { P_PIC, B_PIC, B1_PIC, B2_PIC, B2_PIC, B1_PIC, B2_PIC, B2_PIC },
};

UWORD8 gau1_use_by_cur_pic_flag[MAX_REF_PICS] = { 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 };

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
************************************************************************
* \brief
*    return number of records used by LAP
*
************************************************************************
*/
WORD32 ihevce_lap_get_num_mem_recs(void)
{
    return (NUM_LAP_MEM_RECS);
}

/*!
************************************************************************
* @brief
*    return each record attributes of LAP
************************************************************************
*/
WORD32 ihevce_lap_get_mem_recs(iv_mem_rec_t *ps_mem_tab, WORD32 i4_mem_space)
{
    /* number of NODE memory */
    WORD32 max_nodes = MAX_SUB_GOP_SIZE - 1;

    ps_mem_tab[LAP_CTXT].i4_mem_size = sizeof(lap_struct_t);
    ps_mem_tab[LAP_CTXT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[LAP_CTXT].i4_mem_alignment = 8;

    /* Node memory for 2 sub-gops*/
    ps_mem_tab[LAP_NODE_MEM].i4_mem_size = (max_nodes * sizeof(ihevce_encode_node_t));

    ps_mem_tab[LAP_NODE_MEM].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[LAP_NODE_MEM].i4_mem_alignment = 8;

    return (NUM_LAP_MEM_RECS);
}

/*!
************************************************************************
* @brief
*    Init LAP structure
************************************************************************
*/
void *ihevce_lap_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_lap_static_params_t *ps_lap_params,
    ihevce_static_cfg_params_t *ps_static_cfg_prms)
{
    WORD32 i4_src_interlace_field;
    WORD32 i4_max_temporal_layers;
    ihevce_encode_node_t *ps_encode_node_struct;
    lap_struct_t *ps_lap_struct = (lap_struct_t *)ps_mem_tab[LAP_CTXT].pv_base;
    ihevce_lap_static_params_t *ps_lap_static_params = &ps_lap_struct->s_lap_static_params;
    ps_lap_struct->aps_encode_node[0] = (ihevce_encode_node_t *)ps_mem_tab[LAP_NODE_MEM].pv_base;

    memcpy(
        &ps_lap_struct->s_static_cfg_params,
        ps_static_cfg_prms,
        sizeof(ihevce_static_cfg_params_t));
    memmove(ps_lap_static_params, ps_lap_params, sizeof(ihevce_lap_static_params_t));
    ps_lap_static_params->e_arch_type = ps_static_cfg_prms->e_arch_type;

    /* Set the array to zero */
    memset(&ps_lap_struct->ai4_capture_order_poc[0], 0, MAX_NUM_ENC_NODES * sizeof(WORD32));
    memset(&ps_lap_struct->ai4_encode_order_poc[0], 0, MAX_NUM_ENC_NODES * sizeof(WORD32));
    memset(&ps_lap_struct->ref_poc_array[0], 0xFF, sizeof(ps_lap_struct->ref_poc_array));
    memset(&ps_lap_struct->ai4_pic_type_to_be_removed, 0, NUM_LAP2_LOOK_AHEAD * sizeof(WORD32));
    memset(&ps_lap_struct->ai4_num_buffer[0], 0, sizeof(ps_lap_struct->ai4_num_buffer));

    ps_lap_struct->i4_curr_poc = 0;
    ps_lap_struct->i4_cra_poc = 0;

    i4_max_temporal_layers = ps_lap_static_params->i4_max_temporal_layers;
    i4_src_interlace_field = ps_lap_static_params->i4_src_interlace_field;
    ps_lap_struct->i4_max_idr_period =
        ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period;
    ps_lap_struct->i4_min_idr_period =
        ps_static_cfg_prms->s_coding_tools_prms.i4_min_closed_gop_period;
    ps_lap_struct->i4_max_cra_period =
        ps_static_cfg_prms->s_coding_tools_prms.i4_max_cra_open_gop_period;
    ps_lap_struct->i4_max_i_period =
        ps_static_cfg_prms->s_coding_tools_prms.i4_max_i_open_gop_period;
    ps_lap_struct->i4_idr_counter = 0;
    ps_lap_struct->i4_cra_counter = 0;
    ps_lap_struct->i4_i_counter = 0;
    ps_lap_struct->i4_idr_gop_num = -1;
    ps_lap_struct->i4_curr_ref_pics = 0;
    ps_lap_struct->i4_display_num = 0;
    ps_lap_struct->i4_num_frm_type_decided = 0;
    ps_lap_struct->i4_next_start_ctr = 0;
    ps_lap_struct->ai1_pic_type[0] = PIC_TYPE_IDR;

    ps_lap_struct->i4_enable_logo = ps_lap_static_params->i4_enable_logo;
    ps_lap_struct->i4_cra_i_pic_flag = 0;
    ps_lap_struct->i4_force_end_flag = 0;
    ps_lap_struct->i4_sub_gop_size = (1 << i4_max_temporal_layers);
    ps_lap_struct->i4_sub_gop_size_idr =
        ps_lap_struct->i4_sub_gop_size + (i4_max_temporal_layers > 0);

    ps_lap_struct->i4_is_all_i_pic_in_seq = 0;

    if(ps_lap_struct->i4_max_idr_period == 1 || ps_lap_struct->i4_max_cra_period == 1 ||
       ps_lap_struct->i4_max_i_period == 1)
    {
        ps_lap_struct->i4_is_all_i_pic_in_seq = 1;
    }

    if(1 == i4_src_interlace_field && (!ps_lap_struct->i4_is_all_i_pic_in_seq))
    {
        ps_lap_struct->i4_sub_gop_size <<= 1;
        ps_lap_struct->i4_sub_gop_size_idr <<= 1;
    }

    ps_lap_struct->i4_fixed_open_gop_period = 1;
    ps_lap_struct->i4_fixed_i_period = 1;

    if(ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period <=
       ps_lap_struct->i4_sub_gop_size)
    {
        ps_lap_struct->i4_min_idr_period =
            ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period;
    }
    if(ps_lap_struct->i4_max_idr_period)
    {
        if(ps_lap_struct->i4_max_cra_period)
        {
            ps_lap_struct->i4_gop_period = ps_lap_struct->i4_max_cra_period;
        }
        else if(ps_lap_struct->i4_max_i_period)
        {
            ps_lap_struct->i4_gop_period = ps_lap_struct->i4_max_i_period;
        }
        else
        {
            ps_lap_struct->i4_gop_period = ps_lap_struct->i4_max_idr_period;
        }
    }
    else
    {
        if(ps_lap_struct->i4_max_i_period)
        {
            ps_lap_struct->i4_gop_period = ps_lap_struct->i4_max_i_period;
        }
        else if(ps_lap_struct->i4_max_cra_period)
        {
            ps_lap_struct->i4_gop_period = ps_lap_struct->i4_max_cra_period;
        }
    }

    if(!ps_lap_struct->i4_max_i_period)
    {
        ps_lap_struct->i4_max_i_period =
            2 * MAX(ps_lap_struct->i4_max_idr_period, ps_lap_struct->i4_max_cra_period);
    }

    ps_lap_struct->i4_no_back_to_back_i_avoidance = 0;

    /*Infinite GOP case*/
    if(!ps_lap_struct->i4_gop_period)
    {
        /*max signed 32 bit value which will be ~ 414 days considering 60frames/fields per second*/
        ps_lap_struct->i4_max_i_period = 0x7fffffff;
        ps_lap_struct->i4_gop_period =
            (INFINITE_GOP_CDR_TIME_S * (ps_static_cfg_prms->s_src_prms.i4_frm_rate_num /
                                        ps_static_cfg_prms->s_src_prms.i4_frm_rate_denom));
    }

    if(ps_lap_struct->i4_gop_period < (2 * ps_lap_struct->i4_sub_gop_size))
    {
        ps_lap_struct->i4_no_back_to_back_i_avoidance = 1;
    }

    ps_lap_struct->i4_rc_lap_period =
        ps_static_cfg_prms->s_lap_prms.i4_rc_look_ahead_pics + MIN_L1_L0_STAGGER_NON_SEQ;
    ps_lap_struct->pv_prev_inp_buf = NULL;
    ps_lap_struct->i4_buf_deq_idx = 0;
    ps_lap_struct->i4_deq_idx = 0;
    ps_lap_struct->i4_enq_idx = 0;
    ps_lap_struct->i4_lap2_counter = 0;
    ps_lap_struct->i4_dyn_sub_gop_size = ps_lap_struct->i4_sub_gop_size;
    ps_lap_struct->i4_buf_enq_idx = 0;
    ps_lap_struct->i4_lap_out_idx = 0;
    ps_lap_struct->i4_capture_idx = 0;
    ps_lap_struct->i4_idr_flag = 1;
    ps_lap_struct->i4_num_bufs_encode_order = 0;
    ps_lap_struct->end_flag = 0;
    ps_lap_struct->i4_immediate_idr_case = 0;
    ps_lap_struct->i4_max_buf_in_enc_order = 0;
    ps_lap_struct->i4_end_flag_pic_idx = 0;
    memset(
        &ps_lap_struct->api4_encode_order_array[0],
        0,
        sizeof(ihevce_lap_enc_buf_t *) * MAX_NUM_ENC_NODES);
    ps_lap_struct->i4_sub_gop_pic_idx = 0;
    ps_lap_struct->i4_force_idr_pos = 0;
    ps_lap_struct->i4_num_dummy_pic = 0;
    ps_lap_struct->i4_lap_encode_idx = 0;
    ps_lap_struct->i4_deq_lap_buf = 0;
    ps_lap_struct->i4_sub_gop_end = 0;

    {
        WORD32 node_offset, curr_layer;
        WORD32 i;
        /*intialization of aps_lap_inp_buf*/
        for(i = 0; i < MAX_QUEUE_LENGTH; i++)
        {
            ps_lap_struct->aps_lap_inp_buf[i] = NULL;
        }

        /* init capture order and encode order pointer */
        ps_lap_struct->pi4_capture_poc_ptr = &ps_lap_struct->ai4_capture_order_poc[0];
        ps_lap_struct->pi4_encode_poc_ptr = &ps_lap_struct->ai4_encode_order_poc[0];

        /* init all the buffer status to default values */
        ps_encode_node_struct = ps_lap_struct->aps_encode_node[0];

        ps_encode_node_struct->pv_left_node = NULL;
        ps_encode_node_struct->pv_right_node = NULL;

        /* Initialise the tree */
        node_offset = 1;
        curr_layer = 0;
        ihevce_populate_tree_nodes(
            ps_encode_node_struct,
            ps_encode_node_struct,
            &node_offset,
            curr_layer,
            ps_lap_static_params->i4_max_temporal_layers);
    }

    ps_mem_tab += NUM_LAP_MEM_RECS;

    return ((void *)ps_lap_struct);
}

/*!
******************************************************************************
* \if Function name : ihevce_populate_tree_nodes \endif
*
* \brief
*    LAP populate nodes function
*
* \param[in] encode_parent_node_t node pointer to base
*            encode_node_t        node pointer to current buffer
*            loop_count                layer count
*            hier_layer           total layers
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_populate_tree_nodes(
    ihevce_encode_node_t *encode_parent_node_t,
    ihevce_encode_node_t *encode_node_t,
    WORD32 *loop_count,
    WORD32 layer,
    WORD32 hier_layer)
{
    /* If only I/P pictures, return NULL from the child nodes*/
    if(hier_layer == 0)
    {
        encode_node_t->pv_left_node = NULL;
        encode_node_t->pv_right_node = NULL;
        return;
    }
    if(layer == hier_layer)
        return;

    layer = layer + 1;

    /* If the layers are not exhausted */
    if(layer < hier_layer)
    {
        encode_node_t->pv_left_node = encode_parent_node_t + (*loop_count);
        encode_node_t->pv_right_node = encode_parent_node_t + (*loop_count + 1);
        (*loop_count) = (*loop_count) + 2;
    }
    else
    {
        encode_node_t->pv_left_node = NULL;
        encode_node_t->pv_right_node = NULL;
    }

    /* Populate Left tree nodes */
    ihevce_populate_tree_nodes(
        encode_parent_node_t,
        (ihevce_encode_node_t *)encode_node_t->pv_left_node,
        loop_count,
        layer,
        hier_layer);

    /* Populate right tree nodes */
    ihevce_populate_tree_nodes(
        encode_parent_node_t,
        (ihevce_encode_node_t *)encode_node_t->pv_right_node,
        loop_count,
        layer,
        hier_layer);
}

/*!
************************************************************************
* \brief
*    pad input when its dimensions are not aligned to LCU size
************************************************************************
*/
void ihevce_lap_pad_input_bufs(
    ihevce_lap_enc_buf_t *ps_curr_inp, WORD32 align_pic_wd, WORD32 align_pic_ht)
{
    /* local variables */
    WORD32 ctr_horz, ctr_vert;

    /* ------- Horizontal Right Padding ------ */
    if(align_pic_wd != ps_curr_inp->s_lap_out.s_input_buf.i4_y_wd)
    {
        UWORD8 *pu1_inp;
        UWORD16 *pu2_inp;
        WORD32 pad_wd;
        WORD32 pad_ht;

        /* ------------- LUMA ----------------------------- */
        /* derive the pointers and dimensions to be padded */
        pad_ht = ps_curr_inp->s_lap_out.s_input_buf.i4_y_ht;
        pad_wd = align_pic_wd - ps_curr_inp->s_lap_out.s_input_buf.i4_y_wd;
        pu1_inp = (UWORD8 *)ps_curr_inp->s_lap_out.s_input_buf.pv_y_buf;
        pu1_inp += ps_curr_inp->s_lap_out.s_input_buf.i4_y_wd;

        /* loops for padding the right region for entire pic */
        for(ctr_vert = 0; ctr_vert < pad_ht; ctr_vert++)
        {
            for(ctr_horz = 0; ctr_horz < pad_wd; ctr_horz++)
            {
                /* last pixel is replicated */
                pu1_inp[ctr_horz] = pu1_inp[-1];
            }

            /* row level increments */
            pu1_inp += ps_curr_inp->s_lap_out.s_input_buf.i4_y_strd;
        }

        /* ------------- CHROMA ---------------------------- */
        /* derive the pointers and dimensions to be padded */
        pad_ht = ps_curr_inp->s_lap_out.s_input_buf.i4_uv_ht;
        pad_wd = align_pic_wd - ps_curr_inp->s_lap_out.s_input_buf.i4_uv_wd;
        pad_wd >>= 1;
        pu1_inp = (UWORD8 *)ps_curr_inp->s_lap_out.s_input_buf.pv_u_buf;
        pu2_inp = (UWORD16 *)(pu1_inp + ps_curr_inp->s_lap_out.s_input_buf.i4_uv_wd);

        /* loops for padding the right region for entire pic */
        for(ctr_vert = 0; ctr_vert < pad_ht; ctr_vert++)
        {
            for(ctr_horz = 0; ctr_horz < pad_wd; ctr_horz++)
            {
                /* last pixel is replicated, cb and cr pixel interleaved */
                pu2_inp[ctr_horz] = pu2_inp[-1];
            }

            /* row level increments */
            pu2_inp += (ps_curr_inp->s_lap_out.s_input_buf.i4_uv_strd >> 1);
        }
    }

    /* ------- Vertical Bottom Padding ------ */
    if(align_pic_ht != ps_curr_inp->s_lap_out.s_input_buf.i4_y_ht)
    {
        UWORD8 *pu1_inp, *pu1_src;
        WORD32 pad_ht;

        /* ------------- LUMA ----------------------------- */
        /* derive the pointers and dimensions to be padded */
        pad_ht = align_pic_ht - ps_curr_inp->s_lap_out.s_input_buf.i4_y_ht;
        pu1_inp = (UWORD8 *)ps_curr_inp->s_lap_out.s_input_buf.pv_y_buf;
        pu1_inp += ps_curr_inp->s_lap_out.s_input_buf.i4_y_ht *
                   ps_curr_inp->s_lap_out.s_input_buf.i4_y_strd;

        /* get the pointer of last row */
        pu1_src = pu1_inp - ps_curr_inp->s_lap_out.s_input_buf.i4_y_strd;

        /* loops for padding the bottom region for entire row */
        for(ctr_vert = 0; ctr_vert < pad_ht; ctr_vert++)
        {
            /* copy the eniter orw including horz padd region */
            memcpy(pu1_inp, pu1_src, align_pic_wd);

            /* row level increments */
            pu1_inp += ps_curr_inp->s_lap_out.s_input_buf.i4_y_strd;
        }

        /* ------------- CHROMA ----------------------------- */
        /* derive the pointers and dimensions to be padded */
        pad_ht = (align_pic_ht >> 1) - ps_curr_inp->s_lap_out.s_input_buf.i4_uv_ht;
        pu1_inp = (UWORD8 *)ps_curr_inp->s_lap_out.s_input_buf.pv_u_buf;
        pu1_inp += ps_curr_inp->s_lap_out.s_input_buf.i4_uv_ht *
                   ps_curr_inp->s_lap_out.s_input_buf.i4_uv_strd;

        /* get the pointer of last row */
        pu1_src = pu1_inp - ps_curr_inp->s_lap_out.s_input_buf.i4_uv_strd;

        /* loops for padding the bottom region for entire row */
        for(ctr_vert = 0; ctr_vert < pad_ht; ctr_vert++)
        {
            /* copy the eniter orw including horz padd region */
            memcpy(pu1_inp, pu1_src, align_pic_wd);

            /* row level increments */
            pu1_inp += ps_curr_inp->s_lap_out.s_input_buf.i4_uv_strd;
        }
    }
    return;
}

/*!
************************************************************************
* \brief
*    check for last inp buf
************************************************************************
*/
WORD32 ihevce_check_last_inp_buf(WORD32 *pi4_cmd_buf)
{
    WORD32 cmd = (*pi4_cmd_buf) & (IHEVCE_COMMANDS_TAG_MASK);

    if(IHEVCE_SYNCH_API_FLUSH_TAG == cmd)
        return 1;
    return 0;
}

/*!
************************************************************************
* \brief
*    lap parse sync commands
************************************************************************
*/
void ihevce_lap_parse_sync_cmd(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    ihevce_static_cfg_params_t *ps_static_cfg_prms,
    WORD32 *pi4_cmd_buf,
    ihevce_lap_enc_buf_t *ps_lap_inp_buf,
    WORD32 *pi4_flush_check,
    WORD32 *pi4_force_idr_check)
{
    WORD32 *pi4_tag_parse = pi4_cmd_buf;
    WORD32 i4_cmd_size = ps_lap_inp_buf->s_input_buf.i4_cmd_buf_size;
    WORD32 i4_buf_id = ps_lap_inp_buf->s_input_buf.i4_buf_id;
    UWORD32 u4_num_sei = 0;
    WORD32 i4_end_flag = 0;

    while(i4_cmd_size >= 4)
    {
        switch((*pi4_tag_parse) & (IHEVCE_COMMANDS_TAG_MASK))
        {
        case IHEVCE_SYNCH_API_FLUSH_TAG:
            if(i4_cmd_size < 8 || pi4_tag_parse[1])
            {
                ps_hle_ctxt->ihevce_cmds_error_report(
                    ps_hle_ctxt->pv_cmd_err_cb_handle,
                    IHEVCE_SYNCH_ERR_LENGTH_NOT_ZERO,
                    1,
                    i4_buf_id);
                return;
            }
            (*pi4_flush_check) = 1;
            pi4_tag_parse += 2;
            i4_cmd_size -= 8;
            u4_num_sei++;
            break;
        case IHEVCE_SYNCH_API_FORCE_IDR_TAG:
            if(i4_cmd_size < 8 || pi4_tag_parse[1])
            {
                ps_hle_ctxt->ihevce_cmds_error_report(
                    ps_hle_ctxt->pv_cmd_err_cb_handle,
                    IHEVCE_SYNCH_ERR_LENGTH_NOT_ZERO,
                    1,
                    i4_buf_id);
                return;
            }
            (*pi4_force_idr_check) = 1;
            pi4_tag_parse += 2;
            i4_cmd_size -= 8;
            u4_num_sei++;
            break;
        case IHEVCE_SYNCH_API_END_TAG:
            i4_end_flag = 1;
            i4_cmd_size -= 4;
            break;
        default:
            ps_hle_ctxt->ihevce_cmds_error_report(
                ps_hle_ctxt->pv_cmd_err_cb_handle, IHEVCE_SYNCH_ERR_TLV_ERROR, 1, i4_buf_id);
            i4_end_flag = 1;
        }
        if(i4_end_flag)
            break;
    }
    if(u4_num_sei > MAX_NUMBER_OF_SEI_PAYLOAD)  //Checking for max number of SEI messages.
        ps_hle_ctxt->ihevce_cmds_error_report(
            ps_hle_ctxt->pv_cmd_err_cb_handle, IHEVCE_SYNCH_ERR_TOO_MANY_SEI_MSG, 1, i4_buf_id);

    if(!i4_end_flag)
        ps_hle_ctxt->ihevce_cmds_error_report(
            ps_hle_ctxt->pv_cmd_err_cb_handle, IHEVCE_SYNCH_ERR_NO_END_TAG, 1, i4_buf_id);
}

/*!
************************************************************************
* \brief
*    lap parse Async commands
************************************************************************
*/
void ihevce_lap_parse_async_cmd(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    WORD32 *pi4_cmd_buf,
    WORD32 i4_length,
    WORD32 i4_buf_id,
    WORD32 *pi4_num_set_bitrate_cmds,
    ihevce_dyn_config_prms_t *ps_dyn_br)
{
    WORD32 i4_end_flag = 0;
    WORD32 *pi4_tag_parse = pi4_cmd_buf;

    while(i4_length >= 4)
    {
        switch(*pi4_tag_parse)
        {
        case IHEVCE_ASYNCH_API_SETBITRATE_TAG:
            if(i4_length < (8 + sizeof(ihevce_dyn_config_prms_t)) ||
               pi4_tag_parse[1] != sizeof(ihevce_dyn_config_prms_t))
            {
                ps_hle_ctxt->ihevce_cmds_error_report(
                    ps_hle_ctxt->pv_cmd_err_cb_handle, IHEVCE_ASYNCH_ERR_BR_NOT_BYTE, 1, i4_buf_id);
                return;
            }
            memcpy(
                (void *)ps_dyn_br, (void *)(pi4_tag_parse + 2), sizeof(ihevce_dyn_config_prms_t));
            pi4_tag_parse += (2 + (sizeof(ihevce_dyn_config_prms_t) >> 2));
            i4_length -= (8 + sizeof(ihevce_dyn_config_prms_t));
            *pi4_num_set_bitrate_cmds += 1;
            ps_dyn_br++;
            break;
        case IHEVCE_ASYNCH_API_END_TAG:
            i4_end_flag = 1;
            i4_length -= 4;
            break;
        default:
            ps_hle_ctxt->ihevce_cmds_error_report(
                ps_hle_ctxt->pv_cmd_err_cb_handle, IHEVCE_ASYNCH_ERR_TLV_ERROR, 1, i4_buf_id);
            i4_end_flag = 1;
        }
        if(i4_end_flag)
            break;
    }
    if(!i4_end_flag)
        ps_hle_ctxt->ihevce_cmds_error_report(
            ps_hle_ctxt->pv_cmd_err_cb_handle, IHEVCE_ASYNCH_ERR_NO_END_TAG, 1, i4_buf_id);
}

/*!
************************************************************************
* \brief
*    ref pics weight offset calculation
************************************************************************
*/
void ref_pics_weight_offset_calc(ihevce_lap_output_params_t *ps_lap_out, lap_struct_t *ps_lap_struct)
{
    WORD32 i, j;
    WORD32 *ref_poc_array = ps_lap_struct->ref_poc_array;
    WORD32 ai4_delta_poc[MAX_REF_PICS];
    WORD32 ref_poc_arr_sort[MAX_REF_PICS];

    /* Default weighted pred parameters populated for  now */
    ps_lap_out->i4_log2_luma_wght_denom = DENOM_DEFAULT;
    ps_lap_out->i4_log2_chroma_wght_denom = DENOM_DEFAULT;

    /* sort the ref_poc_array based on delta as
     * in case weighted pred dup pics are inserted and it should consider
     * the neighbors first for prediction than farthest */
    for(i = 0; i < ps_lap_struct->i4_curr_ref_pics; i++)
    {
        ai4_delta_poc[i] = ref_poc_array[i] - ps_lap_out->i4_poc;
    }

    for(i = 0; i < ps_lap_struct->i4_curr_ref_pics; i++)
    {
        WORD32 i4_min, temp;
        i4_min = i;
        for(j = i; j < ps_lap_struct->i4_curr_ref_pics; j++)
        {
            if(abs(ai4_delta_poc[j]) <= abs(ai4_delta_poc[i4_min]))
            {
                i4_min = j;
            }
        }
        temp = ai4_delta_poc[i];
        ai4_delta_poc[i] = ai4_delta_poc[i4_min];
        ai4_delta_poc[i4_min] = temp;
        ref_poc_arr_sort[i] = ai4_delta_poc[i] + ps_lap_out->i4_poc;
    }

    for(i = 0; i < ps_lap_struct->i4_curr_ref_pics; i++)
    {
        ps_lap_out->as_ref_pics[i].i4_ref_pic_delta_poc = ref_poc_arr_sort[i] - ps_lap_out->i4_poc;
        ASSERT(ps_lap_out->as_ref_pics[i].i4_ref_pic_delta_poc);

        /* Enable flag for the reference pics to be used by curr pic */
        ps_lap_out->as_ref_pics[i].i4_used_by_cur_pic_flag = gau1_use_by_cur_pic_flag[i];

        /* Currently no weighted prediction offset added */
        ps_lap_out->as_ref_pics[i].i4_num_duplicate_entries_in_ref_list = 1;
    }
    return;
}

/*!
************************************************************************
* \brief
*    ref b picture population
************************************************************************
*/
void ref_b_pic_population(
    WORD32 curr_layer, ihevce_lap_enc_buf_t *ps_lap_inp, lap_struct_t *ps_lap_struct)
{
    ihevce_lap_output_params_t *ps_lap_out = &ps_lap_inp->s_lap_out;
    WORD32 *ref_poc_array = ps_lap_struct->ref_poc_array;
    WORD32 *p_ref_poc_array = ref_poc_array;
    WORD32 i4_interlace_field = ps_lap_struct->s_lap_static_params.i4_src_interlace_field;
    WORD32 i4_max_ref_pics = ps_lap_struct->s_lap_static_params.i4_max_reference_frames;
    WORD32 max_temporal_layers = ps_lap_struct->s_lap_static_params.i4_max_temporal_layers;

    /* LAP output structure */
    ps_lap_out->i4_poc = ps_lap_struct->pi4_encode_poc_ptr[0];
    ps_lap_out->i4_idr_gop_num = ps_lap_struct->i4_idr_gop_num;
    ps_lap_out->i4_assoc_IRAP_poc = ps_lap_struct->i4_assoc_IRAP_poc;
    ps_lap_out->i4_temporal_lyr_id = curr_layer;
    ps_lap_out->i4_pic_type = IV_B_FRAME;

    if((ps_lap_out->i4_poc > ps_lap_struct->i4_cra_poc) &&
       (ref_poc_array[0] < ps_lap_struct->i4_cra_poc) && ps_lap_struct->i4_cra_i_pic_flag)
    {
        ref_poc_array[0] = ps_lap_struct->i4_cra_poc;
        ps_lap_struct->i4_curr_ref_pics = 1;
    }

    ps_lap_out->i4_num_ref_pics = ps_lap_struct->i4_curr_ref_pics;

    /* Default: Cur pic is ref pic*/
    ps_lap_out->i4_is_ref_pic = 1;

    if(1 == i4_interlace_field)
    {
        WORD32 i4_bottom_field = ps_lap_inp->s_input_buf.i4_bottom_field;
        WORD32 first_field = (ps_lap_inp->s_input_buf.i4_topfield_first ^ i4_bottom_field);

        /*If current pic is top field B picture and is present in top hierarchical layer */
        /* Dereference the curr pic */
        if(ps_lap_out->i4_temporal_lyr_id == max_temporal_layers)
        {
            if(0 == first_field)
                ps_lap_out->i4_is_ref_pic = 0;
            else
                ps_lap_out->i4_is_ref_pic = 2;
        }
    }
    else
    {
        /*If progressive B picture and is present in top hierarchical layer */
        if(ps_lap_out->i4_temporal_lyr_id >= max_temporal_layers)
        {
            ps_lap_out->i4_temporal_lyr_id = max_temporal_layers;
            ps_lap_out->i4_is_ref_pic = 0;
        }
    }

    ref_pics_weight_offset_calc(ps_lap_out, ps_lap_struct);

    /* Updating number of current reference Pictures for the Given Picture */
    /* If the current frame is n-layer B frame, donot increment*/
    if(ps_lap_struct->i4_curr_ref_pics < i4_max_ref_pics)
    {
        if(ps_lap_out->i4_is_ref_pic)
        {
            ps_lap_struct->i4_curr_ref_pics++;
        }
    }

    /* Arrange the reference array in ascending order */
    {
        WORD32 i, j, temp;
        for(i = 0; i < (ps_lap_struct->i4_curr_ref_pics - 1); i++)
        {
            for(j = i + 1; j < ps_lap_struct->i4_curr_ref_pics; j++)
            {
                if(ref_poc_array[i] > ref_poc_array[j])
                {
                    temp = ref_poc_array[i];
                    ref_poc_array[i] = ref_poc_array[j];
                    ref_poc_array[j] = temp;
                }
            }
        }
    }

    {
        WORD32 ref = ps_lap_out->i4_poc;
        if(ps_lap_out->i4_is_ref_pic && ref > *p_ref_poc_array)
        {
            *p_ref_poc_array = ref;
        }
    }

    return;
}

/*!
************************************************************************
* \brief
*    ref i/p pic population
************************************************************************
*/
void ref_pic_population(ihevce_lap_enc_buf_t *ps_lap_inp, lap_struct_t *ps_lap_struct)
{
    ihevce_lap_output_params_t *ps_lap_out = &ps_lap_inp->s_lap_out;
    WORD32 *ref_poc_array = ps_lap_struct->ref_poc_array;
    WORD32 *p_ref_poc_array = ref_poc_array;
    WORD32 i4_max_ref_pics = ps_lap_struct->s_lap_static_params.i4_max_reference_frames;

    /* Update the POC position */
    ps_lap_out->i4_poc = ps_lap_struct->pi4_encode_poc_ptr[0];

    /* picture after CRA can't refer pic before CRA*/
    if((ps_lap_out->i4_poc > ps_lap_struct->i4_cra_poc) &&
       (ref_poc_array[0] <= ps_lap_struct->i4_cra_poc) && ps_lap_struct->i4_cra_i_pic_flag)
    {
        ref_poc_array[0] = ps_lap_struct->i4_cra_poc;
        ps_lap_struct->i4_curr_ref_pics = 1;
    }

    /* For every IDR period, set pic type as IDR frame and reset reference POC array to 0*/
    if(IV_IDR_FRAME == ps_lap_out->i4_pic_type)
    {
        ps_lap_struct->i4_idr_gop_num++;
        ps_lap_struct->i4_curr_ref_pics = 0;
        ps_lap_out->i4_num_ref_pics = 0;
        ps_lap_struct->i4_cra_i_pic_flag = 1;
        ps_lap_struct->i4_cra_poc = ps_lap_out->i4_poc;

        memset(ps_lap_struct->ref_poc_array, 0xFF, sizeof(WORD32) * MAX_REF_PICS);
    }
    else if(IV_I_FRAME == ps_lap_out->i4_pic_type)
    {
        /* For the I-frames after CRA Frame, no pictures should be referenced */
        if((1 == ps_lap_struct->i4_cra_i_pic_flag) && ps_lap_out->i4_is_cra_pic)
        {
            ps_lap_struct->i4_curr_ref_pics = 0;
            ps_lap_out->i4_num_ref_pics = 0;
        }
        ps_lap_struct->i4_cra_poc = ps_lap_out->i4_poc;
        ps_lap_struct->i4_cra_i_pic_flag = ps_lap_out->i4_is_cra_pic;
    }
    else if(IV_P_FRAME == ps_lap_out->i4_pic_type)
    {
        /* If the current POC is the P POC after CRA I POC */
        if(1 == ps_lap_struct->i4_cra_i_pic_flag)
        {
            ps_lap_struct->i4_curr_ref_pics = 1;
            ps_lap_struct->i4_cra_i_pic_flag = 0;
        }
    }

    if(ps_lap_out->i4_pic_type == IV_IDR_FRAME ||
       (ps_lap_out->i4_pic_type == IV_I_FRAME && ps_lap_out->i4_is_cra_pic))
    {
        ps_lap_struct->i4_assoc_IRAP_poc = ps_lap_out->i4_poc;
    }

    /*Update ps_lap_out*/
    ps_lap_out->i4_idr_gop_num = ps_lap_struct->i4_idr_gop_num;
    ps_lap_out->i4_is_ref_pic = 1;
    ps_lap_out->i4_assoc_IRAP_poc = ps_lap_struct->i4_assoc_IRAP_poc;

    /* Reference POCS */
    ps_lap_out->i4_num_ref_pics = ps_lap_struct->i4_curr_ref_pics;

    /* I and P frames are always mapped to layer zero*/
    ps_lap_out->i4_temporal_lyr_id = 0;

    ref_pics_weight_offset_calc(ps_lap_out, ps_lap_struct);

    if(ps_lap_struct->i4_curr_ref_pics < i4_max_ref_pics)
    {
        if(ps_lap_out->i4_is_ref_pic)
        {
            ps_lap_struct->i4_curr_ref_pics++;
        }
    }

    /* Arrange the reference array in ascending order */
    {
        WORD32 i, j, temp;
        for(i = 0; i < (ps_lap_struct->i4_curr_ref_pics - 1); i++)
        {
            for(j = i + 1; j < (ps_lap_struct->i4_curr_ref_pics); j++)
            {
                if(ref_poc_array[i] > ref_poc_array[j])
                {
                    temp = ref_poc_array[i];
                    ref_poc_array[i] = ref_poc_array[j];
                    ref_poc_array[j] = temp;
                }
            }
        }
    }

    {
        /* add the current pictute at the start of the reference queue */
        /*For I and P pictures, all the previous frames are reference frames */
        /* If the current ref POC is greater than the least POC in reference array*/
        /* Then fill the reference array */

        WORD32 ref = ps_lap_out->i4_poc;

        if(ps_lap_out->i4_is_ref_pic && ref > *p_ref_poc_array)
        {
            *p_ref_poc_array = ref;
        }
    }

    return;
}

/*!
************************************************************************
* \brief
*    determine next sub-gop state
************************************************************************
*/
void ihevce_determine_next_sub_gop_state(lap_struct_t *ps_lap_struct)
{
    WORD32 i4_num_b_frames = -1;
    WORD32 i4_sd = ps_lap_struct->i4_sub_gop_size;
    WORD32 i4_sd_idr = ps_lap_struct->i4_sub_gop_size_idr;
    WORD32 i4_Midr = ps_lap_struct->i4_max_idr_period;
    WORD32 i4_midr = ps_lap_struct->i4_min_idr_period;
    WORD32 i4_Mcra = ps_lap_struct->i4_max_cra_period;
    WORD32 i4_Mi = ps_lap_struct->i4_max_i_period;
    WORD32 i4_Cd = ps_lap_struct->i4_idr_counter;
    WORD32 i4_Cc = ps_lap_struct->i4_cra_counter;
    WORD32 i4_Ci = ps_lap_struct->i4_i_counter;

    if(ps_lap_struct->i4_force_idr_pos)
    {
        ps_lap_struct->i4_num_frm_type_decided = 1;
        ps_lap_struct->ai1_pic_type[0] = PIC_TYPE_IDR;
        ps_lap_struct->i4_idr_counter = 0;
        ps_lap_struct->i4_cra_counter = 0;
        ps_lap_struct->i4_i_counter = 0;
        ps_lap_struct->i4_force_idr_pos = 0;
        ps_lap_struct->i4_sub_gop_pic_idx = 0;
    }

    if(i4_Midr)
        ASSERT(i4_Cd < i4_Midr);

    if(i4_Mcra)
        ASSERT(i4_Cc < i4_Mcra);

    if(i4_Mi)
        ASSERT(i4_Ci < i4_Mi);

    /*if all are i pictures */
    if((i4_Midr == 1) || (i4_Mcra == 1) || (i4_Mi == 1))
    {
        ps_lap_struct->i4_num_frm_type_decided = 1;
        if((i4_Midr == 1) || ((i4_Cd + i4_sd) == i4_Midr))
        {
            ps_lap_struct->ai1_pic_type[1] = PIC_TYPE_IDR;
            ps_lap_struct->i4_idr_counter = 0;
            ps_lap_struct->i4_cra_counter = 0;
            ps_lap_struct->i4_i_counter = 0;
        }
        else if((i4_Mcra == 1) || ((i4_Cc + i4_sd) == i4_Mcra))
        {
            ps_lap_struct->ai1_pic_type[1] = PIC_TYPE_CRA;
            ps_lap_struct->i4_idr_counter += 1;
            ps_lap_struct->i4_cra_counter = 0;
            ps_lap_struct->i4_i_counter = 0;
        }
        else
        {
            ps_lap_struct->ai1_pic_type[1] = PIC_TYPE_I;
            ps_lap_struct->i4_idr_counter += 1;
            ps_lap_struct->i4_cra_counter += 1;
            ps_lap_struct->i4_i_counter = 0;
        }
        return;
    }

    if((i4_Cd + i4_sd_idr >= i4_Midr) && i4_Midr)
    {
        /*if idr falls already on sub-gop aligned w.r.t Midr or if strict idr use case*/
        if(i4_sd_idr != i4_sd)
        {
            i4_num_b_frames = i4_Midr - i4_Cd - 2;
            memset(&ps_lap_struct->ai1_pic_type[1], PIC_TYPE_B, i4_num_b_frames);
            ps_lap_struct->ai1_pic_type[i4_num_b_frames + 1] = PIC_TYPE_P;
            ps_lap_struct->ai1_pic_type[i4_num_b_frames + 2] = PIC_TYPE_IDR;
            ps_lap_struct->i4_num_frm_type_decided = i4_num_b_frames + 2;
            ps_lap_struct->i4_idr_counter = 0;
            ps_lap_struct->i4_cra_counter = 0;
            ps_lap_struct->i4_i_counter = 0;
        }
        else
        {
            i4_num_b_frames = 0;
            ps_lap_struct->ai1_pic_type[1] = PIC_TYPE_IDR;
            ps_lap_struct->i4_num_frm_type_decided = 1;
            ps_lap_struct->i4_idr_counter = 0;
            ps_lap_struct->i4_cra_counter = 0;
            ps_lap_struct->i4_i_counter = 0;
        }
    }
    /*if next sub gop is going to have CRA as Cc reaches Mcra*/
    else if(((i4_Cc + i4_sd) >= i4_Mcra) && i4_Mcra)
    {
        if(((i4_Cc + i4_sd) == i4_Mcra) || (1 == ps_lap_struct->i4_fixed_open_gop_period))
        {
            i4_num_b_frames = i4_Mcra - i4_Cc - 1;
            memset(&ps_lap_struct->ai1_pic_type[1], PIC_TYPE_B, i4_num_b_frames);
            ps_lap_struct->ai1_pic_type[i4_num_b_frames + 1] = PIC_TYPE_CRA;
            ps_lap_struct->i4_num_frm_type_decided = i4_num_b_frames + 1;
            ps_lap_struct->i4_idr_counter += ps_lap_struct->i4_num_frm_type_decided;
            ps_lap_struct->i4_cra_counter = 0;
            ps_lap_struct->i4_i_counter = 0;
        }
        else
        {
            ps_lap_struct->ai1_pic_type[0] = PIC_TYPE_CRA;
            i4_num_b_frames = i4_sd - 1;
            memset(&ps_lap_struct->ai1_pic_type[1], PIC_TYPE_B, i4_num_b_frames);
            ps_lap_struct->ai1_pic_type[i4_num_b_frames + 1] = PIC_TYPE_P;
            ps_lap_struct->i4_num_frm_type_decided = i4_num_b_frames + 1;
            ps_lap_struct->i4_idr_counter += ps_lap_struct->i4_num_frm_type_decided;
            ps_lap_struct->i4_cra_counter = ps_lap_struct->i4_num_frm_type_decided;
            ps_lap_struct->i4_i_counter = ps_lap_struct->i4_num_frm_type_decided;
        }
    }
    /*if next sub gop is going to have I_slice as Ci reaches Mi*/
    else if((i4_Ci + i4_sd >= i4_Mi) && i4_Mi)
    {
        if(((i4_Ci + i4_sd) == i4_Mi) || (1 == ps_lap_struct->i4_fixed_i_period))
        {
            i4_num_b_frames = i4_Mi - i4_Ci - 1;
            memset(&ps_lap_struct->ai1_pic_type[1], PIC_TYPE_B, i4_num_b_frames);
            ps_lap_struct->ai1_pic_type[i4_num_b_frames + 1] = PIC_TYPE_I;
            ps_lap_struct->i4_num_frm_type_decided = i4_num_b_frames + 1;
            ps_lap_struct->i4_idr_counter += ps_lap_struct->i4_num_frm_type_decided;
            ps_lap_struct->i4_cra_counter += ps_lap_struct->i4_num_frm_type_decided;
            ps_lap_struct->i4_i_counter = 0;
        }
        else
        {
            ps_lap_struct->ai1_pic_type[0] = PIC_TYPE_I;
            i4_num_b_frames = i4_sd - 1;
            memset(&ps_lap_struct->ai1_pic_type[1], PIC_TYPE_B, i4_num_b_frames);
            ps_lap_struct->ai1_pic_type[i4_num_b_frames + 1] = PIC_TYPE_P;
            ps_lap_struct->i4_num_frm_type_decided = i4_num_b_frames + 1;
            ps_lap_struct->i4_idr_counter += ps_lap_struct->i4_num_frm_type_decided;
            ps_lap_struct->i4_cra_counter += ps_lap_struct->i4_num_frm_type_decided;
            ps_lap_struct->i4_i_counter = ps_lap_struct->i4_num_frm_type_decided;
        }
    }
    /* if next sub-gop is not going to be idr,cra,I*/
    else
    {
        i4_num_b_frames = i4_sd - 1;
        memset(&ps_lap_struct->ai1_pic_type[1], PIC_TYPE_B, i4_num_b_frames);
        ps_lap_struct->ai1_pic_type[i4_num_b_frames + 1] = PIC_TYPE_P;
        ps_lap_struct->i4_num_frm_type_decided = i4_num_b_frames + 1;
        ps_lap_struct->i4_idr_counter += ps_lap_struct->i4_num_frm_type_decided;
        ps_lap_struct->i4_cra_counter += ps_lap_struct->i4_num_frm_type_decided;
        ps_lap_struct->i4_i_counter += ps_lap_struct->i4_num_frm_type_decided;
    }
    ASSERT(i4_num_b_frames != -1);

    return;
}

/*!
************************************************************************
* \brief
*    assign pic type to input buf
************************************************************************
*/
void ihevce_assign_pic_type(lap_struct_t *ps_lap_struct, ihevce_lap_enc_buf_t *ps_lap_inp_buf)
{
    WORD8 pic_type = ps_lap_struct->ai1_pic_type[ps_lap_struct->i4_next_start_ctr];

    switch(pic_type)
    {
    case PIC_TYPE_I:
    {
        ps_lap_inp_buf->s_lap_out.i4_pic_type = IV_I_FRAME;
        ps_lap_inp_buf->s_lap_out.i4_is_cra_pic = 0;
        ps_lap_inp_buf->s_lap_out.i4_is_I_in_any_field = 1;
        break;
    }
    case PIC_TYPE_P:
    {
        ps_lap_inp_buf->s_lap_out.i4_pic_type = IV_P_FRAME;
        ps_lap_inp_buf->s_lap_out.i4_is_cra_pic = 0;
        break;
    }
    case PIC_TYPE_B:
    {
        ps_lap_inp_buf->s_lap_out.i4_pic_type = IV_B_FRAME;
        ps_lap_inp_buf->s_lap_out.i4_is_cra_pic = 0;
        break;
    }
    case PIC_TYPE_IDR:
    {
        ps_lap_struct->i4_curr_poc = 0;
        ps_lap_inp_buf->s_lap_out.i4_pic_type = IV_IDR_FRAME;
        ps_lap_inp_buf->s_lap_out.i4_is_cra_pic = 0;
        break;
    }
    case PIC_TYPE_CRA:
    {
        ps_lap_inp_buf->s_lap_out.i4_pic_type = IV_I_FRAME;
        ps_lap_inp_buf->s_lap_out.i4_is_I_in_any_field = 1;
        ps_lap_inp_buf->s_lap_out.i4_is_cra_pic = 1;
        break;
    }
    default:
        ASSERT(0);
    }
    return;
}

/*!
************************************************************************
* \brief
*    capture order traversal nodes
************************************************************************
*/
void ihevce_encode_order_traversal_nodes(
    ihevce_encode_node_t *encode_node_t,
    ihevce_lap_enc_buf_t **encode_order,
    WORD32 *loop_count,
    WORD32 curr_layer,
    lap_struct_t *ps_lap_struct)
{
    if(encode_node_t == NULL)
        return;

    encode_order[*loop_count] = (ihevce_lap_enc_buf_t *)encode_node_t->ps_lap_top_buff;

    if(encode_order[*loop_count] != NULL)
    {
        ihevce_lap_enc_buf_t *ps_lap_inp;

        ps_lap_struct->pi4_encode_poc_ptr[0] = encode_node_t->data;
        ref_b_pic_population(curr_layer, encode_order[*loop_count], ps_lap_struct);

        ps_lap_inp = (ihevce_lap_enc_buf_t *)encode_order[*loop_count];
        ihevce_rc_populate_common_params(&ps_lap_inp->s_lap_out, &ps_lap_inp->s_rc_lap_out);

        ps_lap_struct->pi4_encode_poc_ptr++;
    }

    (*loop_count) = (*loop_count) + 1;

    /* Pre-order Left-node traversal*/
    ihevce_encode_order_traversal_nodes(
        (ihevce_encode_node_t *)encode_node_t->pv_left_node,
        encode_order,
        loop_count,
        curr_layer + 1,
        ps_lap_struct);

    /* Pre-order Right-node traversal*/
    ihevce_encode_order_traversal_nodes(
        (ihevce_encode_node_t *)encode_node_t->pv_right_node,
        encode_order,
        loop_count,
        curr_layer + 1,
        ps_lap_struct);
}

/*!
************************************************************************
* \brief
*    capture order traversal nodes
************************************************************************
*/
void ihevce_capture_order_traversal_nodes(
    ihevce_encode_node_t *encode_node_t,
    ihevce_lap_enc_buf_t **api4_capture_order_array,
    WORD32 *capture_order_poc_array,
    WORD32 *loop_count,
    WORD32 i4_interlace_field)
{
    if(encode_node_t == NULL)
        return;

    /* Inorder Insertion for the left-child node */
    ihevce_capture_order_traversal_nodes(
        (ihevce_encode_node_t *)encode_node_t->pv_left_node,
        api4_capture_order_array,
        capture_order_poc_array,
        loop_count,
        i4_interlace_field);

    if(i4_interlace_field)
    {
        encode_node_t->ps_lap_top_buff =
            (ihevce_lap_enc_buf_t *)api4_capture_order_array[*loop_count];
        encode_node_t->data = capture_order_poc_array[*loop_count];
        encode_node_t->ps_lap_bottom_buff =
            (ihevce_lap_enc_buf_t *)api4_capture_order_array[*loop_count + 1];
    }
    else
    {
        encode_node_t->ps_lap_top_buff =
            (ihevce_lap_enc_buf_t *)api4_capture_order_array[*loop_count];
        encode_node_t->data = capture_order_poc_array[*loop_count];
    }
    if(i4_interlace_field)
        (*loop_count) = (*loop_count) + 2;
    else
        (*loop_count) = (*loop_count) + 1;

    /* Inorder Insertion for the right-child node */
    ihevce_capture_order_traversal_nodes(
        (ihevce_encode_node_t *)encode_node_t->pv_right_node,
        api4_capture_order_array,
        capture_order_poc_array,
        loop_count,
        i4_interlace_field);
}

/*!
************************************************************************
* \brief
*    I/P pic population
************************************************************************
*/
void ihevce_ip_pic_population(
    ihevce_encode_node_t *ps_encode_node, lap_struct_t *ps_lap_struct, WORD32 i4_first_gop)
{
    ihevce_lap_enc_buf_t *ps_lap_inp = NULL;
    WORD32 hier_layer = ps_lap_struct->s_lap_static_params.i4_max_temporal_layers;
    WORD32 sub_gop_size = ps_lap_struct->i4_dyn_sub_gop_size;
    ihevce_lap_enc_buf_t **api4_capture_order_array = ps_lap_struct->api4_capture_order_array;
    ihevce_lap_enc_buf_t **api4_encode_order_array = ps_lap_struct->api4_encode_order_array;
    WORD32 *ai4_capture_order_poc = ps_lap_struct->pi4_capture_poc_ptr;

    /* Populate the encode order POC dependent on IDR frames and Interlace Field*/
    if(1 == ps_lap_struct->i4_idr_flag)
    {
        if(i4_first_gop)
        {
            api4_encode_order_array[0] = api4_capture_order_array[0];

            if(api4_encode_order_array[0] != NULL)
            {
                ps_lap_struct->pi4_encode_poc_ptr[0] = ai4_capture_order_poc[0];
                ref_pic_population(api4_encode_order_array[0], ps_lap_struct);

                ps_lap_inp = api4_encode_order_array[0];
                ihevce_rc_populate_common_params(&ps_lap_inp->s_lap_out, &ps_lap_inp->s_rc_lap_out);

                ps_lap_struct->pi4_encode_poc_ptr++;
            }

            if(ps_lap_struct->i4_immediate_idr_case != 1)
            {
                api4_encode_order_array[1] = api4_capture_order_array[sub_gop_size];

                if(api4_encode_order_array[1] != NULL)
                {
                    ps_lap_struct->pi4_encode_poc_ptr[0] = ai4_capture_order_poc[sub_gop_size];
                    ref_pic_population(api4_encode_order_array[1], ps_lap_struct);

                    ps_lap_inp = api4_encode_order_array[1];
                    ihevce_rc_populate_common_params(
                        &ps_lap_inp->s_lap_out, &ps_lap_inp->s_rc_lap_out);

                    ps_lap_struct->pi4_encode_poc_ptr++;
                }
            }
        }
        else
        {
            api4_encode_order_array[0] = api4_capture_order_array[sub_gop_size - 1];

            if(api4_encode_order_array[0] != NULL)
            {
                ps_lap_struct->pi4_encode_poc_ptr[0] = ai4_capture_order_poc[sub_gop_size - 1];
                ref_pic_population(api4_encode_order_array[0], ps_lap_struct);

                ps_lap_inp = api4_encode_order_array[0];
                ihevce_rc_populate_common_params(&ps_lap_inp->s_lap_out, &ps_lap_inp->s_rc_lap_out);

                ps_lap_struct->pi4_encode_poc_ptr++;
            }
        }
    }
    else
    {
        api4_encode_order_array[0] = api4_capture_order_array[sub_gop_size - 1];

        if(api4_encode_order_array[0] != NULL)
        {
            ps_lap_struct->pi4_encode_poc_ptr[0] = ai4_capture_order_poc[sub_gop_size - 1];
            ref_pic_population(api4_encode_order_array[0], ps_lap_struct);

            ps_lap_inp = api4_encode_order_array[0];
            ihevce_rc_populate_common_params(&ps_lap_inp->s_lap_out, &ps_lap_inp->s_rc_lap_out);

            ps_lap_struct->pi4_encode_poc_ptr++;
        }
    }
    return;
}

/*!
************************************************************************
* \brief
*    B pic population
************************************************************************
*/
void ihevce_b_pic_population(ihevce_encode_node_t *ps_encode_node, lap_struct_t *ps_lap_struct)
{
    WORD32 interlace_field = ps_lap_struct->s_lap_static_params.i4_src_interlace_field;
    ihevce_lap_enc_buf_t **api4_encode_order_array = ps_lap_struct->api4_encode_order_array;
    WORD32 *capture_order_poc_array = ps_lap_struct->pi4_capture_poc_ptr;
    WORD32 loop_count = 0;

    /* encoder_order offset changed dependent on IDR and Interlace Field */
    if(ps_lap_struct->i4_idr_flag)
        loop_count = 1 + interlace_field;

    /* Inorder Insertion of POC in tree, for capture order */
    ihevce_capture_order_traversal_nodes(
        ps_encode_node,
        &ps_lap_struct->api4_capture_order_array[0],
        capture_order_poc_array,
        &loop_count,
        interlace_field);

    /* encoder_order offset changed dependent on IDR and Interlace Field */
    /* If the gop_size is multiple of CRA period , decrement loop count */
    if(ps_lap_struct->i4_idr_flag)
        loop_count = 2 + (interlace_field * 2);
    else
        loop_count = 1 + interlace_field;

    /* Pre-order traversal of the tree to get encode-order POCs*/
    ihevce_encode_order_traversal_nodes(
        ps_encode_node, api4_encode_order_array, &loop_count, 1, ps_lap_struct);

    return;
}

/*!
************************************************************************
* \brief
*    rc_update_model_control_by_lap_for_modified_sub_gop
************************************************************************
*/
void rc_update_model_control_by_lap_for_modified_sub_gop(
    lap_struct_t *ps_lap_struct, ihevce_lap_enc_buf_t *ps_lap_out_buf)
{
    ihevce_lap_output_params_t *ps_lap_out = &ps_lap_out_buf->s_lap_out;

    /* model update flag for rc*/
    if(ps_lap_out->i4_pic_type == IV_P_FRAME)
    {
        WORD32 i4_loop = 0;
        WORD32 i4_min_delta_poc = 0x7FFFFFFF;

        for(i4_loop = 0; i4_loop < ps_lap_out->i4_num_ref_pics; i4_loop++)
        {
            if(i4_min_delta_poc > ABS(ps_lap_out->as_ref_pics[i4_loop].i4_ref_pic_delta_poc))
            {
                i4_min_delta_poc = ABS(ps_lap_out->as_ref_pics[i4_loop].i4_ref_pic_delta_poc);
            }
        }
    }

    if(ps_lap_out->i4_pic_type == IV_B_FRAME)
    {
        WORD32 i4_loop = 0;
        WORD32 i4_min_delta_poc = 0x7FFFFFFF;
        WORD32 i4_min_delta_poc_for_b =
            (1 << ps_lap_struct->s_lap_static_params.i4_max_temporal_layers) /
            (ps_lap_out->i4_temporal_lyr_id + 1);

        for(i4_loop = 0; i4_loop < ps_lap_out->i4_num_ref_pics; i4_loop++)
        {
            if(i4_min_delta_poc > ABS(ps_lap_out->as_ref_pics[i4_loop].i4_ref_pic_delta_poc))
            {
                i4_min_delta_poc = ABS(ps_lap_out->as_ref_pics[i4_loop].i4_ref_pic_delta_poc);
            }
        }
    }
    return;
}

/*!
************************************************************************
* \brief
*    Update num of pic type for rc
************************************************************************
*/
void update_rc_num_pic_type(lap_struct_t *ps_lap_struct, ihevce_lap_enc_buf_t *ps_lap_out_buf)
{
    WORD32 i4_field_flag = ps_lap_struct->s_lap_static_params.i4_src_interlace_field;
    rc_lap_out_params_t *ps_rc_lap_out = &ps_lap_out_buf->s_rc_lap_out;

    ps_lap_struct->i4_lap2_counter++;

    if(ps_lap_out_buf->s_lap_out.i4_pic_type == IV_I_FRAME ||
       ps_lap_out_buf->s_lap_out.i4_pic_type == IV_IDR_FRAME)
    {
        ps_lap_struct->ai4_pic_type_to_be_removed[ps_lap_struct->i4_enq_idx] = I_PIC;
        GET_IDX_CIRCULAR_BUF(ps_lap_struct->i4_enq_idx, 1, NUM_LAP2_LOOK_AHEAD);
    }
    else if(ps_lap_out_buf->s_lap_out.i4_pic_type == IV_P_FRAME)
    {
        if(ps_lap_out_buf->s_lap_out.i4_first_field)
        {
            ps_lap_struct->ai4_pic_type_to_be_removed[ps_lap_struct->i4_enq_idx] = P_PIC;
        }
        else
        {
            ps_lap_struct->ai4_pic_type_to_be_removed[ps_lap_struct->i4_enq_idx] = P1_PIC;
        }
        GET_IDX_CIRCULAR_BUF(ps_lap_struct->i4_enq_idx, 1, NUM_LAP2_LOOK_AHEAD);
    }
    else if(ps_lap_out_buf->s_lap_out.i4_pic_type == IV_B_FRAME)
    {
        if(ps_lap_out_buf->s_lap_out.i4_temporal_lyr_id == 1)
        {
            if(ps_lap_out_buf->s_lap_out.i4_first_field)
            {
                ps_lap_struct->ai4_pic_type_to_be_removed[ps_lap_struct->i4_enq_idx] = B_PIC;
            }
            else
            {
                ps_lap_struct->ai4_pic_type_to_be_removed[ps_lap_struct->i4_enq_idx] = BB_PIC;
            }
            GET_IDX_CIRCULAR_BUF(ps_lap_struct->i4_enq_idx, 1, NUM_LAP2_LOOK_AHEAD);
        }
        else if(ps_lap_out_buf->s_lap_out.i4_temporal_lyr_id == 2)
        {
            if(ps_lap_out_buf->s_lap_out.i4_first_field)
            {
                ps_lap_struct->ai4_pic_type_to_be_removed[ps_lap_struct->i4_enq_idx] = B1_PIC;
            }
            else
            {
                ps_lap_struct->ai4_pic_type_to_be_removed[ps_lap_struct->i4_enq_idx] = B11_PIC;
            }
            GET_IDX_CIRCULAR_BUF(ps_lap_struct->i4_enq_idx, 1, NUM_LAP2_LOOK_AHEAD);
        }
        else if(ps_lap_out_buf->s_lap_out.i4_temporal_lyr_id == 3)
        {
            if(ps_lap_out_buf->s_lap_out.i4_first_field)
            {
                ps_lap_struct->ai4_pic_type_to_be_removed[ps_lap_struct->i4_enq_idx] = B2_PIC;
            }
            else
            {
                ps_lap_struct->ai4_pic_type_to_be_removed[ps_lap_struct->i4_enq_idx] = B22_PIC;
            }
            GET_IDX_CIRCULAR_BUF(ps_lap_struct->i4_enq_idx, 1, NUM_LAP2_LOOK_AHEAD);
        }
        else
        {
            ASSERT(0);
        }
    }
    else
    {
        ASSERT(0);
    }

    if(!ps_lap_struct->i4_rc_lap_period)
    {
        if(ps_lap_struct->i4_rc_lap_period < ps_lap_struct->i4_gop_period)
        {
            WORD32 i4_loop;
            WORD32 idx = 0;
            WORD32 i4_max_temporal_layer =
                ps_lap_struct->s_lap_static_params.i4_max_temporal_layers;

            for(i4_loop = 0;
                i4_loop < (ps_lap_struct->i4_gop_period - ps_lap_struct->i4_rc_lap_period);
                i4_loop++)
            {
                ps_rc_lap_out->i4_next_sc_i_in_rc_look_ahead++;

                if(i4_max_temporal_layer == 0)
                {
                    if(ps_lap_struct->i4_is_all_i_pic_in_seq)
                    {
                        ps_rc_lap_out->ai4_num_pic_type[I_PIC]++;
                    }
                    else
                    {
                        /*second field*/
                        if((i4_loop & 1) && i4_field_flag)
                        {
                            ps_rc_lap_out->ai4_num_pic_type[P1_PIC]++;
                        }
                        else
                        {
                            ps_rc_lap_out->ai4_num_pic_type[P_PIC]++;
                        }
                    }
                }
                else
                {
                    ps_rc_lap_out->ai4_num_pic_type
                        [gau1_order_insert_pic_type[i4_max_temporal_layer - 1][idx]]++;

                    GET_IDX_CIRCULAR_BUF(idx, 1, (8 << i4_field_flag));
                }
            }
        }
    }
    else
    {
        ASSERT(ps_lap_struct->i4_lap2_counter <= ps_lap_struct->i4_rc_lap_period);

        if(ps_lap_struct->i4_lap2_counter == ps_lap_struct->i4_rc_lap_period)
        {
            WORD32 i4_loop, i4_period, i4_next_i_pic = 0;
            WORD32 i4_stop_count = 0;
            WORD32 i4_temp_deq = ps_lap_struct->i4_deq_idx;
            WORD32 i4_first_pic_type = ps_lap_struct->ai4_pic_type_to_be_removed[i4_temp_deq];

            if(ps_lap_struct->i4_rc_lap_period >= ps_lap_struct->i4_gop_period)
            {
                i4_period = ps_lap_struct->i4_gop_period;
            }
            else
            {
                i4_period = ps_lap_struct->i4_rc_lap_period;
            }

            for(i4_loop = 0; i4_loop < i4_period; i4_loop++)
            {
                if(ps_lap_struct->ai4_pic_type_to_be_removed[i4_temp_deq] == I_PIC && i4_loop &&
                   i4_first_pic_type == I_PIC)
                {
                    i4_stop_count = 1;
                }

                if(!i4_stop_count)
                {
                    ps_rc_lap_out->i4_next_sc_i_in_rc_look_ahead++;
                }

                ps_rc_lap_out
                    ->ai4_num_pic_type[ps_lap_struct->ai4_pic_type_to_be_removed[i4_temp_deq]]++;

                GET_IDX_CIRCULAR_BUF(i4_temp_deq, 1, NUM_LAP2_LOOK_AHEAD);
            }
            if(ps_lap_struct->i4_rc_lap_period < ps_lap_struct->i4_gop_period)
            {
                WORD32 i4_loop;
                WORD32 idx = 0;
                WORD32 i4_max_temporal_layer =
                    ps_lap_struct->s_lap_static_params.i4_max_temporal_layers;

                for(i4_loop = 0;
                    i4_loop < (ps_lap_struct->i4_gop_period - ps_lap_struct->i4_rc_lap_period) &&
                    (!i4_next_i_pic);
                    i4_loop++)
                {
                    if(!i4_stop_count)
                    {
                        ps_rc_lap_out->i4_next_sc_i_in_rc_look_ahead++;
                    }

                    if(i4_max_temporal_layer == 0)
                    {
                        if(ps_lap_struct->i4_is_all_i_pic_in_seq)
                        {
                            ps_rc_lap_out->ai4_num_pic_type[I_PIC]++;
                        }
                        else
                        {
                            /*second field*/
                            if((i4_loop & 1) && i4_field_flag)
                            {
                                ps_rc_lap_out->ai4_num_pic_type[P1_PIC]++;
                            }
                            else
                            {
                                ps_rc_lap_out->ai4_num_pic_type[P_PIC]++;
                            }
                        }
                    }
                    else
                    {
                        ps_rc_lap_out->ai4_num_pic_type
                            [gau1_order_insert_pic_type[i4_max_temporal_layer - 1][idx]]++;
                        GET_IDX_CIRCULAR_BUF(idx, 1, (8 << i4_field_flag));
                    }
                }
            }
            /*remove one pic type*/
            GET_IDX_CIRCULAR_BUF(ps_lap_struct->i4_deq_idx, 1, NUM_LAP2_LOOK_AHEAD);
            ps_lap_struct->i4_lap2_counter--;
        }
    }

    {
        WORD32 i4_loop;
        WORD32 idx = 0;
        WORD32 i4_max_temporal_layer = ps_lap_struct->s_lap_static_params.i4_max_temporal_layers;
        WORD32 i4_num_pictype = 0;

        for(i4_loop = 0; i4_loop < MAX_PIC_TYPE; i4_loop++)
        {
            i4_num_pictype += ps_rc_lap_out->ai4_num_pic_type[i4_loop];
        }

        if(!i4_num_pictype)
        {
            ps_rc_lap_out->i4_next_sc_i_in_rc_look_ahead = ps_lap_struct->i4_gop_period;

            for(i4_loop = 0; i4_loop < (ps_lap_struct->i4_gop_period); i4_loop++)
            {
                if(i4_max_temporal_layer == 0)
                {
                    if(ps_lap_struct->i4_is_all_i_pic_in_seq)
                    {
                        ps_rc_lap_out->ai4_num_pic_type[I_PIC]++;
                    }
                    else
                    {
                        /*second field*/
                        if((i4_loop & 1) && i4_field_flag)
                        {
                            ps_rc_lap_out->ai4_num_pic_type[P1_PIC]++;
                        }
                        else
                        {
                            ps_rc_lap_out->ai4_num_pic_type[P_PIC]++;
                        }
                    }
                }
                else
                {
                    ps_rc_lap_out->ai4_num_pic_type
                        [gau1_order_insert_pic_type[i4_max_temporal_layer - 1][idx]]++;

                    GET_IDX_CIRCULAR_BUF(idx, 1, (8 << i4_field_flag));
                }
            }
        }
    }
    /*FOR RC : ensure  at least 1 I pic in the gop period at any case*/
    if(!ps_rc_lap_out->ai4_num_pic_type[I_PIC])
    {
        ASSERT(ps_rc_lap_out->ai4_num_pic_type[P_PIC]);
        ps_lap_out_buf->s_rc_lap_out.ai4_num_pic_type[P_PIC]--;
        ps_lap_out_buf->s_rc_lap_out.ai4_num_pic_type[I_PIC]++;
    }
    return;
}

/*!
************************************************************************
* \brief
*    pre rel lap output update
************************************************************************
*/
void ihevce_pre_rel_lapout_update(lap_struct_t *ps_lap_struct, ihevce_lap_enc_buf_t *ps_lap_out_buf)
{
    WORD32 i4_first_field = 1;
    WORD32 i4_field = ps_lap_struct->s_lap_static_params.i4_src_interlace_field;

    if(i4_field)
    {
        i4_first_field = ps_lap_out_buf->s_lap_out.i4_first_field;
    }

    ps_lap_out_buf->s_lap_out.i4_used = 0;

    rc_update_model_control_by_lap_for_modified_sub_gop(ps_lap_struct, ps_lap_out_buf);
    update_rc_num_pic_type(ps_lap_struct, ps_lap_out_buf);

    /* curr buf next is null, prev buf next is curr and prev buff equal to curr*/

    ps_lap_out_buf->s_rc_lap_out.ps_rc_lap_out_next_encode = NULL;
    if(ps_lap_struct->pv_prev_inp_buf != NULL &&
       ps_lap_struct->s_lap_static_params.s_lap_params.i4_rc_look_ahead_pics)
    {
        ((ihevce_lap_enc_buf_t *)ps_lap_struct->pv_prev_inp_buf)
            ->s_rc_lap_out.ps_rc_lap_out_next_encode = (void *)&ps_lap_out_buf->s_rc_lap_out;
    }

    ps_lap_struct->pv_prev_inp_buf = (void *)ps_lap_out_buf;
    ps_lap_out_buf->s_lap_out.i4_is_prev_pic_in_Tid0_same_scene = 0;

    /*with force idr below check is not valid*/
#if(!FORCE_IDR_TEST)
    if(ps_lap_struct->i4_max_idr_period == ps_lap_struct->i4_min_idr_period)
    {
        if(!ps_lap_out_buf->s_lap_out.i4_poc)
        {
            ASSERT(ps_lap_struct->i4_max_prev_poc == (ps_lap_struct->i4_max_idr_period - 1));
            ps_lap_struct->i4_max_prev_poc = 0;
        }
    }
#endif

    /*assert if num of reference frame is zero in case of P or B frame*/
    if(ps_lap_out_buf->s_lap_out.i4_pic_type == IV_P_FRAME ||
       ps_lap_out_buf->s_lap_out.i4_pic_type == IV_B_FRAME)
    {
        ASSERT(ps_lap_out_buf->s_lap_out.i4_num_ref_pics != 0);
    }

    /*assert if poc = 0 and pictype is not an idr*/
    if(ps_lap_out_buf->s_lap_out.i4_pic_type != IV_IDR_FRAME &&
       ps_lap_out_buf->s_lap_out.i4_poc == 0)
    {
        ASSERT(0);
    }
    if(ps_lap_out_buf->s_lap_out.i4_pic_type == IV_IDR_FRAME &&
       ps_lap_out_buf->s_lap_out.i4_poc != 0)
    {
        ASSERT(0);
    }
    if(ps_lap_out_buf->s_lap_out.i4_poc < 0)
    {
        ASSERT(0);
    }

#if(!FORCE_IDR_TEST)
    if((!ps_lap_struct->i4_max_idr_period) && ps_lap_out_buf->s_lap_out.i4_display_num != 0)
    {
        ASSERT(ps_lap_out_buf->s_lap_out.i4_pic_type != IV_IDR_FRAME);
    }
#endif
    if(!ps_lap_struct->i4_max_cra_period)
    {
        ASSERT(ps_lap_out_buf->s_lap_out.i4_is_cra_pic != 1);
    }

    if(ps_lap_out_buf->s_lap_out.i4_force_idr_flag)
    {
        ASSERT(ps_lap_out_buf->s_lap_out.i4_pic_type == IV_IDR_FRAME);
    }
    ps_lap_out_buf->s_lap_out.i4_curr_frm_qp = -1;
}

/*!
************************************************************************
* \brief
*    lap queue input
************************************************************************
*/
void ihevce_lap_queue_input(
    lap_struct_t *ps_lap_struct, ihevce_lap_enc_buf_t *ps_input_lap_enc_buf, WORD32 *pi4_tree_num)
{
    ihevce_encode_node_t *ps_encode_node =
        (ihevce_encode_node_t *)ps_lap_struct->aps_encode_node[*pi4_tree_num];

    WORD32 i4_capture_idx = ps_lap_struct->i4_capture_idx;

    /* Static Lap parameters */
    ihevce_lap_static_params_t *ps_lap_static_params =
        (ihevce_lap_static_params_t *)&ps_lap_struct->s_lap_static_params;

    WORD32 hier_layer = ps_lap_static_params->i4_max_temporal_layers;
    WORD32 sub_gop_size = ps_lap_struct->i4_dyn_sub_gop_size;

    /* queue the current input in capture array */
    {
        WORD32 first_gop_flag;

        if(!i4_capture_idx)
        {
            memset(
                &ps_lap_struct->api4_capture_order_array[0],
                0,
                sizeof(ihevce_lap_enc_buf_t *) * MAX_NUM_ENC_NODES);
        }
        ps_lap_struct->api4_capture_order_array[i4_capture_idx] = ps_input_lap_enc_buf;

        if(ps_input_lap_enc_buf != NULL)
        {
            if(ps_input_lap_enc_buf->s_lap_out.i4_end_flag == 1)
                ps_lap_struct->i4_end_flag_pic_idx = i4_capture_idx;
            ps_lap_struct->ai4_capture_order_poc[i4_capture_idx] = ps_lap_struct->i4_curr_poc++;
        }

        if((1 == ps_lap_struct->i4_num_dummy_pic) && (ps_lap_struct->i4_sub_gop_end == 0))
        {
            ps_lap_struct->i4_sub_gop_end = i4_capture_idx - 1;
        }
        i4_capture_idx++;

        /* to take care of buffering 1 extra picture at start or at IDR interval*/
        if(!ps_lap_struct->i4_is_all_i_pic_in_seq)
        {
            if(ps_lap_static_params->i4_src_interlace_field && sub_gop_size <= 2)
            {
                first_gop_flag = 0;
            }
            else
            {
                first_gop_flag = ps_lap_struct->i4_idr_flag
                                 << ps_lap_static_params->i4_src_interlace_field;
            }
        }
        else
        {
            first_gop_flag = ps_lap_struct->i4_idr_flag;
        }

        /* For every IDR period, set idr_flag and reset POC value and gop_size to 0*/
        if(ps_input_lap_enc_buf != NULL)
        {
            if((!first_gop_flag) && (ps_input_lap_enc_buf->s_lap_out.i4_pic_type == IV_IDR_FRAME))
            {
                ps_lap_struct->pi4_encode_poc_ptr = &ps_lap_struct->ai4_encode_order_poc[0];
                ps_lap_struct->i4_idr_flag = 1;
                ps_lap_struct->i4_curr_poc = 0;
                ps_lap_struct->ai4_capture_order_poc[i4_capture_idx - 1] =
                    ps_lap_struct->i4_curr_poc++;
            }
        }

        if(first_gop_flag &&
           (ps_lap_struct->i4_is_all_i_pic_in_seq || ps_lap_struct->i4_immediate_idr_case))
        {
            sub_gop_size = 0;
        }

        if(!first_gop_flag && ps_lap_struct->i4_immediate_idr_case &&
           (i4_capture_idx != (sub_gop_size + first_gop_flag)))
        {
            sub_gop_size = 1 << ps_lap_static_params->i4_src_interlace_field;
            ps_lap_struct->i4_dyn_sub_gop_size = 1 << ps_lap_static_params->i4_src_interlace_field;
        }

        /* reset the queue idx end of every gop */
        if(i4_capture_idx == (sub_gop_size + first_gop_flag))
        {
            ps_lap_struct->pi4_encode_poc_ptr = &ps_lap_struct->ai4_encode_order_poc[0];

            if(ps_lap_struct->i4_end_flag_pic_idx && (1 != sub_gop_size))
            {
                WORD32 i4_temp_poc = 0;
                ihevce_lap_enc_buf_t *ps_temp_lap_enc_buf = NULL;

                /*swap the lap enc buf and poc*/
                ps_temp_lap_enc_buf =
                    ps_lap_struct->api4_capture_order_array[ps_lap_struct->i4_end_flag_pic_idx - 1];
                ps_lap_struct->api4_capture_order_array[ps_lap_struct->i4_end_flag_pic_idx - 1] =
                    NULL;
                ps_lap_struct->api4_capture_order_array[i4_capture_idx - 2] =
                    ps_lap_struct->api4_capture_order_array[ps_lap_struct->i4_end_flag_pic_idx];

                if((i4_capture_idx - 2) != ps_lap_struct->i4_end_flag_pic_idx)
                    ps_lap_struct->api4_capture_order_array[ps_lap_struct->i4_end_flag_pic_idx] =
                        NULL;

                ps_temp_lap_enc_buf->s_lap_out.i4_pic_type = IV_P_FRAME;
                ps_lap_struct->api4_capture_order_array[i4_capture_idx - 1] = ps_temp_lap_enc_buf;

                i4_temp_poc =
                    ps_lap_struct->ai4_capture_order_poc[ps_lap_struct->i4_end_flag_pic_idx - 1];
                ps_lap_struct->ai4_capture_order_poc[i4_capture_idx - 2] =
                    ps_lap_struct->ai4_capture_order_poc[ps_lap_struct->i4_end_flag_pic_idx];

                ps_lap_struct->ai4_capture_order_poc[i4_capture_idx - 1] = i4_temp_poc;
            }

            if(ps_lap_struct->i4_num_dummy_pic)
            {
                WORD32 pic_idx;
                ihevce_lap_enc_buf_t *ps_temp_lap_enc_buf = NULL;
                static const WORD32 subgop_temporal_layer3[8] = { 7, 3, 1, 0, 2, 5, 4, 6 };
                static const WORD32 subgop_temporal_layer2[4] = { 3, 1, 0, 2 };
                const WORD32 *subgop_pic_idx = (ps_lap_static_params->i4_max_temporal_layers == 2)
                                                   ? &subgop_temporal_layer2[0]
                                                   : &subgop_temporal_layer3[0];
                WORD32 max_pic_count = ps_lap_struct->i4_sub_gop_end + 1;

                for(pic_idx = 0; pic_idx < max_pic_count; pic_idx++)
                {
                    WORD32 i4_temp_idx = ps_lap_static_params->i4_max_temporal_layers > 1
                                             ? subgop_pic_idx[pic_idx]
                                             : 1;

                    if(NULL == ps_lap_struct->api4_capture_order_array[i4_temp_idx])
                    {
                        ps_temp_lap_enc_buf =
                            ps_lap_struct->api4_capture_order_array[ps_lap_struct->i4_sub_gop_end];
                        if(pic_idx == 0)
                        {
                            ps_temp_lap_enc_buf->s_lap_out.i4_pic_type = IV_P_FRAME;
                        }
                        ps_lap_struct->api4_capture_order_array[i4_temp_idx] = ps_temp_lap_enc_buf;
                        ps_lap_struct->api4_capture_order_array[ps_lap_struct->i4_sub_gop_end] =
                            NULL;

                        ps_lap_struct->ai4_capture_order_poc[i4_temp_idx] =
                            ps_lap_struct->ai4_capture_order_poc[ps_lap_struct->i4_sub_gop_end];
                        ps_lap_struct->ai4_capture_order_poc[ps_lap_struct->i4_sub_gop_end] = 0;
                        ps_lap_struct->i4_sub_gop_end--;
                    }
                }
                ps_lap_struct->i4_sub_gop_end = 0;
            }
            i4_capture_idx = 0;

            /* add the number of pics in sub gop to the gop counter */
            /* Get reordered Buffer for encoder, wait till all sub-gop buffers are output */

            /* Popluate I/P pictures */
            ihevce_ip_pic_population(ps_encode_node, ps_lap_struct, first_gop_flag);

            /* For hierarchical layers, Populate B picture */
            if((hier_layer > 0) &&
               sub_gop_size > (1 << ps_lap_static_params->i4_src_interlace_field))
            {
                ihevce_b_pic_population(ps_encode_node, ps_lap_struct);
            }

            ps_lap_struct->i4_num_bufs_encode_order = sub_gop_size + first_gop_flag;

            /* correction of encode order in case of multiple non reference B*/
            if(ps_lap_struct->i4_dyn_sub_gop_size > ps_lap_struct->i4_sub_gop_size)
            {
                WORD32 i4_loop;
                ihevce_lap_enc_buf_t *ps_lap_enc_buf, *ps_lap_enc_buf_tmp[MAX_NUM_ENC_NODES];
                WORD32 i4_enc_cnt, i4_cap_cnt;

                i4_cap_cnt = first_gop_flag;
                i4_enc_cnt = 0;

                for(i4_loop = 0; i4_loop < ps_lap_struct->i4_num_bufs_encode_order; i4_loop++)
                {
                    ps_lap_enc_buf = ps_lap_struct->api4_encode_order_array[i4_loop];

                    if(ps_lap_enc_buf != NULL && !ps_lap_enc_buf->s_lap_out.i4_is_ref_pic &&
                       (ps_lap_enc_buf->s_lap_out.i4_temporal_lyr_id ==
                        ps_lap_struct->s_lap_static_params.i4_max_temporal_layers))
                    {
                        if(ps_lap_enc_buf != ps_lap_struct->api4_capture_order_array[i4_cap_cnt])
                        {
                            ps_lap_enc_buf_tmp[i4_enc_cnt] =
                                ps_lap_struct->api4_capture_order_array[i4_cap_cnt];
                            i4_enc_cnt++;
                            i4_loop++;
                        }
                        i4_cap_cnt += 2;
                        ps_lap_enc_buf_tmp[i4_enc_cnt] = ps_lap_enc_buf;
                        i4_enc_cnt++;
                        ps_lap_enc_buf_tmp[i4_enc_cnt] =
                            ps_lap_struct->api4_capture_order_array[i4_cap_cnt];
                        i4_enc_cnt++;
                        i4_cap_cnt += 2;
                        i4_loop++;
                    }
                    else
                    {
                        ps_lap_enc_buf_tmp[i4_enc_cnt] = ps_lap_enc_buf;
                        i4_enc_cnt++;
                    }
                }
                for(i4_loop = 0; i4_loop < ps_lap_struct->i4_num_bufs_encode_order; i4_loop++)
                {
                    ps_lap_struct->api4_encode_order_array[i4_loop] = ps_lap_enc_buf_tmp[i4_loop];
                }
            }

            /* reset the IDR flag */
            ps_lap_struct->i4_idr_flag = 0;
            ps_lap_struct->i4_dyn_sub_gop_size = ps_lap_struct->i4_sub_gop_size;

            /*Copy encode array to lap output buf*/
            memcpy(
                &ps_lap_struct->api4_lap_out_buf[ps_lap_struct->i4_lap_encode_idx],
                &ps_lap_struct->api4_encode_order_array[0],
                sizeof(ihevce_lap_enc_buf_t *) * ps_lap_struct->i4_num_bufs_encode_order);

            memset(
                &ps_lap_struct->api4_encode_order_array[0],
                0,
                sizeof(ihevce_lap_enc_buf_t *) * ps_lap_struct->i4_num_bufs_encode_order);

            ps_lap_struct->ai4_num_buffer[ps_lap_struct->i4_lap_encode_idx] =
                ps_lap_struct->i4_num_bufs_encode_order - ps_lap_struct->i4_num_dummy_pic;

            ps_lap_struct->i4_lap_encode_idx++;
            ps_lap_struct->i4_lap_encode_idx &= (MAX_SUBGOP_IN_ENCODE_QUEUE - 1);
        }

        /* store the capture index */
        ps_lap_struct->i4_capture_idx = i4_capture_idx;
        ps_lap_struct->i4_immediate_idr_case = 0;
    }
    return;
}

/*!
************************************************************************
* \brief
*    lap process
************************************************************************
*/
ihevce_lap_enc_buf_t *ihevce_lap_process(void *pv_interface_ctxt, ihevce_lap_enc_buf_t *ps_curr_inp)
{
    lap_intface_t *ps_lap_interface = (lap_intface_t *)pv_interface_ctxt;
    lap_struct_t *ps_lap_struct = (lap_struct_t *)ps_lap_interface->pv_lap_module_ctxt;
    ihevce_hle_ctxt_t *ps_hle_ctxt = (ihevce_hle_ctxt_t *)ps_lap_interface->pv_hle_ctxt;
    ihevce_lap_enc_buf_t *ps_lap_inp_buf = ps_curr_inp;
    ihevce_tgt_params_t *ps_tgt_params =
        &ps_lap_struct->s_static_cfg_params.s_tgt_lyr_prms.as_tgt_params[0];
    WORD32 i4_field_flag = ps_lap_struct->s_lap_static_params.i4_src_interlace_field;
    WORD32 i4_flush_check = 0;
    WORD32 i4_force_idr_check = 0;
    WORD32 i4_tree_num = 0;
    iv_input_ctrl_buffs_t *ps_ctrl_buf = NULL;
    WORD32 buf_id = 0;
    WORD32 i4_lap_window_size = 1 << ps_lap_struct->s_lap_static_params.i4_max_temporal_layers;

    ps_lap_interface->i4_ctrl_in_que_blocking_mode = BUFF_QUE_NON_BLOCKING_MODE;

    /* ----------- LAP processing ----------- */
    if(ps_lap_struct->end_flag != 1)
    {
        ASSERT(NULL != ps_curr_inp);

        /* ---------- get the filled control command buffer ------------ */
        ps_ctrl_buf = (iv_input_ctrl_buffs_t *)ihevce_q_get_filled_buff(
            ps_hle_ctxt->apv_enc_hdl[0],
            ps_lap_interface->i4_ctrl_in_que_id,
            &buf_id,
            ps_lap_interface->i4_ctrl_in_que_blocking_mode);

        /* ----------- check the command ---------------------- */
        if(NULL != ps_ctrl_buf)
        {
            /* check for async errors */
            ihevce_dyn_config_prms_t as_dyn_br[MAX_NUM_DYN_BITRATE_CMDS];
            WORD32 i4_num_set_bitrate_cmds = 0;
            WORD32 bitrt_ctr = 0;

            ihevce_lap_parse_async_cmd(
                ps_hle_ctxt,
                (WORD32 *)ps_ctrl_buf->pv_asynch_ctrl_bufs,
                ps_ctrl_buf->i4_cmd_buf_size,
                ps_ctrl_buf->i4_buf_id,
                &i4_num_set_bitrate_cmds,
                &as_dyn_br[0]);

            /* Call the call back function to register the new bitrate */
            for(bitrt_ctr = 0; bitrt_ctr < i4_num_set_bitrate_cmds; bitrt_ctr++)
            {
                ps_lap_interface->ihevce_dyn_bitrate_cb(
                    (void *)ps_hle_ctxt, (void *)&as_dyn_br[bitrt_ctr]);
            }
        }

        {
            WORD32 *pi4_cmd_buf = (WORD32 *)ps_lap_inp_buf->s_input_buf.pv_synch_ctrl_bufs;

            /* check for sync cmd buffer error */
            /* check FLUSH comand and Force IDR in the complete buffer */
            i4_flush_check = 0;
            i4_force_idr_check = 0;
            ihevce_lap_parse_sync_cmd(
                ps_hle_ctxt,
                &ps_lap_struct->s_static_cfg_params,
                pi4_cmd_buf,
                ps_lap_inp_buf,
                &i4_flush_check,
                &i4_force_idr_check);

            if(i4_flush_check)
                ps_lap_struct->end_flag = 1;

            ps_lap_inp_buf->s_lap_out.i4_out_flush_flag = 0;
            ps_lap_inp_buf->s_lap_out.i4_end_flag = ps_lap_struct->end_flag;

            /* check if input buffer is a valid buffer */
            if(1 == ps_lap_inp_buf->s_input_buf.i4_inp_frm_data_valid_flag)
            {
                /* Initialise laps input buffer descriptors */
                memset(&ps_lap_inp_buf->s_lap_out, 0, sizeof(ihevce_lap_output_params_t));
                memset(&ps_lap_inp_buf->s_rc_lap_out, 0, sizeof(rc_lap_out_params_t));
                /* Default initialization of lapout parameters */
                ps_lap_inp_buf->s_lap_out.i4_scene_type = SCENE_TYPE_NORMAL;
                ps_lap_inp_buf->s_lap_out.u4_scene_num = 0;
                ps_lap_inp_buf->s_lap_out.i4_display_num = ps_lap_struct->i4_display_num;
                ps_lap_inp_buf->s_lap_out.i4_quality_preset = ps_tgt_params->i4_quality_preset;
                ps_lap_inp_buf->s_lap_out.i1_weighted_pred_flag = 0;
                ps_lap_inp_buf->s_lap_out.i1_weighted_bipred_flag = 0;
                ps_lap_inp_buf->s_lap_out.i4_log2_luma_wght_denom = DENOM_DEFAULT;
                ps_lap_inp_buf->s_lap_out.i4_log2_chroma_wght_denom = DENOM_DEFAULT;
                ps_lap_inp_buf->s_lap_out.as_ref_pics[0].i4_num_duplicate_entries_in_ref_list = 1;
                ps_lap_inp_buf->s_lap_out.as_ref_pics[0].i4_used_by_cur_pic_flag = 1;
                ps_lap_inp_buf->s_lap_out.as_ref_pics[0].as_wght_off[0].u1_luma_weight_enable_flag =
                    0;
                ps_lap_inp_buf->s_lap_out.as_ref_pics[0]
                    .as_wght_off[0]
                    .u1_chroma_weight_enable_flag = 0;
                ps_lap_inp_buf->s_lap_out.i4_first_field = 1;
                ps_lap_inp_buf->s_lap_out.i4_force_idr_flag = 0;
                ps_lap_inp_buf->s_lap_out.i4_curr_frm_qp = ps_tgt_params->ai4_frame_qp[0];
                ps_lap_inp_buf->s_lap_out.i4_used = 1;
                if(i4_force_idr_check)
                {
                    ps_lap_inp_buf->s_lap_out.i4_force_idr_flag = 1;
                }
                /* Populate input params in lap out struct */
                ps_lap_inp_buf->s_lap_out.s_input_buf.pv_y_buf =
                    ps_lap_inp_buf->s_input_buf.s_input_buf.pv_y_buf;
                ps_lap_inp_buf->s_lap_out.s_input_buf.pv_u_buf =
                    ps_lap_inp_buf->s_input_buf.s_input_buf.pv_u_buf;
                ps_lap_inp_buf->s_lap_out.s_input_buf.pv_v_buf =
                    ps_lap_inp_buf->s_input_buf.s_input_buf.pv_v_buf;
                ps_lap_inp_buf->s_lap_out.s_input_buf.i4_y_wd =
                    ps_lap_inp_buf->s_input_buf.s_input_buf.i4_y_wd;
                ps_lap_inp_buf->s_lap_out.s_input_buf.i4_y_ht =
                    ps_lap_inp_buf->s_input_buf.s_input_buf.i4_y_ht;
                ps_lap_inp_buf->s_lap_out.s_input_buf.i4_y_strd =
                    ps_lap_inp_buf->s_input_buf.s_input_buf.i4_y_strd;
                ps_lap_inp_buf->s_lap_out.s_input_buf.i4_uv_wd =
                    ps_lap_inp_buf->s_input_buf.s_input_buf.i4_uv_wd;
                ps_lap_inp_buf->s_lap_out.s_input_buf.i4_uv_ht =
                    ps_lap_inp_buf->s_input_buf.s_input_buf.i4_uv_ht;
                ps_lap_inp_buf->s_lap_out.s_input_buf.i4_uv_strd =
                    ps_lap_inp_buf->s_input_buf.s_input_buf.i4_uv_strd;

                ps_lap_struct->i4_display_num++;
                ps_lap_struct->aps_lap_inp_buf[ps_lap_struct->i4_buf_enq_idx] = ps_lap_inp_buf;
                /* update first field flag */
                ps_lap_inp_buf->s_lap_out.i4_first_field = 1;
                if(i4_field_flag)
                {
                    ps_lap_inp_buf->s_lap_out.i4_first_field =
                        (ps_lap_inp_buf->s_input_buf.i4_topfield_first ^
                         ps_lap_inp_buf->s_input_buf.i4_bottom_field);
                }

                /* force idr in case interlace input can be taken only for first field */
                if(!ps_lap_inp_buf->s_lap_out.i4_first_field)
                {
                    ps_lap_inp_buf->s_lap_out.i4_force_idr_flag = 0;
                }

                if((i4_lap_window_size > 1) &&
                   (ps_lap_struct->ai1_pic_type[ps_lap_struct->i4_next_start_ctr] != PIC_TYPE_IDR))
                {
                    ps_lap_struct->i4_sub_gop_pic_idx++;
                    if(ps_lap_struct->i4_sub_gop_pic_idx > i4_lap_window_size)
                    {
                        ps_lap_struct->i4_sub_gop_pic_idx =
                            ps_lap_struct->i4_sub_gop_pic_idx - i4_lap_window_size;
                    }
                }
                else if(1 == i4_lap_window_size)
                {
                    ps_lap_struct->i4_sub_gop_pic_idx = 1;
                }

                if(i4_force_idr_check &&
                   (ps_lap_struct->ai1_pic_type[ps_lap_struct->i4_next_start_ctr] != PIC_TYPE_IDR))
                {
                    ps_lap_struct->i4_force_idr_pos = ps_lap_struct->i4_sub_gop_pic_idx;
                }

                /* store pictype for next subgop */
                if((0 == ps_lap_struct->i4_num_frm_type_decided) &&
                   (ps_lap_struct->i4_force_idr_pos == 0))
                {
                    ps_lap_struct->ai1_pic_type[0] =
                        ps_lap_struct->ai1_pic_type[ps_lap_struct->i4_next_start_ctr];

                    ihevce_determine_next_sub_gop_state(ps_lap_struct);

                    ps_lap_struct->i4_next_start_ctr = 0;
                }
                else if(
                    i4_force_idr_check &&
                    (ps_lap_struct->i4_force_idr_pos <= ps_lap_struct->i4_sub_gop_size))
                {
                    /*check force idr pos is 1st pic in sub-gop then don't add dummy pics*/
                    if(ps_lap_struct->i4_force_idr_pos != 1)
                    {
                        WORD32 sub_gop_pos = ps_lap_struct->i4_force_idr_pos;
                        while(sub_gop_pos <= ps_lap_struct->i4_sub_gop_size)
                        {
                            ps_lap_struct->i4_num_dummy_pic++;
                            ihevce_lap_queue_input(ps_lap_struct, NULL, &i4_tree_num);
                            sub_gop_pos++;
                        }
                        ps_lap_struct->i4_num_dummy_pic = 0;
                    }
                    ps_lap_struct->ai1_pic_type[0] =
                        ps_lap_struct->ai1_pic_type[ps_lap_struct->i4_next_start_ctr];

                    ihevce_determine_next_sub_gop_state(ps_lap_struct);

                    ps_lap_struct->i4_next_start_ctr = 0;
                }

                if(/*ps_lap_struct->i4_init_delay_over &&*/ 0 !=
                   ps_lap_struct->i4_num_frm_type_decided)
                {
                    ihevce_assign_pic_type(
                        ps_lap_struct,
                        ps_lap_struct->aps_lap_inp_buf[ps_lap_struct->i4_buf_deq_idx]);

                    ps_lap_struct->i4_num_frm_type_decided--;

                    if(NULL != ps_lap_struct->aps_lap_inp_buf[ps_lap_struct->i4_buf_deq_idx])
                    {
                        /*special case of two consequetive idr at the start of encode or due to force idr*/
                        ps_lap_struct->i4_immediate_idr_case =
                            ps_lap_struct->i4_is_all_i_pic_in_seq;
                        if(ps_lap_struct->aps_lap_inp_buf[ps_lap_struct->i4_buf_deq_idx]
                               ->s_lap_out.i4_pic_type == IV_IDR_FRAME)
                        {
                            ps_lap_struct->i4_immediate_idr_case = 1;
                        }
                        else
                        {
                            WORD32 i4_prev_idx = ps_lap_struct->i4_buf_deq_idx > 0
                                                     ? ps_lap_struct->i4_buf_deq_idx - 1
                                                     : ps_lap_struct->i4_buf_deq_idx;
                            /*field case of single IDR field followed by P*/
                            if(NULL != ps_lap_struct->aps_lap_inp_buf[i4_prev_idx] &&
                               i4_field_flag &&
                               ps_lap_struct->aps_lap_inp_buf[i4_prev_idx]->s_lap_out.i4_pic_type ==
                                   IV_IDR_FRAME &&
                               !ps_lap_struct->i4_num_frm_type_decided)
                            {
                                ps_lap_struct->i4_immediate_idr_case = 1;
                            }
                        }
                    }

                    /* Queue in the current input Buffer to LAP que */
                    ihevce_lap_queue_input(
                        ps_lap_struct,
                        ps_lap_struct->aps_lap_inp_buf[ps_lap_struct->i4_buf_deq_idx],
                        &i4_tree_num);

                    ps_lap_struct->i4_next_start_ctr++;
                    ps_lap_struct->i4_buf_deq_idx++;
                    if(ps_lap_struct->i4_buf_deq_idx >= MAX_QUEUE_LENGTH)
                        ps_lap_struct->i4_buf_deq_idx = 0;
                }

                ps_lap_struct->i4_buf_enq_idx++;
                if(ps_lap_struct->i4_buf_enq_idx >= MAX_QUEUE_LENGTH)
                    ps_lap_struct->i4_buf_enq_idx = 0;
            } /* end if for valid input buffer check*/
        }

        /* source pixel padding if width/height is not aligned to 8 pixel */
        if(ps_lap_inp_buf->s_input_buf.i4_inp_frm_data_valid_flag)
        {
            ihevce_src_params_t *ps_src_prms = &ps_lap_struct->s_static_cfg_params.s_src_prms;
            WORD32 i4_align_wd = ps_src_prms->i4_width;
            WORD32 i4_align_ht = ps_src_prms->i4_height;
            WORD32 min_cu_size =
                (1 << ps_lap_struct->s_static_cfg_params.s_config_prms.i4_min_log2_cu_size);

            i4_align_wd += SET_CTB_ALIGN(ps_src_prms->i4_width, min_cu_size);
            i4_align_ht += SET_CTB_ALIGN(ps_src_prms->i4_height, min_cu_size);

            ihevce_lap_pad_input_bufs(ps_lap_inp_buf, i4_align_wd, i4_align_ht);
        }
        {
            ps_lap_inp_buf->s_lap_out.s_logo_ctxt.logo_width = 0;
            ps_lap_inp_buf->s_lap_out.s_logo_ctxt.logo_height = 0;
            ps_lap_inp_buf->s_lap_out.s_logo_ctxt.logo_x_offset = 0;
            ps_lap_inp_buf->s_lap_out.s_logo_ctxt.logo_y_offset = 0;
        }
    }

    if(ps_lap_struct->end_flag == 1)
    {
        ps_lap_struct->aps_lap_inp_buf[ps_lap_struct->i4_buf_enq_idx] = ps_lap_inp_buf;

        /*to be filed*/
        if(0 == ps_lap_struct->i4_num_frm_type_decided)
        {
            ps_lap_struct->ai1_pic_type[0] =
                ps_lap_struct->ai1_pic_type[ps_lap_struct->i4_next_start_ctr];

            ihevce_determine_next_sub_gop_state(ps_lap_struct);

            ps_lap_struct->i4_next_start_ctr = 0;
        }

        if(NULL != ps_lap_struct->aps_lap_inp_buf[ps_lap_struct->i4_buf_deq_idx])
        {
            ihevce_assign_pic_type(
                ps_lap_struct, ps_lap_struct->aps_lap_inp_buf[ps_lap_struct->i4_buf_deq_idx]);
        }

        ps_lap_struct->i4_num_frm_type_decided--;

        if(NULL != ps_lap_struct->aps_lap_inp_buf[ps_lap_struct->i4_buf_deq_idx])
        {
            /*special case of two consequetive idr at the start of encode or due to force idr*/
            ps_lap_struct->i4_immediate_idr_case = ps_lap_struct->i4_is_all_i_pic_in_seq;

            if(ps_lap_struct->aps_lap_inp_buf[ps_lap_struct->i4_buf_deq_idx]
                   ->s_lap_out.i4_pic_type == IV_IDR_FRAME)
            {
                ps_lap_struct->i4_immediate_idr_case = 1;
            }
            else
            {
                WORD32 i4_prev_idx = ps_lap_struct->i4_buf_deq_idx > 0
                                         ? ps_lap_struct->i4_buf_deq_idx - 1
                                         : ps_lap_struct->i4_buf_deq_idx;
                /*field case of single IDR field followed by P*/
                if(NULL != ps_lap_struct->aps_lap_inp_buf[i4_prev_idx] && i4_field_flag &&
                   ps_lap_struct->aps_lap_inp_buf[i4_prev_idx]->s_lap_out.i4_pic_type ==
                       IV_IDR_FRAME &&
                   !ps_lap_struct->i4_num_frm_type_decided)
                {
                    ps_lap_struct->i4_immediate_idr_case = 1;
                }
            }
        }
        /* Queue in the current input Buffer to LAP que */
        ihevce_lap_queue_input(
            ps_lap_struct,
            ps_lap_struct->aps_lap_inp_buf[ps_lap_struct->i4_buf_deq_idx],
            &i4_tree_num);
        ps_lap_struct->i4_max_buf_in_enc_order =
            ps_lap_struct->ai4_num_buffer[ps_lap_struct->i4_deq_lap_buf];
        ps_lap_struct->i4_next_start_ctr++;
        ps_lap_struct->i4_buf_deq_idx++;

        if(ps_lap_struct->i4_buf_deq_idx >= MAX_QUEUE_LENGTH)
            ps_lap_struct->i4_buf_deq_idx = 0;

        ps_lap_struct->i4_buf_enq_idx++;
        if(ps_lap_struct->i4_buf_enq_idx >= MAX_QUEUE_LENGTH)
            ps_lap_struct->i4_buf_enq_idx = 0;
    }

    if(1 == ps_lap_struct->i4_force_end_flag)
    {
        ihevce_force_end(ps_hle_ctxt);
    }

    /*return encode order pic to pre enc*/
    ps_lap_inp_buf = NULL;

    if(NULL !=
       ps_lap_struct->api4_lap_out_buf[ps_lap_struct->i4_deq_lap_buf][ps_lap_struct->i4_lap_out_idx])
    {
        ps_lap_inp_buf =
            ps_lap_struct
                ->api4_lap_out_buf[ps_lap_struct->i4_deq_lap_buf][ps_lap_struct->i4_lap_out_idx];
        ps_lap_struct
            ->api4_lap_out_buf[ps_lap_struct->i4_deq_lap_buf][ps_lap_struct->i4_lap_out_idx] = NULL;
        if(!ps_lap_inp_buf->s_lap_out.i4_end_flag)
            ihevce_pre_rel_lapout_update(ps_lap_struct, ps_lap_inp_buf);

        ps_lap_struct->i4_max_buf_in_enc_order =
            ps_lap_struct->ai4_num_buffer[ps_lap_struct->i4_deq_lap_buf];
    }

    ps_lap_struct->i4_lap_out_idx++;
    if(ps_lap_struct->i4_lap_out_idx == ps_lap_struct->i4_max_buf_in_enc_order)
    {
        if(ps_lap_struct->ai4_num_buffer[ps_lap_struct->i4_deq_lap_buf])
        {
            ps_lap_struct->ai4_num_buffer[ps_lap_struct->i4_deq_lap_buf] = 0;
            ps_lap_struct->i4_deq_lap_buf++;
            ps_lap_struct->i4_deq_lap_buf &= (MAX_SUBGOP_IN_ENCODE_QUEUE - 1);
        }

        ps_lap_struct->i4_lap_out_idx = 0;
    }

    return (ps_lap_inp_buf);
}

/*!
************************************************************************
* \brief
*    lap get input buffer requirement count
************************************************************************
*/
WORD32 ihevce_lap_get_num_ip_bufs(ihevce_lap_static_params_t *ps_lap_stat_prms)
{
    WORD32 i4_lap_window_size = 1;
    WORD32 gop_delay = 1 << ps_lap_stat_prms->i4_max_temporal_layers;

    if(ps_lap_stat_prms->s_lap_params.i4_rc_look_ahead_pics != 0)
    {
        i4_lap_window_size = 1 + ps_lap_stat_prms->s_lap_params.i4_rc_look_ahead_pics;
    }

    gop_delay += (i4_lap_window_size);
    return gop_delay;
}
