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
* \file ihevce_ipe_structs.h
*
* \brief
*    This file contains strcutures of ipe pass
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_IPE_STRUCTS_H_
#define _IHEVCE_IPE_STRUCTS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define MAX_FAST_IP_MODES 23
#define NUM_INTRA_RDOPT_MODES 1
#if 1  // FAST_PART_WITH_OPTION_4
#define MAX_TREE_NODES                                                                             \
    ((MAX_CTB_SIZE == MIN_TU_SIZE)                                                                 \
         ? 1                                                                                       \
         : (MAX_CTB_SIZE == (MIN_TU_SIZE << 1)                                                     \
                ? 5                                                                                \
                : (MAX_CTB_SIZE == (MIN_TU_SIZE << 2)                                              \
                       ? 21                                                                        \
                       : (MAX_CTB_SIZE == (MIN_TU_SIZE << 3) ? 37 : 53))))
#else  // FAST_PART_WITH_OPTION_4
#define MAX_TREE_NODES                                                                             \
    ((MAX_CTB_SIZE == MIN_TU_SIZE)                                                                 \
         ? 1                                                                                       \
         : (MAX_CTB_SIZE == (MIN_TU_SIZE << 1)                                                     \
                ? 5                                                                                \
                : (MAX_CTB_SIZE == (MIN_TU_SIZE << 2)                                              \
                       ? 9                                                                         \
                       : (MAX_CTB_SIZE == (MIN_TU_SIZE << 3) ? 13 : 17))))
#endif  // FAST_PART_WITH_OPTION_4
#define BOTTOM_LEFT_FLAG 0x0000000F
#define LEFT_FLAG 0x000000F0
#define TOP_LEFT_FLAG 0x00010000
#define TOP_FLAG 0x00000F00
#define TOP_RIGHT_FLAG 0x0000F000
#define MAX_UWORD8 0xFF
#define MAX_DOUBLE 1.7e+308  ///< max. value of double-type value
#define MAX_INTRA_COST_IPE 0x0F7F7F7F

#define MAX_TU_ROW_IN_CTB (MAX_CTB_SIZE >> 2)
#define MAX_TU_COL_IN_CTB (MAX_CTB_SIZE >> 2)

#define BIT_DEPTH 8

#define FAST_PARTITION_WITH_TRANSFORM 1

#define IHEVCE_INTRA_REF_FILTERING C
#define IHEVCE_INTRA_LUMA_REF_SUBSTITUTION C
/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
/** /breif 4x4 DST, 4x4, 8x8, 16x16, 32x32 */
#define NUM_TRANS_TYPES 5
#define INTRA_PLANAR 0
#define INTRA_DC 1

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/
#define INTRA_ANGULAR(x) (x)

/** @breif max 30bit value */
#define MAX30 ((1 << 30) - 1)

/* @bried macro to clip a data to max of 30bits (assuming unsgined) */
#define CLIP30(x) ((x) > MAX30 ? MAX30 : (x))

/* @bried compute the (lambda * rate) with a qshift and clip result to 30bits */
#define COMPUTE_RATE_COST_CLIP30(r, l, qshift) ((WORD32)CLIP30((ULWORD64)((r) * (l)) >> (qshift)))

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/
typedef UWORD32 (*pf_res_trans_luma_had)(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 size);

typedef void (*pf_ipe_intra_pred)(
    UWORD8 *pu1_ref, WORD32 src_strd, UWORD8 *pu1_dst, WORD32 dst_strd, WORD32 nt, WORD32 mode);

typedef UWORD32 (*pf_ipe_res_trans)(
    UWORD8 *pu1_src,
    UWORD8 *pu1_pred,
    WORD16 *pi2_tmp,
    WORD16 *pi2_dst,
    WORD32 src_strd,
    WORD32 pred_strd,
    WORD32 dst_strd,
    WORD32 chroma_flag);

typedef FT_CALC_HAD_SATD_8BIT *pf_ipe_res_trans_had;
/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

typedef enum
{

    IPE_CTXT = 0,
    IPE_THRDS_CTXT,

    /* should be last entry */
    NUM_IPE_MEM_RECS

} IPE_MEM_TABS_T;

typedef enum
{
    IPE_FUNC_MODE_0 = 0,
    IPE_FUNC_MODE_1,
    IPE_FUNC_MODE_2,
    IPE_FUNC_MODE_3TO9,
    IPE_FUNC_MODE_10,
    IPE_FUNC_MODE_11TO17,
    IPE_FUNC_MODE_18_34,
    IPE_FUNC_MODE_19TO25,
    IPE_FUNC_MODE_26,
    IPE_FUNC_MODE_27TO33,

    NUM_IPE_FUNCS

} IPE_FUNCS_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
/**
******************************************************************************
 *  @brief    IPE CTB to CU and TU Quadtree Recursive Structure
******************************************************************************
 */

typedef struct ihevce_ipe_cu_tree_t ihevce_ipe_cu_tree_t;

typedef struct ihevce_ipe_cu_tree_t
{
    /**
     * Origin of current coding unit relative to top-left of CTB
     */
    UWORD16 u2_x0;

    UWORD16 u2_y0;

    /**
     * Origin of current coding unit relative to top-left of Picture
     */
    UWORD16 u2_orig_x;

    UWORD16 u2_orig_y;

    /**
     * Size of current coding unit in luma pixels
     */
    UWORD8 u1_cu_size;

    UWORD8 u1_width;

    UWORD8 u1_height;

    UWORD8 u1_depth;

    UWORD8 u1_part_flag_pos;

    UWORD8 u1_log2_nt;

    WORD32 i4_nbr_flag;

    /**
     * Recursive Bracketing Parameters
     */
    UWORD8 best_mode;

    WORD32 best_satd;

    WORD32 best_cost;

    /**
     * Number of pixels available in these neighbors
     */
    UWORD8 u1_num_left_avail;

    UWORD8 u1_num_top_avail;

    UWORD8 u1_num_top_right_avail;

    UWORD8 u1_num_bottom_left_avail;

    UWORD8 au1_best_mode_1tu[NUM_BEST_MODES];

    WORD32 au4_best_cost_1tu[NUM_BEST_MODES];

    UWORD8 au1_best_mode_4tu[NUM_BEST_MODES];

    WORD32 au4_best_cost_4tu[NUM_BEST_MODES];

    ihevce_ipe_cu_tree_t *ps_parent;

    ihevce_ipe_cu_tree_t *ps_sub_cu[4];

    /* best mode bits cost */
    UWORD16 u2_mode_bits_cost;

} ihevce_ipe_cu_tree_node_t;

/**
******************************************************************************
 *  @brief    IPE module context memory
******************************************************************************
 */
typedef struct
{
    ihevce_ipe_cu_tree_t *ps_ipe_cu_tree;

    /* one parent and four children */
    ihevce_ipe_cu_tree_t as_ipe_cu_tree[5];

    UWORD8 au1_ctb_mode_map[MAX_TU_ROW_IN_CTB + 1][MAX_TU_COL_IN_CTB + 1];

    UWORD8 au1_cand_mode_list[3];

    /** Pointer to structure containing function pointers of common*/
    func_selector_t *ps_func_selector;

    /**
     * CU level Qp / 6
     */
    WORD32 i4_cu_qp_div6;

    /**
     * CU level Qp % 6
     */
    WORD32 i4_cu_qp_mod6;

    /** array of luma intra prediction function pointers */
    pf_ipe_intra_pred apf_ipe_lum_ip[NUM_IPE_FUNCS];

    /** array of function pointers for residual and
     *  forward transform for all transform sizes
     */
    pf_res_trans_luma apf_resd_trns[NUM_TRANS_TYPES];

    /** array of function pointers for residual and
     *  forward transform for all transform sizes
     */
    pf_res_trans_luma_had apf_resd_trns_had[NUM_TRANS_TYPES];

    /** array of pointer to store the scaling matrices for
     *  all transform sizes and qp % 6 (pre computed)
     */
    WORD16 *api2_scal_mat[NUM_TRANS_TYPES * 2];

    /** array of pointer to store the re-scaling matrices for
     *  all transform sizes and qp % 6 (pre computed)
     */
    WORD16 *api2_rescal_mat[NUM_TRANS_TYPES * 2];

    /** Qunatization rounding factor for inter and intra CUs */
    WORD32 i4_quant_rnd_factor[2];

    UWORD8 u1_ctb_size;

    UWORD8 u1_min_cu_size;

    UWORD8 u1_min_tu_size;

    UWORD16 u2_ctb_row_num;

    UWORD16 u2_ctb_num_in_row;

    WORD8 i1_QP;

    UWORD8 u1_num_b_frames;

    UWORD8 b_sad_type;

    UWORD8 u1_ipe_step_size;

    WORD32 i4_ol_satd_lambda;

    WORD32 i4_ol_sad_lambda;

    UWORD8 au1_nbr_ctb_map[MAX_PU_IN_CTB_ROW + 1 + 8][MAX_PU_IN_CTB_ROW + 1 + 8];

    /**
     * Pointer to (1,1) location in au1_nbr_ctb_map
     */
    UWORD8 *pu1_ctb_nbr_map;

    /**
     * neigbour map buffer stride;
     */
    WORD32 i4_nbr_map_strd;

    /** CTB neighbour availability flags */
    nbr_avail_flags_t s_ctb_nbr_avail_flags;

    /** Slice Type of the current picture being processed */
    WORD32 i4_slice_type;

    /** Temporal ID of the current picture being processed */
    WORD32 i4_temporal_lyr_id;

    WORD32 i4_ol_sad_lambda_qf_array[MAX_HEVC_QP_10bit + 1];
    WORD32 i4_ol_satd_lambda_qf_array[MAX_HEVC_QP_10bit + 1];

    /************************************************************************/
    /* The fields with the string 'type2' in their names are required */
    /* when both 8bit and hbd lambdas are needed. The lambdas corresponding */
    /* to the bit_depth != internal_bit_depth are stored in these fields */
    /************************************************************************/
    WORD32 i4_ol_sad_type2_lambda_qf_array[MAX_HEVC_QP_10bit + 1];
    WORD32 i4_ol_satd_type2_lambda_qf_array[MAX_HEVC_QP_10bit + 1];

    /*Store the HEVC frame level qp for level modulation*/
    WORD32 i4_hevc_qp;
    /*Store the frame level qscale for level modulation*/
    WORD32 i4_qscale;
#if POW_OPT
    /* Averge activity of 8x8 blocks from previous frame
    *  If L1, maps to 16*16 in L0
    */
    long double ld_curr_frame_8x8_log_avg[2];

    /* Averge activity of 16x16 blocks from previous frame
    *  If L1, maps to 32*32 in L0
    */
    long double ld_curr_frame_16x16_log_avg[3];

    /* Averge activity of 32x32 blocks from previous frame
    *  If L1, maps to 64*64 in L0
    */
    long double ld_curr_frame_32x32_log_avg[3];
#else
    /* Averge activity of 8x8 blocks from previous frame
    *  If L1, maps to 16*16 in L0
    */
    LWORD64 i8_curr_frame_8x8_avg_act[2];

    /* Averge activity of 16x16 blocks from previous frame
    *  If L1, maps to 32*32 in L0
    */
    LWORD64 i8_curr_frame_16x16_avg_act[3];

    /* Averge activity of 32x32 blocks from previous frame
    *  If L1, maps to 64*64 in L0
    */
    LWORD64 i8_curr_frame_32x32_avg_act[3];
#endif
    /** Frame-levelSATD cost accumalator */
    LWORD64 i8_frame_acc_satd_cost;

    /** Frame-levelSATD accumalator */
    LWORD64 i8_frame_acc_satd;

    /** Frame-level activity factor for CU 8x8 accumalator */
    LWORD64 i8_frame_acc_act_factor;

    /** Frame-level Mode Bits cost accumalator */
    LWORD64 i8_frame_acc_mode_bits_cost;

    /** Encoder quality preset : See IHEVCE_QUALITY_CONFIG_T for presets */
    WORD32 i4_quality_preset;

    /** Frame-level SATD/qp accumulator in q10 format*/
    LWORD64 i8_frame_acc_satd_by_modqp_q10;

    /** For testing EIID only. */
    UWORD32 u4_num_16x16_skips_at_L0_IPE;

    /** Reference sample array. Used as local variable in mode_eval_filtering  */
    UWORD8 au1_ref_samples[1028];
    /** filtered reference sample array. Used as local variable in mode_eval_filtering */
    UWORD8 au1_filt_ref_samples[1028];
    /** array for the modes to be evaluated. Used as local variable in mode_eval_filtering */
    UWORD8 au1_modes_to_eval[MAX_NUM_IP_MODES];
    /** temp array for the modes to be evaluated. Used as local variable in mode_eval_filtering */
    UWORD8 au1_modes_to_eval_temp[MAX_NUM_IP_MODES];
    /** pred samples array. Used as local variable in mode_eval_filtering */
    MEM_ALIGN32 UWORD8 au1_pred_samples[4096];
    /** array for storing satd cost. Used as local variable in mode_eval_filtering*/
    UWORD16 au2_mode_bits_satd_cost[MAX_NUM_IP_MODES];
    /** array for storing satd values. used as local variable in mode_eval_filtering */
    UWORD16 au2_mode_bits_satd[MAX_NUM_IP_MODES];

    /** reference data, local for pu_calc_8x8 */
    UWORD8 au1_ref_8x8pu[4][18];
    /** mode_bits_cost, local for pu_calc_8x8 */
    UWORD16 au2_mode_bits_cost_8x8pu[4][MAX_NUM_IP_MODES];
    /** mode_bits, local for pu_calc_8x8 */
    UWORD16 au2_mode_bits_8x8_pu[MAX_NUM_IP_MODES];

    /** tranform coeff temp, local to ihevce_pu_calc_4x4_blk */
    WORD16 *pi2_trans_tmp;  //this memory is overlayed with au1_pred_samples[4096]. First half.

    /** tranform coeff out, local to ihevce_pu_calc_4x4_blk */
    WORD16 *pi2_trans_out;  //this memory is overlayed with au1_pred_samples[4096]. Second half.

    UWORD8 u1_use_lambda_derived_from_min_8x8_act_in_ctb;

    UWORD8 u1_bit_depth;

    rc_quant_t *ps_rc_quant_ctxt;
    /** Flag that specifies whether to use SATD or SAD in L0 IPE */
    UWORD8 u1_use_satd;

    /** Flag that specifies level of refinement */
    UWORD8 u1_level_1_refine_on;

    /** Flag indicates that child mode decision is disabled in L0 IPE recur bracketing */
    UWORD8 u1_disable_child_cu_decide;

    /*Modulation factor*/
    WORD32 ai4_mod_factor_derived_by_variance[2];
    float f_strength;
    WORD32 i4_l0ipe_qp_mod;

    WORD32 i4_frm_qp;
    WORD32 i4_temporal_layer;
    WORD32 i4_pass;

    double f_i_pic_lamda_modifier;
    WORD32 i4_use_const_lamda_modifier;
    WORD32 i4_is_ref_pic;
    LWORD64 i8_curr_frame_avg_mean_act;
    WORD32 i4_enable_noise_detection;

    ihevce_ipe_optimised_function_list_t s_ipe_optimised_function_list;

    ihevce_cmn_opt_func_t s_cmn_opt_func;

} ihevce_ipe_ctxt_t;

/**
******************************************************************************
 *  @brief    IPE module overall context
******************************************************************************
 */
typedef struct
{
    /*array of ipe ctxt */
    ihevce_ipe_ctxt_t *aps_ipe_thrd_ctxt[MAX_NUM_FRM_PROC_THRDS_PRE_ENC];

    /** Number of processing threads created run time */
    WORD32 i4_num_proc_thrds;

} ihevce_ipe_master_ctxt_t;

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
void ihevce_ipe_analyse_update_cost(
    ihevce_ipe_cu_tree_t *ps_cu_node, UWORD8 u1_mode, DOUBLE lf_cost);
#endif /* _IHEVCE_IPE_STRUCTS_H_ */
