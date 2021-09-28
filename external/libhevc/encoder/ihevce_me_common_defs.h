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
* @file ihevce_me_common_defs.h
*
* @brief
*  This file contains structures and interface prototypes for header encoding
*
* @author
*    Ittiam
******************************************************************************
*/

#ifndef _IHEVCE_ME_COMMON_DEFS_H_
#define _IHEVCE_ME_COMMON_DEFS_H_

/****************************************************************************/
/* Constant Macros                                                          */
/****************************************************************************/
/**
*******************************************************************************
@brief We basically store an impossible and unique MV to identify intra blks
or CUs
*******************************************************************************
 */
#define INTRA_MV 0x4000
/**
*******************************************************************************
@brief MAX INT VAL is defined as follows so that adding the four candidates,
   still will be a positive value
*******************************************************************************
 */
#define MAX_INT_VAL (0x7FFFFFF)

/**
*******************************************************************************
@brief Max number of results stored in search result str (per partition) during
refinement search. Needed for memory allocation purposes
*******************************************************************************
 */
#define MAX_REFINE_RESULTS 4

/**
*******************************************************************************
@brief Maximum number of partitions in a CU (NxN case)
*******************************************************************************
 */
#define MAX_NUM_PARTS 4

/** As min CU size is 8, there can only be two partitions in a CU */
#define MAX_NUM_INTER_PARTS 2

/* 4 for the num of REF and 2 for num_results_per_part */
#define MAX_NUM_RESULTS_PER_PART_LIST 8

#define MAX_NUM_RESULTS_PER_PART 2

#define MAX_NUM_REF 12

#define NUM_BEST_ME_OUTPUTS 4

#define MAX_NUM_CLUSTERS_IN_ONE_REF_IDX 5

/* Assumption is (MAX_NUM_CANDS_BESTUNI >= MAX_NUM_CANDS_BESTALT) */
#define MAX_NUM_CANDS_BESTUNI 10

#define MAX_NUM_CANDS_BESTALT 10

#define MAX_NUM_MERGE_CANDTS 4 * (3 * MAX_NUM_CLUSTERS_IN_ONE_REF_IDX + 2 * MAX_NUM_CANDS_BESTUNI)

#define MAX_NUM_CLUSTERS_16x16 8

#define MAX_NUM_CLUSTERS_32x32 10

#define MAX_NUM_CLUSTERS_64x64 10

#define MAX_DISTANCE_FROM_CENTROID_16x16 4

#define MAX_DISTANCE_FROM_CENTROID_32x32 8

#define MAX_DISTANCE_FROM_CENTROID_64x64 16

#define MAX_DISTANCE_FROM_CENTROID_16x16_B 4

#define MAX_DISTANCE_FROM_CENTROID_32x32_B 8

#define MAX_DISTANCE_FROM_CENTROID_64x64_B 16

#define MAX_NUM_CLUSTERS_IN_VALID_16x16_BLK 3

#define MAX_NUM_CLUSTERS_IN_VALID_32x32_BLK 5

#define MAX_NUM_CLUSTERS_IN_VALID_64x64_BLK 5

#define ALL_INTER_COST_DIFF_THR 10

#define MAX_INTRA_PERCENTAGE 25

#define CLUSTER_DATA_DUMP 0

#define DISABLE_INTER_CANDIDATES 0

#define ENABLE_4CTB_EVALUATION 1

#define USE_2N_NBR 1

#define USE_CLUSTER_DATA_AS_BLK_MERGE_CANDTS 0

#define MAX_REFS_SEARCHABLE MAX_NUM_REF

#define DEBUG_TRACE_ENABLE 0

#define DISABLE_INTRA_IN_BPICS 1

#define DISABLE_L0_IPE_INTRA_IN_BPICS 1

#define DISABLE_L2_IPE_INTRA_IN_BPICS 0

#define DISABLE_L2_IPE_INTRA_IN_IPBPICS 0

#define DISABLE_L1_L2_IPE_INTRA_IN_BPICS 1

#define RC_DEPENDENCY_FOR_BPIC 1

#define DISABLE_L1_L2_IPE_INTRA_IN_IPBPICS 0

#define DISABLE_L2_IPE_IN_IPB_L1_IN_B 0

#define DISABLE_L2_IPE_IN_PB_L1_IN_B 1

#define DISBLE_CHILD_CU_EVAL_L0_IPE 0

#define FORCE_NXN_MODE_BASED_ON_OL_IPE 0

#define TEMPORAL_LAYER_DISABLE 0

#define COARSE_ME_OPT 1

#define NUM_RESULTS_TO_EXPORT_MS 3

#define NUM_RESULTS_TO_EXPORT_HS NUM_BEST_ME_OUTPUTS

#define NUM_RESULTS_TO_EXPORT_XS 2

#define DISABLE_MERGE 0

#define INTERP_OUT_BUF_SIZE (64 * 64)

/* NUM_BEST_ME_OUTPUTS - Maximum possible TU Recursion candidates */
/* 2 - Required for Hadamard Transform coefficients */
/* 2 - Required in 'hme_compute_pred_and_evaluate_bi' */
/* 5 of these are also used in 'hme_subpel_refine_cu_hs' */
#define MAX_NUM_PRED_BUFS_USED_FOR_PARTTYPE_DECISIONS (NUM_BEST_ME_OUTPUTS) + 2 + 2

#define MAX_WKG_MEM_SIZE_PER_THREAD                                                                \
    (MAX_NUM_PRED_BUFS_USED_FOR_PARTTYPE_DECISIONS) * (INTERP_OUT_BUF_SIZE)

/**
******************************************************************************
 *  @macro  OLD_XTREME_SPEED
 *  @brief Reverts the changes back to older Xtreme speed model
******************************************************************************
*/
#define OLD_XTREME_SPEED 0
#define OLD_HIGH_SPEED 0

/**
******************************************************************************
 *  @macro  BIT_EN
 *  @brief Enables the bit at a given bit position
******************************************************************************
*/
#define BIT_EN(x) (1 << (x))

/**
******************************************************************************
 *  @macros  ENABLE_mxn
 *  @brief Enables a type or a group of partitions. ENABLE_ALL_PARTS, enables all
 *  partitions, while others enable selected partitions. These can be used
 *  to set the mask of active partitions
******************************************************************************
*/
#define ENABLE_2Nx2N (BIT_EN(PART_ID_2Nx2N))
#define ENABLE_2NxN (BIT_EN(PART_ID_2NxN_T) | BIT_EN(PART_ID_2NxN_B))
#define ENABLE_Nx2N (BIT_EN(PART_ID_Nx2N_L) | BIT_EN(PART_ID_Nx2N_R))
#define ENABLE_NxN                                                                                 \
    (BIT_EN(PART_ID_NxN_TL) | BIT_EN(PART_ID_NxN_TR) | BIT_EN(PART_ID_NxN_BL) |                    \
     BIT_EN(PART_ID_NxN_BR))
#define ENABLE_2NxnU (BIT_EN(PART_ID_2NxnU_T) | BIT_EN(PART_ID_2NxnU_B))
#define ENABLE_2NxnD (BIT_EN(PART_ID_2NxnD_T) | BIT_EN(PART_ID_2NxnD_B))
#define ENABLE_nLx2N (BIT_EN(PART_ID_nLx2N_L) | BIT_EN(PART_ID_nLx2N_R))
#define ENABLE_nRx2N (BIT_EN(PART_ID_nRx2N_L) | BIT_EN(PART_ID_nRx2N_R))
#define ENABLE_AMP ((ENABLE_2NxnU) | (ENABLE_2NxnD) | (ENABLE_nLx2N) | (ENABLE_nRx2N))
#define ENABLE_ALL_PARTS                                                                           \
    ((ENABLE_2Nx2N) | (ENABLE_NxN) | (ENABLE_2NxN) | (ENABLE_Nx2N) | (ENABLE_AMP))

#define DISABLE_THE_CHILDREN_NODES(ps_parent_node)                                                 \
    {                                                                                              \
        (ps_parent_node)->ps_child_node_tl->is_node_valid = 0;                                     \
        (ps_parent_node)->ps_child_node_tr->is_node_valid = 0;                                     \
        (ps_parent_node)->ps_child_node_bl->is_node_valid = 0;                                     \
        (ps_parent_node)->ps_child_node_br->is_node_valid = 0;                                     \
    }

#define NULLIFY_THE_CHILDREN_NODES(ps_parent_node)                                                 \
    {                                                                                              \
        (ps_parent_node)->ps_child_node_tl = NULL;                                                 \
        (ps_parent_node)->ps_child_node_tr = NULL;                                                 \
        (ps_parent_node)->ps_child_node_bl = NULL;                                                 \
        (ps_parent_node)->ps_child_node_br = NULL;                                                 \
    }

#define DISABLE_ALL_KIN_OF_64x64_NODE(ps_tree_root)                                                \
    {                                                                                              \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root));                                                \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_tl);                              \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_tr);                              \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_bl);                              \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_br);                              \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_tl->ps_child_node_tl);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_tl->ps_child_node_tr);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_tl->ps_child_node_bl);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_tl->ps_child_node_br);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_tr->ps_child_node_tl);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_tr->ps_child_node_tr);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_tr->ps_child_node_bl);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_tr->ps_child_node_br);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_bl->ps_child_node_tl);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_bl->ps_child_node_tr);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_bl->ps_child_node_bl);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_bl->ps_child_node_br);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_br->ps_child_node_tl);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_br->ps_child_node_tr);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_br->ps_child_node_bl);            \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_br->ps_child_node_br);            \
    }

#define DISABLE_ALL_KIN_OF_32x32_NODE(ps_tree_root)                                                \
    {                                                                                              \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root));                                                \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_tl);                              \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_tr);                              \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_bl);                              \
        DISABLE_THE_CHILDREN_NODES((ps_tree_root)->ps_child_node_br);                              \
    }

#define ENABLE_THE_CHILDREN_NODES(ps_parent_node)                                                  \
    {                                                                                              \
        (ps_parent_node)->ps_child_node_tl->is_node_valid = 1;                                     \
        (ps_parent_node)->ps_child_node_tr->is_node_valid = 1;                                     \
        (ps_parent_node)->ps_child_node_bl->is_node_valid = 1;                                     \
        (ps_parent_node)->ps_child_node_br->is_node_valid = 1;                                     \
    }

#define CLIP_MV_WITHIN_RANGE(                                                                      \
    x, y, range, fpel_refine_extent, hpel_refine_extent, qpel_refine_extent)                       \
    {                                                                                              \
        WORD16 i4_range_erosion_metric;                                                            \
                                                                                                   \
        i4_range_erosion_metric =                                                                  \
            ((fpel_refine_extent) << 2) + ((hpel_refine_extent) << 1) + (qpel_refine_extent);      \
        i4_range_erosion_metric += 2;                                                              \
        i4_range_erosion_metric >>= 2;                                                             \
                                                                                                   \
        if((x) > ((range)->i2_max_x - i4_range_erosion_metric))                                    \
            (x) = ((range)->i2_max_x - i4_range_erosion_metric);                                   \
        if((x) < ((range)->i2_min_x + i4_range_erosion_metric))                                    \
            (x) = ((range)->i2_min_x + i4_range_erosion_metric);                                   \
        if((y) > ((range)->i2_max_y - i4_range_erosion_metric))                                    \
            (y) = ((range)->i2_max_y - i4_range_erosion_metric);                                   \
        if((y) < ((range)->i2_min_y + i4_range_erosion_metric))                                    \
            (y) = ((range)->i2_min_y + i4_range_erosion_metric);                                   \
    }

/****************************************************************************/
/* Enumerations                                                             */
/****************************************************************************/
/**


******************************************************************************
 *  @enum  CU_SIZE_T
 *  @brief Enumerates all possible CU sizes (8x8 to 64x64)
******************************************************************************
*/
typedef enum
{
    CU_INVALID = -1,
    CU_8x8 = 0,
    CU_16x16,
    CU_32x32,
    CU_64x64,
    NUM_CU_SIZES
} CU_SIZE_T;

/**
******************************************************************************
 *  @enum  PART_TYPE_T
 *  @brief Defines all possible partition splits within a inter CU
******************************************************************************
*/
typedef enum
{
    PRT_INVALID = -1,
    PRT_2Nx2N = 0,
    PRT_2NxN,
    PRT_Nx2N,
    PRT_NxN,
    PRT_2NxnU,
    PRT_2NxnD,
    PRT_nLx2N,
    PRT_nRx2N,
    MAX_PART_TYPES
} PART_TYPE_T;

/**
******************************************************************************
 *  @enum  PART_ID_T
 *  @brief Defines all possible partition ids within a inter CU
******************************************************************************
*/
typedef enum
{
    PART_ID_INVALID = -1,
    PART_ID_2Nx2N = 0,
    /* These 2 belong to 2NxN Part */
    PART_ID_2NxN_T = 1,
    PART_ID_2NxN_B = 2,
    /* These 2 belong to Nx2N */
    PART_ID_Nx2N_L = 3,
    PART_ID_Nx2N_R = 4,

    /* 4 partitions of NxN */
    PART_ID_NxN_TL = 5,
    PART_ID_NxN_TR = 6,
    PART_ID_NxN_BL = 7,
    PART_ID_NxN_BR = 8,

    /*************************************************************************/
    /*  ________                                                             */
    /* |________|-->2NxnU_T                                                  */
    /* |        |                                                            */
    /* |        |-->2NxnU_B                                                  */
    /* |________|                                                            */
    /*************************************************************************/
    PART_ID_2NxnU_T = 9,
    PART_ID_2NxnU_B = 10,

    /*************************************************************************/
    /*  ________                                                             */
    /* |        |                                                            */
    /* |        |-->2NxnD_T                                                  */
    /* |________|                                                            */
    /* |________|-->2NxnD_B                                                  */
    /*************************************************************************/
    PART_ID_2NxnD_T = 11,
    PART_ID_2NxnD_B = 12,

    /*************************************************************************/
    /*  ________                                                             */
    /* | |      |                                                            */
    /* | |      |-->nLx2N_R                                                  */
    /* | |      |                                                            */
    /* |_|______|                                                            */
    /*  |                                                                    */
    /*  v                                                                    */
    /* nLx2N_L                                                               */
    /*************************************************************************/
    PART_ID_nLx2N_L = 13,
    PART_ID_nLx2N_R = 14,

    /*************************************************************************/
    /*  ________                                                             */
    /* |      | |                                                            */
    /* |      | |-->nRx2N_R                                                  */
    /* |      | |                                                            */
    /* |______|_|                                                            */
    /*  |                                                                    */
    /*  v                                                                    */
    /* nRx2N_L                                                               */
    /*************************************************************************/
    /* AMP 12x16 and 4x16 split */
    PART_ID_nRx2N_L = 15,
    PART_ID_nRx2N_R = 16,
    TOT_NUM_PARTS = 17
} PART_ID_T;

/**
******************************************************************************
*  @enum  CU_POS_T
*  @brief Position of a block wrt its parent in the CU tree
******************************************************************************
*/
typedef enum
{
    POS_NA = -1,
    POS_TL = 0,
    POS_TR = 1,
    POS_BL = 2,
    POS_BR = 3
} CU_POS_T;

typedef CU_POS_T TU_POS_T;

/****************************************************************************/
/* Structures                                                               */
/****************************************************************************/

/**
******************************************************************************
 *  @struct  range_prms_t
 *  @brief   Indicates valid range of MV for a given blk/cu/ctb
******************************************************************************
 */
typedef struct
{
    /** Min x value possible, precision inferred from context */
    WORD16 i2_min_x;
    /** Max x value possible, precision inferred from context */
    WORD16 i2_max_x;
    /** Min y value possible, precision inferred from context */
    WORD16 i2_min_y;
    /** Max y value possible, precision inferred from context */
    WORD16 i2_max_y;
} range_prms_t;

/**
******************************************************************************
 *  MACRO for enabling Dynamical Vertical Search Range Support
 *  Note : Should be always 1, else part is not supported
******************************************************************************
 */
#define DVSR_CHANGES 1

/**
******************************************************************************
 *  @struct  dyn_range_prms_t
 *  @brief   Indicates Dynamic search range for a given blk/cu/ctb
******************************************************************************
 */
typedef struct
{
    /** Min x value possible */
    //WORD16 i2_dyn_min_x;
    /** Max x value possible */
    //WORD16 i2_dyn_max_x;
    /** Min y value possible */
    WORD16 i2_dyn_min_y;
    /** Max y value possible */
    WORD16 i2_dyn_max_y;

    /** Pic order count */
    WORD32 i4_poc;

} dyn_range_prms_t;

/**
******************************************************************************
 *  @macro  INIT_DYN_SEARCH_PRMS
 *  @brief   Initializes this dyn_range_prms_t structure. Can be used to zero
 *          out the range
******************************************************************************
 */
#define INIT_DYN_SEARCH_PRMS(x, ref_poc)                                                           \
    {                                                                                              \
        (x)->i2_dyn_min_y = 0;                                                                     \
        (x)->i2_dyn_max_y = 0;                                                                     \
        (x)->i4_poc = ref_poc;                                                                     \
    }

typedef struct
{
    WORD16 mvx;

    WORD16 mvy;

    /* 0=>mv is not a part of bi-pred mv */
    /* 1=>inverse of case 0 */
    UWORD8 is_uni;

    WORD16 pixel_count;

    WORD32 sdi;

} mv_data_t;

/**
******************************************************************************
*  @brief  This struct is stores the search result for a prediction unit (PU)
******************************************************************************
*/

typedef struct
{
    /**
     *  PU attributes likes mvs, refids, pred mode, wdt, heigt, ctbx/y offsets etc
     */
    pu_t pu;

    /* mv cost for this pu */
    WORD32 i4_mv_cost;

    /* total cost for this pu */
    WORD32 i4_tot_cost;

    WORD32 i4_sdi;
} pu_result_t;

/**
******************************************************************************
*  @brief  This struct is stores the search result for partition type of CU
******************************************************************************
*/
typedef struct
{
    /** part results for a part type */
    pu_result_t as_pu_results[MAX_NUM_INTER_PARTS];

    UWORD8 *pu1_pred;

    WORD32 i4_pred_stride;

    /* total cost for part type    */
    WORD32 i4_tot_cost;

    /* TU split flag : tu_split_flag[0] represents the transform splits
     *  for CU size <= 32, for 64x64 each ai4_tu_split_flag corresponds
     *  to respective 32x32  */
    /* For a 8x8 TU - 1 bit used to indicate split */
    /* For a 16x16 TU - LSB used to indicate winner between 16 and 8 TU's. 4 other bits used to indicate split in each 8x8 quadrant */
    /* For a 32x32 TU - See above */
    WORD32 ai4_tu_split_flag[4];

    /* TU early cbf : tu_early_cbf[0] represents the transform splits
     *  for CU size <= 32, for 64x64 each ai4_tu_early_cbf corresponds
     *  to respective 32x32  */
    WORD32 ai4_tu_early_cbf[4];

    /* Populate the tu_split flag cost for the candidates */
    WORD32 i4_tu_split_cost;

    /** partition type : shall be one of PART_TYPE_T */
    UWORD8 u1_part_type;
} part_type_results_t;

/**
******************************************************************************
 *  @struct  part_results_t
 *  @brief   Basic structure used for storage of search results, specification
 *  of init candidates for search etc. This structure is complete for
 *  specification of mv and cost for a given direction of search (L0/L1) but
 *  does not carry information of what type of partition it represents.
******************************************************************************
 */
typedef struct
{
    /** Motion vector X component */
    WORD16 i2_mv_x;

    /** Motion vector Y component */
    WORD16 i2_mv_y;

    /** Ref id, as specified in terms of Lc, unified list */
    WORD8 i1_ref_idx;

    /** SAD / SATD stored here */
    WORD32 i4_sad;
} part_results_t;

/**
******************************************************************************
*  @brief  This struct is used for storing output of me search or block merge
 *         and also all of the intermediate results required
******************************************************************************
*/
typedef struct
{
    /**
     * X and y offsets w.r.t. CTB start in encode layers. For non encode
     * layers, these may typically be 0
     */
    UWORD8 u1_x_off;

    UWORD8 u1_y_off;

    /** cu size as per the CU_SIZE_T enumeration   */
    UWORD8 u1_cu_size;

    WORD32 i4_inp_offset;

    /** best results of a CU sorted in increasing cost   */
    part_type_results_t *ps_best_results;

    /** active partition mask for this CU  */
    WORD32 i4_part_mask;

    /** number of best results mainted for every PU  */
    UWORD8 u1_num_best_results;

    /** Split flag to indicate whether current CU is split or not */
    UWORD8 u1_split_flag;

} inter_cu_results_t;

/**
******************************************************************************
*  @brief  This struct is used for storing input of me search in the form of
 *         pu_results_t structure which is given to hme_decide_part_types as i/p
******************************************************************************
*/
typedef struct
{
    /** ptrs to multiple pu results of a CU. Can be seperated out as seperate structure*/
    pu_result_t *aps_pu_results[2][TOT_NUM_PARTS];

    /** max number of best results mainted for a partition in L0*/
    UWORD8 u1_num_results_per_part_l0[TOT_NUM_PARTS];

    /** max number of best results mainted for a partition in L*/
    UWORD8 u1_num_results_per_part_l1[TOT_NUM_PARTS];
} inter_pu_results_t;

/**
******************************************************************************
 *  @struct  me_results_16x16_t
 *  @brief   Contains complete search result for a CU for a given type of
 *           partition split. Holds ptrs to results for each partition, with
 *           information of partition type.
******************************************************************************
 */
typedef struct
{
    /**
     * X and y offsets w.r.t. CTB start in encode layers. For non encode
     * layers, these may typically be 0
     */
    UWORD8 u1_x_off;

    UWORD8 u1_y_off;

    /**
     * Type of partition that the CU is split into, for which this
     * result is relevant
     */
    PART_TYPE_T e_part_type;

    /**
     * Pointer to results of each individual partitions. Note that max
     * number of partitions a CU can be split into is MAX_NUM_PARTS
     * 3 => L0 best, L1 best and best across L0 and L1
     */
    part_results_t as_part_result[MAX_NUM_PARTS][3];

    /* Contains the best uni dir for each partition type */
    /* enabled for this 16x16 block */
    WORD32 ai4_best_uni_dir[MAX_NUM_PARTS];

    /* Contains the best pred dir for each partition type */
    /* enabled for this 16x16 block */
    WORD32 ai4_best_pred_dir[MAX_NUM_PARTS];
} me_results_16x16_t;

/**
******************************************************************************
 *  @struct  me_results_8x8_t
 *  @brief   Contains complete search result for a CU for a given type of
 *           partition split. Holds ptrs to results for each partition, with
 *           information of partition type.
 * @assumptions e_part_type is always PRT_2Nx2N
******************************************************************************
 */
typedef struct
{
    /**
     * X and y offsets w.r.t. CTB start in encode layers. For non encode
     * layers, these may typically be 0
     */
    UWORD8 u1_x_off;

    UWORD8 u1_y_off;

    /**
     * Type of partition that the CU is split into, for which this
     * result is relevant
     */
    PART_TYPE_T e_part_type;

    /**
     * Pointer to results of each individual partitions. Note that max
     * number of partitions a CU can be split into is MAX_NUM_PARTS
     * 3 => L0 best, L1 best and best across L0 and L1
     */
    part_results_t as_part_result[2];

    /* Contains the best uni dir for each partition type */
    /* enabled for this 16x16 block */
    WORD32 i4_best_uni_dir;

    /* Contains the best pred dir for each partition type */
    /* enabled for this 16x16 block */
    WORD32 i4_best_pred_dir;
} me_results_8x8_t;

/**
******************************************************************************
 *  @struct  cluster_mv_list_t
 *  @brief   Contains data computed by the clustering algorithm
******************************************************************************
 */
typedef struct
{
    mv_t as_mv[MAX_NUM_MERGE_CANDTS];

    WORD32 num_mvs;

} cluster_mv_list_t;

/**
******************************************************************************
 *  @struct  qpel_input_buf_cfg_t
 *  @brief   For QPEL averaging, this descriptor (typically outcome of lookup)
 *           contains info related to the 2 fpel/hpel planes that are to be
 *           averaged along wiht the exact offsets
******************************************************************************
 */
typedef struct
{
    /** id of buf1 for input of averaging: 0-3 */
    WORD8 i1_buf_id1;

    /**
     * x and y offset in buf 1 w.r.t. colocated input point after correcting
     * for fpel mvx and mvy
     */
    WORD8 i1_buf_xoff1;
    WORD8 i1_buf_yoff1;

    /** id of buf2 for input of averaging: 0-3 */
    WORD8 i1_buf_id2;

    /**
     * x and y offset in buf 2 w.r.t. colocated input point after correcting
     * for fpel mvx and mvy
     */
    WORD8 i1_buf_xoff2;
    WORD8 i1_buf_yoff2;
} qpel_input_buf_cfg_t;

typedef struct
{
    UWORD8 *apu1_pred_bufs[MAX_NUM_PRED_BUFS_USED_FOR_PARTTYPE_DECISIONS];

    UWORD32 u4_pred_buf_usage_indicator;
} hme_pred_buf_mngr_t;

#endif
