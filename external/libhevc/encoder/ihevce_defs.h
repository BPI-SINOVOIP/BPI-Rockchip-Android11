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
*  ihevce_defs.h
*
* @brief
*  Definitions used in the codec
*
* @author
*  Ittiam
*
* @remarks
*  None
*
*******************************************************************************
*/
#ifndef _IHEVCE_DEFS_H_
#define _IHEVCE_DEFS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

#define SINGLE_THREAD_INTERFACE 0

#define DEFAULT_VPS_ID 0

#define DEFAULT_SPS_ID 0

#define DEFAULT_PPS_ID 0

#define DEFAULT_CHROMA_FORMAT_IDC 1

#define AMP_ENABLED 1

#define AMP_DISABLED 0

#define LISTS_MODIFICATION_ABSENT 0

#define LISTS_MODIFICATION_PRESENT 1

#define LONG_TERM_REF_PICS_PRESENT 1

#define LONG_TERM_REF_PICS_ABSENT 0

#define PCM_ENABLED 1

#define PCM_DISABLED 0

#define PCM_LOOP_FILTER_DISABLED 1

#define PCM_LOOP_FILTER_ENABLED 0

#define REF_PIC_LISTS_RESTRICTED 1

#define REF_PIC_LISTS_UNRESTRICTED 0

#define SCALING_LIST_DISABLED 0

#define SCALING_LIST_ENABLED 1

#define DEFAULT_SPS_MAX_SUB_LAYERS 1

#define VPS_SUB_LAYER_ORDERING_INFO_PRESENT 1

#define VPS_SUB_LAYER_ORDERING_INFO_ABSENT 0

#define SPS_SUB_LAYER_ORDERING_INFO_PRESENT 1

#define SPS_SUB_LAYER_ORDERING_INFO_ABSENT 0

#define SCALING_LIST_DATA_PRESENT 1

#define SCALING_LIST_DATA_ABSENT 0

#define SPS_TEMPORAL_ID_NESTING_DONE 1

#define NO_SPS_TEMPORAL_ID_NESTING_DONE 0

#define STRONG_INTRA_SMOOTHING_FLAG_DISABLE 0

#define STRONG_INTRA_SMOOTHING_FLAG_ENABLE 1

#define DEFAULT_LOG2_MAX_POC_LSB 10

#define DEFAULT_PIC_CROP_TOP_OFFSET 0

#define DEFAULT_PIC_CROP_LEFT_OFFSET 0

#define DEFAULT_PIC_CROP_RIGHT_OFFSET 0

#define DEFAULT_PIC_CROP_BOTTOM_OFFSET 0

#define DEFAULT_MAX_DEC_PIC_BUFFERING 8

#define DEFAULT_MAX_NUM_REORDER_PICS 8

#define DEFAULT_MAX_LATENCY_INCREASE 8

#define HIGH_TIER 1

#define MAIN_TIER 0

#define DEFAULT_BETA_OFFSET 0

#define CABAC_INIT_PRESENT 1

#define CABAC_INIT_ABSENT 0

#define CU_QP_DELTA_ENABLED 1

#define CU_QP_DELTA_DISABLED 0

#define MAX_MERGE_CANDIDATES 5

#define CONSTR_IPRED_ENABLED 1

#define CONSTR_IPRED_DISABLED 0

#define DISABLE_DEBLOCKING_FLAG 1

#define ENABLE_DEBLOCKING_FLAG 0

#define DEBLOCKING_FILTER_CONTROL_PRESENT 1

#define DEBLOCKING_FILTER_CONTROL_ABSENT 0

#define DEBLOCKING_FILTER_OVERRIDE_ENABLED 1

#define DEBLOCKING_FILTER_OVERRIDE_DISABLED 0

#define DEPENDENT_SLICE_ENABLED 1

#define DEPENDENT_SLICE_DISABLED 0

#define DEFAULT_DIFF_CU_QP_DELTA_DEPTH 0

#define ENTROPY_CODING_SYNC_ENABLED 1

#define ENTROPY_CODING_SYNC_DISABLED 0

#define ENTROPY_SLICE_ENABLED 1

#define ENTROPY_SLICE_DISABLED 0

#define DEFAULT_PARALLEL_MERGE_LEVEL 2

#define DEFAULT_NUM_REF_IDX_L0_DEFAULT_ACTIVE 6

#define DEFAULT_NUM_REF_IDX_L1_DEFAULT_ACTIVE 6

#define NUM_TILES_COLS 0

#define NUM_TILES_ROWS 0

#define OUTPUT_FLAG_PRESENT 1

#define OUTPUT_FLAG_ABSENT 0

#define DEFAULT_PIC_CB_QP_OFFSET 0

#define DEFAULT_PIC_CR_QP_OFFSET 0

#define SLICE_LEVEL_CHROMA_QP_OFFSETS_PRESENT 1

#define SLICE_LEVEL_CHROMA_QP_OFFSETS_ABSENT 0

#define DEBLOCKING_FILTER_DISABLED 1

#define DEBLOCKING_FILTER_ENABLED 0

#define LF_ACROSS_SLICES_ENABLED 1

#define LF_ACROSS_SLICES_DISABLED 0

#define SAO_ENABLED 1

#define SAO_DISABLED 0

#define SCALING_LIST_DATA_PRESENT 1

#define SCALING_LIST_DATA_ABSENT 0

#define SIGN_DATA_HIDDEN 1

#define SIGN_DATA_UNHIDDEN 0

#define SLICE_EXTENSION_PRESENT 1

#define SLICE_EXTENSION_ABSENT 0

#define SLICE_HEADER_EXTENSION_PRESENT 1

#define SLICE_HEADER_EXTENSION_ABSENT 0

#define DEFAULT_TC_OFFSET 0

#define TRANSFORM_SKIP_ENABLED 1

#define TRANSFORM_SKIP_DISABLED 0

#define TRANSFORM_BYPASS_ENABLED 1

#define TRANSFORM_BYPASS_DISABLED 0

#define SPACING_IS_UNIFORM 1

#define SPACING_IS_NONUNIFORM 0

#define TILES_ENABLED 1

#define TILES_DISABLED 0

#define TOTAL_NUM_TIERS 2

#define TOTAL_NUM_LEVELS 13

#define SET_CTB_ALIGN(x, y) ((((x) & ((y)-1)) == 0) ? 0 : (y) - ((x) & ((y)-1)))

/* Enables HM-8.1 compatible stream, setting to 0 will make it 8.2 compatible*/
#define HM_8DOT1_SYNTAX 0

/* Enables cu level RD optimized encoding by computing cabac bits for the cu */
#define RDOPT_ENABLE 1

/* Enables inclusion of chroma coding cost for RD opt decisions */
#define CHROMA_RDOPT_ENABLE 1

/* Enables tu level zero cbf based RD optimized encoding */
#define RDOPT_ZERO_CBF_ENABLE 1

/* Enables bit savings in tu tree of inter cus by merging not coded child nodes to parent node */
#define SHRINK_INTER_TUTREE 1

/*  q format for lamba used in the encoder                                   */
#define LAMBDA_Q_SHIFT 8

/* If 0, Allign PIC Wd/ht to Min CU size  */
/* If 1, Allign PIC Wd/ht to CTB size  */
#define PIC_ALIGN_CTB_SIZE 0

/** Enables DCT integer transform / Hadamard Transform based SATD evaluation
  * 1 : DCT integer Transform,  0 : Hadamard Transform
  */
#define USE_EXACT_TFR 0

/** Enable colocated PU population */
#define ENABLE_COL_PU_POPULATION 1

#define MAX_MVX_SUPPORTED_IN_COARSE_LAYER 128

#define MAX_MVY_SUPPORTED_IN_COARSE_LAYER 64

//ME_Experiments

#define USE_4x4_IN_L1 0

#define DIAMOND_GRID 1

#define SUBPEL_DEDUPLICATE_ENABLE 1

/** Enables CU delta QP population within a frame : Random for now */
//#define RANDOM_CU_QP  0

/**
 * @brief  Mapping of Minimum HEVC qp to MPEG2 QP
 */
#define MIN_RC_QP (1)
/**
 * @brief  Mapping of Maximum HEVC qp to MPEG2 QP
 */
#define MAX_RC_QP (228)
/**
 * @brief  Total NUmber of MPEG2 QPs
 */
#define MPEG2_QP_ELEM (MAX_RC_QP + 1)
/**
 * @brief  Total NUmber of HEVC QPs
 */
#define HEVC_QP_ELEM (MAX_HEVC_QP_10bit + 1)

#define QP_LEVEL_MOD_ACT_FACTOR 10

#define TWO_POW_QP_LEVEL_MOD_ACT_FACTOR (1 << (QP_LEVEL_MOD_ACT_FACTOR))

#define DEFAULT_NON_PACKED_CONSTRAINT_FLAG 1

#define DEFAULT_FRAME_ONLY_CONSTRAINT_FLAG 0

#define ENABLE_CU_TREE_CULLING (1 && ENABLE_4CTB_EVALUATION)

#define RATIONALISE_NUM_RDO_MODES_IN_PQ_AND_HQ 1

#define MAX_NUMBER_OF_INTER_RDOPT_CANDS_IN_PQ_AND_HQ 2

#define MAX_NUMBER_OF_INTER_RDOPT_CANDS_IN_MS 2

#define MAX_NUMBER_OF_INTER_RDOPT_CANDS_IN_HS_AND_XS 1

#define BUFFER_SIZE_MULTIPLIER_IF_HBD 3

/* If  */
/* qp_bdoffset = 6 * (bit_depth - 8) */
/* and */
/* lambda = pow(2.0, (((i4_cur_frame_qp + qp_bdoffset - 12)) / 3)), */
/* Then 'Lambda Types' are - */
/* 0, when bit_depth_in_module = 8 => qp_bdoffset = 0 always */
/* 1, when bit_depth_in_module > 8, => qp_bdoffset = value derived above */
/* 2, when both of the lambdas referred to in the previous cases are required */

#define PRE_ENC_LAMBDA_TYPE 0

#define ENC_LAMBDA_TYPE 0

#define IPE_LAMBDA_TYPE 0

#define ME_LAMBDA_TYPE 0

#define ENC_LOOP_LAMBDA_TYPE 2

#define ENABLE_SSIM 0

#define VUI_BIT_RATE_SCALE 6

#define VUI_CPB_SIZE_SCALE 8

#define CBP_VCL_FACTOR 1000

#define ENABLE_REFINED_QP_MOD 1

#if ENABLE_REFINED_QP_MOD
/* to find the uncovered region (or new region) which will be used for reference for the upcoming pictures
    will be coded well to enhance coding efficiency*/
#define ENABLE_TEMPORAL_WEIGHING 0

/* to enable modulation factor  based on spatial variance when we calculate activity factor using
    the following equaltion
    act_factor = (m * c + a )/(c + m * a)*/
// SATD_NOISE_FLOOR_THRESHOLD was earlier controlled using this
#define ENABLE_QP_MOD_BASED_ON_SPATIAL_VARIANCE 0

/* To enable the trace for delta Qp bits */
#define QP_DELTA_BITS_TRACE

/* to enable modulation based LAP2 average satd*/
#define MODULATION_OVER_LAP 1

/* 0 - Lamda and Qp are decoupled,
       1 - Lamda and Qp are coupled*/
#define LAMDA_BASED_ON_QUANT 0

/*
       0 - act_factor = (m * c + a )/(c + m * a)
           m = modulation factor
           c = cur satd
           a = average satd
       ----------------------------------------
       1 - act_factor = (c/a) ^ (s/3)
           s = strength
           c = cur satd
           a = average satd
    */
#define LOGARITHMIC_MODULATION 1

#define MEDIAN_ENABLE 1
#define MIN_ENABLE 0

/* well compensatable regions are not considered for
    QP modulation*/
#define DISABLE_COMPENSATION 1

#define CST_NOISE_THRSHLD 0

/*decrease intra cu qp by 1 in Inter Pictures*/
#define DECREASE_QP 0

/*strength calculation based on deviation*/
#define STRENGTH_BASED_ON_DEVIATION 1

/*enable allow cliping of qctivity factor such that
    deviation of qp in modulation is controlled*/
#define ALLOW_ACT_FACTOR_CLIP_IN_QP_MOD 1

/*instead of avg activity use sqrt(avg of satd square)*/
#define USE_SQRT_AVG_OF_SATD_SQR 1

/*use sum of squared transform coeff*/
#define USE_SQR_SATD_COEFF 0

/*instead of L1 IPE SATD, use L1 CUR SATD*/
#define USE_CUR_SATD 0  // else it will use satd of cur - pred

/*use L0 CUR SATD */
#define USE_CUR_L0_SATD 0

/* strength based on only curr frame deviation else it is based on average over lap2 */
#define STRENGTH_BASED_ON_CURR_FRM 0

#define POW_OPT 1

#else /*INITIAL QP MOD*/
/*Same as 11_0 Mod version */
// SATD_NOISE_FLOOR_THRESHOLD was earlier controlled using this
#define ENABLE_QP_MOD_BASED_ON_SPATIAL_VARIANCE 0
#define ENABLE_TEMPORAL_WEIGHING 0
#define MODULATION_OVER_LAP 0
#define LAMDA_BASED_ON_QUANT 1
#define LOGARITHMIC_MODULATION 0
#define MIN_ENABLE 1
#define DISABLE_COMPENSATION 1
#define CST_NOISE_THRSHLD 1
#define DECREASE_QP 0
#endif

#define MASK_4AC 0xFFFFFFFEFEFEFCE0
#define MASK_3AC 0xFFFFFFFFFEFEFCF0
#define MASK_2AC 0xFFFFFFFFFFFEFCF8
#define MASK_DC 0xFFFFFFFFFFFFFFFE
#define I_PIC_LAMDA_MODIFIER 0.5
#define CONST_LAMDA_MODIFIER 1
#define NO_EXTRA_MULTIPLIER 1
#define NEW_LAMDA_MODIFIER (!CONST_LAMDA_MODIFIER)
#define LAMDA_MODIFIER(QP, Tid)                                                                    \
    (0.85 * pow(2.0, (Tid * (CLIP3(((QP + 5.0) / 25.0), 1.0, 2.0) - 1.0)) / 3.0))
#define CONST_LAMDA_MOD_VAL (0.85)
#define MEAN_BASED_QP_MOD 0

#if MEDIAN_ENABLE
#define MEDIAN_CU_TU 1
#define MEDIAN_CU_TU_BY_2 3
#define MEDIAN_CU_TU_BY_4 10
#endif

#if MIN_ENABLE
#define MEDIAN_CU_TU 0
#define MEDIAN_CU_TU_BY_2 0
#define MEDIAN_CU_TU_BY_4 0
#endif

#define COMP_RATIO_NORM 5
#define COMP_RATIO_MIN 0
#define COMP_RATIO_MAX 3
#define NOISE_THRE_MAP_TO_8 3

#define REF_MOD_VARIANCE (0.6696)

#define REF_MOD_DEVIATION (473.0)  //(0.6696) //
#define NO_MOD_DEVIATION (220.0)
#define BELOW_REF_DEVIATION (0.0)
#define ABOVE_REF_DEVIATION (220.0)

#define MIN_QP_MOD_OFFSET -10
#define MAX_QP_MOD_OFFSET 3
#define TOT_QP_MOD_OFFSET (MAX_QP_MOD_OFFSET - MIN_QP_MOD_OFFSET + 1)

#define ENABLE_UNIFORM_CU_SIZE_16x16 0

#define ENABLE_UNIFORM_CU_SIZE_8x8 0

#define MAX_QP_BD_OFFSET 24

// chroma mode index for derived from luma intra mode
#define DM_CHROMA_IDX 36

#define DISABLE_RDOQ 0

#define DISABLE_SKIP_AND_MERGE_EVAL 0

#define ENABLE_PICKING_4_BEST_IN_B_PIC_IN_ME 0

#define ENABLE_TU_TREE_DETERMINATION_IN_RDOPT 0

#define MAX_NUM_MIXED_MODE_INTER_RDO_CANDS (MAX_NUMBER_OF_INTER_RDOPT_CANDS_IN_PQ_AND_HQ * 2)

#define MAX_NUM_CU_MERGE_SKIP_CAND (MAX_NUMBER_OF_INTER_RDOPT_CANDS_IN_PQ_AND_HQ + 1)

#define NUM_MODE_COMBINATIONS_IN_INTER_CU_WITH_2_PUS 4

/* +1 for skip candidate */
#define MAX_NUM_INTER_RDO_CANDS                                                                    \
    (NUM_MODE_COMBINATIONS_IN_INTER_CU_WITH_2_PUS * MAX_NUMBER_OF_INTER_RDOPT_CANDS_IN_PQ_AND_HQ + \
     1)

#define UNI_SATD_SCALE 1

#define ENABLE_MIXED_INTER_MODE_EVAL 1

#define DISABLE_SAO 0

#define DISABLE_LUMA_SAO (0 || (DISABLE_SAO))

#define DISABLE_CHROMA_SAO (0 || (DISABLE_SAO))

#define MAX_NUM_INTER_CANDS_PQ 4 /*MAX_NUM_INTER_RDO_CANDS*/

#define MAX_NUM_INTER_CANDS_HQ 4 /*MAX_NUM_INTER_RDO_CANDS*/

#define MAX_NUM_INTER_CANDS_MS 3

#define MAX_NUM_INTER_CANDS_HS 2

#define MAX_NUM_INTER_CANDS_ES 2

#define RESTRICT_NUM_INTER_CANDS_PER_PART_TYPE 0

#define MAX_NUM_INTER_CANDS_PER_PART_TYPE 3

#define PICK_ONLY_BEST_CAND_PER_PART_TYPE 0

#define REUSE_ME_COMPUTED_ERROR_FOR_INTER_CAND_SIFTING 0

#define DISABLE_SBH 0

#define DISABLE_TMVP 0

#define DISABLE_QUANT_ROUNDING 0

#define ENABLE_SEPARATE_LUMA_CHROMA_INTRA_MODE 1

#define FORCE_INTRA_TU_DEPTH_TO_0 0

#define WEIGH_CHROMA_COST 1

#define ENABLE_ZERO_CBF_IN_INTRA 0

#define DISABLE_ZERO_ZBF_IN_INTER 0

#define ENABLE_INTER_ZCU_COST 1

#define ADAPT_COLOCATED_FROM_L0_FLAG 1

#define CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT 10

#define ENABLE_SSD_CALC_RC 0

#define SRC_PADDING_FOR_TRAQO 1

#define ENABLE_CU_SPLIT_FLAG_RATE_ESTIMATION 1

#define ZCBF_SKIP_DISTORTION_THRESHOLD (1.2)

#define ENABLE_CHROMA_RDOPT_EVAL_IN_PQ 1

#define ENABLE_CHROMA_RDOPT_EVAL_IN_HQ 1

#define ENABLE_CHROMA_RDOPT_EVAL_IN_MS 1

#define ENABLE_CHROMA_RDOPT_EVAL_IN_HS 0

#define ENABLE_CHROMA_RDOPT_EVAL_IN_XS 0

#define ENABLE_CHROMA_RDOPT_EVAL_IN_XS6 0

#define ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_PQ                                                  \
    (1 && (ENABLE_CHROMA_RDOPT_EVAL_IN_PQ) && (ENABLE_SEPARATE_LUMA_CHROMA_INTRA_MODE))

#define ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_HQ                                                  \
    (1 && (ENABLE_CHROMA_RDOPT_EVAL_IN_HQ) && (ENABLE_SEPARATE_LUMA_CHROMA_INTRA_MODE))

#define ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_MS                                                  \
    (0 && (ENABLE_CHROMA_RDOPT_EVAL_IN_MS) && (ENABLE_SEPARATE_LUMA_CHROMA_INTRA_MODE))

#define ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_HS                                                  \
    (0 && (ENABLE_CHROMA_RDOPT_EVAL_IN_HS) && (ENABLE_SEPARATE_LUMA_CHROMA_INTRA_MODE))

#define ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_XS                                                  \
    (0 && (ENABLE_CHROMA_RDOPT_EVAL_IN_XS) && (ENABLE_SEPARATE_LUMA_CHROMA_INTRA_MODE))

#define ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_XS6                                                 \
    (0 && (ENABLE_CHROMA_RDOPT_EVAL_IN_XS6) && (ENABLE_SEPARATE_LUMA_CHROMA_INTRA_MODE))

#define RC_BUFFER_INFO 0

#define DISABLE_SMP_IN_XS25 1

#define DISABLE_64X64_BLOCK_MERGE_IN_ME_IN_XS25 1

#define MAX_NUM_TU_RECUR_CANDS_IN_XS25 1

#define MAX_NUM_CANDS_FOR_FPEL_REFINE_IN_XS25 1

#define MAX_NUM_CONSTITUENT_MVS_TO_ENABLE_32MERGE_IN_XS25 4

#define NUM_INIT_SEARCH_CANDS_IN_L1_AND_L2_ME_IN_XS25 2

#define DISABLE_TOP_SYNC 0

#define ENABLE_MULTI_THREAD_FILE_WRITES 0

#define DISABLE_EARLY_ZCBF 0

#define EARLY_CBF_ON 1

#define DUMP_CBF_HIST_DATA 0

#define ENABLE_INTRA_MODE_FILTERING_IN_XS25 1

#define MAX_NUM_INTRA_MODES_PER_TU_DISTRIBUTION_IN_XS25 2

#define MAX_NUM_REFS_IN_PPICS_IN_XS25 1

#define USE_CONSTANT_LAMBDA_MODIFIER 0

/* Actual Lambda in ME -> (100 - ME_LAMBDA_DISCOUNT) * lambda / 100 */
#define ME_LAMBDA_DISCOUNT 0

#define FORCE_AT_LEAST_1_UNICAND_IN_BPICS 0

#define MULTI_REF_ENABLE 1

#define PROHIBIT_INTRA_QUANT_ROUNDING_FACTOR_TO_DROP_BELOW_1BY3 0

#define ENABLE_INTRA_GATING_FOR_HQ 0

#define ADD_NOISE_TERM_TO_COST 1

#define ALPHA_Q_FORMAT 4
#define ALPHA_FOR_NOISE_TERM_IN_ME_P (1 << ((ALPHA_Q_FORMAT)-2))  //0.25
#define ALPHA_FOR_NOISE_TERM_IN_ME_BREF (1 << ((ALPHA_Q_FORMAT)-2))  //0.25
#define ALPHA_FOR_NOISE_TERM_IN_RDOPT_P (1 << ((ALPHA_Q_FORMAT)-2))

#define ALPHA_FOR_NOISE_TERM (1 << ((ALPHA_Q_FORMAT)-2))

#define ALPHA_FOR_NOISE_TERM_IN_ME (ALPHA_FOR_NOISE_TERM)

#define ALPHA_FOR_NOISE_TERM_IN_RDOPT (ALPHA_FOR_NOISE_TERM)

#define ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT 50

#define ALPHA_FOR_ZERO_CODING_DECISIONS (ALPHA_FOR_NOISE_TERM_IN_RDOPT)

#define STIM_Q_FORMAT 8

#define USE_NOISE_TERM_IN_L0_ME (1 && (ADD_NOISE_TERM_TO_COST))

#define USE_NOISE_TERM_IN_ENC_LOOP (1 && (ADD_NOISE_TERM_TO_COST))

#define COMPUTE_NOISE_TERM_AT_THE_TU_LEVEL (1 && (USE_NOISE_TERM_IN_ENC_LOOP))

#define DISABLE_SUBPEL_REFINEMENT_WHEN_SRC_IS_NOISY (0 && (USE_NOISE_TERM_IN_L0_ME))

#define USE_NOISE_TERM_DURING_BICAND_SEARCH (1 && (USE_NOISE_TERM_IN_L0_ME))

#define DISABLE_BLK_MERGE_WHEN_NOISY (0 && (USE_NOISE_TERM_IN_L0_ME))

/* Macros for Noise detection implmentation */
#define NOISE_DETECT (ADD_NOISE_TERM_TO_COST)

#define PSY_RD_DEBUG_CTBX 2048
#define PSY_RD_DEBUG_CTBY 1600
#define DEBUG_POC_NO 0

#define DISABLE_LARGE_INTRA_PQ 1

#define EVERYWHERE_NOISY 0

#define DEBUG_PRINT_NOISE_SPATIAL 0

#define DEBUG_PRINT_NOISE_TEMPORAL 0

#define TEMPORAL_NOISE_DETECT (1 && (USE_NOISE_TERM_IN_L0_ME) && !(EVERYWHERE_NOISY))

#define MIN_NOISY_BLOCKS_CNT_16x16 7

#define ALTERNATE_METRIC 0

#define PSY_STRENGTH_CHROMA 2  // 0.5 in Q2

#define Q_PSY_STRENGTH_CHROMA 2

#define PSY_STRENGTH 4  // 0.5 in Q2

#define Q_PSY_STRENGTH 2

/* between 0 and 100 */
#define MIN_ENERGY_FOR_NOISE_PERCENT_16x16 20

/* normalised value between 0 and 1 */
#define MIN_VARIANCE_FOR_NOISE_16x16 0.6

/* HAD size is restricted to square blocks only. so we specify only one dimension */
#define HAD_BLOCK_SIZE_16x16 16

#define MIN_NUM_COEFFS_ABOVE_AVG_16x16 41

#define MIN_COEFF_AVG_ENERGY_16x16 0

#define MIN_NOISY_BLOCKS_CNT_8x8 30

/* between 0 and 100 */
#define MIN_ENERGY_FOR_NOISE_PERCENT_8x8 20

/* normalised value between 0 and 1 */
#define MIN_VARIANCE_FOR_NOISE_8x8 0.6

/* HAD size is restricted to square blocks only. so we specify only one dimension */
#define HAD_BLOCK_SIZE_8x8 8

#define MIN_NUM_COEFFS_ABOVE_AVG_8x8 17

#define MIN_COEFF_AVG_ENERGY_8x8 0

#define SATD_NOISE_FLOOR_THRESHOLD 16

#define ENABLE_DEBUG_PRINTS_IN_ME 0

#define RC_DEBUG_LEVEL_1 0

#define RC_2PASS_GOP_DEBUG 0

#define DUMP_NOISE_MAP 0

#define DISABLE_SKIP 0

#define DISABLE_NOISE_DETECTION_IN_P_PICS (0 && (NOISE_DETECT))

#define MAX_LAYER_ID_OF_B_PICS_WITHOUT_NOISE_DETECTION                                             \
    ((1 == (DISABLE_NOISE_DETECTION_IN_P_PICS)) ? 0 : 0)

#define DISABLE_INTRA_WHEN_NOISY (0 && (NOISE_DETECT))

#define DISABLE_BIPRED_MODES_WHEN_NOISY (0 && (ADD_NOISE_TERM_TO_COST))

#define TEMPORAL_VARIANCE_FACTOR 3  // in Q2

#define Q_TEMPORAL_VARIANCE_FACTOR 2

/* Actual Lambda -> (100 - ME_LAMBDA_DISCOUNT_WHEN_NOISY) * lambda / 100 */
/*(((100 * (ALPHA_FOR_NOISE_TERM_IN_ME) + (1 << ((ALPHA_Q_FORMAT) - 1)))) >> (ALPHA_Q_FORMAT))*/
#define ME_LAMBDA_DISCOUNT_WHEN_NOISY 50

/*(((100 * (ALPHA_FOR_NOISE_TERM_IN_RDOPT) + (1 << ((ALPHA_Q_FORMAT) - 1)))) >> (ALPHA_Q_FORMAT))*/
#define RDOPT_LAMBDA_DISCOUNT_WHEN_NOISY 25

#define DISABLE_SKIP_AND_MERGE_WHEN_NOISY (0 && (USE_NOISE_TERM_IN_ENC_LOOP))

#define NO_QP_OFFSET 0

#define CONVERT_SSDS_TO_SPATIAL_DOMAIN (1 || (USE_NOISE_TERM_IN_ENC_LOOP))

#define MAX_QP_WHERE_SPATIAL_SSD_ENABLED 18

#define USE_RECON_TO_EVALUATE_STIM_IN_RDOPT (1 && (USE_NOISE_TERM_IN_ENC_LOOP))

#define DISABLE_SAO_WHEN_NOISY (1 && (USE_NOISE_TERM_IN_ENC_LOOP))

#define MAX_TU_SIZE_WHEN_NOISY 64

#define RANDOMIZE_MERGE_IDX_WHEN_NOISY (0 && (USE_NOISE_TERM_IN_ENC_LOOP))

#define MAX_CU_SIZE_WHERE_MERGE_AND_SKIPS_ENABLED_AND_WHEN_NOISY 64

#define NUM_ELEMENTS_IN_RANDOMIZED_MERGE_IDX_LIST 113

#define NUM_MERGE_INDICES_TO_PICK_WHEN_LIST_RANDOMIZED_MAXIDX4                                     \
    ((DISABLE_SKIP_AND_MERGE_WHEN_NOISY) ? 0 : 2)

#define NUM_MERGE_INDICES_TO_PICK_WHEN_LIST_RANDOMIZED_MAXIDX3                                     \
    ((DISABLE_SKIP_AND_MERGE_WHEN_NOISY) ? 0 : 2)

#define NUM_MERGE_INDICES_TO_PICK_WHEN_LIST_RANDOMIZED_MAXIDX2                                     \
    ((DISABLE_SKIP_AND_MERGE_WHEN_NOISY) ? 0 : 1)

#define NUM_MERGE_INDICES_TO_PICK_WHEN_LIST_RANDOMIZED_MAXIDX1                                     \
    ((DISABLE_SKIP_AND_MERGE_WHEN_NOISY) ? 0 : 1)

#define USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS                                          \
    (1 && (COMPUTE_NOISE_TERM_AT_THE_TU_LEVEL))

#define BITPOS_IN_VQ_TOGGLE_FOR_CONTROL_TOGGLER 0

#define BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_NOISE_PRESERVATION 29

#define BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_1 5
#define BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_2 6
#define BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_3 7

#define MODULATE_LAMDA_WHEN_SPATIAL_MOD_ON 1

#define MODULATE_LAMDA_WHEN_TRAQO_MOD_ON 1

#define ENABLE_RUNTIME_ARCH_SWITCH 1

#define DISABLE_8X8CUS_IN_NREFBPICS_IN_P6 1

#define DISABLE_8X8CUS_IN_REFBPICS_IN_P6 (0 && (DISABLE_8X8CUS_IN_NREFBPICS_IN_P6))

#define DISABLE_8X8CUS_IN_PPICS_IN_P6 (0 && (DISABLE_8X8CUS_IN_REFBPICS_IN_P6))

#define L0ME_IN_OPENLOOP_MODE 0

#define DISABLE_INTRAS_IN_BPIC 0

#define MAX_RE_ENC 1

#define ENABLE_RDO_BASED_TU_RECURSION 1

#define ENABLE_TOP_DOWN_TU_RECURSION 1

#define INCLUDE_CHROMA_DURING_TU_RECURSION (0 && (ENABLE_RDO_BASED_TU_RECURSION))

#define PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS (0)

#define PROCESS_INTRA_AND_INTER_CU_TREES_SEPARATELY                                                \
    (0 && (PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS))

#define RESTRICT_NUM_PARTITION_LEVEL_L0ME_RESULTS_TO_1 1

#define RESTRICT_NUM_PARTITION_LEVEL_L1ME_RESULTS_TO_1 1

#define RESTRICT_NUM_PARTITION_LEVEL_L2ME_RESULTS_TO_1 1

#define RESTRICT_NUM_SEARCH_CANDS_PER_SEARCH_CAND_LOC 1

#define RESTRICT_NUM_2NX2N_TU_RECUR_CANDS 1

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

#define CREATE_SUBBLOCK2CSBFID_MAP(map, numMapElements, transSize, csbfBufStride)                  \
    {                                                                                              \
        WORD32 i, j;                                                                               \
                                                                                                   \
        WORD32 i4NumSubBlocksPerRow = transSize / 4;                                               \
        WORD32 i4NumSubBlocksPerColumn = i4NumSubBlocksPerRow;                                     \
                                                                                                   \
        ASSERT(numMapElements >= i4NumSubBlocksPerRow * i4NumSubBlocksPerColumn);                  \
                                                                                                   \
        for(i = 0; i < i4NumSubBlocksPerColumn; i++)                                               \
        {                                                                                          \
            for(j = 0; j < i4NumSubBlocksPerRow; j++)                                              \
            {                                                                                      \
                map[j + i * i4NumSubBlocksPerRow] = j + i * csbfBufStride;                         \
            }                                                                                      \
        }                                                                                          \
    }

#define COPY_CABAC_STATES(dest, src, size)                                                         \
    {                                                                                              \
        memcpy(dest, src, size);                                                                   \
    }

#define COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(dest, src, size)                                   \
    {                                                                                              \
        memcpy(dest, src, size);                                                                   \
    }

#define PAD_BUF(pu1_start, stride, wd, ht, p_x, p_y, plane, function_pointer1, function_pointer2)  \
    {                                                                                              \
        function_pointer1(pu1_start, stride, ht, wd, p_x);                                         \
        function_pointer2((pu1_start) - (p_x), stride, ht, wd + ((p_x) << 1), p_y);                \
    }

#define PAD_BUF_HOR(pu1_start, stride, ht, p_x, p_y, function_pointer)                             \
    {                                                                                              \
        function_pointer(pu1_start, stride, ht, p_x);                                              \
    }

#define PAD_BUF_VER(pu1_start, stride, wd, p_x, p_y, function_pointer)                             \
    {                                                                                              \
        function_pointer(pu1_start, stride, wd, p_y);                                              \
    }

#define POPULATE_PART_RESULTS(ps_part_results, ps_search_node)                                     \
    {                                                                                              \
        ps_part_results->i1_ref_idx = ps_search_node->i1_ref_idx;                                  \
        ps_part_results->i2_mv_x = ps_search_node->i2_mv_x;                                        \
        ps_part_results->i2_mv_y = ps_search_node->i2_mv_y;                                        \
        ps_part_results->i4_sad = ps_search_node->i4_sad;                                          \
    }

#define GET_IDX_CIRCULAR_BUF(idx, increment, size)                                                 \
    {                                                                                              \
        if(increment < 0)                                                                          \
        {                                                                                          \
            idx += increment;                                                                      \
            if(idx < 0)                                                                            \
            {                                                                                      \
                idx += size;                                                                       \
            }                                                                                      \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            idx += increment;                                                                      \
            if(idx >= size)                                                                        \
            {                                                                                      \
                idx %= size;                                                                       \
            }                                                                                      \
        }                                                                                          \
    }

#define CLIPUCHAR(x) CLIP3((x), 0, 255)

#define CLIPUCHAR10BIT(x) CLIP3((x), 0, 1023)

#define CEIL4(x) (((x + 3) >> 2) << 2)

#define CEIL8(x) (((x + 7) >> 3) << 3)

#define CEIL2(x) (((x + 1) >> 1) << 1)

#define CEIL16(x) (((x) + 15) & (~15))

#define CEIL_POW2(x, align) (((x) + (align)-1) & (~((align)-1)))

#define PAD_SUBPEL PAD_BUF
#define PAD_FPEL PAD_BUF
#define PAD_FPEL_HOR PAD_BUF_HOR
#define PAD_FPEL_VER PAD_BUF_VER

/* Defining a printf macro: To disable all prints inside codec in release mode */
#ifdef _DEBUG
#define DBG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DBG_PRINTF(...)
#endif

/*****************************************************************************/
/* Enumerations                                                              */
/*****************************************************************************/

typedef enum
{
    LEVEL1 = 30,
    LEVEL2 = 60,
    LEVEL2_1 = 63,
    LEVEL3 = 90,
    LEVEL3_1 = 93,
    LEVEL4 = 120,
    LEVEL4_1 = 123,
    LEVEL5 = 150,
    LEVEL5_1 = 153,
    LEVEL5_2 = 156,
    LEVEL6 = 180,
    LEVEL6_1 = 183,
    LEVEL6_2 = 186
} LEVEL_T;

typedef enum
{
    LIST_0,
    LIST_1,

    NUM_REF_LISTS

} REF_LISTS_t;

typedef enum SSD_TYPE_T
{
    NULL_TYPE = -1,
    SPATIAL_DOMAIN_SSD,
    FREQUENCY_DOMAIN_SSD

} SSD_TYPE_T;

#endif
