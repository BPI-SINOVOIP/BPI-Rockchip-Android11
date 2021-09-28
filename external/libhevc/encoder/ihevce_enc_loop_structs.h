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
* \file ihevce_enc_loop_structs.h
*
* \brief
*    This file contains strcutures of enc_loop pass
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_ENC_LOOP_STRUCTS_H_
#define _IHEVCE_ENC_LOOP_STRUCTS_H_

#include "ihevc_macros.h"

extern UWORD16 gau2_ihevce_cabac_bin_to_bits[64 * 2];

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
/** /breif 4x4 DST, 4x4, 8x8, 16x16, 32x32 */
#define NUM_TRANS_TYPES 5
#define INTRA_PLANAR 0
#define INTRA_DC 1
#define NUM_POSSIBLE_TU_SIZES_CHR_INTRA_SATD 2
#define MAX_TU_IN_TU_EQ_DIV_2 4
#define MAX_MVP_LIST_CAND 2
#define MAX_COST 0x7ffffff
#define MAX_COST_64 0x7ffffffffffffff
#define NUM_32CU_AND_64CU_IN_CTB 5 /* 4 - 32x32 + 1 64x64*/
#define PING_PONG 2
#define MAX_SAO_RD_CAND 10
#define SCRATCH_BUF_STRIDE 80

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/
#define INTRA_ANGULAR(x) (x)

/** @breif max 30bit value */
#define MAX30 ((1 << 30) - 1)

/* @brief macro to clip a data to max of 30bits (assuming unsgined) */
#define CLIP30(x) ((x) > MAX30 ? MAX30 : (x))

/* @brief compute the (lambda * rate) with a qshift and clip result to 30bits */
#define COMPUTE_RATE_COST_CLIP30(r, l, qshift) ((WORD32)CLIP30((ULWORD64)((r) * (l)) >> (qshift)))

#define IHEVCE_INV_WT_PRED(inp, wt, off, shift)                                                    \
    (((((inp) - (off)) << (shift)) * wt + (1 << 14)) >> 15)

#define POPULATE_PU_STRUCT(ps_pu, mvx, mvy, offset_x, offset_y, wd, ht, ref_idx, pred_lx)          \
    {                                                                                              \
        (ps_pu)->b4_pos_x = (offset_x) >> 2;                                                       \
        (ps_pu)->b4_pos_y = (offset_y) >> 2;                                                       \
        (ps_pu)->b4_wd = ((wd) >> 2) - 1;                                                          \
        (ps_pu)->b4_ht = ((ht) >> 2) - 1;                                                          \
        (ps_pu)->b1_intra_flag = 0;                                                                \
        (ps_pu)->b2_pred_mode = pred_lx;                                                           \
        if(pred_lx)                                                                                \
        {                                                                                          \
            (ps_pu)->mv.i1_l0_ref_idx = -1;                                                        \
            (ps_pu)->mv.i1_l1_ref_idx = ref_idx;                                                   \
            (ps_pu)->mv.s_l1_mv.i2_mvx = mvx;                                                      \
            (ps_pu)->mv.s_l1_mv.i2_mvy = mvy;                                                      \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            (ps_pu)->mv.i1_l0_ref_idx = ref_idx;                                                   \
            (ps_pu)->mv.i1_l1_ref_idx = -1;                                                        \
            (ps_pu)->mv.s_l0_mv.i2_mvx = mvx;                                                      \
            (ps_pu)->mv.s_l0_mv.i2_mvy = mvy;                                                      \
        }                                                                                          \
    }

#define GET_FRAME_QSTEP_FROM_QP(frame_qp, frame_qstep)                                             \
    {                                                                                              \
        double q_steps[6] = { 0.625, 0.703, 0.79, 0.889, 1.0, 1.125 };                             \
                                                                                                   \
        frame_qstep = (WORD32)((1 << ((frame_qp) / 6)) * q_steps[(frame_qp) % 6]);                 \
    }

#define INITIALISE_MERGE_RESULT_STRUCT(ps_merge_data, pas_pu_results)                              \
    {                                                                                              \
        WORD32 i, j, k;                                                                            \
                                                                                                   \
        for(i = 0; i < TOT_NUM_PARTS; i++)                                                         \
        {                                                                                          \
            (ps_merge_data)->s_pu_results.u1_num_results_per_part_l0[i] = 0;                       \
            (ps_merge_data)->s_pu_results.u1_num_results_per_part_l1[i] = 0;                       \
        }                                                                                          \
        for(i = 0; i < 2; i++)                                                                     \
        {                                                                                          \
            for(j = 0; j < TOT_NUM_PARTS; j++)                                                     \
            {                                                                                      \
                (ps_merge_data)->s_pu_results.aps_pu_results[i][j] = pas_pu_results[i][j];         \
                for(k = 0; k < MAX_NUM_RESULTS_PER_PART_LIST; k++)                                 \
                {                                                                                  \
                    pas_pu_results[i][j][k].i4_tot_cost = MAX_COST;                                \
                    pas_pu_results[i][j][k].pu.mv.i1_l0_ref_idx = -1;                              \
                    pas_pu_results[i][j][k].pu.mv.i1_l1_ref_idx = -1;                              \
                }                                                                                  \
            }                                                                                      \
        }                                                                                          \
    }

#define POPULATE_CTB_PARAMS                                                                        \
    (ps_common_frm_prms,                                                                           \
     apu1_wt_inp,                                                                                  \
     i4_ctb_x_off,                                                                                 \
     i4_ctb_y_off,                                                                                 \
     ppu1_pred,                                                                                    \
     cu_size,                                                                                      \
     ref_stride,                                                                                   \
     bidir_enabled,                                                                                \
     num_refs,                                                                                     \
     pps_rec_list_l0,                                                                              \
     pps_rec_list_l1,                                                                              \
     pu1_non_wt_inp,                                                                               \
     lambda,                                                                                       \
     lambda_q_shift,                                                                               \
     wpred_log_wdc)                                                                                \
    {                                                                                              \
        WORD32 i, j;                                                                               \
        (ps_common_frm_prms)->i4_bidir_enabled = bidir_enabled;                                    \
        (ps_common_frm_prms)->i4_ctb_x_off = i4_ctb_x_off;                                         \
        (ps_common_frm_prms)->i4_ctb_y_off = i4_ctb_y_off;                                         \
        (ps_common_frm_prms)->i4_inp_stride = cu_size;                                             \
        (ps_common_frm_prms)->i4_lamda = lambda;                                                   \
        (ps_common_frm_prms)->i4_pred_stride = cu_size;                                            \
        (ps_common_frm_prms)->i4_rec_stride = ref_stride;                                          \
        (ps_common_frm_prms)->pps_rec_list_l0 = pps_rec_list_l0;                                   \
        (ps_common_frm_prms)->pps_rec_list_l1 = pps_rec_list_l1;                                   \
        (ps_common_frm_prms)->ppu1_pred = ppu1_pred;                                               \
        (ps_common_frm_prms)->pu1_non_wt_inp = pu1_non_wt_inp;                                     \
        (ps_common_frm_prms)->pu1_wkg_mem = NULL;                                                  \
        (ps_common_frm_prms)->u1_lamda_qshift = lambda_q_shift;                                    \
        (ps_common_frm_prms)->u1_num_ref = num_refs;                                               \
        (ps_common_frm_prms)->wpred_log_wdc = wpred_log_wdc;                                       \
        for(i = 0; i < 2; i++)                                                                     \
        {                                                                                          \
            for(j = 0; j < MAX_NUM_REF; j++)                                                       \
            {                                                                                      \
                (ps_common_frm_prms)->apu1_wt_inp = (apu1_wt_inp)[i][j];                           \
            }                                                                                      \
        }                                                                                          \
    }

#define COMPUTE_MERGE_IDX_COST(merge_idx_0_model, merge_idx, max_merge_cand, lambda, cost)         \
    {                                                                                              \
        WORD32 cab_bits_q12 = 0;                                                                   \
                                                                                                   \
        /* sanity checks */                                                                        \
        ASSERT((merge_idx >= 0) && (merge_idx < max_merge_cand));                                  \
                                                                                                   \
        /* encode the merge idx only if required */                                                \
        if(max_merge_cand > 1)                                                                     \
        {                                                                                          \
            WORD32 bin = (merge_idx > 0);                                                          \
                                                                                                   \
            /* bits for the context modelled first bin */                                          \
            cab_bits_q12 += gau2_ihevce_cabac_bin_to_bits[merge_idx_0_model ^ bin];                \
                                                                                                   \
            /* bits for larged merge idx coded as bypass tunary */                                 \
            if((max_merge_cand > 2) && (merge_idx > 0))                                            \
            {                                                                                      \
                cab_bits_q12 += (MIN(merge_idx, (max_merge_cand - 2))) << CABAC_FRAC_BITS_Q;       \
            }                                                                                      \
                                                                                                   \
            cost = COMPUTE_RATE_COST_CLIP30(                                                       \
                cab_bits_q12, lambda, (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));                       \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            cost = 0;                                                                              \
        }                                                                                          \
    }

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

typedef FT_CALC_HAD_SATD_8BIT *pf_res_trans_luma_had_chroma;

/** \breif function pointer prototype for residue and transform enc_loop */
typedef UWORD32 (*pf_res_trans_chroma)(
    UWORD8 *pu1_src,
    UWORD8 *pu1_pred,
    WORD32 *pi4_tmp,
    WORD16 *pi2_dst,
    WORD32 src_strd,
    WORD32 pred_strd,
    WORD32 dst_strd,
    CHROMA_PLANE_ID_T e_chroma_plane);

/** \breif function pointer prototype for quantization and inv Quant for ssd
calc. for all transform sizes */
typedef WORD32 (*pf_quant_iquant_ssd)(
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32 trans_size,
    WORD32 qp_div, /* qpscaled / 6 */
    WORD32 qp_rem, /* qpscaled % 6 */
    WORD32 q_add,
    WORD32 *pi4_quant_round_factor_0_1,
    WORD32 *pi4_quant_round_factor_1_2,
    WORD32 src_strd,
    WORD32 dst_q_strd,
    WORD32 dst_iq_strd,
    UWORD8 *csbf,
    WORD32 csbf_strd,
    WORD32 *zero_col,
    WORD32 *zero_row,
    WORD16 *pi2_dequant_coeff,
    LWORD64 *pi8_cost);

/** \breif function pointer prototype for quantization and inv Quant for ssd
calc. for all transform sizes (in case of RDOQ + SBH) */
typedef WORD32 (*pf_quant_iquant_ssd_sbh)(
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32 trans_size,
    WORD32 qp_div, /* qpscaled / 6 */
    WORD32 qp_rem, /* qpscaled % 6 */
    WORD32 q_add,
    WORD32 src_strd,
    WORD32 dst_q_strd,
    WORD32 dst_iq_strd,
    UWORD8 *csbf,
    WORD32 csbf_strd,
    WORD32 *zero_col,
    WORD32 *zero_row,
    WORD16 *pi2_dequant_coeff,
    WORD32 *pi4_cost,
    WORD32 i4_scan_idx,
    WORD32 i4_perform_rdoq);

/** \breif function pointer prototype for inverse transform and recon
for all transform sizes : Luma */
typedef void (*pf_it_recon)(
    WORD16 *pi2_src,
    WORD16 *pi2_tmp,
    UWORD8 *pu1_pred,
    UWORD8 *pu1_dst,
    WORD32 src_strd,
    WORD32 pred_strd,
    WORD32 dst_strd,
    WORD32 zero_cols,
    WORD32 zero_rows);

/** \breif function pointer prototype for inverse transform and recon
for all transform sizes : Chroma */
typedef void (*pf_it_recon_chroma)(
    WORD16 *pi2_src,
    WORD16 *pi2_tmp,
    UWORD8 *pu1_pred,
    UWORD8 *pu1_dst,
    WORD32 src_strd,
    WORD32 pred_strd,
    WORD32 dst_strd,
    WORD32 zero_cols,
    WORD32 zero_rows);

/** \breif function pointer prototype for luma sao. */
typedef void (*pf_sao_luma)(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_src_left,
    UWORD8 *pu1_src_top,
    UWORD8 *pu1_src_top_left,
    UWORD8 *pu1_src_top_right,
    UWORD8 *pu1_src_bot_left,
    UWORD8 *pu1_avail,
    WORD8 *pi1_sao_offset,
    WORD32 wd,
    WORD32 ht);

/** \breif function pointer prototype for chroma sao. */
typedef void (*pf_sao_chroma)(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_src_left,
    UWORD8 *pu1_src_top,
    UWORD8 *pu1_src_top_left,
    UWORD8 *pu1_src_top_right,
    UWORD8 *pu1_src_bot_left,
    UWORD8 *pu1_avail,
    WORD8 *pi1_sao_offset_u,
    WORD8 *pi1_sao_offset_v,
    WORD32 wd,
    WORD32 ht);

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

typedef enum
{
    IP_FUNC_MODE_0 = 0,
    IP_FUNC_MODE_1,
    IP_FUNC_MODE_2,
    IP_FUNC_MODE_3TO9,
    IP_FUNC_MODE_10,
    IP_FUNC_MODE_11TO17,
    IP_FUNC_MODE_18_34,
    IP_FUNC_MODE_19TO25,
    IP_FUNC_MODE_26,
    IP_FUNC_MODE_27TO33,

    NUM_IP_FUNCS

} IP_FUNCS_T;

typedef enum
{
    /* currently only cu and cu/2 modes are supported */
    TU_EQ_CU = 0,
    TU_EQ_CU_DIV2,
    TU_EQ_SUBCU, /* only applicable for NXN mode at mincusize */

    /* support for below modes needs to be added */
    TU_EQ_CU_DIV4,
    TU_EQ_CU_DIV8,
    TU_EQ_CU_DIV16,

    NUM_TU_WRT_CU,

} TU_SIZE_WRT_CU_T;

typedef enum
{
    RDOPT_MODE = 0,
    RDOPT_SKIP_MODE = 1,

    NUM_CORE_CALL_MODES,

} CORE_FUNC_CALL_MODE_T;

typedef enum
{
    ENC_LOOP_CTXT = 0,
    ENC_LOOP_THRDS_CTXT,
    ENC_LOOP_SCALE_MAT,
    ENC_LOOP_RESCALE_MAT,
    ENC_LOOP_TOP_LUMA,
    ENC_LOOP_TOP_CHROMA,
    ENC_LOOP_TOP_NBR4X4,
    ENC_LOOP_RC_PARAMS, /* memory to dump rate control parameters by each thread for each bit-rate instance */
    ENC_LOOP_QP_TOP_4X4,
    ENC_LOOP_DEBLOCKING,
    ENC_LOOP_422_CHROMA_INTRA_PRED,
    ENC_LOOP_INTER_PRED,
    ENC_LOOP_CHROMA_PRED_INTRA,
    ENC_LOOP_REF_SUB_OUT,
    ENC_LOOP_REF_FILT_OUT,
    ENC_LOOP_CU_RECUR_LUMA_RECON,
    ENC_LOOP_CU_RECUR_CHROMA_RECON,
    ENC_LOOP_CU_RECUR_LUMA_PRED,
    ENC_LOOP_CU_RECUR_CHROMA_PRED,
    ENC_LOOP_LEFT_LUMA_DATA,
    ENC_LOOP_LEFT_CHROMA_DATA,
    ENC_LOOP_SAO,
    ENC_LOOP_CU_COEFF_DATA,
    ENC_LOOP_CU_RECUR_COEFF_DATA,
    ENC_LOOP_CU_DEQUANT_DATA,
    ENC_LOOP_RECON_DATA_STORE,
    /* should always be the last entry */
    NUM_ENC_LOOP_MEM_RECS

} ENC_LOOP_MEM_TABS_T;

/** This is for assigning the pred buiffers for luma (2 ping-pong) and
chroma(1)   */
typedef enum
{
    CU_ME_INTRA_PRED_LUMA_IDX0 = 0,
    CU_ME_INTRA_PRED_LUMA_IDX1,
    CU_ME_INTRA_PRED_CHROMA_IDX,

    /* should be always the last entry */
    NUM_CU_ME_INTRA_PRED_IDX

} CU_ME_INTRA_PRED_IDX_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/**
******************************************************************************
*  @brief     Structure to store TU prms req. for enc_loop only
******************************************************************************
*/
typedef struct
{
    /** Zero_col info. for the current TU Luma */
    UWORD32 u4_luma_zero_col;
    /** Zero_row info. for the current TU Luma */
    UWORD32 u4_luma_zero_row;

    /** Zero_col info. for the current TU Chroma Cb */
    UWORD32 au4_cb_zero_col[2];
    /** Zero_row info. for the current TU Chroma Cb */
    UWORD32 au4_cb_zero_row[2];
    /** Zero_col info. for the current TU Chroma Cr */
    UWORD32 au4_cr_zero_col[2];
    /** Zero_row info. for the current TU Chroma Cr */
    UWORD32 au4_cr_zero_row[2];

    /** bytes consumed by the luma ecd data */
    WORD16 i2_luma_bytes_consumed;
    /** bytes consumed by the Cb ecd data */
    WORD16 ai2_cb_bytes_consumed[2];
    /** bytes consumed by the Cr ecd data */
    WORD16 ai2_cr_bytes_consumed[2];

    /** flag to re-evaluate IQ and Coeff data of luma in the final_recon
    function. If zero, uses the data from RDOPT cand.                   */
    UWORD16 b1_eval_luma_iq_and_coeff_data : 1;
    /** flag to re-evaluate IQ and Coeff data of chroma in the final_recon
    function. If zero, uses the data from RDOPT cand.                   */
    UWORD16 b1_eval_chroma_iq_and_coeff_data : 1;

    /* TO DO : No support now, need to add. Always comapre ZERO_CBF cost */
    /** Luma ZERO_CBF cost is compared with residue coding cost only if this
    flag is enabled */
    UWORD16 b1_eval_luma_zero_cbf_cost : 1;
    /** Chroma ZERO_CBF cost is compared with residue coding cost only if this
    flag is enabled */
    UWORD16 b1_eval_chroma_zero_cbf_cost : 1;

    /** Reserved to make WORD32 alignment */
    UWORD16 b12_reserved : 12;

} tu_enc_loop_temp_prms_t;

typedef struct recon_datastore_t
{
    /* 2 to store current and best */
    void *apv_luma_recon_bufs[2];

    /* 0 to store cur chroma mode recon */
    /* 1 to store winning independent chroma mode with a single TU's recon */
    /* 2 to store winning independent chroma mode with 4 TUs' recon */
    void *apv_chroma_recon_bufs[3];

    /* The following two arrays are used to store the ID's of the buffers */
    /* where the winning recon is being stored */
    /* For Luma buffers, the permissible values are 0, 1 and UCHAR_MAX */
    /* For Chroma buffers, the permissible values are 0, 1, 2 and UCHAR_MAX */
    /* The value 'UCHAR_MAX' indicates the absence of Recon for that particular TU */
    UWORD8 au1_bufId_with_winning_LumaRecon[MAX_TU_IN_CTB_ROW * MAX_TU_IN_CTB_ROW];

    /* 2 - 2 Chroma planes */
    /* 2 - 2 possible subTU's */
    UWORD8 au1_bufId_with_winning_ChromaRecon[2][MAX_TU_IN_CTB_ROW * MAX_TU_IN_CTB_ROW][2];

    WORD32 i4_lumaRecon_stride;

    WORD32 i4_chromaRecon_stride;

    UWORD8 au1_is_chromaRecon_available[3];

    UWORD8 u1_is_lumaRecon_available;

} recon_datastore_t;

typedef struct enc_loop_cu_final_prms_t
{
    recon_datastore_t s_recon_datastore;

    /**
    * Cu size of the current cu being processed
    */
    UWORD8 u1_cu_size;
    /**
    * flags to indicate the final cu prediction mode
    */
    UWORD8 u1_intra_flag;

    /**
    * flags to indicate Skip mode for CU
    */
    UWORD8 u1_skip_flag;

    /**
    * number of tu in current cu for a given mode
    * if skip then this value should be 1
    */
    UWORD16 u2_num_tus_in_cu;

    /**
    * number of pu in current cu for a given mode
    * if skip then this value should be 1
    */
    UWORD16 u2_num_pus_in_cu;

    /**
    * total bytes produced in ECD data buffer
    * if skip then this value should be 0
    */
    WORD32 i4_num_bytes_ecd_data;

    /**
    * Partition mode of the best candidate
    * if skip then this value should be SIZE_2Nx2N
    * @sa PART_SIZE_E
    */
    UWORD8 u1_part_mode;

    /**
    * indicates if inter cu has coded coeffs 1: coded, 0: not coded
    * if skip then this value shoudl be ignored
    */
    UWORD8 u1_is_cu_coded;

    /**
    * Chroma pred mode as signalled in bitstream
    */
    UWORD8 u1_chroma_intra_pred_mode;

    /**
    * To store the best chroma mode for TU. Will be same for NxN case.
    * Actual Chroma pred
    */
    UWORD8 u1_chroma_intra_pred_actual_mode;

    /**
    * sad accumulated over all Tus of given CU
    */
    UWORD32 u4_cu_sad;

    /**
    * sad accumulated over all Tus of given CU
    */
    LWORD64 i8_cu_ssd;

    /**
    * open loop intra sad
    */
    UWORD32 u4_cu_open_intra_sad;

    /**
    * header bits of cu estimated during RDO evaluation.
    * Includes tu splits flags excludes cbf flags
    */
    UWORD32 u4_cu_hdr_bits;
    /**
    * luma residual bits of a cu estimated during RDO evaluation.
    */
    UWORD32 u4_cu_luma_res_bits;

    /**
    * chroma residual bits of a cu estimated during RDO evaluation.
    */
    UWORD32 u4_cu_chroma_res_bits;

    /**
    * cbf bits of a cu estimated during RDO evaluation (considered as part of texture bits later)
    */
    UWORD32 u4_cu_cbf_bits;

    /**
    * array of PU for current CU
    * For Inter PUs this will contain the follwoing
    *   - merge flag
    *   - (MVD and reference indicies) or (Merge Index)
    *   - (if Cu is skipped then Merge index for skip
    *      will be in 1st PU entry in array)
    * for intra PU only intra flag will be set to 1
    *
    */
    pu_t as_pu_enc_loop[NUM_PU_PARTS];

    /**
    * array of PU for chroma usage
    * in case of Merge MVs and reference idx of the final candidate
    * used by luma need sto be stored
    * for intra PU this will not be used
    */
    pu_t as_pu_chrm_proc[NUM_PU_PARTS];

    /**
    * array of colocated PU for current CU
    * MV and Ref pic id should be stored in this
    * for intra PU only intra flag will be set to 1
    */
    pu_col_mv_t as_col_pu_enc_loop[NUM_INTER_PU_PARTS];

    /** array to store the intra mode pred related params
    * if nxn mode the all 4 lcoations will be used
    */
    intra_prev_rem_flags_t as_intra_prev_rem[NUM_PU_PARTS];

    /**
    * array to store TU propeties of the each tu in a CU
    */
    tu_enc_loop_out_t as_tu_enc_loop[MAX_TU_IN_CTB_ROW * MAX_TU_IN_CTB_ROW];

    /**
    * array to store TU propeties (req. for enc_loop only and not for
    * entropy) of the each tu in a CU
    */
    tu_enc_loop_temp_prms_t as_tu_enc_loop_temp_prms[MAX_TU_IN_CTB_ROW * MAX_TU_IN_CTB_ROW];

    /**
    * Neighbour flags stored for chroma reuse
    */
    UWORD32 au4_nbr_flags[MAX_TU_IN_CTB_ROW * MAX_TU_IN_CTB_ROW];

    /**
    * intra pred modes stored for chroma reuse
    */
    UWORD8 au1_intra_pred_mode[4];

    /**
    * array for storing coeffs during RD opt stage at CU level.
    * Luma and chroma together
    */
    UWORD8 *pu1_cu_coeffs;

    /**
    * Chroma deq_coeffs start point in the ai2_cu_deq_coeffs buffer.
    */
    WORD32 i4_chrm_cu_coeff_strt_idx;

    /**
    * array for storing dequantized vals. during RD opt stage at CU level
    * Luma and chroma together.
    * Stride is assumed to be cu_size
    * u-v interleaved storing is at TU level
    */
    WORD16 *pi2_cu_deq_coeffs;

    /**
    * Chroma deq_coeffs start point in the ai2_cu_deq_coeffs buffer.
    */
    WORD32 i4_chrm_deq_coeff_strt_idx;

    /**
    * The total RDOPT cost of the CU for the best mode
    */
    LWORD64 i8_best_rdopt_cost;

    /**
    * The current running RDOPT cost for the current mode
    */
    LWORD64 i8_curr_rdopt_cost;

    LWORD64 i8_best_distortion;

} enc_loop_cu_final_prms_t;

typedef struct
{
    /** Current Cu chroma recon pointer in pic buffer */
    UWORD8 *pu1_final_recon;

    UWORD16 *pu2_final_recon;

    /** Current Cu chroma source pointer in pic buffer */
    UWORD8 *pu1_curr_src;

    UWORD16 *pu2_curr_src;

    /** Current CU chroma reocn buffer stride */
    WORD32 i4_chrm_recon_stride;

    /** Current CU chroma source buffer stride */
    WORD32 i4_chrm_src_stride;

    /** Current Cu chroma Left pointer for intra pred */
    UWORD8 *pu1_cu_left;

    UWORD16 *pu2_cu_left;

    /** Left buffer stride */
    WORD32 i4_cu_left_stride;

    /** Current Cu chroma top pointer for intra pred */
    UWORD8 *pu1_cu_top;

    UWORD16 *pu2_cu_top;

    /** Current Cu chroma top left pointer for intra pred */
    UWORD8 *pu1_cu_top_left;

    UWORD16 *pu2_cu_top_left;

} enc_loop_chrm_cu_buf_prms_t;

typedef struct
{
    /** cost of the current satd cand */
    WORD32 i4_cost;

    /** tu size w.r.t to cu of the current satd cand
    * @sa TU_SIZE_WRT_CU_T
    */
    WORD8 i4_tu_depth;

    /**
    *  access valid number of entries in this array based on u1_part_size
    */
    UWORD8 au1_intra_luma_modes[NUM_PU_PARTS];

    /** @remarks u1_part_size 2Nx2N or  NxN  */
    UWORD8 u1_part_mode; /* @sa: PART_SIZE_E */

    /** Flag to indicate whether current candidate needs to be evaluated */
    UWORD8 u1_eval_flag;

} cu_intra_satd_out_t;

/** \brief cu level parameters for SATD / RDOPT function */

typedef struct
{
    /** pointer to source luma pointer
    *  pointer will be pointing to CTB start location
    *  At CU level based on the CU position this pointer
    *  has to appropriately incremented
    */
    UWORD8 *pu1_luma_src;

    UWORD16 *pu2_luma_src;

    /** pointer to source chroma pointer
    *  pointer will be pointing to CTB start location
    *  At CU level based on the CU position this pointer
    *  has to appropriately incremented
    */
    UWORD8 *pu1_chrm_src;

    UWORD16 *pu2_chrm_src;

    /** pointer to recon luma pointer
    *  pointer will be pointing to CTB start location
    *  At CU level based on the CU position this pointer
    *  has to appropriately incremented
    */
    UWORD8 *pu1_luma_recon;

    UWORD16 *pu2_luma_recon;

    /** pointer to recon chroma pointer
    *  pointer will be pointing to CTB start location
    *  At CU level based on the CU position this pointer
    *  has to appropriately incremented
    */
    UWORD8 *pu1_chrm_recon;

    UWORD16 *pu2_chrm_recon;

    /*1st pass parallel dpb buffer pointers aimilar to the above*/
    UWORD8 *pu1_luma_recon_src;

    UWORD16 *pu2_luma_recon_src;

    UWORD8 *pu1_chrm_recon_src;

    UWORD16 *pu2_chrm_recon_src;

    /** Pointer to Subpel Plane Buffer */
    UWORD8 *pu1_sbpel_hxfy;

    /** Pointer to Subpel Plane Buffer */
    UWORD8 *pu1_sbpel_fxhy;

    /** Pointer to Subpel Plane Buffer */
    UWORD8 *pu1_sbpel_hxhy;

    /** Luma source stride */
    WORD32 i4_luma_src_stride;

    /** chroma soruce stride */
    WORD32 i4_chrm_src_stride;

    /** Luma recon stride */
    WORD32 i4_luma_recon_stride;

    /** chroma recon stride */
    WORD32 i4_chrm_recon_stride;

    /** ctb size */
    WORD32 i4_ctb_size;

    /** current ctb postion horz */
    WORD32 i4_ctb_pos;

    /** number of PU finalized for curr CU  */
    WORD32 i4_num_pus_in_cu;

    /** number of bytes consumed for current in ecd data buf */
    WORD32 i4_num_bytes_cons;

    UWORD8 u1_is_cu_noisy;

    UWORD8 *pu1_is_8x8Blk_noisy;

} enc_loop_cu_prms_t;

/**
******************************************************************************
*  @brief Pad inter pred recon context
******************************************************************************
*/
typedef struct
{
    /** Pointer to Subpel Plane Buffer */
    UWORD8 *pu1_sbpel_hxfy;

    /** Pointer to Subpel Plane Buffer */
    UWORD8 *pu1_sbpel_fxhy;

    /** Pointer to Subpel Plane Buffer */
    UWORD8 *pu1_sbpel_hxhy;

    /** pointer to recon luma pointer
    *  pointer will be pointing to CTB start location
    *  At CU level based on the CU position this pointer
    *  has to appropriately incremented
    */
    UWORD8 *pu1_luma_recon;

    /** pointer to recon chroma pointer
    *  pointer will be pointing to CTB start location
    *  At CU level based on the CU position this pointer
    *  has to appropriately incremented
    */
    UWORD8 *pu1_chrm_recon;

    /*FOr recon source 1st pass starts*/

    UWORD8 *pu1_luma_recon_src;

    /** pointer to recon chroma pointer
    *  pointer will be pointing to CTB start location
    *  At CU level based on the CU position this pointer
    *  has to appropriately incremented
    */
    UWORD8 *pu1_chrm_recon_src;
    /*FOr recon source 1st pass ends */
    /** Luma recon stride */
    WORD32 i4_luma_recon_stride;

    /** chroma recon stride */
    WORD32 i4_chrm_recon_stride;

    /** ctb size */
    WORD32 i4_ctb_size;

    /* 0 - 400; 1 - 420; 2 - 422; 3 - 444 */
    UWORD8 u1_chroma_array_type;

} pad_interp_recon_frm_t;

/**
******************************************************************************
*  @brief inter prediction (MC) context for enc loop
******************************************************************************
*/
/*IMPORTANT please keep inter_pred_ctxt_t and inter_pred_me_ctxt_t as identical*/
typedef struct
{
    /** pointer to reference lists */
    recon_pic_buf_t *(*ps_ref_list)[HEVCE_MAX_REF_PICS * 2];

    /** scratch buffer for horizontal interpolation destination */
    WORD16 MEM_ALIGN16 ai2_horz_scratch[MAX_CTB_SIZE * (MAX_CTB_SIZE + 8)];

    /** scratch 16 bit buffer for interpolation in l0 direction */
    WORD16 MEM_ALIGN16 ai2_scratch_buf_l0[MAX_CTB_SIZE * MAX_CTB_SIZE];

    /** scratch 16 bit buffer for interpolation in l1 direction */
    WORD16 MEM_ALIGN16 ai2_scratch_buf_l1[MAX_CTB_SIZE * MAX_CTB_SIZE];

    /** Pointer to struct containing function pointers to
    functions in the 'common' library' */
    func_selector_t *ps_func_selector;

    /** common denominator used for luma weights */
    WORD32 i4_log2_luma_wght_denom;

    /** common denominator used for chroma weights */
    WORD32 i4_log2_chroma_wght_denom;

    /**  offset w.r.t frame start in horz direction (pels) */
    WORD32 i4_ctb_frm_pos_x;

    /**  offset w.r.t frame start in vert direction (pels) */
    WORD32 i4_ctb_frm_pos_y;

    /* Bit Depth of Input */
    WORD32 i4_bit_depth;

    /* 0 - 400; 1 - 420; 2 - 422; 3 - 444 */
    UWORD8 u1_chroma_array_type;

    /** weighted_pred_flag      */
    WORD8 i1_weighted_pred_flag;

    /** weighted_bipred_flag    */
    WORD8 i1_weighted_bipred_flag;

    /** Structure to describe extra CTBs around frame due to search
    range associated with distributed-mode. Entries are top, left,
    right and bottom */
    WORD32 ai4_tile_xtra_pel[4];

} inter_pred_ctxt_t;
/*IMPORTANT please keep inter_pred_ctxt_t and inter_pred_me_ctxt_t as identical*/

typedef IV_API_CALL_STATUS_T (*PF_LUMA_INTER_PRED_PU)(
    void *pv_inter_pred_ctxt,
    pu_t *ps_pu,
    void *pv_dst_buf,
    WORD32 dst_stride,
    WORD32 i4_flag_inter_pred_source);

/**
******************************************************************************
*  @brief  Motion predictor context structure
******************************************************************************
*/
typedef struct
{
    /** pointer to reference lists */
    recon_pic_buf_t *(*ps_ref_list)[HEVCE_MAX_REF_PICS * 2];

    /** pointer to the slice header */
    slice_header_t *ps_slice_hdr;

    /** pointer to SPS */
    sps_t *ps_sps;

    /** CTB x. In CTB unit*/
    WORD32 i4_ctb_x;

    /** CTB y. In CTB unit */
    WORD32 i4_ctb_y;

    /** Log2 Parallel Merge Level - 2  */
    WORD32 i4_log2_parallel_merge_level_minus2;

    /* Number of extra CTBs external to tile due to fetched search-range around Tile */
    /* TOP, left, right and bottom */
    WORD32 ai4_tile_xtra_ctb[4];

} mv_pred_ctxt_t;

/**
******************************************************************************
*  @brief  Deblocking and Boundary strength CTB level structure
******************************************************************************
*/
typedef struct
{
    /** Array to store the packed BS values in horizontal direction  */
    UWORD32 au4_horz_bs[(MAX_CTB_SIZE >> 3) + 1];

    /** Array to store the packed BS values in vertical direction  */
    UWORD32 au4_vert_bs[(MAX_CTB_SIZE >> 3) + 1];

    /** CTB neighbour availability flags for deblocking */
    UWORD8 u1_not_first_ctb_col_of_frame;
    UWORD8 u1_not_first_ctb_row_of_frame;

} deblk_bs_ctb_ctxt_t;

/**
******************************************************************************
*  @brief  Deblocking and CTB level structure
******************************************************************************
*/
typedef struct
{
    /**
    * BS of the last vertical 4x4 column of previous CTB
    */
    UWORD8 au1_prev_bs[MAX_CTB_SIZE >> 3];

    /**
    * BS of the last vertical 4x4 column of previous CTB
    */
    UWORD8 au1_prev_bs_uv[MAX_CTB_SIZE >> 3];

    /** pointer to top 4x4 ctb nbr structure; for accessing qp  */
    nbr_4x4_t *ps_top_ctb_nbr_4x4;

    /** pointer to left 4x4 ctb nbr structure; for accessing qp */
    nbr_4x4_t *ps_left_ctb_nbr_4x4;

    /** pointer to current 4x4 ctb nbr structure; for accessing qp */
    nbr_4x4_t *ps_cur_ctb_4x4;

    /** max of 8 such contiguous bs to be computed for 64x64 ctb */
    UWORD32 *pu4_bs_horz;

    /** max of 8 such contiguous bs to be computed for 64x64 ctb */
    UWORD32 *pu4_bs_vert;

    /** ptr to current ctb luma pel in frame */
    UWORD8 *pu1_ctb_y;

    UWORD16 *pu2_ctb_y;

    /** ptr to current ctb sp interleaved chroma pel in frame */
    UWORD8 *pu1_ctb_uv;

    UWORD16 *pu2_ctb_uv;

    func_selector_t *ps_func_selector;

    /** left nbr buffer stride in terms of 4x4 units */
    WORD32 i4_left_nbr_4x4_strd;

    /** current  buffer stride in terms of 4x4 units */
    WORD32 i4_cur_4x4_strd;

    /** size in pels 16 / 32 /64 */
    WORD32 i4_ctb_size;

    /** stride for luma       */
    WORD32 i4_luma_pic_stride;

    /** stride for  chroma */
    WORD32 i4_chroma_pic_stride;

    /** boolean indicating if left ctb edge is to be deblocked or not */
    WORD32 i4_deblock_left_ctb_edge;

    /** boolean indicating if top ctb edge is to be deblocked or not */
    WORD32 i4_deblock_top_ctb_edge;

    /** beta offset index */
    WORD32 i4_beta_offset_div2;

    /** tc offset index */
    WORD32 i4_tc_offset_div2;

    /** chroma cb qp offset index */
    WORD32 i4_cb_qp_indx_offset;

    /** chroma cr qp offset index */
    WORD32 i4_cr_qp_indx_offset;

    WORD32 i4_bit_depth;

    /* 0 - 400; 1 - 420; 2 - 422; 3 - 444 */
    UWORD8 u1_chroma_array_type;

} deblk_ctb_params_t;

/**
******************************************************************************
*  @brief  Stores the BS and Qp of a CTB row. For CTB-row level deblocking
******************************************************************************
*/
typedef struct deblk_ctbrow_prms
{
    /**
    * Refer to ihevce_enc_loop_get_mem_recs() and
    * ihevce_enc_loop_init()for more info
    * regarding memory allocation to each one below.
    */

    /**
    * Stores the vertical boundary strength of a CTB row.
    */
    UWORD32 *pu4_ctb_row_bs_vert;

    /**
    * Storage is same as above. Contains horizontal BS.
    */
    UWORD32 *pu4_ctb_row_bs_horz;

    /**
    * Pointer to the CTB row's Qp storage
    */
    WORD8 *pi1_ctb_row_qp;

    /**
    * Stride of the pu1_ctb_row_qp_p buffer in WORD32 unit
    */
    WORD32 u4_qp_buffer_stride;

    /*
    *   Pointer to the  memory which contains the Qp of
    *   top4x4 neighbour blocks for each CTB row.
    *   This memory is at frame level.
    */
    WORD8 *api1_qp_top_4x4_ctb_row[MAX_NUM_ENC_LOOP_PARALLEL];

    /*
    *   Stride of the above memory location.
    *   Values in one-stride correspondes to one CTB row.
    */
    WORD32 u4_qp_top_4x4_buf_strd;

    /*size of frm level qp buffer*/
    WORD32 u4_qp_top_4x4_buf_size;

} deblk_ctbrow_prms_t;

/**
******************************************************************************
*  @brief  Entropy rd opt context for cabac bit estimation and RDO
******************************************************************************
*/
typedef struct rdopt_entropy_ctxt
{
    /**
    * array for entropy contexts during RD opt stage at CU level
    * one best and one current is required
    */
    entropy_context_t as_cu_entropy_ctxt[2];

    /**
    * init state of entropy context models during CU RD opt stage,
    * required for saving and restoring the cabac states
    */
    UWORD8 au1_init_cabac_ctxt_states[IHEVC_CAB_CTXT_END];

    /*
    * ptr to top row cu skip flags (1 bit per 8x8CU)
    */
    UWORD8 *pu1_cu_skip_top_row;

    /**
    * Current entropy ctxt idx
    */
    WORD32 i4_curr_buf_idx;

} rdopt_entropy_ctxt_t;

/**
******************************************************************************
*  @brief  structure to save predicted data from Inter SATD stage to Inter RD opt stage
******************************************************************************
*/
typedef struct
{
    /*Buffer to store the predicted data after motion compensation for merge and
    * skip candidates.
    * [2] Because for a given candidate we do motion compensation for 5 merge candidates.
    *     store the pred data after mc for the first 2 candidates and from 3rd candidate
    *     onwards, overwrite the data which has higher SATD cost.
    */
    void *apv_pred_data[2];

    /** Stride to store the predicted data
    */
    WORD32 i4_pred_data_stride;

} merge_skip_pred_data_t;
/**
******************************************************************************
*  @brief  Structure to hold Rate control related parameters
*          for each bit-rate instance and each thread
******************************************************************************
*/
typedef struct
{
    /**
    *frame level open loop intra sad
    *
    */
    LWORD64 i8_frame_open_loop_ssd;

    /**
    *frame level open loop intra sad
    *
    */
    UWORD32 u4_frame_open_loop_intra_sad;
    /**
    * frame level intra sad accumulator
    */
    UWORD32 u4_frame_intra_sad;

    /**
    *  frame level sad accumulator
    */
    UWORD32 u4_frame_sad_acc;

    /**
    *  frame level intra sad accumulator
    */
    UWORD32 u4_frame_inter_sad_acc;

    /**
    *  frame level inter sad accumulator
    */
    UWORD32 u4_frame_intra_sad_acc;

    /**
    *  frame level cost accumulator
    */
    LWORD64 i8_frame_cost_acc;

    /**
    *  frame level intra cost accumulator
    */
    LWORD64 i8_frame_inter_cost_acc;

    /**
    *  frame level inter cost accumulator
    */
    LWORD64 i8_frame_intra_cost_acc;

    /**
    * frame level rdopt bits accumulator
    */
    UWORD32 u4_frame_rdopt_bits;

    /**
    * frame level rdopt header bits accumulator
    */
    UWORD32 u4_frame_rdopt_header_bits;

    /* Sum the Qps of each 8*8 block in CU
    * 8*8 block is considered as Min CU size possible as per standard is 8
    * 0 corresponds to INTER and 1 corresponds to INTRA
    */
    WORD32 i4_qp_normalized_8x8_cu_sum[2];

    /* Count the number of 8x8 blocks in each CU type (INTER/INTRA)
    * 0 corresponds to INTER and 1 corresponds to INTRA
    */
    WORD32 i4_8x8_cu_sum[2];

    /* SAD/Qscale accumulated over all CUs. CU size is inherently
    * taken care in SAD
    */
    LWORD64 i8_sad_by_qscale[2];

} enc_loop_rc_params_t;
/**
******************************************************************************
*  @brief  CU information structure. This is to store the
*  CU final out after Recursion
******************************************************************************
*/
typedef struct ihevce_enc_cu_node_ctxt_t
{
    /* CU params */
    /** CU X position in terms of min CU (8x8) units */
    UWORD8 b3_cu_pos_x : 3;

    /** CU Y position in terms of min CU (8x8) units */
    UWORD8 b3_cu_pos_y : 3;

    /** reserved bytes */
    UWORD8 b2_reserved : 2;

    /** CU size 2N (width or height) in pixels */
    UWORD8 u1_cu_size;

    /**
    * array for storing cu level final params for a given mode
    * one best and one current is required
    */
    enc_loop_cu_final_prms_t s_cu_prms;

    /**
    * array for storing cu level final params for a given mode
    * one best and one current is required
    */
    enc_loop_cu_final_prms_t *ps_cu_prms;

    /* flag to indicate if current CU is the first
    CU of the Quantisation group*/
    UWORD32 b1_first_cu_in_qg : 1;

    /** qp used during for CU
    * @remarks :
    */
    WORD8 i1_cu_qp;

} ihevce_enc_cu_node_ctxt_t;

typedef struct
{
    WORD32 i4_sad;

    WORD32 i4_mv_cost;

    WORD32 i4_tot_cost;

    WORD8 i1_ref_idx;

    mv_t s_mv;

} block_merge_nodes_t;

/**
******************************************************************************
*  @brief  This struct is used for storing output of block merge
******************************************************************************
*/
typedef struct
{
    block_merge_nodes_t *aps_best_results[MAX_NUM_PARTS];

    /* Contains the best uni dir for each partition type */
    WORD32 ai4_best_uni_dir[MAX_NUM_PARTS];

    /* Contains the best pred dir for each partition type */
    WORD32 ai4_best_pred_dir[MAX_NUM_PARTS];

    WORD32 i4_tot_cost;

    PART_TYPE_T e_part_type;
} block_merge_results_t;

/**
******************************************************************************
*  @brief  This struct is used for storing output of block merge and also
*          all of the intermediate results required
******************************************************************************
*/
typedef struct
{
    block_merge_results_t as_best_results[3 + 1][NUM_BEST_ME_OUTPUTS];

    block_merge_nodes_t as_nodes[3][TOT_NUM_PARTS][NUM_BEST_ME_OUTPUTS];

    WORD32 part_mask;

    WORD32 num_results_per_part;

    WORD32 num_best_results;

    /**
    * Overall best CU cost, while other entries store CU costs
    * in single direction, this is best CU cost, where each
    * partition cost is evaluated as best of uni/bi
    */
    WORD32 best_cu_cost;

} block_merge_data_t;
/**
******************************************************************************
*  @brief  CU nbr information structure. This is to store the
*  neighbour information for final reconstruction function
******************************************************************************
*/
typedef struct
{
    /* Pointer to top-left nbr */
    nbr_4x4_t *ps_topleft_nbr_4x4;
    /* Pointer to left nbr */
    nbr_4x4_t *ps_left_nbr_4x4;
    /* Pointer to top nbr */
    nbr_4x4_t *ps_top_nbr_4x4;
    /* stride of left_nbr_4x4 */
    WORD32 nbr_4x4_left_strd;

    /* Pointer to CU top */
    UWORD8 *pu1_cu_top;

    UWORD16 *pu2_cu_top;

    /* Pointer to CU top-left */
    UWORD8 *pu1_cu_top_left;

    UWORD16 *pu2_cu_top_left;

    /* Pointer to CU left */
    UWORD8 *pu1_cu_left;

    UWORD16 *pu2_cu_left;

    /* stride of left pointer */
    WORD32 cu_left_stride;
} cu_nbr_prms_t;

/** Structure to save the flags required for Final mode Reconstruction
function. These flags are set based on quality presets and
the bit-rate we are working on */
typedef struct
{
    /** Flag to indicate whether Luma pred data need to recomputed in the
    final_recon function. Now disabled for all modes */
    UWORD8 u1_eval_luma_pred_data;

    /** Flag to indicate whether Chroma pred data need to recomputed in the
    final_recon function. Now disabled for MedSpeed only */
    UWORD8 u1_eval_chroma_pred_data;

    /** Flag to indicate whether header data need to recomputed in the
    final_recon function. Now disabled for all modes */
    UWORD8 u1_eval_header_data;

    UWORD8 u1_eval_recon_data;
} cu_final_recon_flags_t;

/**
******************************************************************************
*  @brief  structure to save pred data of ME cand. 1 ping-pong to store the
*  the best and current luma cand. 1 buffer to store the best chroma pred
******************************************************************************
*/
typedef struct
{
    /** Pointers to store luma pred data of me/intra cand.(2) and chroma(1) */
    UWORD8 *pu1_pred_data[NUM_CU_ME_INTRA_PRED_IDX];

    UWORD16 *pu2_pred_data[NUM_CU_ME_INTRA_PRED_IDX];

    /** Stride to store the predicted data of me/intra cand.(2) and chroma(1) */
    WORD32 ai4_pred_data_stride[NUM_CU_ME_INTRA_PRED_IDX];
    /** Counter saying how many pointers are assigned */
    WORD32 i4_pointer_count;

} cu_me_intra_pred_prms_t;

/**
******************************************************************************
*  @brief  Chroma RDOPT context structure
******************************************************************************
*/
typedef struct
{
    /** Storing the inverse quantized data (cb) for the special modes*/
    WORD16 ai2_iq_data_cb[(MAX_TU_SIZE * MAX_TU_SIZE) << 1];

    /** Storing the inverse quantized data (cr) for the special modes*/
    WORD16 ai2_iq_data_cr[(MAX_TU_SIZE * MAX_TU_SIZE) << 1];

    /** Storing the scan coeffs (cb) for the special modes*/
    UWORD8 au1_scan_coeff_cb[2][(MAX_TU_IN_CTB >> 1) * MAX_SCAN_COEFFS_BYTES_4x4];

    /** Storing the scan coeffs (cb) for the special modes*/
    UWORD8 au1_scan_coeff_cr[2][(MAX_TU_IN_CTB >> 1) * MAX_SCAN_COEFFS_BYTES_4x4];

    /** Max number of bytes filled in scan coeff data (cb) per TU*/
    WORD32 ai4_num_bytes_scan_coeff_cb_per_tu[2][MAX_TU_IN_TU_EQ_DIV_2];

    /** Max number of bytes filled in scan coeff data (cr) per TU*/
    WORD32 ai4_num_bytes_scan_coeff_cr_per_tu[2][MAX_TU_IN_TU_EQ_DIV_2];

    /** Stride of the iq buffer*/
    WORD32 i4_iq_buff_stride;

    /** Storing the pred data
    The predicted data is always interleaved. Therefore the size of this array will be
    ((MAX_TU_SIZE * MAX_TU_SIZE) >> 2) * 2)*/
    void *pv_pred_data;

    /** Predicted data stride*/
    WORD32 i4_pred_stride;

    /** Storing the cbfs for each tu
    For 1 tu case, only the 0th element will be valid*/
    UWORD8 au1_cbf_cb[2][MAX_TU_IN_TU_EQ_DIV_2];

    /** Storing the cbfs for each tu
    For 1 tu case, only the 0th element will be valid*/
    UWORD8 au1_cbf_cr[2][MAX_TU_IN_TU_EQ_DIV_2];

    /** To store the cabac ctxt model updated by the RDOPT of best chroma mode
    [0] : for 1 TU case, [1] : for 4 TU case */
    UWORD8 au1_chrm_satd_updated_ctxt_models[IHEVC_CAB_CTXT_END];

    /** Best SATD chroma mode, [0] : for 1 TU case (TU_EQ_CU) , [1] : for 4 TU case
    Values : 0(PLANAR), 1(VERT), 2(HOR), 3(DC) chroma mode per each TU */
    UWORD8 u1_best_cr_mode;

    /** Best SATD chroma mode's RDOPT cost, [0] : for 1 TU case, [1] : for 4 TU case */
    LWORD64 i8_chroma_best_rdopt;

    /* Account for coding b3_chroma_intra_pred_mode prefix and suffix bins */
    /* This is done by adding the bits for signalling chroma mode (0-3)    */
    /* and subtracting the bits for chroma mode same as luma mode (4)      */
    LWORD64 i8_cost_to_encode_chroma_mode;

    /** Best SATD chroma mode's tu bits, [0] : for 1 TU case, [1] : for 4 TU case */
    WORD32 i4_chrm_tu_bits;

    /** Storing the zero col values for each TU for cb*/
    WORD32 ai4_zero_col_cb[2][MAX_TU_IN_TU_EQ_DIV_2];

    /** Storing the zero col values for each TU for cr*/
    WORD32 ai4_zero_col_cr[2][MAX_TU_IN_TU_EQ_DIV_2];

    /** Storing the zero row values for each TU for cb*/
    WORD32 ai4_zero_row_cb[2][MAX_TU_IN_TU_EQ_DIV_2];

    /** Storing the zero row values for each TU for cr*/
    WORD32 ai4_zero_row_cr[2][MAX_TU_IN_TU_EQ_DIV_2];
} chroma_intra_satd_ctxt_t;

/**
******************************************************************************
*  @brief  Chroma RDOPT context structure
******************************************************************************
*/
typedef struct
{
    /** Chroma SATD context structure. It is an array of two to account for the TU_EQ_CU candidate
    and the TU_EQ_CU_DIV2 candidate*/
    chroma_intra_satd_ctxt_t as_chr_intra_satd_ctxt[NUM_POSSIBLE_TU_SIZES_CHR_INTRA_SATD];

    /** Chroma SATD has has to be evaluated only for the HIGH QUALITY */
    UWORD8 u1_eval_chrm_satd;

    /** Chroma RDOPT has to be evaluated only for the HIGH QUALITY / MEDIUM SPEED preset */
    UWORD8 u1_eval_chrm_rdopt;

} ihevce_chroma_rdopt_ctxt_t;

typedef struct
{
    inter_cu_results_t s_cu_results;

    inter_pu_results_t s_pu_results;
} block_merge_output_t;

/**
******************************************************************************
*  @brief  Structure to store the Merge/Skip Cand. for EncLoop
******************************************************************************
*/
typedef struct
{
    /** List of all  merge/skip candidates to be evalauted (SATD/RDOPT) for
    * this CU
    */
    cu_inter_cand_t as_cu_inter_merge_skip_cand[MAX_NUM_CU_MERGE_SKIP_CAND];

    /** number of merge/skip candidates
    */
    UWORD8 u1_num_merge_cands;

    UWORD8 u1_num_skip_cands;

    UWORD8 u1_num_merge_skip_cands;

} cu_inter_merge_skip_t;

/** Structure to store the Mixed mode Cand. for EncLoop */
typedef struct
{
    cu_inter_cand_t as_cu_data[MAX_NUM_MIXED_MODE_INTER_RDO_CANDS];

    UWORD8 u1_num_mixed_mode_type0_cands;

    UWORD8 u1_num_mixed_mode_type1_cands;

} cu_mixed_mode_inter_t;

typedef struct
{
    /* +2 because an additional buffer is required for */
    /* storing both cur and best during merge eval */
    void *apv_inter_pred_data[MAX_NUM_INTER_RDO_CANDS + 4];

    /* Bit field used to determine the indices of free bufs in 'apv_pred_data' buf array */
    UWORD32 u4_is_buf_in_use;

    /* Assumption is that the same stride is used for the */
    /* entire set of buffers above and is equal to the */
    /* CU size */
    WORD32 i4_pred_stride;

} ihevce_inter_pred_buf_data_t;
/** Structure to store the Inter Cand. info in EncLoop */
typedef struct
{
    cu_inter_cand_t *aps_cu_data[MAX_NUM_INTER_RDO_CANDS];

    UWORD32 au4_cost[MAX_NUM_INTER_RDO_CANDS];

    UWORD8 au1_pred_buf_idx[MAX_NUM_INTER_RDO_CANDS];

    UWORD32 u4_src_variance;

    UWORD8 u1_idx_of_worst_cost_in_cost_array;

    UWORD8 u1_idx_of_worst_cost_in_pred_buf_array;

    UWORD8 u1_num_inter_cands;

} inter_cu_mode_info_t;
typedef struct
{
    /*Frame level base pointer of buffers for each ctb row to store the top pixels
    *and top left pixel for the next ctb row.These buffers are common accross all threads
    */
    UWORD8 *apu1_sao_src_frm_top_luma[MAX_NUM_ENC_LOOP_PARALLEL];
    /*Ctb level pointer to buffer to store the top pixels
    *and top left pixel for the next ctb row.These buffers are common accross all threads
    */
    UWORD8 *pu1_curr_sao_src_top_luma;
    /*Buffer to store the left boundary before
    * doing sao on current ctb for the next ctb in the current row
    */
    UWORD8 au1_sao_src_left_luma[MAX_CTB_SIZE];
    /*Frame level base pointer of buffers for each ctb row to store the top pixels
    *and top left pixel for the next ctb row.These buffers are common accross all threads
    */
    UWORD8 *apu1_sao_src_frm_top_chroma[MAX_NUM_ENC_LOOP_PARALLEL];

    WORD32 i4_frm_top_chroma_buf_stride;

    /*Ctb level pointer to buffer to store the top chroma pixels
    *and top left pixel for the next ctb row.These buffers are common accross all threads
    */
    UWORD8 *pu1_curr_sao_src_top_chroma;

    /*Scratch buffer to store the left boundary before
    * doing sao on current ctb for the next ctb in the current row
    */
    UWORD8 au1_sao_src_left_chroma[MAX_CTB_SIZE * 2];

    /**
    * Luma recon buffer
    */
    UWORD8 *pu1_frm_luma_recon_buf;
    /**
    * Chroma recon buffer
    */
    UWORD8 *pu1_frm_chroma_recon_buf;
    /**
    * Luma recon buffer for curr ctb
    */
    UWORD8 *pu1_cur_luma_recon_buf;
    /**
    * Chroma recon buffer for curr ctb
    */
    UWORD8 *pu1_cur_chroma_recon_buf;
    /**
    * Luma src buffer
    */
    UWORD8 *pu1_frm_luma_src_buf;
    /**
    * Chroma src buffer
    */
    UWORD8 *pu1_frm_chroma_src_buf;
    /**
    * Luma src(input yuv) buffer for curr ctb
    */
    UWORD8 *pu1_cur_luma_src_buf;
    /**
    * Chroma src buffer for curr ctb
    */
    UWORD8 *pu1_cur_chroma_src_buf;
    /* Left luma scratch buffer required for sao RD optimisation*/
    UWORD8 au1_left_luma_scratch[MAX_CTB_SIZE];

    /* Left chroma scratch buffer required for sao RD optimisation*/
    /* Min size required= MAX_CTB_SIZE/2 * 2
    * Multiplied by 2 because size reuired is MAX_CTB_SIZE/2 each for U and V
    */
    UWORD8 au1_left_chroma_scratch[MAX_CTB_SIZE * 2];

    /* Top luma scratch buffer required for sao RD optimisation*/
    UWORD8 au1_top_luma_scratch[MAX_CTB_SIZE + 2];  // +1 for top left pixel and +1 for top right

    /* Top chroma scratch buffer required for sao RD optimisation*/
    UWORD8 au1_top_chroma_scratch[MAX_CTB_SIZE + 4];  // +2 for top left pixel and +2 for top right

    /* Scratch buffer to store the sao'ed output during sao RD optimisation*/
    /* One extra row(bot pixels) is copied to scratch buf but 2d buf copy func copies multiple of 4 ,hence
    MAX_CTB _SIZE + 4*/
    UWORD8 au1_sao_luma_scratch[PING_PONG][SCRATCH_BUF_STRIDE * (MAX_CTB_SIZE + 4)];

    /* Scratch buffer to store the sao'ed output during sao RD optimisation*/
    /* One extra row(bot pixels) is copied to scratch buf but 2d buf copy func copies multiple of 4 ,hence
    MAX_CTB _SIZE + 4*/
    UWORD8 au1_sao_chroma_scratch[PING_PONG][SCRATCH_BUF_STRIDE * (MAX_CTB_SIZE + 4)];

    /**
    * CTB size
    */
    WORD32 i4_ctb_size;
    /**
    * Luma recon buffer stride
    */
    WORD32 i4_frm_luma_recon_stride;
    /**
    * Chroma recon buffer stride
    */
    WORD32 i4_frm_chroma_recon_stride;
    /**
    * Luma recon buffer stride for curr ctb
    */
    WORD32 i4_cur_luma_recon_stride;
    /**
    * Chroma recon buffer stride for curr ctb
    */
    WORD32 i4_cur_chroma_recon_stride;
    /**
    * Luma src buffer stride
    */
    WORD32 i4_frm_luma_src_stride;
    /**
    * Chroma src buffer stride
    */
    WORD32 i4_frm_chroma_src_stride;

    WORD32 i4_frm_top_luma_buf_stride;
    /**
    * Luma src buffer stride for curr ctb
    */
    WORD32 i4_cur_luma_src_stride;
    /**
    * Chroma src buffer stride for curr ctb
    */
    WORD32 i4_cur_chroma_src_stride;

    /* Top luma buffer size */
    WORD32 i4_top_luma_buf_size;

    /* Top Chroma buffer size */
    WORD32 i4_top_chroma_buf_size;

    /*** Number of CTB units **/
    WORD32 i4_num_ctb_units;

    /**
    * CTB x pos
    */
    WORD32 i4_ctb_x;
    /**
    * CTB y pos
    */
    WORD32 i4_ctb_y;
    /* SAO block width*/
    WORD32 i4_sao_blk_wd;

    /* SAO block height*/
    WORD32 i4_sao_blk_ht;

    /* Last ctb row flag*/
    WORD32 i4_is_last_ctb_row;

    /* Last ctb col flag*/
    WORD32 i4_is_last_ctb_col;

    /* CTB aligned width */
    UWORD32 u4_ctb_aligned_wd;

    /* Number of ctbs in a row*/
    UWORD32 u4_num_ctbs_horz;

    UWORD32 u4_num_ctbs_vert;
    /**
    * Closed loop SSD Lambda
    * This is multiplied with bits for RD cost computations in SSD mode
    * This is represented in q format with shift of LAMBDA_Q_SHIFT
    */
    LWORD64 i8_cl_ssd_lambda_qf;

    /**
    * Closed loop SSD Lambda for chroma (chroma qp is different from luma qp)
    * This is multiplied with bits for RD cost computations in SSD mode
    * This is represented in q format with shift of LAMBDA_Q_SHIFT
    */
    LWORD64 i8_cl_ssd_lambda_chroma_qf;
    /**
    * Pointer to current PPS
    */
    pps_t *ps_pps;  //not used currently
    /**
    * Pointer to current SPS
    */
    sps_t *ps_sps;

    /**
    * Pointer to current slice header structure
    */
    slice_header_t *ps_slice_hdr;
    /**
    * Pointer to current frame ctb out array of structures
    */
    ctb_enc_loop_out_t *ps_ctb_out;
    /**
    *  context for cabac bit estimation used during rdopt stage
    */
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt;
    /**
    * Pointer to sao_enc_t for the current ctb
    */
    sao_enc_t *ps_sao;
    /*
    * Pointer to an array to store the sao information of the top ctb
    * This is required for to decide top merge
    */
    sao_enc_t *aps_frm_top_ctb_sao[MAX_NUM_ENC_LOOP_PARALLEL];

    /*
    * Pointer to structure to store the sao parameters of (x,y)th ctb
    * for top merge of (x,y+1)th ctb
    */
    sao_enc_t *ps_top_ctb_sao;

    /* structure to store the sao parameters of (x,y)th ctb for
    * the left merge of (x+1,y)th ctb
    */
    sao_enc_t s_left_ctb_sao;

    /* Array of structures for SAO RDO candidates*/
    sao_enc_t as_sao_rd_cand[MAX_SAO_RD_CAND];

    /** array of function pointers for luma sao */
    pf_sao_luma apf_sao_luma[4];

    /** array of function pointers for chroma sao */
    pf_sao_chroma apf_sao_chroma[4];

    /* Flag to do SAO luma and chroma filtering*/
    WORD8 i1_slice_sao_luma_flag;

    WORD8 i1_slice_sao_chroma_flag;

#if DISABLE_SAO_WHEN_NOISY
    ctb_analyse_t *ps_ctb_data;

    WORD32 i4_ctb_data_stride;
#endif

    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list;

} sao_ctxt_t;

/**
******************************************************************************
*  @brief  Encode loop module context structure
******************************************************************************
*/
typedef struct
{
#if ENABLE_TU_TREE_DETERMINATION_IN_RDOPT
    void *pv_err_func_selector;
#endif

    /**
    * Quality preset for comtrolling numbe of RD opt cand
    * @sa : IHEVCE_QUALITY_CONFIG_T
    */
    WORD32 i4_quality_preset;
    /**
    *
    *
    */
    WORD32 i4_rc_pass;
    /**
    * Lamda to be mulitplied with bits for SATD
    * should be equal to Lamda*Qp
    */
    WORD32 i4_satd_lamda;

    /**
    * Lamda to be mulitplied with bits for SAD
    * should be equal to Lamda*Qp
    */
    WORD32 i4_sad_lamda;

    /**
    * Closed loop SSD Lambda
    * This is multiplied with bits for RD cost computations in SSD mode
    * This is represented in q format with shift of LAMBDA_Q_SHIFT
    */
    LWORD64 i8_cl_ssd_lambda_qf;

    /**
    * Closed loop SSD Lambda for chroma (chroma qp is different from luma qp)
    * This is multiplied with bits for RD cost computations in SSD mode
    * This is represented in q format with shift of LAMBDA_Q_SHIFT
    */
    LWORD64 i8_cl_ssd_lambda_chroma_qf;

    /**
    * Ratio of Closed loop SSD Lambda and Closed loop SSD Lambda for chroma
    * This is multiplied with (1 << CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT)
    * to keep the precision of the ratio
    */
    UWORD32 u4_chroma_cost_weighing_factor;
    /**
    * Frame level QP to be used
    */
    WORD32 i4_frame_qp;

    WORD32 i4_frame_mod_qp;

    WORD32 i4_frame_qstep;

    UWORD8 u1_max_tr_depth;

    /**
    * CU level Qp
    */
    WORD32 i4_cu_qp;

    /**
    * CU level Qp / 6
    */
    WORD32 i4_cu_qp_div6;

    /**
    * CU level Qp % 6
    */
    WORD32 i4_cu_qp_mod6;

    /**
    *  CU level QP to be used
    */
    WORD32 i4_chrm_cu_qp;

    /**
    * CU level Qp / 6
    */
    WORD32 i4_chrm_cu_qp_div6;

    /**
    * CU level Qp % 6
    */
    WORD32 i4_chrm_cu_qp_mod6;

    /** previous cu qp
    * @remarks : This needs to be remembered to handle skip cases in deblocking.
    */
    WORD32 i4_prev_cu_qp;

    /** chroma qp offset
    * @remarks : Used to calculate chroma qp and other qp related parameter at CU level
    */
    WORD32 i4_chroma_qp_offset;

    /**
    * Buffer Pointer to populate the scale matrix for all transform size
    */
    WORD16 *pi2_scal_mat;

    /**
    * Buffer Pointer to populate the rescale matrix for all transform size
    */
    WORD16 *pi2_rescal_mat;

    /** array of pointer to store the scaling matrices for
    *  all transform sizes and qp % 6 (pre computed)
    */
    WORD16 *api2_scal_mat[NUM_TRANS_TYPES * 2];

    /** array of pointer to store the re-scaling matrices for
    *  all transform sizes and qp % 6 (pre computed)
    */
    WORD16 *api2_rescal_mat[NUM_TRANS_TYPES * 2];

    /** array of function pointers for residual and
    *  forward transform for all transform sizes
    */
    pf_res_trans_luma apf_resd_trns[NUM_TRANS_TYPES];

    /** array of function pointers for residual and
    *  forward HAD transform for all transform sizes
    */
    pf_res_trans_luma_had_chroma apf_chrm_resd_trns_had[NUM_TRANS_TYPES - 2];

    /** array of function pointers for residual and
    *  forward transform for all transform sizes
    *  for chroma
    */
    pf_res_trans_chroma apf_chrm_resd_trns[NUM_TRANS_TYPES - 2];

    /** array of function pointers for qunatization and
    *  inv Quant for ssd calc. for all transform sizes
    */
    pf_quant_iquant_ssd apf_quant_iquant_ssd[4];

    /** array of function pointers for inv.transform and
    *  recon for all transform sizes
    */
    pf_it_recon apf_it_recon[NUM_TRANS_TYPES];

    /** array of function pointers for inverse transform
    * and recon for all transform sizes for chroma
    */
    pf_it_recon_chroma apf_chrm_it_recon[NUM_TRANS_TYPES - 2];

    /** array of luma intra prediction function pointers */
    pf_intra_pred apf_lum_ip[NUM_IP_FUNCS];

    /** array of chroma intra prediction function pointers */
    pf_intra_pred apf_chrm_ip[NUM_IP_FUNCS];

    /* - Function pointer to cu_mode_decide function */
    /* - The 'void *' is used since one of the parameters of */
    /* this class of functions is the current structure */
    /* - This function pointer is used to choose the */
    /* appropriate function depending on whether bit_depth is */
    /* chosen as 8 bits or greater */
    /* - This function pointer's type is defined at the end */
    /* of this file */
    void *pv_cu_mode_decide;

    /* Infer from the comment for the variable 'pv_cu_mode_decide' */
    void *pv_inter_rdopt_cu_mc_mvp;

    /* Infer from the comment for the variable 'pv_cu_mode_decide' */
    void *pv_inter_rdopt_cu_ntu;

    /* Infer from the comment for the variable 'pv_cu_mode_decide' */
    void *pv_intra_chroma_pred_mode_selector;

    /* Infer from the comment for the variable 'pv_cu_mode_decide' */
    void *pv_intra_rdopt_cu_ntu;

    /* Infer from the comment for the variable 'pv_cu_mode_decide' */
    void *pv_final_rdopt_mode_prcs;

    /* Infer from the comment for the variable 'pv_cu_mode_decide' */
    void *pv_store_cu_results;

    /* Infer from the comment for the variable 'pv_cu_mode_decide' */
    void *pv_enc_loop_cu_bot_copy;

    /* Infer from the comment for the variable 'pv_cu_mode_decide' */
    void *pv_final_mode_reevaluation_with_modified_cu_qp;

    /* Infer from the comment for the variable 'pv_cu_mode_decide' */
    void *pv_enc_loop_ctb_left_copy;

    /** Qunatization rounding factor for inter and intra CUs */
    WORD32 i4_quant_rnd_factor[2];

    /**
    * Frame Buffer Pointer to store the top row luma data.
    * one pixel row in every ctb row
    */
    void *apv_frm_top_row_luma[MAX_NUM_ENC_LOOP_PARALLEL];

    /**
    * One CTB row size of Top row luma data buffer
    */
    WORD32 i4_top_row_luma_stride;

    /**
    * One frm of Top row luma data buffer
    */
    WORD32 i4_frm_top_row_luma_size;

    /**
    * Current luma row bottom data store pointer
    */
    void *pv_bot_row_luma;

    /**
    * Top luma row top data access pointer
    */
    void *pv_top_row_luma;

    /**
    * Frame Buffer Pointer to store the top row chroma data (Cb  Cr pixel interleaved )
    * one pixel row in every ctb row
    */
    void *apv_frm_top_row_chroma[MAX_NUM_ENC_LOOP_PARALLEL];

    /**
    * One CTB row size of Top row chroma data buffer (Cb  Cr pixel interleaved )
    */
    WORD32 i4_top_row_chroma_stride;

    /**
    * One frm size of Top row chroma data buffer (Cb  Cr pixel interleaved )
    */
    WORD32 i4_frm_top_row_chroma_size;

    /**
    * Current chroma row bottom data store pointer
    */
    void *pv_bot_row_chroma;

    /**
    * Top chroma row top data access pointer
    */
    void *pv_top_row_chroma;

    /**
    * Frame Buffer Pointer to store the top row neighbour modes stored at 4x4 level
    * one 4x4 row in every ctb row
    */
    nbr_4x4_t *aps_frm_top_row_nbr[MAX_NUM_ENC_LOOP_PARALLEL];

    /**
    * One CTB row size of Top row nbr 4x4 params buffer
    */
    WORD32 i4_top_row_nbr_stride;

    /**
    * One frm size of Top row nbr 4x4 params buffer
    */
    WORD32 i4_frm_top_row_nbr_size;

    /**
    * Current row nbr prms bottom data store pointer
    */
    nbr_4x4_t *ps_bot_row_nbr;

    /**
    * Top row nbr prms top data access pointer
    */
    nbr_4x4_t *ps_top_row_nbr;

    /**
    * Pointer to (1,1) location in au1_nbr_ctb_map
    */
    UWORD8 *pu1_ctb_nbr_map;

    /**
    * neigbour map buffer stride;
    */
    WORD32 i4_nbr_map_strd;

    /**
    * Array at ctb level to store the neighour map
    * its size is 25x25 for ctb size of 64x64
    */
    UWORD8 au1_nbr_ctb_map[MAX_PU_IN_CTB_ROW + 1 + 8][MAX_PU_IN_CTB_ROW + 1 + 8];

    /**
    * Array to store left ctb data for luma
    * some padding is added to take care of unconditional access
    */
    void *pv_left_luma_data;

    /**
    * Array to store left ctb data for chroma (cb abd cr pixel interleaved
    * some padding is added to take care of unconditional access
    */
    void *pv_left_chrm_data;

    /**
    * Array to store the left neighbour modes at 4x4 level
    */
    nbr_4x4_t as_left_col_nbr[MAX_PU_IN_CTB_ROW];

    /**
    * Array to store currrent CTb pred modes at a 4x4 level
    * used for prediction inside ctb
    */
    nbr_4x4_t as_ctb_nbr_arr[MAX_PU_IN_CTB_ROW * MAX_PU_IN_CTB_ROW];

    /**
    * array for storing csbf during RD opt stage at CU level
    * one best and one current is required
    */
    UWORD8 au1_cu_csbf[MAX_TU_IN_CTB_ROW * MAX_TU_IN_CTB_ROW];

    /**
    * Stride of csbf buffer. will be useful for scanning access
    * if stored in a 2D order. right now set to max tx size >> 4;
    */
    WORD32 i4_cu_csbf_strd;

    /**
    * Array to store pred modes  during SATD and RD opt stage at CU level
    * one best and one current is required
    */
    nbr_4x4_t as_cu_nbr[2][MAX_PU_IN_CTB_ROW * MAX_PU_IN_CTB_ROW];

    /**
    * array to store the output of reference substitution process output
    * for intra CUs
    * TOP (32 x 2) + Left (32 x 2) + Top left (1) + Alignment (3)
    */
    void *pv_ref_sub_out;

    /**
    * array to store the filtered reference samples for intra CUs
    * TOP (32 x 2) + Left (32 x 2) + Top left (1) + Alignment (3)
    */
    void *pv_ref_filt_out;

    /**
    * Used for 3 purposes
    *
    * 1. MC Intermediate buffer
    * array for storing intermediate 16-bit value for hxhy subpel
    * generation at CTB level (+ 16) for subpel planes boundary
    * +4 is for horizontal 4pels
    *
    * 2. Temprory scratch buffer for transform and coeffs storage
    * MAX_TRANS_SIZE *2 for trans_scratch(32bit) and MAX_TRANS_SIZE *1 for trans_values
    * The first part i.e. from 0 to MAX_TRANS_SIZE is then reused for storing the quant coeffs
    * Max of both are used
    *
    * 3. MC Intermediate buffer
    * buffer for storing intermediate 16 bit values prior to conversion to 8bit in HBD
    *
    */
    MEM_ALIGN16 WORD16 ai2_scratch[(MAX_CTB_SIZE + 8 + 8) * (MAX_CTB_SIZE + 8 + 8 + 8) * 2];

    /**
    * array for storing cu level final params for a given mode
    * one best and one current is required
    */
    enc_loop_cu_final_prms_t as_cu_prms[2];

    /**
    * Scan index to be used for any gien transform
    * this is a scartch variable used to communicate
    * scan idx at every transform level
    */
    WORD32 i4_scan_idx;

    /**
    * Buffer index in ping pong buffers
    * to be used SATD mode evaluations
    */
    WORD32 i4_satd_buf_idx;

    /**
    * Motion Compensation module context structre
    */
    inter_pred_ctxt_t s_mc_ctxt;

    /**
    * MV pred module context structre
    */
    mv_pred_ctxt_t s_mv_pred_ctxt;

    /**
    * Deblock BS ctb structure
    */
    deblk_bs_ctb_ctxt_t s_deblk_bs_prms;

    /**
    * Deblocking ctb structure
    */
    deblk_ctb_params_t s_deblk_prms;

    /**
    * Deblocking structure. For ctb-row level
    */
    deblk_ctbrow_prms_t s_deblk_ctbrow_prms;

    /**
    * Deblocking enable flag
    */
    WORD32 i4_deblock_type;

    /**
    *  context for cabac bit estimation used during rdopt stage
    */
    rdopt_entropy_ctxt_t s_rdopt_entropy_ctxt;

    /**
    * Context models stored for RDopt store and restore purpose
    */
    UWORD8 au1_rdopt_init_ctxt_models[IHEVC_CAB_CTXT_END];

    /**
    * current picture slice type
    */
    WORD8 i1_slice_type;

    /**
    * strong_intra_smoothing_enable_flag
    */
    WORD8 i1_strong_intra_smoothing_enable_flag;

    /** Pointer to Dep Mngr for controlling Top-Right CU dependency */
    void *pv_dep_mngr_enc_loop_cu_top_right;

    /** Pointer to Dep Mngr for controlling Deblocking Top dependency */
    void *pv_dep_mngr_enc_loop_dblk;

    /** Pointer to Dep Mngr for controlling Deblocking Top dependency */
    void *pv_dep_mngr_enc_loop_sao;

    /** pointer to store the cabac states at end of second CTB in current row */
    UWORD8 *pu1_curr_row_cabac_state;

    /** pointer to copy the cabac states at start of first CTB in current row */
    UWORD8 *pu1_top_rt_cabac_state;
    /** flag to indicate rate control mode.
    * @remarks :  To enable CU level qp modulation only when required.
    */
    WORD8 i1_cu_qp_delta_enable;

    /** flag to indicate rate control mode.
    * @remarks :  Entropy sync enable flag
    */
    WORD8 i1_entropy_coding_sync_enabled_flag;

    /** Use SATD or SAD for best merge candidate evaluation */
    WORD32 i4_use_satd_for_merge_eval;

    UWORD8 u1_use_early_cbf_data;

    /** Use SATD or SAD for best CU merge candidate evaluation */
    WORD32 i4_use_satd_for_cu_merge;

    /** Maximum number of merge candidates to be evaluated */
    WORD32 i4_max_merge_candidates;

    /** Flag to indicate whether current pictute needs to be deblocked,
    padded and hpel planes need to be generated.
    These are turned off typically in non referecne pictures when psnr
    and recon dump is disabled
    */
    WORD32 i4_deblk_pad_hpel_cur_pic;

    /* Array of structures for storing mc predicted data for
    * merge and skip modes
    */
    merge_skip_pred_data_t as_merge_skip_pred_data[MAX_NUM_CU_MERGE_SKIP_CAND];

    /* Sum the Qps of each 8*8 block in CU
    * 8*8 block is considered as Min CU size possible as per standard is 8
    * 0 corresponds to INTER and 1 corresponds to INTRA
    */
    LWORD64 i8_cl_ssd_lambda_qf_array[MAX_HEVC_QP_12bit + 1];
    UWORD32 au4_chroma_cost_weighing_factor_array[MAX_HEVC_QP_12bit + 1];
    LWORD64 i8_cl_ssd_lambda_chroma_qf_array[MAX_HEVC_QP_12bit + 1];
    WORD32 i4_satd_lamda_array[MAX_HEVC_QP_12bit + 1];
    WORD32 i4_sad_lamda_array[MAX_HEVC_QP_12bit + 1];

    /************************************************************************/
    /* The fields with the string 'type2' in their names are required */
    /* when both 8bit and hbd lambdas are needed. The lambdas corresponding */
    /* to the bit_depth != internal_bit_depth are stored in these fields */
    /************************************************************************/
    LWORD64 i8_cl_ssd_type2_lambda_qf_array[MAX_HEVC_QP_12bit + 1];
    LWORD64 i8_cl_ssd_type2_lambda_chroma_qf_array[MAX_HEVC_QP_12bit + 1];
    WORD32 i4_satd_type2_lamda_array[MAX_HEVC_QP_12bit + 1];
    WORD32 i4_sad_type2_lamda_array[MAX_HEVC_QP_12bit + 1];

    /* Lokesh: Added to find if the CU is the first to be coded in the group */
    WORD32 i4_is_first_cu_qg_coded;

    /* Chroma RDOPT related parameters */
    ihevce_chroma_rdopt_ctxt_t s_chroma_rdopt_ctxt;

    /* Structure to save pred data of ME/Intra cand */
    cu_me_intra_pred_prms_t s_cu_me_intra_pred_prms;

    /* Structure to save the flags required for Final mode Reconstruction
    function. These flags are set based on quality presets and bit-rate
    we are working on */
    cu_final_recon_flags_t s_cu_final_recon_flags;

    /* Parameter to how at which level RDOQ will be implemented:
    0 - RDOQ disbaled
    1 - RDOQ enabled during RDOPT for all candidates
    2 - RDOQ enabled only for the final candidate*/
    WORD32 i4_rdoq_level;

    /* Parameter to how at which level Quant rounding factors are computed:
    FIXED_QUANT_ROUNDING       : Fixed Quant rounding values are used
    NCTB_LEVEL_QUANT_ROUNDING  : NCTB level Cmputed Quant rounding values are used
    CTB_LEVEL_QUANT_ROUNDING   : CTB level Cmputed Quant rounding values are used
    CU_LEVEL_QUANT_ROUNDING    : CU level Cmputed Quant rounding values are used
    TU_LEVEL_QUANT_ROUNDING    : TU level Cmputed Quant rounding values are used*/
    WORD32 i4_quant_rounding_level;

    /* Parameter to how at which level Quant rounding factors are computed:
    CHROMA_QUANT_ROUNDING    : Chroma Quant rounding values are used for chroma */
    WORD32 i4_chroma_quant_rounding_level;

    /* Parameter to how at which level RDOQ will be implemented:
    0 - SBH disbaled
    1 - SBH enabled during RDOPT for all candidates
    2 - SBH enabled only for the final candidate*/
    WORD32 i4_sbh_level;

    /* Parameter to how at which level ZERO CBF RDO will be implemented:
    0 - ZCBF disbaled
    1 - ZCBF enabled during RDOPT for all candidates
    2 - ZCBF enabled only for the final candidate
    */
    WORD32 i4_zcbf_rdo_level;

    /*RDOQ-SBH context structure*/
    rdoq_sbh_ctxt_t s_rdoq_sbh_ctxt;

    /** Structure to store the Merge/Skip Cand. for EncLoop */
    cu_inter_merge_skip_t s_cu_inter_merge_skip;
    /** Structure to store the Mixed mode Cand. for EncLoop */
    cu_mixed_mode_inter_t s_mixed_mode_inter_cu;

    ihevce_inter_pred_buf_data_t s_pred_buf_data;

    void *pv_422_chroma_intra_pred_buf;

    WORD32 i4_max_num_inter_rdopt_cands;

    /* Output Struct per each CU during recursions */
    ihevce_enc_cu_node_ctxt_t as_enc_cu_ctxt[MAX_CU_IN_CTB + 1];

    /* Used to store best inter candidate. Used only when */
    /* 'CU modulated QP override' is enabled */
    cu_inter_cand_t as_best_cand[MAX_CU_IN_CTB + 1];

    cu_inter_cand_t *ps_best_cand;

    UWORD8 au1_cu_init_cabac_state_a_priori[MAX_CU_IN_CTB + 1][IHEVC_CAB_CTXT_END];

    UWORD8 (*pau1_curr_cu_a_priori_cabac_state)[IHEVC_CAB_CTXT_END];

    /* Used to store pred data of each CU in the CTB. */
    /* Used only when 'CU modulated QP override' is enabled */
    void *pv_CTB_pred_luma;

    void *pv_CTB_pred_chroma;

    /**
    * array for storing recon during SATD and RD opt stage at CU level
    * one best and one current is required.Luma and chroma together
    */
    void *pv_cu_luma_recon;

    /**
    * array for storing recon during SATD and RD opt stage at CU level
    * one best and one current is required.Luma and chroma together
    */
    void *pv_cu_chrma_recon;

    /**
    * Array to store pred modes  during SATD and RD opt stage at CU level
    * one best and one current is required
    */
    nbr_4x4_t as_cu_recur_nbr[MAX_PU_IN_CTB_ROW * MAX_PU_IN_CTB_ROW];

    /**
    * Pointer to Array to store pred modes  during SATD and RD opt stage at CU level
    * one best and one current is required
    */
    nbr_4x4_t *ps_cu_recur_nbr;

    /**
    * Context models stored for CU recursion parent evaluation
    */
    UWORD8 au1_rdopt_recur_ctxt_models[4][IHEVC_CAB_CTXT_END];

    ihevce_enc_cu_node_ctxt_t *ps_enc_out_ctxt;

    /**
    * array for storing coeffs during RD opt stage at CU level
    * one best and one current is required. Luma and chroma together
    */
    /*UWORD8 au1_cu_recur_coeffs[MAX_LUMA_COEFFS_CTB + MAX_CHRM_COEFFS_CTB];*/

    UWORD8 *pu1_cu_recur_coeffs;

    UWORD8 *apu1_cu_level_pingpong_coeff_buf_addr[2];

    WORD16 *api2_cu_level_pingpong_deq_buf_addr[2];

    UWORD8 *pu1_ecd_data;

    /* OPT: flag to skip parent CU=4TU eval during recursion */
    UWORD8 is_parent_cu_rdopt;

    /**
    *   Array of structs containing block merge data for
    *   4 32x32 CU's in indices 1 - 4 and 64x64 CU at 0
    */
    UWORD8 u1_cabac_states_next_row_copied_flag;

    UWORD8 u1_cabac_states_first_cu_copied_flag;

    UWORD32 u4_cur_ctb_wd;

    UWORD32 u4_cur_ctb_ht;

    /* thread id of the current context */
    WORD32 thrd_id;

    /** Number of processing threads created run time */
    WORD32 i4_num_proc_thrds;

    /* Instance number of bit-rate for multiple bit-rate encode */
    WORD32 i4_bitrate_instance_num;

    WORD32 i4_num_bitrates;

    WORD32 i4_enc_frm_id;

    /* Flag to indicate if chroma needs to be considered for cost calculation */
    WORD32 i4_consider_chroma_cost;

    /* Number of modes to be evaluated for intra */
    WORD32 i4_num_modes_to_evaluate_intra;

    /* Number of modes to be evaluated for inter */
    WORD32 i4_num_modes_to_evaluate_inter;
    /*pointers for struct to hold RC parameters for each bit-rate instance */
    enc_loop_rc_params_t
        *aaps_enc_loop_rc_params[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /** Pointer to structure containing function pointers of common*/
    func_selector_t *ps_func_selector;

    /* Flag to control Top Right Sync for during Merge */
    UWORD8 u1_use_top_at_ctb_boundary;

    UWORD8 u1_is_input_data_hbd;

    UWORD8 u1_bit_depth;

    /* 0 - 400; 1 - 420; 2 - 422; 3 - 444 */
    UWORD8 u1_chroma_array_type;

    rc_quant_t *ps_rc_quant_ctxt;

    sao_ctxt_t s_sao_ctxt_t;

    /* Offset to get the Qp for the last CU of upper CTB-row.
    This offset is from the current tile top row QP map start.
    This will only be consumed by the first CU of current CTB-row
    iff [it is skip && entropy sync is off] */
    WORD32 *pi4_offset_for_last_cu_qp;

    double i4_lamda_modifier;
    double i4_uv_lamda_modifier;
    WORD32 i4_temporal_layer_id;

    UWORD8 u1_disable_intra_eval;

    WORD32 i4_quant_round_tu[2][32 * 32];

    WORD32 *pi4_quant_round_factor_tu_0_1[5];
    WORD32 *pi4_quant_round_factor_tu_1_2[5];

    WORD32 i4_quant_round_4x4[2][4 * 4];
    WORD32 i4_quant_round_8x8[2][8 * 8];
    WORD32 i4_quant_round_16x16[2][16 * 16];
    WORD32 i4_quant_round_32x32[2][32 * 32];

    WORD32 *pi4_quant_round_factor_cu_ctb_0_1[5];
    WORD32 *pi4_quant_round_factor_cu_ctb_1_2[5];

    WORD32 i4_quant_round_cr_4x4[2][4 * 4];
    WORD32 i4_quant_round_cr_8x8[2][8 * 8];
    WORD32 i4_quant_round_cr_16x16[2][16 * 16];

    WORD32 *pi4_quant_round_factor_cr_cu_ctb_0_1[3];
    WORD32 *pi4_quant_round_factor_cr_cu_ctb_1_2[3];
    /* cost for not coding cu residue i.e forcing no residue syntax as 1 */
    LWORD64 i8_cu_not_coded_cost;

    /* dependency manager for forward ME  sync */
    void *pv_dep_mngr_encloop_dep_me;

    LWORD64 ai4_source_satd_8x8[64];

    LWORD64 ai4_source_chroma_satd[256];

    UWORD8 u1_is_refPic;

    WORD32 i4_qp_mod;

    WORD32 i4_is_ref_pic;

    WORD32 i4_chroma_format;

    WORD32 i4_temporal_layer;

    WORD32 i4_use_const_lamda_modifier;

    double f_i_pic_lamda_modifier;

    LWORD64 i8_distortion;

    WORD32 i4_use_ctb_level_lamda;

    float f_str_ratio;

    /* Flag to indicate if current frame is to be shared with other clients.
    Used only in distributed-encoding */
    WORD32 i4_share_flag;

    /* Pointer to the current recon being processed.
    Needed for enabling TMVP in dist-encoding */
    void *pv_frm_recon;

    ihevce_cmn_opt_func_t s_cmn_opt_func;

    /* The ME analogue to the struct above was not included since */
    /* that would have entailed inclusion of all ME specific */
    /* header files */
    /*FT_SAD_EVALUATOR **/

    /*FT_SAD_EVALUATOR **/
    void *pv_evalsad_pt_npu_mxn_8bit;
    UWORD8 u1_enable_psyRDOPT;

    UWORD8 u1_is_stasino_enabled;

    UWORD32 u4_psy_strength;
    /*Sub PIC rc context */

    WORD32 i4_sub_pic_level_rc;
    WORD32 i4_num_ctb_for_out_scale;

    /**
     * Accumalated bits of all cu for required CTBS estimated during RDO evaluation.
     * Required for sup pic level RC. Reset when required CU/CTB count is reached.
     */
    UWORD32 u4_total_cu_bits;

    UWORD32 u4_total_cu_bits_mul_qs;

    UWORD32 u4_total_cu_hdr_bits;

    UWORD32 u4_cu_tot_bits_into_qscale;

    UWORD32 u4_cu_tot_bits;

    /*Scale added to the current qscale, output from sub pic rc*/
    WORD32 i4_cu_qp_sub_pic_rc;

    /*Frame level L1 IPE sad*/
    LWORD64 i8_frame_l1_ipe_sad;

    /*Frame level L0 IPE satd*/
    LWORD64 i8_frame_l0_ipe_satd;

    /*Frame level L1 ME sad*/
    LWORD64 i8_frame_l1_me_sad;

    /*Frame level L1 activity factor*/
    LWORD64 i8_frame_l1_activity_fact;
    /*bits esimated for frame calulated for sub pic rc bit control */
    WORD32 ai4_frame_bits_estimated[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];
    /** I Scene cut */
    WORD32 i4_is_I_scenecut;

    /** Non Scene cut */
    WORD32 i4_is_non_I_scenecut;

    /** Frames for which online/offline model is not valid */
    WORD32 i4_is_model_valid;

    /** Steady State Frame */
    //WORD32 i4_is_steady_state;

    WORD32 i4_is_first_query;

    /* Pointer to Tile params base */
    void *pv_tile_params_base;

    /** The index of column tile for which it is working */
    WORD32 i4_tile_col_idx;

    WORD32 i4_max_search_range_horizontal;

    WORD32 i4_max_search_range_vertical;

    WORD32 i4_is_ctb_qp_modified;

    WORD32 i4_display_num;

    WORD32 i4_pred_qp;

    /*assumption of qg size is 8x8 block size*/
    WORD32 ai4_qp_qg[8 * 8];

    WORD32 i4_last_cu_qp_from_prev_ctb;

    WORD32 i4_prev_QP;

    UWORD8 u1_max_inter_tr_depth;

    UWORD8 u1_max_intra_tr_depth;

} ihevce_enc_loop_ctxt_t;

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/** @brief RDOQ_LEVELS_T: This enumeration specifies the RDOQ mode of operation
*
*  NO_RDOQ    : RDOQ is not performed
*  BEST_CAND_RDOQ : RDOQ for final candidate only
*  ALL_CAND_RDOQ : RDOQ for all candidates
*/
typedef enum
{
    NO_RDOQ,
    BEST_CAND_RDOQ,
    ALL_CAND_RDOQ,
} RDOQ_LEVELS_T;

/** @brief QUANT_ROUNDING_COEFF_LEVELS_T: This enumeration specifies the Coef level RDOQ mode of operation
*
*  FIXED_QUANT_ROUNDING       : Fixed Quant rounding values are used
*  NCTB_LEVEL_QUANT_ROUNDING  : NCTB level Cmputed Quant rounding values are used
*  CTB_LEVEL_QUANT_ROUNDING   : CTB level Cmputed Quant rounding values are used
*  CU_LEVEL_QUANT_ROUNDING    : CU level Cmputed Quant rounding values are used
*  TU_LEVEL_QUANT_ROUNDING    : TU level Cmputed Quant rounding values are used
*               Defaulat for all candidtes, based on RDOQ_LEVELS_T choose to best candidate
*/
typedef enum
{
    FIXED_QUANT_ROUNDING,
    NCTB_LEVEL_QUANT_ROUNDING,
    CTB_LEVEL_QUANT_ROUNDING,
    CU_LEVEL_QUANT_ROUNDING,
    TU_LEVEL_QUANT_ROUNDING,
    CHROMA_QUANT_ROUNDING
} QUANT_ROUNDING_COEFF_LEVELS_T;

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/** @brief SBH_LEVELS_T: This enumeration specifies the RDOQ mode of operation
*
*  NO_SBH    : SBH is not performed
*  BEST_CAND_SBH : SBH for final candidate only
*  ALL_CAND_SBH : SBH for all candidates
*/
typedef enum
{
    NO_SBH,
    BEST_CAND_SBH,
    ALL_CAND_SBH,
} SBH_LEVELS_T;

/** @brief ZCBF_LEVELS_T: This enumeration specifies the ZeroCBF RDO mode of operation
*
*  NO_ZCBF    : ZCBF RDO is not performed
*  ALL_CAND_ZCBF : ZCBF RDO for all candidates
*/
typedef enum
{
    NO_ZCBF,
    ZCBF_ENABLE,
} ZCBF_LEVELS_T;

/**
******************************************************************************
*  @brief  Encode loop master context structure
******************************************************************************
*/
typedef struct
{
    /** Array of encode loop structure */
    ihevce_enc_loop_ctxt_t *aps_enc_loop_thrd_ctxt[MAX_NUM_FRM_PROC_THRDS_ENC];

    /** Number of processing threads created run time */
    WORD32 i4_num_proc_thrds;

    /**
    *  Array of top row cu skip flags (1 bit per 8x8CU)
    */
    UWORD8 au1_cu_skip_top_row[HEVCE_MAX_WIDTH >> 6];

    /** Context models stored at the end of second CTB in a row)
    *  stored in packed form pState[bits6-1] | MPS[bit0]
    *  for each CTB row
    *  using entropy sync model in RD opt
    */
    UWORD8 au1_ctxt_models[MAX_NUM_CTB_ROWS_FRM][IHEVC_CAB_CTXT_END];

    /** Dependency manager for controlling EncLoop Top-Right CU dependency
    * One per each bit-rate and one per each frame in parallel
    */
    void *aapv_dep_mngr_enc_loop_cu_top_right[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /** Dependency manager for controlling Deblocking Top dependency
    * One per each bit-rate and one per each frame in parallel
    */
    void *aapv_dep_mngr_enc_loop_dblk[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /** Dependency manager for controlling Sao Top dependency
    * One per each bit-rate and one per each frame in parallel
    */
    void *aapv_dep_mngr_enc_loop_sao[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /** number of bit-rate instances running */
    WORD32 i4_num_bitrates;

    /** number of enc frames running in parallel */
    WORD32 i4_num_enc_loop_frm_pllel;

    /* Pointer to Tile params base */
    void *pv_tile_params_base;
    /* Offset to get the Qp for the last CU of upper CTB-row.
    This offset is from the current tile top row QP map start.

    This will only be consumed by the first CU of current CTB-row
    iff [it is skip && entropy sync is off]
    There is one entry of every tile-column bcoz offset remains constant
    for all tiles lying in a tile-column */
    WORD32 ai4_offset_for_last_cu_qp[MAX_TILE_COLUMNS];
} ihevce_enc_loop_master_ctxt_t;

/**
******************************************************************************
*  @brief  This struct is used for storing data required by the block merge
*          function
******************************************************************************
*/
typedef struct
{
    block_data_8x8_t *ps_8x8_data;

    block_data_16x16_t *ps_16x16_data;

    block_data_32x32_t *ps_32x32_data;

    block_data_64x64_t *ps_64x64_data;

    part_type_results_t **ps_32x32_results;

    cur_ctb_cu_tree_t *ps_cu_tree;

    ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb;

    mv_pred_ctxt_t *ps_mv_pred_ctxt;

    recon_pic_buf_t *(*aps_ref_list)[HEVCE_MAX_REF_PICS * 2];

    nbr_4x4_t *ps_top_nbr_4x4;

    nbr_4x4_t *ps_left_nbr_4x4;

    nbr_4x4_t *ps_curr_nbr_4x4;

    UWORD8 *pu1_inp;

    UWORD8 *pu1_ctb_nbr_map;

    WORD32 i4_nbr_map_strd;

    WORD32 inp_stride;

    WORD32 i4_ctb_x_off;

    WORD32 i4_ctb_y_off;

    WORD32 use_satd_for_err_calc;

    WORD32 lambda;

    WORD32 lambda_q_shift;

    WORD32 frm_qstep;

    WORD32 num_4x4_in_ctb;

    UWORD8 *pu1_wkg_mem;

    UWORD8 **ppu1_pred;

    UWORD8 u1_bidir_enabled;

    UWORD8 u1_max_tr_depth;

    WORD32 i4_ctb_pos;

    WORD32 i4_ctb_size;

    UWORD8 *apu1_wt_inp[MAX_REFS_SEARCHABLE + 1];

    /** Pointer of Dep Mngr for EncLoop Top-Right CU dependency */
    void *pv_dep_mngr_enc_loop_cu_top_right;
    /** The current cu row no. for Dep Manager to Check */
    WORD32 i4_dep_mngr_cur_cu_row_no;
    /** The Top cu row no. for Dep Manager to Check */
    WORD32 i4_dep_mngr_top_cu_row_no;

    WORD8 i1_quality_preset;

    /* Flag to control Top Right Sync for during Merge */
    UWORD8 u1_use_top_at_ctb_boundary;

} block_merge_input_t;

/* Structure which stores the info regarding the TU's present in the CU*/
typedef struct tu_prms_t
{
    UWORD8 u1_tu_size;

    UWORD8 u1_x_off;

    UWORD8 u1_y_off;

    WORD32 i4_tu_cost;

    WORD32 i4_early_cbf;

} tu_prms_t;

typedef struct
{
    cu_enc_loop_out_t **pps_cu_final;

    pu_t **pps_row_pu;

    tu_enc_loop_out_t **pps_row_tu;

    UWORD8 **ppu1_row_ecd_data;

    WORD32 *pi4_num_pus_in_ctb;

    WORD32 *pi4_last_cu_pos_in_ctb;

    WORD32 *pi4_last_cu_size;

    UWORD8 *pu1_num_cus_in_ctb_out;

} cu_final_update_prms;

typedef struct
{
    cu_nbr_prms_t *ps_cu_nbr_prms;

    cu_inter_cand_t *ps_best_inter_cand;

    enc_loop_chrm_cu_buf_prms_t *ps_chrm_cu_buf_prms;

    WORD32 packed_pred_mode;

    WORD32 rd_opt_best_idx;

    void *pv_src;

    WORD32 src_strd;

    void *pv_pred;

    WORD32 pred_strd;

    void *pv_pred_chrm;

    WORD32 pred_chrm_strd;

    UWORD8 *pu1_final_ecd_data;

    UWORD8 *pu1_csbf_buf;

    WORD32 csbf_strd;

    void *pv_luma_recon;

    WORD32 recon_luma_strd;

    void *pv_chrm_recon;

    WORD32 recon_chrma_strd;

    UWORD8 u1_cu_pos_x;

    UWORD8 u1_cu_pos_y;

    UWORD8 u1_cu_size;

    WORD8 i1_cu_qp;

    UWORD8 u1_will_cabac_state_change;

    UWORD8 u1_recompute_sbh_and_rdoq;

    UWORD8 u1_is_first_pass;

#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
    UWORD8 u1_is_cu_noisy;
#endif

} final_mode_process_prms_t;

typedef struct
{
    cu_inter_cand_t s_best_cand;

    /* The size is twice of what is required to ensure availability */
    /* of adequate space for 'HBD' case */
    UWORD8 au1_pred_luma[MAX_CU_SIZE * MAX_CU_SIZE * 2];

    /* The size is twice of what is required to ensure availability */
    /* of adequate space for 422 case */
    UWORD8 au1_pred_chroma[MAX_CU_SIZE * MAX_CU_SIZE * 2];
} final_mode_state_t;

typedef struct
{
    cu_mixed_mode_inter_t *ps_mixed_modes_datastore;

    cu_inter_cand_t *ps_me_cands;

    cu_inter_cand_t *ps_merge_cands;

    mv_pred_ctxt_t *ps_mv_pred_ctxt;

    inter_pred_ctxt_t *ps_mc_ctxt;

    UWORD8 *pu1_ctb_nbr_map;

    void *pv_src;

    nbr_4x4_t *ps_cu_nbr_buf;

    nbr_4x4_t *ps_left_nbr_4x4;

    nbr_4x4_t *ps_top_nbr_4x4;

    nbr_4x4_t *ps_topleft_nbr_4x4;

    WORD32 i4_ctb_nbr_map_stride;

    WORD32 i4_src_strd;

    WORD32 i4_nbr_4x4_left_strd;

    UWORD8 u1_cu_size;

    UWORD8 u1_cu_pos_x;

    UWORD8 u1_cu_pos_y;

    UWORD8 u1_num_me_cands;

    UWORD8 u1_num_merge_cands;

    UWORD8 u1_max_num_mixed_mode_cands_to_select;

    UWORD8 u1_max_merge_candidates;

    UWORD8 u1_use_satd_for_merge_eval;

} ihevce_mixed_inter_modes_selector_prms_t;

typedef struct
{
    LWORD64 i8_ssd;

    LWORD64 i8_cost;

#if ENABLE_INTER_ZCU_COST
    LWORD64 i8_not_coded_cost;
#endif

    UWORD32 u4_sad;

    WORD32 i4_bits;

    WORD32 i4_num_bytes_used_for_ecd;

    WORD32 i4_zero_col;

    WORD32 i4_zero_row;

    UWORD8 u1_cbf;

    UWORD8 u1_reconBufId;

    UWORD8 u1_is_valid_node;

    UWORD8 u1_size;

    UWORD8 u1_posx;

    UWORD8 u1_posy;
} tu_node_data_t;

typedef struct tu_tree_node_t
{
    struct tu_tree_node_t *ps_child_node_tl;

    struct tu_tree_node_t *ps_child_node_tr;

    struct tu_tree_node_t *ps_child_node_bl;

    struct tu_tree_node_t *ps_child_node_br;

    tu_node_data_t s_luma_data;

    /* 2 because of the 2 subTU's when input is 422 */
    tu_node_data_t as_cb_data[2];

    tu_node_data_t as_cr_data[2];

    UWORD8 u1_is_valid_node;

} tu_tree_node_t;

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/
typedef LWORD64 (*pf_cu_mode_decide)(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    cu_analyse_t *ps_cu_analyse,
    final_mode_state_t *ps_final_mode_state,
    UWORD8 *pu1_ecd_data,
    pu_col_mv_t *ps_col_pu,
    UWORD8 *pu1_col_pu_map,
    WORD32 col_start_pu_idx);

typedef LWORD64 (*pf_inter_rdopt_cu_mc_mvp)(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    cu_inter_cand_t *ps_inter_cand,
    WORD32 cu_size,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y,
    nbr_4x4_t *ps_left_nbr_4x4,
    nbr_4x4_t *ps_top_nbr_4x4,
    nbr_4x4_t *ps_topleft_nbr_4x4,
    WORD32 nbr_4x4_left_strd,
    WORD32 curr_buf_idx);

typedef LWORD64 (*pf_inter_rdopt_cu_ntu)(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    void *pv_src,
    WORD32 cu_size,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y,
    WORD32 curr_buf_idx,
    enc_loop_chrm_cu_buf_prms_t *ps_chrm_cu_buf_prms,
    cu_inter_cand_t *ps_inter_cand,
    cu_analyse_t *ps_cu_analyse,
    WORD32 i4_alpha_stim_multiplier);

typedef void (*pf_intra_chroma_pred_mode_selector)(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_chrm_cu_buf_prms_t *ps_chrm_cu_buf_prms,
    cu_analyse_t *ps_cu_analyse,
    WORD32 rd_opt_curr_idx,
    WORD32 tu_mode,
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy);

typedef LWORD64 (*pf_intra_rdopt_cu_ntu)(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    void *pv_pred_org,
    WORD32 pred_strd_org,
    enc_loop_chrm_cu_buf_prms_t *ps_chrm_cu_buf_prms,
    UWORD8 *pu1_luma_mode,
    cu_analyse_t *ps_cu_analyse,
    void *pv_curr_src,
    void *pv_cu_left,
    void *pv_cu_top,
    void *pv_cu_top_left,
    nbr_4x4_t *ps_left_nbr_4x4,
    nbr_4x4_t *ps_top_nbr_4x4,
    WORD32 nbr_4x4_left_strd,
    WORD32 cu_left_stride,
    WORD32 curr_buf_idx,
    WORD32 func_proc_mode,
    WORD32 i4_alpha_stim_multiplier);

typedef void (*pf_final_rdopt_mode_prcs)(
    ihevce_enc_loop_ctxt_t *ps_ctxt, final_mode_process_prms_t *ps_prms);

typedef void (*pf_store_cu_results)(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    final_mode_state_t *ps_final_state);

typedef void (*pf_enc_loop_cu_bot_copy)(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    ihevce_enc_cu_node_ctxt_t *ps_enc_out_ctxt,
    WORD32 curr_cu_pos_in_row,
    WORD32 curr_cu_pos_in_ctb);

typedef void (*pf_enc_loop_ctb_left_copy)(
    ihevce_enc_loop_ctxt_t *ps_ctxt, enc_loop_cu_prms_t *ps_cu_prms);

#endif /* _IHEVCE_ENC_LOOP_STRUCTS_H_ */
