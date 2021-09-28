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
* \file ihevce_enc_structs.h
*
* \brief
*    This file contains structure definations of Encoder
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_ENC_STRUCTS_H_
#define _IHEVCE_ENC_STRUCTS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define HEVCE_MAX_WIDTH 1920
#define HEVCE_MAX_HEIGHT 1088

#define HEVCE_MIN_WIDTH 64
#define HEVCE_MIN_HEIGHT 64

#define MAX_CTBS_IN_FRAME (HEVCE_MAX_WIDTH * HEVCE_MAX_HEIGHT) / (MIN_CTB_SIZE * MIN_CTB_SIZE)
#define MAX_NUM_CTB_ROWS_FRM (HEVCE_MAX_HEIGHT) / (MIN_CTB_SIZE)

#define MIN_VERT_PROC_UNIT (8)
#define MAX_NUM_VERT_UNITS_FRM (HEVCE_MAX_HEIGHT) / (MIN_VERT_PROC_UNIT)

#define HEVCE_MAX_REF_PICS 8
#define HEVCE_MAX_DPB_PICS (HEVCE_MAX_REF_PICS + 1)

#define PAD_HORZ 80
#define PAD_VERT 80

#define DEFAULT_MAX_REFERENCE_PICS 4

#define BLU_RAY_SUPPORT 231457

/** @brief max number of parts in minCU : max 4 for NxN */
#define NUM_PU_PARTS 4
/** @brief max number of parts in Inter CU */
#define NUM_INTER_PU_PARTS (MAX_NUM_INTER_PARTS)
#define SEND_BI_RDOPT
#ifdef SEND_BI_RDOPT
/** @brief */
#define MAX_INTER_CU_CANDIDATES 4
#else
/** @brief */
#define MAX_INTER_CU_CANDIDATES 3
#endif
/** @brief */
#define MAX_INTRA_CU_CANDIDATES 3

#define MAX_INTRA_CANDIDATES 35

/** For each resolution & bit-rate instance, one entropy thread is created */
#define NUM_ENTROPY_THREADS (IHEVCE_MAX_NUM_RESOLUTIONS * IHEVCE_MAX_NUM_BITRATES)

/* Number of buffers between Decomp and HME layers 1 : Seq mode >1 parallel mode */
#define NUM_BUFS_DECOMP_HME 1

/** Macro to indicate pre me and L0 ipe stagger in pre enc*/
/** Implies MAX_PRE_ENC_STAGGER - 1 max stagger*/
#define MAX_PRE_ENC_STAGGER (NUM_LAP2_LOOK_AHEAD + 1 + MIN_L1_L0_STAGGER_NON_SEQ)

#define NUM_ME_ENC_BUFS (MAX_NUM_ENC_LOOP_PARALLEL)

#define MIN_L0_IPE_ENC_STAGGER 1

/*stagger between L0 IPE and enc*/
#define MAX_L0_IPE_ENC_STAGGER (NUM_ME_ENC_BUFS + (MIN_L0_IPE_ENC_STAGGER))

#define MAX_PRE_ENC_RC_DELAY (MAX_L0_IPE_ENC_STAGGER + 1 + NUM_BUFS_DECOMP_HME)

#define MIN_PRE_ENC_RC_DELAY (MIN_L0_IPE_ENC_STAGGER + 1 + NUM_BUFS_DECOMP_HME)

/** @brief number of ctb contexts maintained at frame level b/w encode : entropy */
#define NUM_FRMPROC_ENTCOD_BUFS 1

/** @brief number of extra recon buffs required for stagger design*/
#define NUM_EXTRA_RECON_BUFS 0

/** recon picture buffer size need to be increased to support EncLoop Parallelism **/
#define NUM_EXTRA_RECON_BUFS_FOR_ELP 0

/** @brief maximum number of bytes in 4x4 afetr scanning */
#define MAX_SCAN_COEFFS_BYTES_4x4 (48)

/** @brief maximum number of luma coeffs bytes after scan at CTB level  */
#define MAX_LUMA_COEFFS_CTB ((MAX_SCAN_COEFFS_BYTES_4x4) * (MAX_TU_IN_CTB)*4)

/** @brief maximum number of chroma coeffs bytes after scan at CTB level  */
#define MAX_CHRM_COEFFS_CTB ((MAX_SCAN_COEFFS_BYTES_4x4) * ((MAX_TU_IN_CTB >> 1)) * 4)

/** @brief maximum number of coeffs bytes after scan at CTB level  */
#define MAX_SCAN_COEFFS_CTB ((MAX_LUMA_COEFFS_CTB) + (MAX_CHRM_COEFFS_CTB))

/** @breif PU map CTB buffer buyes for neighbour availibility */
#define MUN_PU_MAP_BYTES_PER_CTB (MAX_PU_IN_CTB_ROW * MAX_PU_IN_CTB_ROW)

/** @brief tottal system memory records */
#define TOTAL_SYSTEM_MEM_RECS 120

/** @brief number of input async command buffers */
#define NUM_AYSNC_CMD_BUFS 4

/** @brief Comand buffers size */
#define ENC_COMMAND_BUFF_SIZE 512 /* 512 bytes */

/** @brief Number of output buffers */
#define NUM_OUTPUT_BUFS 4

/** @brief Lamda for SATD cost estimation */
#define LAMDA_SATD 1

/** @brief Maximum number of 1s in u2_sig_coeff_abs_gt1_flags */
#define MAX_GT_ONE 8

/** MAX num ipntra pred modes */
#define MAX_NUM_IP_MODES 35

/** Number of best intra modes used for intra mode refinement */
#define NUM_BEST_MODES 3

/** Maximim number of parallel frame processing threads in pre enocde group */
#define MAX_NUM_FRM_PROC_THRDS_PRE_ENC MAX_NUM_CORES

/** Maximim number of parallel frame processing threads in encode group */
#define MAX_NUM_FRM_PROC_THRDS_ENC MAX_NUM_CORES

/** Macro to indicate teh PING_PONG buffers for stagger*/
#define PING_PONG_BUF 2

/** Max number of layers in Motion estimation
 * should be greater than or equal to MAX_NUM_LAYERS defined in hme_interface.h
 */

#define MAX_NUM_HME_LAYERS 5
/**
******************************************************************************
 *  @brief      Maximum number of layers allowed
******************************************************************************
 */
#define MAX_NUM_LAYERS 4

#define NUM_RC_PIC_TYPE 9

#define MAX_NUM_NODES_CU_TREE (85)

/* macros to control Dynamic load balance */
#define DYN_LOAD_BAL_UPPER_LIMIT 0.80

#define DYN_LOAD_BAL_LOWER_LIMIT 0.20

#define NUM_SUB_GOP_DYN_BAL 1

#define MIN_NUM_FRMS_DYN_BAL 4

#define CORES_SRES_OR_MRES 2

#define HME_HIGH_SAD_BLK_THRESH 35

/* Enable to compare cabac states of final entropy thread with enc loop states */
#define VERIFY_ENCLOOP_CABAC_STATES 0

#define MAX_NUM_BLKS_IN_MAX_CU 64 /* max cu size is 64x64 */

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/
typedef void (*pf_iq_it_rec)(
    WORD16 *pi2_src,
    WORD16 *pi2_tmp,
    UWORD8 *pu1_pred,
    WORD16 *pi2_dequant_coeff,
    UWORD8 *pu1_dst,
    WORD32 qp_div, /* qpscaled / 6 */
    WORD32 qp_rem, /* qpscaled % 6 */
    WORD32 src_strd,
    WORD32 pred_strd,
    WORD32 dst_strd,
    WORD32 zero_cols,
    WORD32 zero_rows);

typedef void (*pf_intra_pred)(
    UWORD8 *pu1_ref, WORD32 src_strd, UWORD8 *pu1_dst, WORD32 dst_strd, WORD32 nt, WORD32 mode);

typedef UWORD32 (*pf_res_trans_luma)(
    UWORD8 *pu1_src,
    UWORD8 *pu1_pred,
    WORD32 *pi4_tmp,
    WORD16 *pi2_dst,
    WORD32 src_strd,
    WORD32 pred_strd,
    WORD32 dst_strd,
    CHROMA_PLANE_ID_T e_chroma_plane);

typedef WORD32 (*pf_quant)(
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_dst,
    WORD32 qp_div, /* qpscaled / 6 */
    WORD32 qp_rem, /* qpscaled % 6 */
    WORD32 q_add,
    WORD32 src_strd,
    WORD32 dst_strd,
    UWORD8 *pu1_csbf_buf,
    WORD32 csbf_strd,
    WORD32 *zero_cols,
    WORD32 *zero_row);

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
/// supported partition shape
typedef enum
{
    SIZE_2Nx2N = 0,  ///< symmetric motion partition,  2Nx2N
    SIZE_2NxN = 1,  ///< symmetric motion partition,  2Nx N
    SIZE_Nx2N = 2,  ///< symmetric motion partition,   Nx2N
    SIZE_NxN = 3,  ///< symmetric motion partition,   Nx N
    SIZE_2NxnU = 4,  ///< asymmetric motion partition, 2Nx( N/2) + 2Nx(3N/2)
    SIZE_2NxnD = 5,  ///< asymmetric motion partition, 2Nx(3N/2) + 2Nx( N/2)
    SIZE_nLx2N = 6,  ///< asymmetric motion partition, ( N/2)x2N + (3N/2)x2N
    SIZE_nRx2N = 7  ///< asymmetric motion partition, (3N/2)x2N + ( N/2)x2N
} PART_SIZE_E;

/** @brief  Interface level Queues of Encoder */

typedef enum
{
    IHEVCE_INPUT_DATA_CTRL_Q = 0,
    IHEVCE_ENC_INPUT_Q,
    IHEVCE_INPUT_ASYNCH_CTRL_Q,
    IHEVCE_OUTPUT_DATA_Q,
    IHEVCE_OUTPUT_STATUS_Q,
    IHEVCE_RECON_DATA_Q,  //   /*que for holding recon buffer */

    IHEVCE_FRM_PRS_ENT_COD_Q, /*que for holding output buffer of enc_loop |input buffer of entropy */

    IHEVCE_PRE_ENC_ME_Q, /*que for holding input buffer to ME | output of pre-enc */

    IHEVCE_ME_ENC_RDOPT_Q, /* que for holding output buffer of ME or input buffer of Enc-RDopt */

    IHEVCE_L0_IPE_ENC_Q, /* Queue for holding L0 ipe data to enc loop*/

    /* should be last entry */
    IHEVCE_MAX_NUM_QUEUES

} IHEVCE_Q_DESC_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/**
RC_QP_QSCALE conversion structures
**/
typedef struct
{
    WORD16 i2_min_qp;

    WORD16 i2_max_qp;

    WORD16 i2_min_qscale;

    WORD16 i2_max_qscale;

    WORD32 *pi4_qscale_to_qp;

    WORD32 *pi4_qp_to_qscale_q_factor;

    WORD32 *pi4_qp_to_qscale;

    WORD8 i1_qp_offset;

} rc_quant_t;

/**
******************************************************************************
 *  @brief     4x4 level structure which contains all the parameters
 *             for neighbour prediction puopose
******************************************************************************
 */
typedef struct
{
    /** PU motion vectors */
    pu_mv_t mv;
    /** Intra or Inter flag for each partition - 0 or 1  */
    UWORD16 b1_intra_flag : 1;
    /** CU skip flag - 0 or 1  */
    UWORD16 b1_skip_flag : 1;
    /** CU depth in CTB tree (0-3)  */
    UWORD16 b2_cu_depth : 2;

    /** Y Qp  for loop filter */
    WORD16 b8_qp : 8;

    /** Luma Intra Mode 0 - 34   */
    UWORD16 b6_luma_intra_mode : 6;

    /** Y CBF  for BS compute */
    UWORD16 b1_y_cbf : 1;
    /** Pred L0 flag of current 4x4 */
    UWORD16 b1_pred_l0_flag : 1;

    /** Pred L0 flag of current 4x4 */
    UWORD16 b1_pred_l1_flag : 1;
} nbr_4x4_t;

typedef struct
{
    /** Bottom Left availability flag */
    UWORD8 u1_bot_lt_avail;

    /** Left availability flag */
    UWORD8 u1_left_avail;

    /** Top availability flag */
    UWORD8 u1_top_avail;

    /** Top Right availability flag */
    UWORD8 u1_top_rt_avail;

    /** Top Left availability flag */
    UWORD8 u1_top_lt_avail;

} nbr_avail_flags_t;

typedef struct
{
    /** prev intra flag*/
    UWORD8 b1_prev_intra_luma_pred_flag : 1;

    /** mpm_idx */
    UWORD8 b2_mpm_idx : 2;

    /** reminder pred mode */
    UWORD8 b5_rem_intra_pred_mode : 5;

} intra_prev_rem_flags_t;

/**
******************************************************************************
 *  @brief     calc (T+Q+RDOQ) output TU structure; entropy input TU structure
******************************************************************************
 */
typedef struct
{
    /** base tu structure */
    tu_t s_tu;

    /** offset of luma data in ecd buffer */
    WORD32 i4_luma_coeff_offset;

    /** offset of cb data in ecd buffer */
    WORD32 ai4_cb_coeff_offset[2];

    /** offset of cr data in ecd buffer */
    WORD32 ai4_cr_coeff_offset[2];

} tu_enc_loop_out_t;

typedef struct
{
    /* L0 Motion Vector */
    mv_t s_l0_mv;

    /* L1 Motion Vector */
    mv_t s_l1_mv;

    /* L0 Ref index */
    WORD8 i1_l0_ref_idx;

    /*  L1 Ref index */
    WORD8 i1_l1_ref_idx;

    /* L0 Ref Pic Buf ID */
    WORD8 i1_l0_pic_buf_id;

    /* L1 Ref Pic Buf ID */
    WORD8 i1_l1_pic_buf_id;

    /** intra flag */
    UWORD8 b1_intra_flag : 1;

    /* Pred mode */
    UWORD8 b2_pred_mode : 2;

    /* reserved flag can be used for something later */
    UWORD8 u1_reserved;

} pu_col_mv_t;

/*****************************************************************************/
/* Encoder uses same structure as pu_t for prediction unit                   */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief     Encode loop (T+Q+RDOQ) output CU structure; entropy input CU structure
******************************************************************************
 */
typedef struct
{
    /* CU X position in terms of min CU (8x8) units */
    UWORD32 b3_cu_pos_x : 3;

    /* CU Y position in terms of min CU (8x8) units */
    UWORD32 b3_cu_pos_y : 3;

    /** CU size in terms of min CU (8x8) units */
    UWORD32 b4_cu_size : 4;

    /** transquant bypass flag ; 0 for this encoder */
    UWORD32 b1_tq_bypass_flag : 1;

    /** cu skip flag */
    UWORD32 b1_skip_flag : 1;

    /** intra / inter CU flag */
    UWORD32 b1_pred_mode_flag : 1;

    /** indicates partition information for CU
     *  For intra 0 : for 2Nx2N / 1 for NxN iff CU=minCBsize
     *  For inter 0 : @sa PART_SIZE_E
     */
    UWORD32 b3_part_mode : 3;

    /** 0 for this encoder */
    UWORD32 b1_pcm_flag : 1;

    /** only applicable for intra cu */
    UWORD32 b3_chroma_intra_pred_mode : 3;

    /** no residue flag for cu */
    UWORD32 b1_no_residual_syntax_flag : 1;

    /* flag to indicate if current CU is the first
    CU of the Quantisation group*/
    UWORD32 b1_first_cu_in_qg : 1;

    /** Intra prev and reminder flags
     * if part is NxN the tntries 1,2,3 will be valid
     * other wise only enry 0 will be set.
     */
    intra_prev_rem_flags_t as_prev_rem[NUM_PU_PARTS];

    /**
     *  Access valid  number of pus in this array based on u1_part_mode
     *  Moiton vector differentials and reference idx should be
     *  populated in this structure
     *  @remarks shall be accessed only for inter pus
     */
    pu_t *ps_pu;

    /**
     *  pointer to first tu of this cu. Each TU need to be populated
     *  in TU order by calc. Total TUs in CU is given by u2_num_tus_in_cu
     */
    tu_enc_loop_out_t *ps_enc_tu;

    /** total TUs in this CU; shall be 0 if b1_no_residual_syntax_flag = 1 */
    UWORD16 u2_num_tus_in_cu;

    /** Coeff bufer pointer */
    /* Pointer to transform coeff data */
    /*************************************************************************/
    /* Following format is repeated for every coded TU                       */
    /* Luma Block                                                            */
    /* num_coeffs      : 16 bits                                             */
    /* zero_cols       : 8 bits ( 1 bit per 4 columns)                       */
    /* sig_coeff_map   : ((TU Size * TU Size) + 31) >> 5 number of WORD32s   */
    /* coeff_data      : Non zero coefficients                               */
    /* Cb Block (only for last TU in 4x4 case else for every luma TU)        */
    /* num_coeffs      : 16 bits                                             */
    /* zero_cols       : 8 bits ( 1 bit per 4 columns)                       */
    /* sig_coeff_map   : ((TU Size * TU Size) + 31) >> 5 number of WORD32s   */
    /* coeff_data      : Non zero coefficients                               */
    /* Cr Block (only for last TU in 4x4 case else for every luma TU)        */
    /* num_coeffs      : 16 bits                                             */
    /* zero_cols       : 8 bits ( 1 bit per 4 columns)                       */
    /* sig_coeff_map   : ((TU Size * TU Size) + 31) >> 5 number of WORD32s   */
    /* coeff_data      : Non zero coefficients                               */
    /*************************************************************************/
    void *pv_coeff;

    /** qp used during for CU
      * @remarks :
      */
    WORD8 i1_cu_qp;

} cu_enc_loop_out_t;

/**
 * SAO
 */
typedef struct
{
    /**
     * sao_type_idx_luma
     */
    UWORD32 b3_y_type_idx : 3;

    /**
     * luma sao_band_position
     */
    UWORD32 b5_y_band_pos : 5;

    /**
     * sao_type_idx_chroma
     */
    UWORD32 b3_cb_type_idx : 3;

    /**
     * cb sao_band_position
     */
    UWORD32 b5_cb_band_pos : 5;

    /**
     * sao_type_idx_chroma
     */
    UWORD32 b3_cr_type_idx : 3;

    /**
     * cb sao_band_position
     */
    UWORD32 b5_cr_band_pos : 5;

    /*SAO Offsets
     * In all these offsets, 0th element is not used
     */
    /**
     * luma SaoOffsetVal[i]
     */
    WORD8 u1_y_offset[5];

    /**
     * chroma cb SaoOffsetVal[i]
     */
    WORD8 u1_cb_offset[5];

    /**
     * chroma cr SaoOffsetVal[i]
     */
    WORD8 u1_cr_offset[5];

    /**
     * sao_merge_left_flag common for y,cb,cr
     */
    UWORD32 b1_sao_merge_left_flag : 1;

    /**
     * sao_merge_up_flag common for y,cb,cr
     */
    UWORD32 b1_sao_merge_up_flag : 1;

} sao_enc_t;

/**
******************************************************************************
 *  @brief       ctb output structure; output of Encode loop, input to entropy
******************************************************************************
 */
typedef struct
{
    /**
     * bit0     :  depth0 split flag, (64x64 splits)
     * bits 1-3 :  not used
     * bits 4-7 :  depth1 split flags; valid iff depth0 split=1 (32x32 splits)
     * bits 8-23:  depth2 split flags; (if 0 16x16 is cu else 8x8 min cu)

     * if a split flag of n is set for depth 1, check the following split flags
     * of [(8 + 4*(n-4)): (8 + 4*(n-4)+ 3)] for depth 2:
     *
     */
    UWORD32 u4_cu_split_flags;

    /***************************************************************
     * For any given CU position CU_posx, CU_posy access
     *  au4_packed_tu_split_flags[(CU_posx >> 5)[(CU_posy >> 5)]
     * Note : For CTB size smaller than 64x64 only use u4_packed_tu_split_flags[0]
     ****************************************************************/

    /**
     * access bits corresponding to actual CU size till leaf nodes
     * bit0     :  (32x32 TU split flag)
     * bits 1-3 :  not used
     * bits 4-7 :  (16x16 TUsplit flags)
     * bits 8-23:  (8x8  TU split flags)

     * if a split flag of n is set for depth 1, check the following split flags
     * of [(8 + 4*(n-4)): (8 + 4*(n-4)+ 3)] for depth 2:
     *
     * @remarks     As tu sizes are relative to CU sizes the producer has to
     * make sure the correctness of u4_packed_tu_split_flags.
     *
     * @remarks     au4_packed_tu_split_flags_cu[1]/[2]/[3] to be used only
     *              for 64x64 ctb.
     */
    UWORD32 au4_packed_tu_split_flags_cu[4];

    /**
     *  pointer to first CU of CTB. Each CU need to be populated
     *  in CU order by calc. Total CUs in CTB is given by u1_num_cus_in_ctb
     */
    cu_enc_loop_out_t *ps_enc_cu;

    /** total TUs in this CU; shall be 0 if b1_no_residual_syntax_flag = 1 */
    UWORD8 u1_num_cus_in_ctb;

    /** CTB neighbour availability flags */
    nbr_avail_flags_t s_ctb_nbr_avail_flags;

    /* SAO parameters of the CTB */
    sao_enc_t s_sao;

} ctb_enc_loop_out_t;

/**
******************************************************************************
 *  @brief      cu inter candidate for encoder
******************************************************************************
 */
typedef struct
{
    /** base pu structure
     *  access valid  number of entries in this array based on u1_part_size
     */
    pu_t as_inter_pu[NUM_INTER_PU_PARTS];

    /* TU split flag : tu_split_flag[0] represents the transform splits
     *  for CU size <= 32, for 64x64 each ai4_tu_split_flag corresponds
     *  to respective 32x32  */
    /* For a 8x8 TU - 1 bit used to indicate split */
    /* For a 16x16 TU - LSB used to indicate winner between 16 and 8 TU's. 4 other bits used to indicate split in each 8x8 quadrant */
    /* For a 32x32 TU - See above */
    WORD32 ai4_tu_split_flag[4];

    /* TU split flag : tu_split_flag[0] represents the transform splits
     *  for CU size <= 32, for 64x64 each ai4_tu_split_flag corresponds
     *  to respective 32x32  */
    /* For a 8x8 TU - 1 bit used to indicate split */
    /* For a 16x16 TU - LSB used to indicate winner between 16 and 8 TU's. 4 other bits used to indicate split in each 8x8 quadrant */
    /* For a 32x32 TU - See above */
    WORD32 ai4_tu_early_cbf[4];

    /**Pointer to the buffer having predicted data after mc in SATD stage
     * Since we have 2 buffers for each candidate pred data for best merge candidate
     * can be in one of the 2 buffers.
     */
    UWORD8 *pu1_pred_data;

    UWORD16 *pu2_pred_data;

    UWORD8 *pu1_pred_data_scr;

    UWORD16 *pu2_pred_data_src;

    /* Total cost: SATD cost + MV cost */
    WORD32 i4_total_cost;

    /** Stride for predicted data*/
    WORD32 i4_pred_data_stride;

    /** @remarks u1_part_size can be non square only for  Inter   */
    UWORD8 b3_part_size : 3; /* @sa: PART_SIZE_E */

    /** evaluate transform for cusize iff this flag is 1 */
    /** this flag should be set 0 if CU is 64x64         */
    UWORD8 b1_eval_tx_cusize : 1;

    /** evaluate transform for cusize/2 iff this flag is 1 */
    UWORD8 b1_eval_tx_cusize_by2 : 1;

    /** Skip Flag : ME should always set this 0 for the candidates */
    UWORD8 b1_skip_flag : 1;

    UWORD8 b1_intra_has_won : 1;

    /* used to mark if this mode needs to be evaluated in auxiliary mode */
    /* if 1, this mode will be evaluated otherwise not.*/
    UWORD8 b1_eval_mark : 1;

} cu_inter_cand_t;

/**
******************************************************************************
 *  @brief      cu intra candidate for encoder
******************************************************************************
 */
typedef struct
{
    UWORD8 au1_intra_luma_mode_nxn_hash[NUM_PU_PARTS][MAX_INTRA_CANDIDATES];

    /**
     *  List of NxN PU candidates in CU  for each partition
     *  valid only of if current cusize = mincusize
     * +1 to signal the last flag invalid value of 255 needs to be stored
     */
    UWORD8 au1_intra_luma_modes_nxn[NUM_PU_PARTS][(MAX_INTRA_CU_CANDIDATES * (4)) + 2 + 1];

    /* used to mark if this mode needs to be evaluated in auxiliary mode */
    /* if 1, this mode will be evaluated otherwise not.*/
    UWORD8 au1_nxn_eval_mark[NUM_PU_PARTS][MAX_INTRA_CU_CANDIDATES + 1];

    /**
     *  List of 2Nx2N PU candidates in CU
     * +1 to signal the last flag invalid value of 255 needs to be stored
     */
    UWORD8 au1_intra_luma_modes_2nx2n_tu_eq_cu[MAX_INTRA_CU_CANDIDATES + 1];

    /**
     *  List of 2Nx2N PU candidates in CU
     * +1 to signal the last flag invalid value of 255 needs to be stored
     */
    UWORD8 au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[MAX_INTRA_CU_CANDIDATES + 1];

    /* used to mark if this mode needs to be evaluated in auxiliary mode */
    /* if 1, this mode will be evaluated otherwise not.*/
    UWORD8 au1_2nx2n_tu_eq_cu_eval_mark[MAX_INTRA_CU_CANDIDATES + 1];

    /* used to mark if this mode needs to be evaluated in auxiliary mode */
    /* if 1, this mode will be evaluated otherwise not.*/
    UWORD8 au1_2nx2n_tu_eq_cu_by_2_eval_mark[MAX_INTRA_CU_CANDIDATES + 1];

    UWORD8 au1_num_modes_added[NUM_PU_PARTS];

    /** evaluate transform for cusize iff this flag is 1 */
    /** this flag should be set 0 if CU is 64x64         */
    UWORD8 b1_eval_tx_cusize : 1;

    /** evaluate transform for cusize/2 iff this flag is 1 */
    UWORD8 b1_eval_tx_cusize_by2 : 1;

    /** number of intra candidates for SATD evaluation in */
    UWORD8 b6_num_intra_cands : 6;

} cu_intra_cand_t;

/**
******************************************************************************
 *  @brief      cu structure for mode analysis/evaluation
******************************************************************************
 */
typedef struct
{
    /** CU X position in terms of min CU (8x8) units */
    UWORD8 b3_cu_pos_x : 3;

    /** CU Y position in terms of min CU (8x8) units */
    UWORD8 b3_cu_pos_y : 3;

    /** reserved bytes */
    UWORD8 b2_reserved : 2;

    /** CU size 2N (width or height) in pixels */
    UWORD8 u1_cu_size;

    /** Intra CU candidates after FAST CU decision (output of IPE)
     *  8421 algo along with transform size evalution will
     *  be done for these modes in Encode loop pass.
     */
    cu_intra_cand_t s_cu_intra_cand;

    /** indicates the angular mode (0 - 34) for chroma,
     *  Note : No provision currently to take chroma through RDOPT or SATD
     */
    UWORD8 u1_chroma_intra_pred_mode;

    /** number of inter candidates in as_cu_inter_cand[]
      * shall be 0 for intra frames.
      * These inters are evaluated for RDOPT apart from merge/skip candidates
      */
    UWORD8 u1_num_inter_cands;

    /** List of candidates to be evalauted (SATD/RDOPT) for this CU
      * @remarks : all  merge/skip candidates not a part of this list
      */
    cu_inter_cand_t as_cu_inter_cand[MAX_INTER_CU_CANDIDATES];

    WORD32 ai4_mv_cost[MAX_INTER_CU_CANDIDATES][NUM_INTER_PU_PARTS];

#if REUSE_ME_COMPUTED_ERROR_FOR_INTER_CAND_SIFTING
    WORD32 ai4_err_metric[MAX_INTER_CU_CANDIDATES][NUM_INTER_PU_PARTS];
#endif

    /* Flag to convey if Inta or Inter is the best candidate among the
    candidates populated
     0: If inter is the winner and 1: if Intra is winner*/
    UWORD8 u1_best_is_intra;

    /** number of intra rdopt candidates
      * @remarks : shall be <= u1_num_intra_cands
      */
    UWORD8 u1_num_intra_rdopt_cands;
    /** qp used during for CU
      * @remarks :
      */
    WORD8 i1_cu_qp;
    /** Activity factor used in pre enc thread for deriving the Qp
      * @remarks : This is in Q format
      */
    WORD32 i4_act_factor[4][2];

} cu_analyse_t;

/**
******************************************************************************
 *  @brief      Structure for CU recursion
******************************************************************************
 */
typedef struct cur_ctb_cu_tree_t
{
    /** CU X position in terms of min CU (8x8) units */
    UWORD8 b3_cu_pos_x : 3;

    /** CU X position in terms of min CU (8x8) units */
    UWORD8 b3_cu_pos_y : 3;

    /** reserved bytes */
    UWORD8 b2_reserved : 2;

    UWORD8 u1_cu_size;

    UWORD8 u1_intra_eval_enable;

    UWORD8 u1_inter_eval_enable;

    /* Flag that indicates whether to evaluate this node */
    /* during RDOPT evaluation. This does not mean that */
    /* evaluation of the children need to be abandoned */
    UWORD8 is_node_valid;

    LWORD64 i8_best_rdopt_cost;

    struct cur_ctb_cu_tree_t *ps_child_node_tl;

    struct cur_ctb_cu_tree_t *ps_child_node_tr;

    struct cur_ctb_cu_tree_t *ps_child_node_bl;

    struct cur_ctb_cu_tree_t *ps_child_node_br;

} cur_ctb_cu_tree_t;

typedef struct
{
    WORD32 num_best_results;

    part_type_results_t as_best_results[NUM_BEST_ME_OUTPUTS];

} block_data_32x32_t;

/**
******************************************************************************
 *  @brief      Structure for storing data about all the 64x64
 *              block in a 64x64 CTB
******************************************************************************
 */
typedef block_data_32x32_t block_data_64x64_t;

/**
******************************************************************************
 *  @brief      Structure for storing data about all 16 16x16
 *              blocks in a 64x64 CTB and each of their partitions
******************************************************************************
 */
typedef struct
{
    WORD32 num_best_results;

    /**
     * mask of active partitions, Totally 17 bits. For a given partition
     * id, as per PART_ID_T enum the corresponding bit position is 1/0
     * indicating that partition is active or inactive
     */
    /*WORD32 i4_part_mask;*/

    part_type_results_t as_best_results[NUM_BEST_ME_OUTPUTS];

} block_data_16x16_t;

typedef struct
{
    WORD32 num_best_results;

    part_type_results_t as_best_results[NUM_BEST_ME_OUTPUTS];
} block_data_8x8_t;

/**
******************************************************************************
 *  @brief      Structure for data export from ME to Enc_Loop
******************************************************************************
 */
typedef struct
{
    block_data_8x8_t as_8x8_block_data[64];

    block_data_16x16_t as_block_data[16];

    block_data_32x32_t as_32x32_block_data[4];

    block_data_64x64_t s_64x64_block_data;

} me_ctb_data_t;

/**
******************************************************************************
 *  @brief   noise detection related structure
 *
******************************************************************************
 */

typedef struct
{
    WORD32 i4_noise_present;

    UWORD8 au1_is_8x8Blk_noisy[MAX_CU_IN_CTB];

    UWORD32 au4_variance_src_16x16[MAX_CU_IN_CTB];
} ihevce_ctb_noise_params;

/**
******************************************************************************
 *  @brief      ctb structure for mode analysis/evaluation
******************************************************************************
 */
typedef struct
{
    /**
     * CU decision in a ctb is frozen by ME/IPE and populated in
     * u4_packed_cu_split_flags.
     * @remarks
     * TODO:review comment
     * bit0     :  64x64 split flag,  (depth0 flag for 64x64 ctb unused for smaller ctb)
     * bits 1-3 :  not used
     * bits 4-7 :  32x32 split flags; (depth1 flags for 64x64ctb / only bit4 used for 32x32ctb)
     * bits 8-23:  16x16 split flags; (depth2 flags for 64x64 / depth1[bits8-11] for 32x32 [bit8 for ctb 16x16] )

     * if a split flag of n is set for depth 1, check the following split flags
     * of [(8 + 4*(n-4)): (8 + 4*(n-4)+ 3)] for depth 2:
     *
     */
    UWORD32 u4_cu_split_flags;

    UWORD8 u1_num_cus_in_ctb;

    cur_ctb_cu_tree_t *ps_cu_tree;

    me_ctb_data_t *ps_me_ctb_data;

    ihevce_ctb_noise_params s_ctb_noise_params;

} ctb_analyse_t;
/**
******************************************************************************
 *  @brief Structures for tapping ssd and bit-estimate information for all CUs
******************************************************************************
 */

typedef struct
{
    LWORD64 i8_cost;
    WORD32 i4_idx;
} cost_idx_t;

/**
******************************************************************************
 *  @brief      reference/non reference pic context for encoder
******************************************************************************
 */
typedef struct

{
    /**
     * YUV buffer discriptor for the recon
     * Allocation per frame for Y = ((ALIGN(frame width, MAX_CTB_SIZE)) +  2 * PAD_HORZ)*
     *                              ((ALIGN(frame height, MAX_CTB_SIZE)) + 2 * PAD_VERT)
     */
    iv_enc_yuv_buf_t s_yuv_buf_desc;

    iv_enc_yuv_buf_src_t s_yuv_buf_desc_src;

    /* Pointer to Luma (Y) sub plane buffers Horz/ Vert / HV grid            */
    /* When (L0ME_IN_OPENLOOP_MODE == 1), additional buffer required to store */
    /* the fullpel plane for use as reference */
    UWORD8 *apu1_y_sub_pel_planes[3 + L0ME_IN_OPENLOOP_MODE];

    /**
     * frm level pointer to pu bank for colocated  mv access
     * Allocation per frame = (ALIGN(frame width, MAX_CTB_SIZE) / MIN_PU_SIZE) *
     *                         (ALIGN(frame height, MAX_CTB_SIZE) / MIN_PU_SIZE)
     */
    pu_col_mv_t *ps_frm_col_mv;
    /**
     ************************************************************************
     * Pointer to a PU map stored at frame level,
     * It contains a 7 bit pu index in encoder order w.r.t to a ctb at a min
     * granularirty of MIN_PU_SIZE size.
     ************************************************************************
     */
    UWORD8 *pu1_frm_pu_map;

    /** CTB level frame buffer to store the accumulated sum of
     * number of PUs for every row */
    UWORD16 *pu2_num_pu_map;

    /** Offsets in the PU buffer at every CTB level */
    UWORD32 *pu4_pu_off;

    /**  Collocated POC for reference list 0
     * ToDo: Change the array size when multiple slices are to be supported */
    WORD32 ai4_col_l0_poc[HEVCE_MAX_REF_PICS];

    /** Collocated POC for reference list 1 */
    WORD32 ai4_col_l1_poc[HEVCE_MAX_REF_PICS];

    /** 0 = top field,  1 = bottom field  */
    WORD32 i4_bottom_field;

    /** top field first input in case of interlaced case */
    WORD32 i4_topfield_first;

    /** top field first input in case of interlaced case */
    WORD32 i4_poc;

    /** unique buffer id */
    WORD32 i4_buf_id;

    /** is this reference frame or not */
    WORD32 i4_is_reference;

    /** Picture type of current picture */
    WORD32 i4_pic_type;

    /** Flag to indicate whether current pictute is free or in use */
    WORD32 i4_is_free;

    /** Bit0 -  of this Flag to indicate whether current pictute needs to be deblocked,
        padded and hpel planes need to be generated.
        These are turned off typically in non referecne pictures when psnr
        and recon dump is disabled.

        Bit1 - of this flag set to 1 if sao is enabled. This is to enable deblocking when sao is enabled
     */
    WORD32 i4_deblk_pad_hpel_cur_pic;

    /**
     * weight and offset for this ref pic. To be initialized for every pic
     * based on the lap output
     */
    ihevce_wght_offst_t s_weight_offset;

    /**
     * Reciprocal of the lumaweight in q15 format
     */
    WORD32 i4_inv_luma_wt;

    /**
     * Log to base 2 of the common denominator used for luma weights across all ref pics
     */
    WORD32 i4_log2_wt_denom;

    /**
     * Used as Reference for encoding current picture flag
     */
    WORD32 i4_used_by_cur_pic_flag;

#if ADAPT_COLOCATED_FROM_L0_FLAG
    WORD32 i4_frame_qp;
#endif
    /*
    * IDR GOP number
    */

    WORD32 i4_idr_gop_num;

    /*
    * non-ref-free_flag
    */
    WORD32 i4_non_ref_free_flag;
    /**
      * Dependency manager instance for ME - Prev recon dep
      */
    void *pv_dep_mngr_recon;

    /*display num*/
    WORD32 i4_display_num;
} recon_pic_buf_t;

/**
******************************************************************************
 *  @brief  Lambda values used for various cost computations
******************************************************************************
 */
typedef struct
{
    /************************************************************************/
    /* The fields with the string 'type2' in their names are required */
    /* when both 8bit and hbd lambdas are needed. The lambdas corresponding */
    /* to the bit_depth != internal_bit_depth are stored in these fields */
    /************************************************************************/

    /**
     * Closed loop SSD Lambda
     * This is multiplied with bits for RD cost computations in SSD mode
     * This is represented in q format with shift of LAMBDA_Q_SHIFT
     */
    LWORD64 i8_cl_ssd_lambda_qf;

    LWORD64 i8_cl_ssd_type2_lambda_qf;

    /**
     * Closed loop SSD Lambda for chroma residue (chroma qp is different from luma qp)
     * This is multiplied with bits for RD cost computations in SSD mode
     * This is represented in q format with shift of LAMBDA_Q_SHIFT
     */
    LWORD64 i8_cl_ssd_lambda_chroma_qf;

    LWORD64 i8_cl_ssd_type2_lambda_chroma_qf;

    /**
     * Closed loop SAD Lambda
     * This is multiplied with bits for RD cost computations in SAD mode
     * This is represented in q format with shift of LAMBDA_Q_SHIFT
     */
    WORD32 i4_cl_sad_lambda_qf;

    WORD32 i4_cl_sad_type2_lambda_qf;

    /**
     * Open loop SAD Lambda
     * This is multiplied with bits for RD cost computations in SAD mode
     * This is represented in q format with shift of LAMBDA_Q_SHIFT
     */
    WORD32 i4_ol_sad_lambda_qf;

    WORD32 i4_ol_sad_type2_lambda_qf;

    /**
     * Closed loop SATD Lambda
     * This is multiplied with bits for RD cost computations in SATD mode
     * This is represented in q format with shift of LAMBDA_Q_SHIFT
     */
    WORD32 i4_cl_satd_lambda_qf;

    WORD32 i4_cl_satd_type2_lambda_qf;

    /**
     * Open loop SATD Lambda
     * This is multiplied with bits for RD cost computations in SATD mode
     * This is represented in q format with shift of LAMBDA_Q_SHIFT
     */
    WORD32 i4_ol_satd_lambda_qf;

    WORD32 i4_ol_satd_type2_lambda_qf;

    double lambda_modifier;

    double lambda_uv_modifier;

    UWORD32 u4_chroma_cost_weighing_factor;

} frm_lambda_ctxt_t;
/**
******************************************************************************
*  @brief  Mode attributes for 4x4 block populated by early decision
******************************************************************************
 */
typedef struct
{
    /* If best mode is present or not */
    UWORD8 mode_present;

    /** Best mode for the current 4x4 prediction block */
    UWORD8 best_mode;

    /** sad for the best mode for the current 4x4 prediction block */
    UWORD16 sad;

    /** cost for the best mode for the current 4x4 prediction block */
    UWORD16 sad_cost;

} ihevce_ed_mode_attr_t;  //early decision

/**
******************************************************************************
 *  @brief  Structure at 4x4 block level which has parameters about early
 *          intra or inter decision
******************************************************************************
 */
typedef struct
{
    /**
     * Final parameter of Intra-Inter early decision for the current 4x4.
     * 0 - invalid decision
     * 1 - eval intra only
     * 2 - eval inter only
     * 3 - eval both intra and inter
     */
    UWORD8 intra_or_inter;

    UWORD8 merge_success;

    /** Best mode for the current 4x4 prediction block */
    UWORD8 best_mode;

    /** Best mode for the current 4x4 prediction block */
    UWORD8 best_merge_mode;

    /** Store SATD at 4*4 level for current layer (L1) */
    WORD32 i4_4x4_satd;

} ihevce_ed_blk_t;  //early decision

/* l1 ipe ctb analyze structure */
/* Contains cu level qp mod related information for all possible cu
sizes (16,32,64 in L0) in a CTB*/
typedef struct
{
    WORD32 i4_sum_4x4_satd[16];
    WORD32 i4_min_4x4_satd[16];

    /* satd for L1_8x8 blocks in L1_32x32
     * [16] : num L1_8x8 in L1_32x32
     * [2]  : 0 - sum of L1_4x4 @ L1_8x8
     *          - equivalent to transform size of 16x16 @ L0
     *        1 - min/median of L1_4x4 @ L1_8x8
     *          - equivalent to transform size of 8x8 @ L0
     */
    WORD32 i4_8x8_satd[16][2];

    /* satd for L1_16x16 blocks in L1_32x32
     * [4] : num L1_16x16 in L1_32x32
     * [3] : 0 - sum of (sum of L1_4x4 @ L1_8x8) @ L1_16x16
     *         - equivalent to transform size of 32x32 @ L0
     *       1 - min/median of (sum of L1_4x4 @ L1_8x8) @ L1_16x16
     *         - equivalent to transform size of 16x16 @ L0
     *       2 - min/median of (min/median of L1_4x4 @ L1_8x8) @ L1_16x16
     *         - equivalent to transform size of 8x8 @ L0
     */
    WORD32 i4_16x16_satd[4][3];

    /* Please note that i4_32x32_satd[0][3] contains sum of all 32x32 */
    /* satd for L1_32x32 blocks in L1_32x32
     * [1] : num L1_32x32 in L1_32x32
     * [4] : 0 - min/median of (sum of (sum of L1_4x4 @ L1_8x8) @ L1_16x16) @ L1_32x32
     *         - equivalent to transform size of 32x32 @ L0
     *       1 - min/median of (sum of L1_4x4 @ L1_8x8) @ L1_32x32
     *         - equivalent to transform size of 16x16 @ L0
     *       2 - min/median of (min/median of L1_4x4 @ L1_8x8) @ L1_32x32
     *         - equivalent to transform size of 8x8 @ L0
     *       3 - sum of (sum of (sum of L1_4x4 @ L1_8x8) @ L1_16x16) @ L1_32x32
     */
    WORD32 i4_32x32_satd[1][4];

    /*Store SATD at 8x8 level for current layer (L1)*/
    WORD32 i4_best_satd_8x8[16];

    /* EIID: This will be used for early inter intra decisions */
    /*SAD at 8x8 level for current layer (l1) */
    /*Cost based on sad at 8x8 level for current layer (l1) */
    WORD32 i4_best_sad_cost_8x8_l1_ipe[16];

    WORD32 i4_best_sad_8x8_l1_ipe[16];
    /* SAD at 8x8 level for ME. All other cost are IPE cost */
    WORD32 i4_best_sad_cost_8x8_l1_me[16];

    /* SAD at 8x8 level for ME. for given reference */
    WORD32 i4_sad_cost_me_for_ref[16];

    /* SAD at 8x8 level for ME. for given reference */
    WORD32 i4_sad_me_for_ref[16];

    /* SAD at 8x8 level for ME. All other cost are IPE cost */
    WORD32 i4_best_sad_8x8_l1_me[16];

    WORD32 i4_best_sad_8x8_l1_me_for_decide[16];

    /*Mean @ L0 16x16*/
    WORD32 ai4_16x16_mean[16];

    /*Mean @ L0 32x32*/
    WORD32 ai4_32x32_mean[4];

    /*Mean @ L0 64x64*/
    WORD32 i4_64x64_mean;

} ihevce_ed_ctb_l1_t;  //early decision

/**
******************************************************************************
 *  @brief   8x8 Intra analyze structure
******************************************************************************
 */
typedef struct
{
    /** Best intra modes for 8x8 transform.
     *  Insert 255 in the end to limit number of modes
     */
    UWORD8 au1_best_modes_8x8_tu[MAX_INTRA_CU_CANDIDATES + 1];

    /** Best 8x8 intra modes for 4x4 transform
     *  Insert 255 in the end to limit number of modes
     */
    UWORD8 au1_best_modes_4x4_tu[MAX_INTRA_CU_CANDIDATES + 1];

    /** Best 4x4 intra modes
     *  Insert 255 in the end to limit number of modes
     */
    UWORD8 au1_4x4_best_modes[4][MAX_INTRA_CU_CANDIDATES + 1];

    /** flag to indicate if nxn pu mode (different pu at 4x4 level) is enabled */
    UWORD8 b1_enable_nxn : 1;

    /** valid cu flag : required for incomplete ctbs at frame boundaries */
    UWORD8 b1_valid_cu : 1;

    /** dummy bits */
    UWORD8 b6_reserved : 6;

} intra8_analyse_t;

/**
******************************************************************************
 *  @brief   16x16 Intra analyze structure
******************************************************************************
 */
typedef struct
{
    /** Best intra modes for 16x16 transform.
     *  Insert 255 in the end to limit number of modes
     */
    UWORD8 au1_best_modes_16x16_tu[MAX_INTRA_CU_CANDIDATES + 1];

    /** Best 16x16 intra modes for 8x8 transform
     *  Insert 255 in the end to limit number of modes
     */
    UWORD8 au1_best_modes_8x8_tu[MAX_INTRA_CU_CANDIDATES + 1];

    /** 8x8 children intra analyze for this 16x16 */
    intra8_analyse_t as_intra8_analyse[4];

    /* indicates if 16x16 is best cu or 8x8 cu */
    UWORD8 b1_split_flag : 1;

    /* indicates if 8x8 vs 16x16 rdo evaluation needed */
    /* or only 8x8's rdo evaluation needed */
    UWORD8 b1_merge_flag : 1;

    /**
     * valid cu flag : required for incomplete ctbs at frame boundaries
     * or if CTB size is lower than 32
     */
    UWORD8 b1_valid_cu : 1;

    /** dummy bits */
    UWORD8 b6_reserved : 5;

} intra16_analyse_t;

/**
******************************************************************************
 *  @brief   32x32 Intra analyze structure
******************************************************************************
 */
typedef struct
{
    /** Best intra modes for 32x32 transform.
     *  Insert 255 in the end to limit number of modes
     */
    UWORD8 au1_best_modes_32x32_tu[MAX_INTRA_CU_CANDIDATES + 1];

    /** Best 32x32 intra modes for 16x16 transform
     *  Insert 255 in the end to limit number of modes
     */
    UWORD8 au1_best_modes_16x16_tu[MAX_INTRA_CU_CANDIDATES + 1];

    /** 16x16 children intra analyze for this 32x32 */
    intra16_analyse_t as_intra16_analyse[4];

    /* indicates if 32x32 is best cu or 16x16 cu    */
    UWORD8 b1_split_flag : 1;

    /* indicates if 32x32 vs 16x16 rdo evaluation needed */
    /* or 16x16 vs 8x8 evaluation is needed */
    UWORD8 b1_merge_flag : 1;

    /**
     * valid cu flag : required for incomplete ctbs at frame boundaries
     * or if CTB size is lower than 64
     */
    UWORD8 b1_valid_cu : 1;

    /** dummy bits */
    UWORD8 b6_reserved : 5;

} intra32_analyse_t;

/**
******************************************************************************
 *  @brief  IPE L0 analyze structure for L0 ME to do intra/inter CU decisions
 *          This is a CTB level structure encapsulating IPE modes, cost at all
 *          level. IPE also recommemds max intra CU sizes which is required
 *          by ME for CU size determination in intra dominant CTB
******************************************************************************
 */
typedef struct
{
    /** Best 64x64 intra modes for 32x32 transform.
     *  Insert 255 in the end to limit number of modes
     */
    UWORD8 au1_best_modes_32x32_tu[MAX_INTRA_CU_CANDIDATES + 1];

    /** 32x32 children intra analyze for this 32x32    */
    intra32_analyse_t as_intra32_analyse[4];

    /* indicates if 64x64 is best CUs or 32x32 CUs      */
    UWORD8 u1_split_flag;

    /* CTB level best 8x8 intra costs  */
    WORD32 ai4_best8x8_intra_cost[MAX_CU_IN_CTB];

    /* CTB level best 16x16 intra costs */
    WORD32 ai4_best16x16_intra_cost[MAX_CU_IN_CTB >> 2];

    /* CTB level best 32x32 intra costs */
    WORD32 ai4_best32x32_intra_cost[MAX_CU_IN_CTB >> 4];

    /* best 64x64 intra cost */
    WORD32 i4_best64x64_intra_cost;

    /*
    @L0 level
    4 => 0 - 32x32 TU in 64x64 CU
         1 - 16x16 TU in 64x64 CU
         2 - 8x8  TU in 64x64 CU
         3 - 64x64 CU
    2 => Intra/Inter */
    WORD32 i4_64x64_act_factor[4][2];

    /*
    @L0 level
    4 => num 32x32 in CTB
    3 => 0 - 32x32 TU in 64x64 CU
         1 - 16x16 TU in 64x64 CU
         2 - 8x8  TU in 64x64 CU
    2 => Intra/Inter */
    WORD32 i4_32x32_act_factor[4][3][2];

    /*
    @L0 level
    16 => num 16x16 in CTB
    2 => 0 - 16x16 TU in 64x64 CU
         1 - 8x8  TU in 64x64 CU
    2 => Intra/Inter */
    WORD32 i4_16x16_act_factor[16][2][2];

    WORD32 nodes_created_in_cu_tree;

    cur_ctb_cu_tree_t *ps_cu_tree_root;

    WORD32 ai4_8x8_act_factor[16];
    WORD32 ai4_best_sad_8x8_l1_me[MAX_CU_IN_CTB];
    WORD32 ai4_best_sad_8x8_l1_ipe[MAX_CU_IN_CTB];
    WORD32 ai4_best_sad_cost_8x8_l1_me[MAX_CU_IN_CTB];
    WORD32 ai4_best_sad_cost_8x8_l1_ipe[MAX_CU_IN_CTB];

    /*Ctb level accumalated satd*/
    WORD32 i4_ctb_acc_satd;

    /*Ctb level accumalated mpm bits*/
    WORD32 i4_ctb_acc_mpm_bits;

} ipe_l0_ctb_analyse_for_me_t;

typedef struct
{
    WORD16 i2_mv_x;
    WORD16 i2_mv_y;
} global_mv_t;

/**
******************************************************************************
 *  @brief  Pre Encode pass and ME pass shared variables and buffers
******************************************************************************
 */
typedef struct
{
    /**
     * Buffer id
     */
    WORD32 i4_buf_id;

    /**
    * Flag will be set to 1 by frame processing thread after receiving flush
    * command from application
    */
    WORD32 i4_end_flag;

    /** frame leve ctb analyse  buffer pointer */
    ctb_analyse_t *ps_ctb_analyse;

    /** frame level cu analyse  buffer pointer for IPE */
    //cu_analyse_t       *ps_cu_analyse;

    /** current input pointer */
    ihevce_lap_enc_buf_t *ps_curr_inp;

    /** current inp buffer id */
    WORD32 curr_inp_buf_id;

    /** Slice header parameters   */
    slice_header_t s_slice_hdr;

    /** sps parameters activated by current slice  */
    sps_t *ps_sps;

    /** pps parameters activated by current slice  */
    pps_t *ps_pps;

    /** vps parameters activated by current slice  */
    vps_t *ps_vps;
    /**  Pointer to Penultilate Layer context memory internally has MV bank buff and related params */
    void *pv_me_lyr_ctxt;

    /**  Pointer to Penultilate Layer  NV bank context memory */
    void *pv_me_lyr_bnk_ctxt;

    /**  Pointer to Penultilate Layer MV bank buff */
    void *pv_me_mv_bank;

    /**  Pointer to Penultilate Layer reference idx buffer */
    void *pv_me_ref_idx;
    /**
     * Array to store 8x8 cost (partial 8x8 sad + level adjusted cost)
     * The order of storing is raster scan order within CTB and
     * CTB order is raster scan within frame.
     */
    double *plf_intra_8x8_cost;

    /**
     * L0 layer ctb anaylse frame level buffer.
     * IPE wil populate the cost and best modes at all levels in this buffer
     *  for every CTB in a frame
     */
    // moved to shorter buffer queue
    //ipe_l0_ctb_analyse_for_me_t *ps_ipe_analyse_ctb;

    /** Layer L1 buffer pointer */
    ihevce_ed_blk_t *ps_layer1_buf;

    /** Layer L2 buffer pointer */
    ihevce_ed_blk_t *ps_layer2_buf;

    /*ME reverse map info*/
    UWORD8 *pu1_me_reverse_map_info;

    /** Buffer pointer for CTB level information in pre intra pass*/
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1;

    /** vps parameters activated by current slice  */
    sei_params_t s_sei;

    /** nal_type for the slice to be encoded  */
    WORD32 i4_slice_nal_type;

    /** input time stamp in terms of ticks: lower 32  */
    WORD32 i4_inp_timestamp_low;

    /** input time stamp in terms of ticks: higher 32 */
    WORD32 i4_inp_timestamp_high;

    /** input frame ctxt of app to be retured in output buffer */
    void *pv_app_frm_ctxt;

    /** current frm valid flag :
     * will be 1 if valid input was processed by frame proc thrd
     */
    WORD32 i4_frm_proc_valid_flag;

    /**
     * Qp to be used for current frame
     */
    WORD32 i4_curr_frm_qp;

    /**
     * Frame level Lambda parameters
     */
    frm_lambda_ctxt_t as_lambda_prms[IHEVCE_MAX_NUM_BITRATES];

    /** Frame-levelSATDcost accumalator */
    LWORD64 i8_frame_acc_satd_cost;

    /** Frame - L1 coarse me cost accumulated */
    LWORD64 i8_acc_frame_coarse_me_cost;
    /** Frame - L1 coarse me cost accumulated */
    //LWORD64 i8_acc_frame_coarse_me_cost_for_ref;

    /** Frame - L1 coarse me sad accumulated */
    LWORD64 i8_acc_frame_coarse_me_sad;

    /* Averge activity of 4x4 blocks from previous frame
    *  If L1, maps to 8*8 in L0
    */
    WORD32 i4_curr_frame_4x4_avg_act;

    WORD32 ai4_mod_factor_derived_by_variance[2];

    float f_strength;

    /* Averge activity of 8x8 blocks from previous frame
    *  If L1, maps to 16*16 in L0
    */

    long double ld_curr_frame_8x8_log_avg[2];

    LWORD64 i8_curr_frame_8x8_avg_act[2];

    LWORD64 i8_curr_frame_8x8_sum_act[2];

    WORD32 i4_curr_frame_8x8_sum_act_for_strength[2];

    ULWORD64 u8_curr_frame_8x8_sum_act_sqr;

    WORD32 i4_curr_frame_8x8_num_blks[2];

    LWORD64 i8_acc_frame_8x8_sum_act[2];
    LWORD64 i8_acc_frame_8x8_sum_act_sqr;
    WORD32 i4_acc_frame_8x8_num_blks[2];
    LWORD64 i8_acc_frame_8x8_sum_act_for_strength;
    LWORD64 i8_curr_frame_8x8_sum_act_for_strength;

    /* Averge activity of 16x16 blocks from previous frame
    *  If L1, maps to 32*32 in L0
    */

    long double ld_curr_frame_16x16_log_avg[3];

    LWORD64 i8_curr_frame_16x16_avg_act[3];

    LWORD64 i8_curr_frame_16x16_sum_act[3];

    WORD32 i4_curr_frame_16x16_num_blks[3];

    LWORD64 i8_acc_frame_16x16_sum_act[3];
    WORD32 i4_acc_frame_16x16_num_blks[3];

    /* Averge activity of 32x32 blocks from previous frame
    *  If L1, maps to 64*64 in L0
    */

    long double ld_curr_frame_32x32_log_avg[3];

    LWORD64 i8_curr_frame_32x32_avg_act[3];

    global_mv_t s_global_mv[MAX_NUM_REF];
    LWORD64 i8_curr_frame_32x32_sum_act[3];

    WORD32 i4_curr_frame_32x32_num_blks[3];

    LWORD64 i8_acc_frame_32x32_sum_act[3];
    WORD32 i4_acc_frame_32x32_num_blks[3];

    LWORD64 i8_acc_num_blks_high_sad;

    LWORD64 i8_total_blks;

    WORD32 i4_complexity_percentage;

    WORD32 i4_is_high_complex_region;

    WORD32 i4_avg_noise_thrshld_4x4;

    LWORD64 i8_curr_frame_mean_sum;
    WORD32 i4_curr_frame_mean_num_blks;
    LWORD64 i8_curr_frame_avg_mean_act;

} pre_enc_me_ctxt_t;

/**
******************************************************************************
 *  @brief  buffers from L0 IPE to ME and enc loop
******************************************************************************
 */
typedef struct
{
    WORD32 i4_size;

    ipe_l0_ctb_analyse_for_me_t *ps_ipe_analyse_ctb;
} pre_enc_L0_ipe_encloop_ctxt_t;
/**
******************************************************************************
 *  @brief  Frame process and Entropy coding pass shared variables and buffers
******************************************************************************
 */

typedef struct
{
    /*PIC level Info*/
    ULWORD64 i8_total_cu;
    ULWORD64 i8_total_cu_min_8x8;
    ULWORD64 i8_total_pu;
    ULWORD64 i8_total_intra_cu;
    ULWORD64 i8_total_inter_cu;
    ULWORD64 i8_total_skip_cu;
    ULWORD64 i8_total_cu_based_on_size[4];

    ULWORD64 i8_total_intra_pu;
    ULWORD64 i8_total_merge_pu;
    ULWORD64 i8_total_non_skipped_inter_pu;

    ULWORD64 i8_total_2nx2n_intra_pu[4];
    ULWORD64 i8_total_nxn_intra_pu;
    ULWORD64 i8_total_2nx2n_inter_pu[4];
    ULWORD64 i8_total_smp_inter_pu[4];
    ULWORD64 i8_total_amp_inter_pu[3];
    ULWORD64 i8_total_nxn_inter_pu[3];

    ULWORD64 i8_total_L0_mode;
    ULWORD64 i8_total_L1_mode;
    ULWORD64 i8_total_BI_mode;

    ULWORD64 i8_total_L0_ref_idx[MAX_DPB_SIZE];
    ULWORD64 i8_total_L1_ref_idx[MAX_DPB_SIZE];

    ULWORD64 i8_total_tu;
    ULWORD64 i8_total_non_coded_tu;
    ULWORD64 i8_total_inter_coded_tu;
    ULWORD64 i8_total_intra_coded_tu;

    ULWORD64 i8_total_tu_based_on_size[4];
    ULWORD64 i8_total_tu_cu64[4];
    ULWORD64 i8_total_tu_cu32[4];
    ULWORD64 i8_total_tu_cu16[3];
    ULWORD64 i8_total_tu_cu8[2];

    LWORD64 i8_total_qp;
    LWORD64 i8_total_qp_min_cu;
    WORD32 i4_min_qp;
    WORD32 i4_max_qp;
    LWORD64 i8_sum_squared_frame_qp;
    LWORD64 i8_total_frame_qp;
    WORD32 i4_max_frame_qp;
    float f_total_buffer_underflow;
    float f_total_buffer_overflow;
    float f_max_buffer_underflow;
    float f_max_buffer_overflow;

    UWORD8 i1_num_ref_idx_l0_active;
    UWORD8 i1_num_ref_idx_l1_active;

    WORD32 i4_ref_poc_l0[MAX_DPB_SIZE];
    WORD32 i4_ref_poc_l1[MAX_DPB_SIZE];

    WORD8 i1_list_entry_l0[MAX_DPB_SIZE];
    DOUBLE i2_luma_weight_l0[MAX_DPB_SIZE];
    WORD16 i2_luma_offset_l0[MAX_DPB_SIZE];
    WORD8 i1_list_entry_l1[MAX_DPB_SIZE];
    DOUBLE i2_luma_weight_l1[MAX_DPB_SIZE];
    WORD16 i2_luma_offset_l1[MAX_DPB_SIZE];

    ULWORD64 u8_bits_estimated_intra;
    ULWORD64 u8_bits_estimated_inter;
    ULWORD64 u8_bits_estimated_slice_header;
    ULWORD64 u8_bits_estimated_sao;
    ULWORD64 u8_bits_estimated_split_cu_flag;
    ULWORD64 u8_bits_estimated_cu_hdr_bits;
    ULWORD64 u8_bits_estimated_split_tu_flag;
    ULWORD64 u8_bits_estimated_qp_delta_bits;
    ULWORD64 u8_bits_estimated_cbf_luma_bits;
    ULWORD64 u8_bits_estimated_cbf_chroma_bits;

    ULWORD64 u8_bits_estimated_res_luma_bits;
    ULWORD64 u8_bits_estimated_res_chroma_bits;

    ULWORD64 u8_bits_estimated_ref_id;
    ULWORD64 u8_bits_estimated_mvd;
    ULWORD64 u8_bits_estimated_merge_flag;
    ULWORD64 u8_bits_estimated_mpm_luma;
    ULWORD64 u8_bits_estimated_mpm_chroma;

    ULWORD64 u8_total_bits_generated;
    ULWORD64 u8_total_bits_vbv;

    ULWORD64 u8_total_I_bits_generated;
    ULWORD64 u8_total_P_bits_generated;
    ULWORD64 u8_total_B_bits_generated;

    UWORD32 u4_frame_sad;
    UWORD32 u4_frame_intra_sad;
    UWORD32 u4_frame_inter_sad;

    ULWORD64 i8_frame_cost;
    ULWORD64 i8_frame_intra_cost;
    ULWORD64 i8_frame_inter_cost;
} s_pic_level_acc_info_t;

typedef struct
{
    UWORD32 u4_target_bit_rate_sei_entropy;
    UWORD32 u4_buffer_size_sei_entropy;
    UWORD32 u4_dbf_entropy;

} s_pic_level_sei_info_t;
/**
******************************************************************************
*  @brief  ME pass and Main enocde pass shared variables and buffers
******************************************************************************
*/
typedef struct
{
    /**
    * Buffer id
    */
    WORD32 i4_buf_id;

    /**
    * Flag will be set to 1 by frame processing thread after receiving flush
    * command from application
    */
    WORD32 i4_end_flag;

    /** current input pointer */
    ihevce_lap_enc_buf_t *ps_curr_inp;

    /** current inp buffer id */
    WORD32 curr_inp_buf_id;

    /** current input buffers from ME */
    pre_enc_me_ctxt_t *ps_curr_inp_from_me_prms;

    /** current inp buffer id from ME */
    WORD32 curr_inp_from_me_buf_id;

    /** current input buffers from L0 IPE */
    pre_enc_L0_ipe_encloop_ctxt_t *ps_curr_inp_from_l0_ipe_prms;

    /** current inp buffer id from L0 IPE */
    WORD32 curr_inp_from_l0_ipe_buf_id;

    /** Slice header parameters   */
    slice_header_t s_slice_hdr;

    /** current frm valid flag :
     * will be 1 if valid input was processed by frame proc thrd
     */
    WORD32 i4_frm_proc_valid_flag;

    /**
     * Array of reference picture list for ping instance
     * 2=> ref_pic_list0 and ref_pic_list1
     */
    recon_pic_buf_t as_ref_list[IHEVCE_MAX_NUM_BITRATES][2][HEVCE_MAX_REF_PICS * 2];

    /**
     * Array of reference picture list
     * 2=> ref_pic_list0 and ref_pic_list1
     */
    recon_pic_buf_t *aps_ref_list[IHEVCE_MAX_NUM_BITRATES][2][HEVCE_MAX_REF_PICS * 2];

    /**  Job Queue Memory encode */
    job_queue_t *ps_job_q_enc;

    /** Array of Job Queue handles of enc group for ping and pong instance*/
    job_queue_handle_t as_job_que_enc_hdls[NUM_ENC_JOBS_QUES];

    /** Array of Job Queue handles of enc group for re-encode*/
    job_queue_handle_t as_job_que_enc_hdls_reenc[NUM_ENC_JOBS_QUES];
    /** frame level me_ctb_data_t buffer pointer
      */
    me_ctb_data_t *ps_cur_ctb_me_data;

    /** frame level cur_ctb_cu_tree_t buffer pointer for ME
      */
    cur_ctb_cu_tree_t *ps_cur_ctb_cu_tree;

    /** Pointer to Dep. Mngr for CTBs processed in every row of a frame.
     * ME is producer, EncLoop is the consumer
     */
    void *pv_dep_mngr_encloop_dep_me;

} me_enc_rdopt_ctxt_t;

typedef struct
{
    UWORD32 u4_payload_type;
    UWORD32 u4_payload_length;
    UWORD8 *pu1_sei_payload;
} sei_payload_t;

typedef struct
{
    /**
    * Flag will be set to 1 by frame processing thread after receiving flush
    * command from application
    */
    WORD32 i4_end_flag;

    /** frame level ctb allocation for ctb after aligning to max cu size */
    ctb_enc_loop_out_t *ps_frm_ctb_data;

    /** frame level cu allocation for ctb after aligning to max cu size  */
    cu_enc_loop_out_t *ps_frm_cu_data;

    /** frame level tu allocation for ctb after aligning to max cu size  */
    tu_enc_loop_out_t *ps_frm_tu_data;

    /** frame level pu allocation for ctb after aligning to max cu size  */
    pu_t *ps_frm_pu_data;

    /**  frame level coeff allocation for ctb after aligning to max cu size */
    void *pv_coeff_data;

    /** Slice header parameters   */
    slice_header_t s_slice_hdr;

    /** sps parameters activated by current slice  */
    sps_t *ps_sps;

    /** pps parameters activated by current slice  */
    pps_t *ps_pps;

    /** vps parameters activated by current slice  */
    vps_t *ps_vps;

    /** vps parameters activated by current slice  */
    sei_params_t s_sei;

    /* Flag to indicate if AUD NAL is present */
    WORD8 i1_aud_present_flag;

    /* Flag to indicate if EOS NAL is present */
    WORD8 i1_eos_present_flag;

    /** nal_type for the slice to be encoded  */
    WORD32 i4_slice_nal_type;

    /** input time stamp in terms of ticks: lower 32  */
    WORD32 i4_inp_timestamp_low;

    /** input time stamp in terms of ticks: higher 32 */
    WORD32 i4_inp_timestamp_high;

    /** input frame ctxt of app to be retured in output buffer */
    void *pv_app_frm_ctxt;

    /** current frm valid flag :
     * will be 1 if valid input was processed by frame proc thrd
     */
    WORD32 i4_frm_proc_valid_flag;

    /** To support entropy sync the bitstream offset of each CTB row
     * is populated in this array any put in slice header in the end
     */
    WORD32 ai4_entry_point_offset[MAX_NUM_CTB_ROWS_FRM];

    /** RDopt estimation of bytes generated based on which rc update happens
     *
     */
    WORD32 i4_rdopt_bits_generated_estimate;

    /* These params are passed from enc-threads to entropy thread for
        passing params needed for PSNR caclulation and encoding
        summary prints */
    DOUBLE lf_luma_mse;
    DOUBLE lf_cb_mse;
    DOUBLE lf_cr_mse;

    DOUBLE lf_luma_ssim;
    DOUBLE lf_cb_ssim;
    DOUBLE lf_cr_ssim;

    WORD32 i4_qp;
    WORD32 i4_poc;
    WORD32 i4_display_num;
    WORD32 i4_pic_type;

    /** I-only SCD */
    WORD32 i4_is_I_scenecut;

    WORD32 i4_is_non_I_scenecut;
    WORD32 i4_sub_pic_level_rc;

    WORD32 ai4_frame_bits_estimated;
    s_pic_level_acc_info_t s_pic_level_info;

    LWORD64 i8_buf_level_bitrate_change;

    WORD32 i4_is_end_of_idr_gop;

    sei_payload_t as_sei_payload[MAX_NUMBER_OF_SEI_PAYLOAD];

    UWORD32 u4_num_sei_payload;
    /* Flag used only in mres single output case to flush out one res and start with next */
    WORD32 i4_out_flush_flag;

} frm_proc_ent_cod_ctxt_t;

/**
******************************************************************************
*  @brief  ME pass and Main enocde pass shared variables and buffers
******************************************************************************
*/
typedef struct
{
    /*BitRate ID*/
    WORD32 i4_br_id;

    /*Frame ID*/
    WORD32 i4_frm_id;

    /*Number of CTB, after ich data is populated*/
    WORD32 i4_ctb_count_in_data;

    /*Number of CTB, after ich scale is computed*/
    WORD32 i4_ctb_count_out_scale;

    /*Bits estimated for the frame */
    /* For NON-I SCD max buf bits*/
    LWORD64 i8_frame_bits_estimated;

    /* Bits consumed till the nctb*/
    LWORD64 i8_nctb_bits_consumed;

    /* Bits consumed till the nctb*/
    LWORD64 i8_acc_bits_consumed;

    /*Frame level Best of Ipe and ME sad*/
    LWORD64 i8_frame_l1_me_sad;

    /*SAD accumalted till NCTB*/
    LWORD64 i8_nctb_l1_me_sad;

    /*Frame level IPE sad*/
    LWORD64 i8_frame_l1_ipe_sad;

    /*SAD accumalted till NCTB*/
    LWORD64 i8_nctb_l1_ipe_sad;

    /*Frame level L0 IPE satd*/
    LWORD64 i8_frame_l0_ipe_satd;

    /*L0 SATD accumalted till NCTB*/
    LWORD64 i8_nctb_l0_ipe_satd;

    /*Frame level Activity factor acc at 8x8 level */
    LWORD64 i8_frame_l1_activity_fact;

    /*NCTB Activity factor acc at 8x8 level */
    LWORD64 i8_nctb_l1_activity_fact;

    /*L0 MPM bits accumalted till NCTB*/
    LWORD64 i8_nctb_l0_mpm_bits;

    /*Encoder hdr accumalted till NCTB*/
    LWORD64 i8_nctb_hdr_bits_consumed;

} ihevce_sub_pic_rc_ctxt_t;

/**
******************************************************************************
 *  @brief  Memoery manager context (stores the memory tables allcoated)
******************************************************************************
 */
typedef struct
{
    /**
    * Total number of memtabs (Modules and system)
    * during create time
    */
    WORD32 i4_num_create_memtabs;

    /**
    * Pointer to the mem tabs
    * of crate time
    */
    iv_mem_rec_t *ps_create_memtab;

    /**
    * Total number of memtabs Data and control Ques
    * during Ques create time
    */
    WORD32 i4_num_q_memtabs;

    /**
    * Pointer to the mem tabs
    * of crate time
    */
    iv_mem_rec_t *ps_q_memtab;

} enc_mem_mngr_ctxt;

/**
******************************************************************************
 *  @brief  Encoder Interafce Queues Context
******************************************************************************
 */
typedef struct
{
    /** Number of Queues at interface context level */
    WORD32 i4_num_queues;

    /** Array of Queues handle */
    void *apv_q_hdl[IHEVCE_MAX_NUM_QUEUES];

    /** Mutex for encuring thread safety of the access of the queues */
    void *pv_q_mutex_hdl;

} enc_q_ctxt_t;

/**
******************************************************************************
 *  @brief  Module context of different modules in encoder
******************************************************************************
 */

typedef struct
{
    /** Motion estimation context pointer */
    void *pv_me_ctxt;
    /** Coarse Motion estimation context pointer */
    void *pv_coarse_me_ctxt;

    /** Intra Prediction context pointer */
    void *pv_ipe_ctxt;

    /** Encode Loop context pointer */
    void *pv_enc_loop_ctxt;

    /** Entropy Coding context pointer */
    void *apv_ent_cod_ctxt[IHEVCE_MAX_NUM_BITRATES];

    /** Look Ahead Processing context pointer */
    void *pv_lap_ctxt;
    /** Rate control context pointer */
    void *apv_rc_ctxt[IHEVCE_MAX_NUM_BITRATES];
    /** Decomposition pre intra context pointer */
    void *pv_decomp_pre_intra_ctxt;

} module_ctxt_t;

/**
******************************************************************************
 *  @brief  Threads semaphore handles
******************************************************************************
 */
typedef struct
{
    /** LAP semaphore handle */
    void *pv_lap_sem_handle;

    /** Encode frame Process semaphore handle */
    void *pv_enc_frm_proc_sem_handle;

    /** Pre Encode frame Process semaphore handle */
    void *pv_pre_enc_frm_proc_sem_handle;

    /** Entropy coding semaphore handle
        One semaphore for each entropy thread, i.e. for each bit-rate instance*/
    void *apv_ent_cod_sem_handle[IHEVCE_MAX_NUM_BITRATES];

    /**
     *  Semaphore handle corresponding to get free inp frame buff
     *  function call from app if called in blocking mode
     */
    void *pv_inp_data_sem_handle;

    /**
     *  Semaphore handle corresponding to get free inp control command buff
     *  function call from app if called in blocking mode
     */
    void *pv_inp_ctrl_sem_handle;

    /**
     *  Semaphore handle corresponding to get filled out bitstream buff
     *  function call from app if called in blocking mode
     */
    void *apv_out_strm_sem_handle[IHEVCE_MAX_NUM_BITRATES];

    /**
     *  Semaphore handle corresponding to get filled out recon buff
     *  function call from app if called in blocking mode
     */
    void *apv_out_recon_sem_handle[IHEVCE_MAX_NUM_BITRATES];

    /**
     *  Semaphore handle corresponding to get filled out control status buff
     *  function call from app if called in blocking mode
     */
    void *pv_out_ctrl_sem_handle;

    /**
     *  Semaphore handle corresponding to get filled out control status buff
     *  function call from app if called in blocking mode
     */
    void *pv_lap_inp_data_sem_hdl;

    /**
     *  Semaphore handle corresponding to get filled out control status buff
     *  function call from app if called in blocking mode
     */
    void *pv_preenc_inp_data_sem_hdl;

    /**
     *  Semaphore handle corresponding to Multi Res Single output case
     */
    void *pv_ent_common_mres_sem_hdl;
    void *pv_out_common_mres_sem_hdl;

} thrd_que_sem_hdl_t;

/**
******************************************************************************
 *  @brief  Frame level structure which has parameters about CTBs
******************************************************************************
 */
typedef struct
{
    /** CTB size of all CTB in a frame in pixels
     *  this will be create time value,
     *  run time change in this value is not supported
     */
    WORD32 i4_ctb_size;

    /** Minimum CU size of CTB in a frame in pixels
     *  this will be create time value,
     *  run time change in this value is not supported
     */
    WORD32 i4_min_cu_size;

    /** Worst case num CUs in CTB based on i4_ctb_size */
    WORD32 i4_num_cus_in_ctb;

    /** Worst case num PUs in CTB based on i4_ctb_size */
    WORD32 i4_num_pus_in_ctb;

    /** Worst case num TUs in CTB based on i4_ctb_size */
    WORD32 i4_num_tus_in_ctb;

    /** Number of CTBs in horizontal direction
      * this is based on run time source width and i4_ctb_size
      */
    WORD32 i4_num_ctbs_horz;

    /** Number of CTBs in vertical direction
     *  this is based on run time source height and i4_ctb_size
     */
    WORD32 i4_num_ctbs_vert;

    /** MAX CUs in horizontal direction
     * this is based on run time source width, i4_ctb_size and  i4_num_cus_in_ctb
     */
    WORD32 i4_max_cus_in_row;

    /** MAX PUs in horizontal direction
     * this is based on run time source width, i4_ctb_size and  i4_num_pus_in_ctb
     */
    WORD32 i4_max_pus_in_row;

    /** MAX TUs in horizontal direction
     * this is based on run time source width, i4_ctb_size and  i4_num_tus_in_ctb
     */
    WORD32 i4_max_tus_in_row;

    /**
     * CU aligned picture width (currently aligned to MAX CU size)
     * should be modified to be aligned to MIN CU size
     */

    WORD32 i4_cu_aligned_pic_wd;

    /**
     * CU aligned picture height (currently aligned to MAX CU size)
     * should be modified to be aligned to MIN CU size
     */

    WORD32 i4_cu_aligned_pic_ht;

    /* Pointer to a frame level memory,
    Stride is = 1 + (num ctbs in a ctb-row) + 1
    Hieght is = 1 + (num ctbs in a ctb-col)
    Contains tile-id of each ctb */
    WORD32 *pi4_tile_id_map;

    /* stride in units of ctb */
    WORD32 i4_tile_id_ctb_map_stride;

} frm_ctb_ctxt_t;

/**
******************************************************************************
 *  @brief  ME Job Queue desc
******************************************************************************
 */
typedef struct
{
    /** Number of output dependencies which need to be set after
     *  current job is complete,
     *  should be less than or equal to MAX_OUT_DEP defined in
     *  ihevce_multi_thrd_structs.h
     */
    WORD32 i4_num_output_dep;

    /** Array of offsets from the start of output dependent layer's Job Ques
     *  which are dependent on current Job to be complete
     */
    WORD32 ai4_out_dep_unit_off[MAX_OUT_DEP];

    /** Number of input dependencies to be resolved for current job to start
     *  these many jobs in lower layer should be complete to
     *  start the current JOB
     */
    WORD32 i4_num_inp_dep;

} multi_thrd_me_job_q_prms_t;

/**
 *  @brief  structure in which recon data
 *          and related parameters are sent from Encoder
 */
typedef struct
{
    /** Kept for maintaining backwards compatibility in future */
    WORD32 i4_size;

    /** Buffer id for the current buffer */
    WORD32 i4_buf_id;

    /** POC of the current buffer */
    WORD32 i4_poc;

    /** End flag to communicate this is last frame output from encoder */
    WORD32 i4_end_flag;

    /** End flag to communicate encoder that this is the last buffer from application
        1 - Last buf, 0 - Not last buffer. No other values are supported.
        Application has to set the appropriate value before queing in encoder queue */

    WORD32 i4_is_last_buf;

    /** Recon luma buffer pointer */
    void *pv_y_buf;

    /** Recon cb buffer pointer */
    void *pv_cb_buf;

    /** Recon cr buffer pointer */
    void *pv_cr_buf;

    /** Luma size **/
    WORD32 i4_y_pixels;

    /** Chroma size **/
    WORD32 i4_uv_pixels;

} iv_enc_recon_data_buffs_t;

/**
******************************************************************************
 *  @brief  Multi Thread context structure
******************************************************************************
 */
typedef struct
{
    /* Flag to indicate to enc and pre-enc thrds that app has sent force end cmd*/
    WORD32 i4_force_end_flag;

    /** Force all active threads flag
      * This flag will be set to 1 if all Number of cores givento the encoder
      * is less than or Equal to MAX_NUM_CORES_SEQ_EXEC. In this mode
      * All pre enc threads and enc threads will run of the same cores with
      * time sharing ar frame level
      */
    WORD32 i4_all_thrds_active_flag;

    /** Flag to indicate that core manager has been configured to enable
     * sequential execution
     */
    WORD32 i4_seq_mode_enabled_flag;
    /*-----------------------------------------------------------------------*/
    /*--------- Params related to encode group  -----------------------------*/
    /*-----------------------------------------------------------------------*/

    /** Number of processing threads created runtime in encode group */
    WORD32 i4_num_enc_proc_thrds;

    /** Number of processing threads active for a given frame
     * This value will be monitored at frame level, so as to
     * have provsion for increasing / decreasing threads
     * based on Load balance b/w stage in encoder
     */
    WORD32 i4_num_active_enc_thrds;

    /** Mutex for ensuring thread safety of the access of Job queues in encode group */
    void *pv_job_q_mutex_hdl_enc_grp_me;

    /** Mutex for ensuring thread safety of the access of Job queues in encode group */
    void *pv_job_q_mutex_hdl_enc_grp_enc_loop;

    /** Array of Semaphore handles (for each frame processing threads ) */
    void *apv_enc_thrd_sem_handle[MAX_NUM_FRM_PROC_THRDS_ENC];

    /** Array for ME to export the Job que dependency for all layers */
    multi_thrd_me_job_q_prms_t as_me_job_q_prms[MAX_NUM_HME_LAYERS][MAX_NUM_VERT_UNITS_FRM];

    /* pointer to the mutex handle*/
    void *apv_mutex_handle[MAX_NUM_ME_PARALLEL];

    /* pointer to the mutex handle for frame init*/
    void *apv_mutex_handle_me_end[MAX_NUM_ME_PARALLEL];

    /* pointer to the mutex handle for frame init*/
    void *apv_mutex_handle_frame_init[MAX_NUM_ENC_LOOP_PARALLEL];

    /*pointer to the mutex handle*/
    void *apv_post_enc_mutex_handle[MAX_NUM_ENC_LOOP_PARALLEL];

    /* Flag to indicate that master has done ME init*/
    WORD32 ai4_me_master_done_flag[MAX_NUM_ME_PARALLEL];

    /* Counter to keep track of me num of thrds exiting critical section*/
    WORD32 me_num_thrds_exited[MAX_NUM_ME_PARALLEL];

    /* Flag to indicate that master has done the frame init*/
    WORD32 enc_master_done_frame_init[MAX_NUM_ENC_LOOP_PARALLEL];

    /* Counter to keep track of num of thrds exiting critical section*/
    WORD32 num_thrds_exited[MAX_NUM_ENC_LOOP_PARALLEL];

    /* Counter to keep track of num of thrds exiting critical section for re-encode*/
    WORD32 num_thrds_exited_for_reenc;

    /* Array to store the curr qp for ping and pong instance*/
    WORD32 cur_qp[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /* Pointers to store output buffers for ping and pong instance*/
    frm_proc_ent_cod_ctxt_t *ps_curr_out_enc_grp[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /* Pointer to store input buffers for me*/
    pre_enc_me_ctxt_t *aps_cur_inp_me_prms[MAX_NUM_ME_PARALLEL];

    /*pointers to store output buffers from me */
    me_enc_rdopt_ctxt_t *aps_cur_out_me_prms[NUM_ME_ENC_BUFS];

    /*pointers to store input buffers to enc-rdopt */
    me_enc_rdopt_ctxt_t *aps_cur_inp_enc_prms[NUM_ME_ENC_BUFS];

    /*Shared memory for Sub Pic rc */
    /*Qscale calulated by sub pic rc bit control for Intra Pic*/
    WORD32 ai4_curr_qp_estimated[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /*Header bits error by sub pic rc bit control*/
    float af_acc_hdr_bits_scale_err[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /*Accumalated ME SAD for NCTB*/
    LWORD64 ai8_nctb_me_sad[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES]
                           [MAX_NUM_FRM_PROC_THRDS_ENC];

    /*Accumalated IPE SAD for NCTB*/
    LWORD64 ai8_nctb_ipe_sad[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES]
                            [MAX_NUM_FRM_PROC_THRDS_ENC];

    /*Accumalated L0 IPE SAD for NCTB*/
    LWORD64 ai8_nctb_l0_ipe_sad[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES]
                               [MAX_NUM_FRM_PROC_THRDS_ENC];

    /*Accumalated Activity Factor for NCTB*/
    LWORD64 ai8_nctb_act_factor[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES]
                               [MAX_NUM_FRM_PROC_THRDS_ENC];

    /*Accumalated Ctb counter across all threads*/
    WORD32 ai4_ctb_ctr[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /*Bits threshold reached for across all threads*/
    WORD32 ai4_threshold_reached[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /*To hold the Previous In-frame RC chunk QP*/
    WORD32 ai4_prev_chunk_qp[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /*Accumalated Ctb counter across all threads*/
    WORD32 ai4_acc_ctb_ctr[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /*Flag to check if thread is initialized */
    WORD32 ai4_thrd_id_valid_flag[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES]
                                 [MAX_NUM_FRM_PROC_THRDS_ENC];

    /*Accumalated Ctb counter across all threads*/
    //WORD32 ai4_acc_qp[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES][MAX_NUM_FRM_PROC_THRDS_ENC];

    /*Accumalated bits consumed for nctbs across all threads*/
    LWORD64 ai8_nctb_bits_consumed[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES]
                                  [MAX_NUM_FRM_PROC_THRDS_ENC];

    /*Accumalated hdr bits consumed for nctbs across all threads*/
    LWORD64 ai8_nctb_hdr_bits_consumed[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES]
                                      [MAX_NUM_FRM_PROC_THRDS_ENC];

    /*Accumalated l0 mpm bits consumed for nctbs across all threads*/
    LWORD64 ai8_nctb_mpm_bits_consumed[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES]
                                      [MAX_NUM_FRM_PROC_THRDS_ENC];

    /*Accumalated bits consumed for total ctbs across all threads*/
    LWORD64 ai8_acc_bits_consumed[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES]
                                 [MAX_NUM_FRM_PROC_THRDS_ENC];

    /*Accumalated bits consumed for total ctbs across all threads*/
    LWORD64 ai8_acc_bits_mul_qs_consumed[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES]
                                        [MAX_NUM_FRM_PROC_THRDS_ENC];

    /*Qscale calulated by sub pic rc bit control */
    WORD32 ai4_curr_qp_acc[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];
    /* End of Sub pic rc variables */

    /* Pointers to store input (only L0 IPE)*/
    pre_enc_L0_ipe_encloop_ctxt_t *aps_cur_L0_ipe_inp_prms[MAX_NUM_ME_PARALLEL];

    /* Array tp store L0 IPE input buf ids*/
    WORD32 ai4_in_frm_l0_ipe_id[MAX_NUM_ME_PARALLEL];

    /* Array to store output buffer ids for ping and pong instances*/
    WORD32 out_buf_id[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /* Array of pointers to store the recon buf pointers*/
    iv_enc_recon_data_buffs_t *ps_recon_out[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /* Array of pointers to frame recon for ping and pong instances*/
    recon_pic_buf_t *ps_frm_recon[NUM_ME_ENC_BUFS][IHEVCE_MAX_NUM_BITRATES];

    /* Array of recon buffer ids for ping and pong instance*/
    WORD32 recon_buf_id[NUM_ME_ENC_BUFS][IHEVCE_MAX_NUM_BITRATES];

    /* Counter to keep track of num thrds done*/
    WORD32 num_thrds_done;

    /* Flags to keep track of dumped ping pong recon buffer*/
    WORD32 is_recon_dumped[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /* Flags to keep track of dumped ping pong output buffer*/
    WORD32 is_out_buf_freed[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /* flag to produce output buffer by the thread who ever is finishing
    enc-loop processing first, so that the entropy thread can start processing */
    WORD32 ai4_produce_outbuf[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];

    /* Flags to keep track of dumped ping pong input buffer*/
    WORD32 is_in_buf_freed[MAX_NUM_ENC_LOOP_PARALLEL];

    /* Flags to keep track of dumped ping pong L0 IPE to enc buffer*/
    WORD32 is_L0_ipe_in_buf_freed[MAX_NUM_ENC_LOOP_PARALLEL];

    /** Dependency manager for checking whether prev. EncLoop done before
        current frame EncLoop starts */
    void *apv_dep_mngr_prev_frame_done[MAX_NUM_ENC_LOOP_PARALLEL];

    /** Dependency manager for checking whether prev. EncLoop done before
        re-encode of the current frame */
    void *pv_dep_mngr_prev_frame_enc_done_for_reenc;

    /** Dependency manager for checking whether prev. me done before
        current frame me starts */
    void *apv_dep_mngr_prev_frame_me_done[MAX_NUM_ME_PARALLEL];

    /** ME coarsest layer JOB queue type */
    WORD32 i4_me_coarsest_lyr_type;

    /** number of encloop frames running in parallel */
    WORD32 i4_num_enc_loop_frm_pllel;

    /** number of me frames running in parallel */
    WORD32 i4_num_me_frm_pllel;

    /*-----------------------------------------------------------------------*/
    /*--------- Params related to pre-enc stage -----------------------------*/
    /*-----------------------------------------------------------------------*/

    /** Number of processing threads created runtime in pre encode group */
    WORD32 i4_num_pre_enc_proc_thrds;

    /** Number of processing threads active for a given frame
     * This value will be monitored at frame level, so as to
     * have provsion for increasing / decreasing threads
     * based on Load balance b/w stage in encoder
     */
    WORD32 i4_num_active_pre_enc_thrds;
    /** number of threads that have done processing the current frame
        Use to find out the last thread that is coming out of pre-enc processing
        so that the last thread can do de-init of pre-enc stage */
    WORD32 ai4_num_thrds_processed_pre_enc[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /** number of threads that have done processing the current frame
        Use to find out the first thread and last inoder to get qp query. As the query
        is not read only , the quer should be done only once by thread that comes first
        and other threads should get same value*/
    WORD32 ai4_num_thrds_processed_L0_ipe_qp_init[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /** number of threads that have done proessing decomp_intra
        Used to find out the last thread that is coming out so that
        the last thread can set flag for decomp_pre_intra_finish */
    WORD32 ai4_num_thrds_processed_decomp[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /** number of threads that have done proessing coarse_me
        Used to find out the last thread that is coming out so that
        the last thread can set flag for coarse_me_finish */
    WORD32 ai4_num_thrds_processed_coarse_me[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /*Flag to indicate if current instance (frame)'s Decomp_pre_intra and Coarse_ME is done.
      Used to check if previous frame is done proecessing decom_pre_intra and coarse_me */
    WORD32 ai4_decomp_coarse_me_complete_flag[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /** Dependency manager for checking whether prev. frame decomp_intra
        done before current frame  decomp_intra starts */
    void *pv_dep_mngr_prev_frame_pre_enc_l1;

    /** Dependency manager for checking whether prev. frame L0 IPE done before
        current frame L0 IPE starts */
    void *pv_dep_mngr_prev_frame_pre_enc_l0;

    /** Dependency manager for checking whether prev. frame coarse_me done before
        current frame coarse_me starts */
    void *pv_dep_mngr_prev_frame_pre_enc_coarse_me;

    /** flag to indicate if pre_enc_init is done for current frame */
    WORD32 ai4_pre_enc_init_done[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /** flag to indicate if pre_enc_hme_init is done for current frame */
    WORD32 ai4_pre_enc_hme_init_done[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /** flag to indicate if pre_enc_deinit is done for current frame */
    WORD32 ai4_pre_enc_deinit_done[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /** Flag to indicate the end of processing when all the frames are done processing */
    WORD32 ai4_end_flag_pre_enc[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /** Flag to indicate the control blocking mode indicating input command to pre-enc
    group should be blocking or unblocking */
    WORD32 i4_ctrl_blocking_mode;

    /** Current input pointer */
    ihevce_lap_enc_buf_t *aps_curr_inp_pre_enc[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    WORD32 i4_last_inp_buf;

    /* buffer id for input buffer */
    WORD32 ai4_in_buf_id_pre_enc[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /** Current output pointer */
    pre_enc_me_ctxt_t *aps_curr_out_pre_enc[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /*Current L0 IPE to enc output pointer */
    pre_enc_L0_ipe_encloop_ctxt_t *ps_L0_IPE_curr_out_pre_enc;

    /** buffer id for output buffer */
    WORD32 ai4_out_buf_id_pre_enc[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /** buffer id for L0 IPE enc buffer*/
    WORD32 i4_L0_IPE_out_buf_id;

    /** Current picture Qp */
    WORD32 ai4_cur_frame_qp_pre_enc[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /** Decomp layer buffers indicies */
    WORD32 ai4_decomp_lyr_buf_idx[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /*since it is guranteed that cur frame ipe will not start unless prev frame ipe is completly done,
      an array of MAX_PRE_ENC_STAGGER might not be required*/
    WORD32 i4_qp_update_l0_ipe;

    /** Current picture encoded is the last picture to be encoded flag */
    WORD32 i4_last_pic_flag;

    /** Mutex for ensuring thread safety of the access of Job queues in decomp stage */
    void *pv_job_q_mutex_hdl_pre_enc_decomp;

    /** Mutex for ensuring thread safety of the access of Job queues in HME group */
    void *pv_job_q_mutex_hdl_pre_enc_hme;

    /** Mutex for ensuring thread safety of the access of Job queues in l0 ipe stage */
    void *pv_job_q_mutex_hdl_pre_enc_l0ipe;

    /** mutex handle for pre-enc init */
    void *pv_mutex_hdl_pre_enc_init;

    /** mutex handle for pre-enc decomp deinit */
    void *pv_mutex_hdl_pre_enc_decomp_deinit;

    /** mutex handle for pre enc hme init */
    void *pv_mutex_hdl_pre_enc_hme_init;

    /** mutex handle for pre-enc hme deinit */
    void *pv_mutex_hdl_pre_enc_hme_deinit;

    /*qp qurey before l0 ipe is done by multiple frame*/
    /** mutex handle for L0 ipe(pre-enc init)*/
    void *pv_mutex_hdl_l0_ipe_init;

    /** mutex handle for pre-enc deinit */
    void *pv_mutex_hdl_pre_enc_deinit;

    /** Array of Semaphore handles (for each frame processing threads ) */
    void *apv_pre_enc_thrd_sem_handle[MAX_NUM_FRM_PROC_THRDS_ENC];
    /** array which will tell the number of CTB processed in each row,
    *   used for Row level sync in IPE pass
    */
    WORD32 ai4_ctbs_in_row_proc_ipe_pass[MAX_NUM_CTB_ROWS_FRM];

    /**  Job Queue Memory pre encode */
    job_queue_t *aps_job_q_pre_enc[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME];

    /** Array of Job Queue handles enc group */
    job_queue_handle_t as_job_que_preenc_hdls[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME]
                                             [NUM_PRE_ENC_JOBS_QUES];

    /* accumulate intra sad across all thread to get qp before L0 IPE*/
    WORD32 ai4_intra_satd_acc[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME]
                             [MAX_NUM_FRM_PROC_THRDS_PRE_ENC];

    WORD32 i4_delay_pre_me_btw_l0_ipe;

    /*** This variable has the maximum delay between hme and l0ipe ***/
    /*** This is used for wrapping around L0IPE index ***/
    WORD32 i4_max_delay_pre_me_btw_l0_ipe;

    /* This is to register the handles of Dep Mngr b/w EncLoop and ME */
    /* This is used to delete the Mngr at the end                          */
    void *apv_dep_mngr_encloop_dep_me[NUM_ME_ENC_BUFS];
    /*flag to track buffer in me/enc que is produced or not*/
    WORD32 ai4_me_enc_buff_prod_flag[NUM_ME_ENC_BUFS];

    /*out buf que id for me */
    WORD32 ai4_me_out_buf_id[NUM_ME_ENC_BUFS];

    /*in buf que id for enc from me*/
    WORD32 i4_enc_in_buf_id[NUM_ME_ENC_BUFS];

    /* This is used to tell whether the free of recon buffers are done or not */
    WORD32 i4_is_recon_free_done;

    /* index for DVSR population */
    WORD32 i4_idx_dvsr_p;
    WORD32 aai4_l1_pre_intra_done[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME]
                                 [(HEVCE_MAX_HEIGHT >> 1) / 8];

    WORD32 i4_rc_l0_qp;

    /* Used for mres single out cases. Checks whether a particular resolution is active or passive */
    /* Only one resolution should be active for mres_single_out case */
    WORD32 *pi4_active_res_id;

    /**
     * Sub Pic bit control mutex lock handle
     */
    void *pv_sub_pic_rc_mutex_lock_hdl;

    void *pv_sub_pic_rc_for_qp_update_mutex_lock_hdl;

    WORD32 i4_encode;
    WORD32 i4_in_frame_rc_enabled;
    WORD32 i4_num_re_enc;

} multi_thrd_ctxt_t;

/**
 *  @brief    Structure to describe tile params
 */
typedef struct
{
    /* flag to indicate tile encoding enabled/disabled */
    WORD32 i4_tiles_enabled_flag;

    /* flag to indicate unifrom spacing of tiles */
    WORD32 i4_uniform_spacing_flag;

    /* num tiles in a tile-row. num tiles in tile-col */
    WORD32 i4_num_tile_cols;
    WORD32 i4_num_tile_rows;

    /* Curr tile width and height*/
    WORD32 i4_curr_tile_width;
    WORD32 i4_curr_tile_height;

    /* Curr tile width and heignt in CTB units*/
    WORD32 i4_curr_tile_wd_in_ctb_unit;
    WORD32 i4_curr_tile_ht_in_ctb_unit;

    /* frame resolution */
    //WORD32  i4_frame_width;  /* encode-width  */
    //WORD32  i4_frame_height; /* encode-height */

    /* total num of tiles "in frame" */
    WORD32 i4_num_tiles;

    /* Curr tile id. Assigned by raster scan order in a frame */
    WORD32 i4_curr_tile_id;

    /* x-pos of first ctb of the slice in ctb */
    /* y-pos of first ctb of the slice in ctb */
    WORD32 i4_first_ctb_x;
    WORD32 i4_first_ctb_y;

    /* x-pos of first ctb of the slice in samples */
    /* y-pos of first ctb of the slice in samples */
    WORD32 i4_first_sample_x;
    WORD32 i4_first_sample_y;

} ihevce_tile_params_t;

/**
******************************************************************************
 *  @brief  Encoder context structure
******************************************************************************
 */

typedef struct
{
    /**
     *  vps parameters
     */
    vps_t as_vps[IHEVCE_MAX_NUM_BITRATES];

    /**
     *  sps parameters
     */
    sps_t as_sps[IHEVCE_MAX_NUM_BITRATES];

    /**
     *  pps parameters
     *  Required for each bitrate separately, mainly because
     *  init qp etc parameters needs to be different for each instance
     */
    pps_t as_pps[IHEVCE_MAX_NUM_BITRATES];

    /**
     * Rate control mutex lock handle
     */
    void *pv_rc_mutex_lock_hdl;

    /** frame level cu analyse  buffer pointer for ME
     * ME will get ps_ctb_analyse structure populated with ps_cu pointers
     * pointing to ps_cu_analyse buffer from IPE.
      */
    //cu_analyse_t       *ps_cu_analyse_inter[PING_PONG_BUF];

    /**
      *  CTB frame context between encoder (producer) and entropy (consumer)
      */
    enc_q_ctxt_t s_enc_ques;

    /**
     *  Encoder memory manager ctxt
     */
    enc_mem_mngr_ctxt s_mem_mngr;

    /**
     * Semaphores of all the threads created in HLE
     * and Que handle for buffers b/w frame process and entropy
     */
    thrd_que_sem_hdl_t s_thrd_sem_ctxt;

    /**
     *  Reference /recon buffer Que pointer
     */
    recon_pic_buf_t **pps_recon_buf_q[IHEVCE_MAX_NUM_BITRATES];

    /**
     * Number of buffers in Recon buffer queue
     */
    WORD32 ai4_num_buf_recon_q[IHEVCE_MAX_NUM_BITRATES];

    /**
     * Reference / recon buffer Que pointer for Pre Encode group
     * this will be just a container and no buffers will be allcoated
     */
    recon_pic_buf_t **pps_pre_enc_recon_buf_q;

    /**
     * Number of buffers in Recon buffer queue
     */
    WORD32 i4_pre_enc_num_buf_recon_q;

    /**
      * frame level CTB parameters and worst PU CU and TU in a CTB row
      */
    frm_ctb_ctxt_t s_frm_ctb_prms;

    /*
     * Moudle ctxt pointers of all modules
     */
    module_ctxt_t s_module_ctxt;

    /*
     * LAP static parameters
     */
    ihevce_lap_static_params_t s_lap_stat_prms;

    /*
     * Run time dynamic source params
     */

    ihevce_src_params_t s_runtime_src_prms;

    /*
     *Target params
     */
    ihevce_tgt_params_t s_runtime_tgt_params;

    /*
     *  Run time dynamic coding params
     */
    ihevce_coding_params_t s_runtime_coding_prms;

    /**
     * Pointer to static config params
     */
    ihevce_static_cfg_params_t *ps_stat_prms;

    /**
     * the following structure members used for copying recon buf info
     * in case of duplicate pics
     */

    /**
     * Array of reference picture list for pre enc group
     * Separate list for ping_pong instnaces
     * 2=> ref_pic_list0 and ref_pic_list1
     */
    recon_pic_buf_t as_pre_enc_ref_lists[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME][2]
                                        [HEVCE_MAX_REF_PICS * 2];

    /**
     * Array of reference picture list for pre enc group
     * Separate list for ping_pong instnaces
     * 2=> ref_pic_list0 and ref_pic_list1
     */
    recon_pic_buf_t *aps_pre_enc_ref_lists[MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME][2]
                                          [HEVCE_MAX_REF_PICS * 2];

    /**
     *  Number of input frames per input queue
     */
    WORD32 i4_num_input_buf_per_queue;

    /**
     *  poc of the Clean Random Access(CRA)Ipic
     */
    WORD32 i4_cra_poc;

    /** Number of ref pics in list 0 for any given frame */
    WORD32 i4_num_ref_l0;

    /** Number of ref pics in list 1 for any given frame */
    WORD32 i4_num_ref_l1;

    /** Number of active ref pics in list 0 for cur frame */
    WORD32 i4_num_ref_l0_active;

    /** Number of active ref pics in list 1 for cur frame */
    WORD32 i4_num_ref_l1_active;

    /** Number of ref pics in list 0 for any given frame pre encode stage */
    WORD32 i4_pre_enc_num_ref_l0;

    /** Number of ref pics in list 1 for any given frame  pre encode stage */
    WORD32 i4_pre_enc_num_ref_l1;

    /** Number of active ref pics in list 0 for cur frame  pre encode stage */
    WORD32 i4_pre_enc_num_ref_l0_active;

    /** Number of active ref pics in list 1 for cur frame  pre encode stage */
    WORD32 i4_pre_enc_num_ref_l1_active;

    /**
     *  working mem to be used for frm level activities
     * One example is interplation at frame level. This requires memory
     * of (max width + 16) * (max_height + 7 + 16 ) * 2 bytes.
     * This is so since we generate interp output for max_width + 16 x
     * max_height + 16, and then the intermediate output is 16 bit and
     * is max_height + 16 + 7 rows
     */
    UWORD8 *pu1_frm_lvl_wkg_mem;

    /**
     * Multi thread processing context
     * This memory contains the variables and pointers shared across threads
     * in enc-group and pre-enc-group
     */
    multi_thrd_ctxt_t s_multi_thrd;

    /** I/O Queues created status */
    WORD32 i4_io_queues_created;

    WORD32 i4_end_flag;

    /** number of bit-rate instances running */
    WORD32 i4_num_bitrates;

    /** number of enc frames running in parallel */
    WORD32 i4_num_enc_loop_frm_pllel;

    /*ref bitrate id*/
    WORD32 i4_ref_mbr_id;

    /* Flag to indicate app, that end of processing has reached */
    WORD32 i4_frame_limit_reached;

    /*Structure to store the function selector
     * pointers for common and encoder */
    func_selector_t s_func_selector;

    /*ref resolution id*/
    WORD32 i4_resolution_id;

    /*hle context*/
    void *pv_hle_ctxt;

    rc_quant_t s_rc_quant;
    /*ME cost of P pic stored for the next ref B pic*/
    //LWORD64 i8_acc_me_cost_of_p_pic_for_b_pic[2];

    UWORD32 u4_cur_pic_encode_cnt;
    UWORD32 u4_cur_pic_encode_cnt_dbp;
    /*past 2 p pics high complexity status*/
    WORD32 ai4_is_past_pic_complex[2];

    WORD32 i4_is_I_reset_done;
    WORD32 i4_past_RC_reset_count;

    WORD32 i4_future_RC_reset;

    WORD32 i4_past_RC_scd_reset_count;

    WORD32 i4_future_RC_scd_reset;
    WORD32 i4_poc_reset_values;

    /*Place holder to store the length of LAP in first pass*/
    /** Number of frames to look-ahead for RC by -
     * counts 2 fields as one frame for interlaced
     */
    WORD32 i4_look_ahead_frames_in_first_pass;

    WORD32 ai4_mod_factor_derived_by_variance[2];
    float f_strength;

    /*for B frames use the avg activity
    from the layer 0 (I or P) which is the average over
    Lap2 window*/
    LWORD64 ai8_lap2_8x8_avg_act_from_T0[2];

    LWORD64 ai8_lap2_16x16_avg_act_from_T0[3];

    LWORD64 ai8_lap2_32x32_avg_act_from_T0[3];

    /*for B frames use the log of avg activity
    from the layer 0 (I or P) which is the average over
    Lap2 window*/
    long double ald_lap2_8x8_log_avg_act_from_T0[2];

    long double ald_lap2_16x16_log_avg_act_from_T0[3];

    long double ald_lap2_32x32_log_avg_act_from_T0[3];

    ihevce_tile_params_t *ps_tile_params_base;

    WORD32 ai4_column_width_array[MAX_TILE_COLUMNS];

    WORD32 ai4_row_height_array[MAX_TILE_ROWS];

    /* Architecture */
    IV_ARCH_T e_arch_type;

    UWORD8 u1_is_popcnt_available;

    WORD32 i4_active_scene_num;

    WORD32 i4_max_fr_enc_loop_parallel_rc;
    WORD32 ai4_rc_query[IHEVCE_MAX_NUM_BITRATES];
    WORD32 i4_active_enc_frame_id;

    /**
    * LAP interface ctxt pointer
    */
    void *pv_lap_interface_ctxt;

    /* If enable, enables blu ray compatibility of op*/
    WORD32 i4_blu_ray_spec;

} enc_ctxt_t;

/**
******************************************************************************
*  @brief  This struct contains the inter CTB params needed for the decision
*   of the best inter CU results
******************************************************************************
*/
typedef struct
{
    hme_pred_buf_mngr_t s_pred_buf_mngr;

    /** X and y offset of ctb w.r.t. start of pic */
    WORD32 i4_ctb_x_off;
    WORD32 i4_ctb_y_off;

    /**
     * Pred buffer ptr, updated inside subpel refinement process. This
     * location passed to the leaf fxn for copying the winner pred buf
     */
    UWORD8 **ppu1_pred;

    /** Working mem passed to leaf fxns */
    UWORD8 *pu1_wkg_mem;

    /** prediction buffer stride fo rleaf fxns to copy the pred winner buf */
    WORD32 i4_pred_stride;

    /** Stride of input buf, updated inside subpel fxn */
    WORD32 i4_inp_stride;

    /** stride of recon buffer */
    WORD32 i4_rec_stride;

    /** Indicates if bi dir is enabled or not */
    WORD32 i4_bidir_enabled;

    /**
     * Total number of references of current picture which is enocded
     */
    UWORD8 u1_num_ref;

    /** Recon Pic buffer pointers for L0 list */
    recon_pic_buf_t **pps_rec_list_l0;

    /** Recon Pic buffer pointers for L1 list */
    recon_pic_buf_t **pps_rec_list_l1;

    /**
     * These pointers point to modified input, one each for one ref idx.
     * Instead of weighting the reference, we weight the input with inverse
     * wt and offset for list 0 and list 1.
     */
    UWORD8 *apu1_wt_inp[2][MAX_NUM_REF];

    /* Since ME uses weighted inputs, we use reciprocal of the actual weights */
    /* that are signaled in the bitstream */
    WORD32 *pi4_inv_wt;
    WORD32 *pi4_inv_wt_shift_val;

    /* Map between L0 Reference indices and LC indices */
    WORD8 *pi1_past_list;

    /* Map between L1 Reference indices and LC indices */
    WORD8 *pi1_future_list;

    /**
     * Points to the non-weighted input data for the current CTB
     */
    UWORD8 *pu1_non_wt_inp;

    /**
     * Store the pred lambda and lamda_qshifts for all the reference indices
     */
    WORD32 i4_lamda;

    UWORD8 u1_lamda_qshift;

    WORD32 wpred_log_wdc;

    /**
     * Number of active references in l0
     */
    UWORD8 u1_num_active_ref_l0;

    /**
     * Number of active references in l1
     */
    UWORD8 u1_num_active_ref_l1;

    /** The max_depth for inter tu_tree */
    UWORD8 u1_max_tr_depth;

    /** Quality Preset */
    WORD8 i1_quality_preset;

    /** SATD or SAD */
    UWORD8 u1_use_satd;

    /* Frame level QP */
    WORD32 i4_qstep_ls8;

    /* Pointer to an array of PU level src variances */
    UWORD32 *pu4_src_variance;

    WORD32 i4_alpha_stim_multiplier;

    UWORD8 u1_is_cu_noisy;

    ULWORD64 *pu8_part_src_sigmaX;

    ULWORD64 *pu8_part_src_sigmaXSquared;

    UWORD8 u1_max_2nx2n_tu_recur_cands;

} inter_ctb_prms_t;

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/
extern const double lamda_modifier_for_I_pic[8];

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

#endif /* _IHEVCE_ENC_STRUCTS_H_ */
