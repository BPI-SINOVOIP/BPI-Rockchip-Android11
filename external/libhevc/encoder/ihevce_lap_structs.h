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
*  ihevce_lap_structs.h
*
* @brief
*  This file contains structure definitions related to look-ahead processing
*
* @author
*  ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_LAP_STRUCTS_H_
#define _IHEVCE_LAP_STRUCTS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define EVAL_VERSION 0
#define EVAL_MODE_FORCE_LOGO 0
#define MAX_FRAMES_EVAL_VERSION 50000
#define LAP_DEBUG_PRINT 0
#define FORCE_IDR_TEST 1
#define MAX_NUM_ENC_NODES 8
#define MAX_QUEUE_LENGTH (MAX_LAP_WINDOW_SIZE + MAX_SUB_GOP_SIZE + 2)
#define MAX_SUBGOP_IN_ENCODE_QUEUE 4

#if(MAX_SUBGOP_IN_ENCODE_QUEUE) & (MAX_SUBGOP_IN_ENCODE_QUEUE - 1)
#error max_subgop_in_encode_queue must be a power of 2
#endif

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
typedef enum
{
    LAP_CTXT = 0,
    LAP_NODE_MEM,
    NUM_LAP_MEM_RECS,
} LAP_MEM_T;

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/* Picture types */
typedef enum PIC_TYPE_E
{
    PIC_TYPE_NA = -1, /* Invalid frame type*/
    PIC_TYPE_I, /* I frame */
    PIC_TYPE_P, /* P frame */
    PIC_TYPE_B, /* B frame */
    PIC_TYPE_IDR, /* IDR frame */
    PIC_TYPE_CRA, /* CRA frame */
    MAX_NUM_PIC_TYPES
} PIC_TYPE_E;

typedef struct ihevce_encode_node_t
{
    WORD32 data;
    void *pv_left_node;
    void *pv_right_node;
    WORD32 i4_hierachical_layer;
    WORD32 i4_interlace_field;
    ihevce_lap_enc_buf_t *ps_lap_top_buff;
    ihevce_lap_enc_buf_t *ps_lap_bottom_buff;

} ihevce_encode_node_t;

/**
******************************************************************************
 *  @brief  lap context
******************************************************************************
 */
typedef struct
{
    // cfg params
    ihevce_static_cfg_params_t s_static_cfg_params;
    ihevce_lap_static_params_t s_lap_static_params;

    //pic reorder info
    ihevce_lap_enc_buf_t *aps_lap_inp_buf[MAX_QUEUE_LENGTH];

    ihevce_encode_node_t *aps_encode_node[1];

    /** Array of nodes in encode order*/
    ihevce_lap_enc_buf_t *api4_encode_order_array[MAX_NUM_ENC_NODES];

    /** Array of lap output in lap encode array*/
    ihevce_lap_enc_buf_t *api4_lap_out_buf[MAX_SUBGOP_IN_ENCODE_QUEUE][MAX_NUM_ENC_NODES];

    /** Array of nodes in capture order*/
    ihevce_lap_enc_buf_t *api4_capture_order_array[MAX_NUM_ENC_NODES];

    /**Array of POCS in encode order*/
    WORD32 ai4_encode_order_poc[MAX_NUM_ENC_NODES];

    /**Array of POCS in capture order*/
    WORD32 ai4_capture_order_poc[MAX_NUM_ENC_NODES];

    /** Pointer to POC in encode order*/
    WORD32 *pi4_encode_poc_ptr;

    /** Pointer to POC in capture order*/
    WORD32 *pi4_capture_poc_ptr;

    WORD32 ai4_pic_type_to_be_removed[NUM_LAP2_LOOK_AHEAD];

    WORD32 ai4_num_buffer[MAX_SUBGOP_IN_ENCODE_QUEUE];

    void *pv_prev_inp_buf;

    WORD32 i4_buf_enq_idx;
    WORD32 i4_buf_deq_idx;
    WORD32 i4_lap_out_idx;
    WORD32 i4_capture_idx;
    WORD32 i4_idr_flag;
    WORD32 i4_num_bufs_encode_order;
    WORD32 i4_deq_idx;
    WORD32 i4_enq_idx;
    // poc info
    WORD32 ref_poc_array[MAX_REF_PICS];
    WORD8 ai1_pic_type[10];
    WORD32 i4_curr_poc;
    WORD32 i4_cra_poc;
    WORD32 i4_assoc_IRAP_poc;
    // counters
    WORD32 i4_max_idr_period;
    WORD32 i4_min_idr_period;
    WORD32 i4_max_cra_period;
    WORD32 i4_max_i_period;
    WORD32 i4_idr_counter;
    WORD32 i4_cra_counter;
    WORD32 i4_i_counter;
    WORD32 i4_idr_gop_num;
    WORD32 i4_curr_ref_pics;
    WORD32 i4_display_num;
    WORD32 i4_num_frm_type_decided;
    WORD32 i4_frm_gop_idx;
    WORD32 i4_is_all_i_pic_in_seq;
    WORD32 i4_next_start_ctr;
    WORD32 i4_fixed_open_gop_period;
    WORD32 i4_fixed_i_period;
    // misc
    WORD32 i4_enable_logo;
    WORD32 i4_cra_i_pic_flag;
    WORD32 i4_force_end_flag;
    WORD32 i4_sub_gop_size;
    WORD32 i4_sub_gop_size_idr;
    WORD32 i4_dyn_sub_gop_size;
    WORD32 end_flag;
    WORD32 i4_immediate_idr_case;
    WORD32 i4_max_buf_in_enc_order;
    WORD32 i4_end_flag_pic_idx;
    WORD32 i4_lap2_counter;
    WORD32 i4_rc_lap_period;
    WORD32 i4_gop_period;
    WORD32 i4_no_back_to_back_i_avoidance;
    WORD32 i4_sub_gop_pic_idx;
    WORD32 i4_force_idr_pos;
    WORD32 i4_num_dummy_pic;
    WORD32 i4_sub_gop_end;
    WORD32 i4_lap_encode_idx;
    WORD32 i4_deq_lap_buf;
} lap_struct_t;

void ihevce_populate_tree_nodes(
    ihevce_encode_node_t *encode_parent_node_t,
    ihevce_encode_node_t *encode_node_t,
    WORD32 *loop_count,
    WORD32 layer,
    WORD32 hier_layer);

#endif /* _IHEVCE_LAP_STRUCTS_H_ */
