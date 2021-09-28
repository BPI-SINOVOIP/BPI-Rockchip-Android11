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
*******************************************************************************
* @file
*  ihevc_rdoq_macros.h
*
* @brief
*  Macros used for RDOQ algorthm
*
* @author
*  Ittiam
*
* @remarks
*  None
*
*******************************************************************************
*/
#ifndef IHEVC_RDOQ_MACROS_H_
#define IHEVC_RDOQ_MACROS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
/*Used for calculating the distortion in the transform domain*/
#define CALC_SSD_IN_TRANS_DOMAIN(a, b, i4_round_val, i4_shift_val)                                 \
    (SHR_NEG(((((a) - (b)) * ((a) - (b))) + i4_round_val), i4_shift_val))
#define CALC_CUMMUL_SSD_IN_TRANS_DOMAIN(a, b, i4_round_val, i4_shift_val)                          \
    (SHR_NEG((((a) - (b)) + i4_round_val), i4_shift_val))

#define MAX_INT 0x7FFFFFFF

#define COMPUTE_RATE_COST_CLIP30_RDOQ(r, l, qshift)                                                \
    ((WORD32)CLIP30((((ULWORD64)r) * ((ULWORD64)l)) >> (qshift)))

/*This macro is required to test the RDOQ changes*/
/*1 implies cabac context validation using the test-bench*/
/*Also prints some debug information*/
#define TEST_BENCH_RDOQ 0

/*Macro to enable and disable coefficient RDOQ. When 1, coefficient RDOQ is enabled*/
#define COEFF_RDOQ 0

/*Macro to optimize the copying of cabac states across various temp/scratch cabac contexts
  Should always be 0 when COEFF_RDOQ is 1*/
#define OPT_MEMCPY 1

/** Macro which accounts subtracts 4096 bits from the total bits generated per TU in the RDOPT stage
    if SBH is on*/
#define ACCOUNT_SIGN_BITS 0

/*Macro defining the maximum number of context elements in the cabac state*/
//#define MAX_NUM_CONTEXT_ELEMENTS 5

/*****************************************************************************/
/* Enum                                                                      */
/*****************************************************************************/
/*Enum to indicate which context element in the cabac state is currently being altered*/
typedef enum
{
    LASTXY,
    SUB_BLK_CODED_FLAG,
    SIG_COEFF,
    GRTR_THAN_1,
    GRTR_THAN_2,
    MAX_NUM_CONTEXT_ELEMENTS
} BACKUP_CTXT_ELEMENTS;

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/*Structure defined to optimize copying of cabac states across various temporary/scratch cabac states*/
typedef struct
{
    // clang-format off
    /**
    ai4_ctxt_to_backup[x] tells us if xth element has been altered. where
    x       context element                 Meaning
    0       IHEVC_CAB_COEFFX_PREFIX         lastx last y has been coded
    1       IHEVC_CAB_CODED_SUBLK_IDX       sub-blk coded or not flag has been coded
    2       IHEVC_CAB_COEFF_FLAG            sigcoeff has been coded
    3       IHEVC_CAB_COEFABS_GRTR1_FLAG    greater than 1 bin has been coded
    4       IHEVC_CAB_COEFABS_GRTR2_FLAG    greater than 2 bin has been coded
    */
    // clang-format on
    UWORD8 au1_ctxt_to_backup[MAX_NUM_CONTEXT_ELEMENTS];

    /** Number of bits generated */
    WORD32 i4_num_bits;
} backup_ctxt_t;

/**
******************************************************************************
* @brief Structure to store the position of the coefficient to be changed
         through SBH
******************************************************************************
 */
typedef struct
{
    UWORD8 x;
    UWORD8 y;
    UWORD8 is_valid_pos;
    WORD16 i2_old_coeff;
} s_sbh_coeff_pos_t;

/**
******************************************************************************
 *  @brief  RDOQ SBH context for cabac bit estimation etc
******************************************************************************
 */

typedef struct
{
    /** TU size */
    WORD32 i4_trans_size;

    /** Log 2 TU size */
    WORD32 i4_log2_trans_size;

    /**
     * Boolean value representing if the current TU is luma or not.
     * 1 => Luma
     */
    WORD32 i4_is_luma;

    /**
     * Calculate rounding and shifting values required for normalizing original
     * and inverse quantized transform coefficients (for calculation of SSD in
     * transform domain)
     */
    WORD32 i4_round_val_ssd_in_td;
    WORD32 i4_shift_val_ssd_in_td;

    /** Matrix used in inverse quantization */
    WORD32 quant_scale_mat_offset;

    /** Index of the csb within the TU*/
    WORD32 i4_trans_idx;

    /** value of lambda used in the D+Rlambda metric*/
    LWORD64 i8_cl_ssd_lambda_qf;

    /** Used while inverse quantizing*/
    WORD16 i2_qp_rem;
    WORD32 i4_qp_div;

    /** Scan index of the csbs within the TU   */
    WORD32 i4_scan_idx;

    /** Pointer to the csbf buf. This buffer will contain 1 if the csb is coded
     *  and 0 if it is not*/
    UWORD8 *pu1_csbf_buf;

    /** Boolean value which is 1 if any of the csbs in the current TU are
     * coded*/
    UWORD8 i1_tu_is_coded;

    /**
     * Pointer to an array of pointer to store the scaling matrices for
     * all transform sizes and qp % 6 (pre computed)
     */
    WORD16 *pi2_dequant_coeff;

    /** Pointer to the quantized coeffs*/
    WORD16 *pi2_quant_coeffs;

    /** Pointer to the inverse quantized values*/
    WORD16 *pi2_iquant_coeffs;

    /** Pointer ot the transformed values(before quantization) */
    WORD16 *pi2_trans_values;

    /** Stride of the inverse quant data*/
    WORD32 i4_iq_data_strd;

    /** Stride of the quant data*/
    WORD32 i4_q_data_strd;

    /** Intermediate array to store transform output for RDOQ*/
    WORD16 ai2_trans_values[MAX_TRANS_SIZE];

    /** Pointer to zero rows and zero cols*/
    WORD32 *pi4_zero_row;
    WORD32 *pi4_zero_col;

    /** Array containing information about the position of the coefficient
     * to be altered during SBH
     */
    s_sbh_coeff_pos_t s_best_pos[(MAX_TU_SIZE * MAX_TU_SIZE / 4 / 4) + 1];

    /** SSD cost for this particular TU*/
    LWORD64 i8_ssd_cost;

    WORD32 i4_perform_all_cand_rdoq;
    WORD32 i4_perform_best_cand_rdoq;
    WORD32 i4_perform_all_cand_sbh;
    WORD32 i4_perform_best_cand_sbh;

    WORD32 i4_bit_depth;

    WORD32 *pi4_subBlock2csbfId_map;

} rdoq_sbh_ctxt_t;

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

void ihevce_sign_data_hiding(rdoq_sbh_ctxt_t *ps_rdoq_sbh_params);

#endif /* IHEVC_RDOQ_MACROS_H_ */
