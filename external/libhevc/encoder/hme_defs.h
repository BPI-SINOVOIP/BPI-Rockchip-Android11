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
* \file hme_defs.h
*
* \brief
*    Important definitions, enumerations, macros and structures used by ME
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _HME_DEFS_H_
#define _HME_DEFS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
/**
*******************************************************************************
@brief Blk size of the CTB in the max possible case
*******************************************************************************
 */
#define CTB_BLK_SIZE 64

/**
*******************************************************************************
@brief Maximun number of results per partition
*******************************************************************************
 */
#define MAX_RESULTS_PER_PART 2

/**
*******************************************************************************
@brief Not used currently
*******************************************************************************
 */
#define MAX_NUM_UNIFIED_RESULTS 10
#define MAX_NUM_CTB_NODES 10

/**
*******************************************************************************
@brief For 64x64 CTB, we have 16x16 MV grid for prediction purposes (cost calc)
This has 1 padding at boundaries for causal neighbours
*******************************************************************************
 */
#define CTB_MV_GRID_PAD 1

/**
*******************************************************************************
@brief number of bits per bin
*******************************************************************************
 */
#define HME_CABAC_BITS_PER_BIN 0.5

/**
*******************************************************************************
@brief bin count to bit count conversion
*******************************************************************************
 */
#define HME_GET_CAB_BIT(x) (U08(((x)*HME_CABAC_BITS_PER_BIN + 0.5)))

/**
*******************************************************************************
@brief Columns in the MV grid
*******************************************************************************
 */
#define NUM_COLUMNS_IN_CTB_GRID (((CTB_BLK_SIZE) >> 2) + (2 * CTB_MV_GRID_PAD))

/**
*******************************************************************************
@brief Rows in MV grid
*******************************************************************************
 */
#define NUM_ROWS_IN_CTB_GRID (NUM_COLUMNS_IN_CTB_GRID)

/**
*******************************************************************************
@brief Total number of MVs held in CTB grid for prediction pourposes
*******************************************************************************
 */
#define NUM_MVS_IN_CTB_GRID ((NUM_COLUMNS_IN_CTB_GRID) * (NUM_ROWS_IN_CTB_GRID))

/**
*******************************************************************************
@brief Max number of candidates used for refinement during CU merge stage
*******************************************************************************
 */
#define MAX_MERGE_CANDTS 64

/**
*******************************************************************************
@brief For BIDIR refinement, we use 2I-P0 as input, done max at CTB level, so
stride for this input is 64
*******************************************************************************
 */
#define BACK_PREDICTION_INPUT_STRIDE 64

/**
*******************************************************************************
@brief We basically store an impossible and unique MV to identify intra blks
or CUs
*******************************************************************************
 */
#define INTRA_MV 0x4000

/**
*******************************************************************************
@brief Defines the largest CTB supported by HME
*******************************************************************************
 */
#define HME_MAX_CTB_SIZE 64

/**
*******************************************************************************
@brief Maximum number of 16x16 blks possible in a CTB. The basic search unit
in the encode layer is 16x16
*******************************************************************************
 */
#define HME_MAX_16x16_IN_CTB ((HME_MAX_CTB_SIZE >> 4) * (HME_MAX_CTB_SIZE >> 4))

/**
*******************************************************************************
@brief Max number of 8x8s possible in a CTB, this in other words is also the
maximum number of CUs possible in a CTB
*******************************************************************************
 */
#define HME_MAX_8x8_IN_CTB ((HME_MAX_CTB_SIZE >> 3) * (HME_MAX_CTB_SIZE >> 3))

/**
*******************************************************************************
@brief Maximum number of init candts supported for refinement search.
*******************************************************************************
 */
#define MAX_INIT_CANDTS 60

/**
*******************************************************************************
@brief Maximum MV in X and Y directions in fullpel units allowed in any layer
Any computed range for MV hasto be within this
*******************************************************************************
 */
#define MAX_MV_X_FINEST 1024
#define MAX_MV_Y_FINEST 512

#define MAX_NUM_RESULTS 10

#define USE_MODIFIED 1

#define ENABLE_EXPLICIT_SEARCH_IN_P_IN_L0 1

#define ENABLE_EXPLICIT_SEARCH_IN_PQ 0

/**
*******************************************************************************
@brief Driven by reasoning that we can tolerate an error of 4 in global mv
 in coarsest layer per comp, assuming we have search range of 1024x512, the mv
 range in coarse layer is 128x64, total bins is then 256/4 x 128/4 or 2K bins
*******************************************************************************
 */
#define LOG_MAX_NUM_BINS 11
#define MAX_NUM_BINS (1 << LOG_MAX_NUM_BINS)

#define NEXT_BLOCK_OFFSET_IN_L0_ME 22

#define PREV_BLOCK_OFFSET_IN_L0_ME 6

#define COLOCATED_BLOCK_OFFSET 2

#define COLOCATED_4X4_NEXT_BLOCK_OFFSET 14

#define MAP_X_MAX 16

#define MAP_Y_MAX 16

#define NUM_POINTS_IN_RECTANGULAR_GRID 9

/*
******************************************************************************
@brief Maximum number of elements in the sigmaX and sigmaX-Square array
computed at 4x4 level for any CU size
******************************************************************************
*/
#define MAX_NUM_SIGMAS_4x4 256

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/**
*******************************************************************************
@brief Calculates number of blks in picture, given width, ht, and a variable
shift that controls basic blk size
*******************************************************************************
 */
#define GET_NUM_BLKS_IN_PIC(wd, ht, shift, num_cols, num_blks)                                     \
    {                                                                                              \
        S32 y, rnd;                                                                                \
        rnd = (1 << shift) - 1;                                                                    \
        num_cols = (wd + rnd) >> shift;                                                            \
        y = (ht + rnd) >> shift;                                                                   \
        num_blks = num_cols * y;                                                                   \
    }

#define COUNT_CANDS(a, b)                                                                          \
    {                                                                                              \
        b = (((a) & (1))) + (((a >> 1) & (1))) + (((a >> 2) & (1))) + (((a >> 3) & (1))) +         \
            (((a >> 4) & (1))) + (((a >> 5) & (1))) + (((a >> 6) & (1))) + (((a >> 7) & (1))) +    \
            (((a >> 8) & (1)));                                                                    \
    }

#define COPY_MV_TO_SEARCH_NODE(node, mv, pref, refid, shift)                                       \
    {                                                                                              \
        (node)->s_mv.i2_mvx = (mv)->i2_mv_x;                                                       \
        (node)->s_mv.i2_mvy = (mv)->i2_mv_y;                                                       \
        (node)->i1_ref_idx = *pref;                                                                \
        (node)->u1_is_avail = 1;                                                                   \
                                                                                                   \
        /* Can set the availability flag for MV Pred purposes */                                   \
        if(((node)->i1_ref_idx < 0) || ((node)->s_mv.i2_mvx == INTRA_MV))                          \
        {                                                                                          \
            (node)->u1_is_avail = 0;                                                               \
            (node)->i1_ref_idx = refid;                                                            \
            (node)->s_mv.i2_mvx = 0;                                                               \
            (node)->s_mv.i2_mvy = 0;                                                               \
        }                                                                                          \
        (node)->s_mv.i2_mvx >>= (shift);                                                           \
        (node)->s_mv.i2_mvy >>= (shift);                                                           \
        (node)->u1_subpel_done = (shift) ? 0 : 1;                                                  \
    }

#define COMPUTE_MVD(ps_mv, ps_data, cumulative_mv_distance)                                        \
    {                                                                                              \
        S32 mvx_q8 = (ps_mv)->mvx << 8;                                                            \
        S32 mvy_q8 = (ps_mv)->mvy << 8;                                                            \
        S32 mvcx_q8 = (ps_data)->s_centroid.i4_pos_x_q8;                                           \
        S32 mvcy_q8 = (ps_data)->s_centroid.i4_pos_y_q8;                                           \
                                                                                                   \
        S32 mvdx_q8 = mvx_q8 - mvcx_q8;                                                            \
        S32 mvdy_q8 = mvy_q8 - mvcy_q8;                                                            \
                                                                                                   \
        S32 mvdx = (mvdx_q8 + (1 << 7)) >> 8;                                                      \
        S32 mvdy = (mvdy_q8 + (1 << 7)) >> 8;                                                      \
                                                                                                   \
        S32 mvd = ABS(mvdx) + ABS(mvdy);                                                           \
                                                                                                   \
        cumulative_mv_distance += mvd;                                                             \
    }

#define STATS_COLLECTOR_MV_INSERT(                                                                 \
    ps_mv_store, num_mvs_stored, mvx_cur, mvy_cur, stats_struct, check_for_duplicate, ref_idx)     \
    {                                                                                              \
        S32 i4_j;                                                                                  \
        (stats_struct).f_num_cands_being_processed++;                                              \
        check_for_duplicate = 0;                                                                   \
                                                                                                   \
        for(i4_j = 0; i4_j < (num_mvs_stored); i4_j++)                                             \
        {                                                                                          \
            if(((ps_mv_store)[i4_j].s_mv.i2_mvx == (mvx_cur)) &&                                   \
               ((ps_mv_store)[i4_j].s_mv.i2_mvy == (mvy_cur)) &&                                   \
               ((ps_mv_store)[i4_j].i1_ref_idx == ref_idx))                                        \
            {                                                                                      \
                (stats_struct).f_num_duplicates_amongst_processed++;                               \
                check_for_duplicate = 0;                                                           \
                break;                                                                             \
            }                                                                                      \
        }                                                                                          \
                                                                                                   \
        if(i4_j == (num_mvs_stored))                                                               \
        {                                                                                          \
            (ps_mv_store)[i4_j].s_mv.i2_mvx = (mvx_cur);                                           \
            (ps_mv_store)[i4_j].s_mv.i2_mvy = (mvy_cur);                                           \
            (ps_mv_store)[i4_j].i1_ref_idx = ref_idx;                                              \
            (num_mvs_stored)++;                                                                    \
        }                                                                                          \
    }

#define UPDATE_CLUSTER_METADATA_POST_MERGE(ps_cluster)                                             \
    {                                                                                              \
        S32 m;                                                                                     \
                                                                                                   \
        S32 num_clusters_evaluated = 0;                                                            \
                                                                                                   \
        for(m = 0; num_clusters_evaluated < (ps_cluster)->num_clusters; m++)                       \
        {                                                                                          \
            if(!((ps_cluster)->as_cluster_data[m].is_valid_cluster))                               \
            {                                                                                      \
                if(-1 != (ps_cluster)->as_cluster_data[m].ref_id)                                  \
                {                                                                                  \
                    (ps_cluster)->au1_num_clusters[(ps_cluster)->as_cluster_data[m].ref_id]--;     \
                }                                                                                  \
            }                                                                                      \
            else                                                                                   \
            {                                                                                      \
                num_clusters_evaluated++;                                                          \
            }                                                                                      \
        }                                                                                          \
    }

#define SET_VALUES_FOR_TOP_REF_IDS(ps_cluster_blk, best_uni_ref, best_alt_ref, num_ref)            \
    {                                                                                              \
        ps_cluster_blk->best_uni_ref = best_uni_ref;                                               \
        ps_cluster_blk->best_alt_ref = best_alt_ref;                                               \
        ps_cluster_blk->num_refs = num_ref;                                                        \
    }

#define MAP_X_MAX 16
#define MAP_Y_MAX 16

#define CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(                                                   \
    ps_dedup_enabler, num_cands, mvx, mvy, check_for_duplicate)                                    \
    {                                                                                              \
        S32 center_mvx;                                                                            \
        S32 center_mvy;                                                                            \
        S32 mvdx;                                                                                  \
        S32 mvdy;                                                                                  \
        U32 *pu4_node_map;                                                                         \
        S32 columnar_presence;                                                                     \
                                                                                                   \
        (check_for_duplicate) = 0;                                                                 \
        {                                                                                          \
            subpel_dedup_enabler_t *ps_dedup = &(ps_dedup_enabler)[0];                             \
            center_mvx = ps_dedup->i2_mv_x;                                                        \
            center_mvy = ps_dedup->i2_mv_y;                                                        \
            pu4_node_map = ps_dedup->au4_node_map;                                                 \
                                                                                                   \
            mvdx = (mvx)-center_mvx;                                                               \
            mvdy = (mvy)-center_mvy;                                                               \
                                                                                                   \
            if(((mvdx < MAP_X_MAX) && (mvdx >= -MAP_X_MAX)) &&                                     \
               ((mvdy < MAP_Y_MAX) && (mvdy >= -MAP_Y_MAX)))                                       \
            {                                                                                      \
                columnar_presence = pu4_node_map[MAP_X_MAX + mvdx];                                \
                                                                                                   \
                if(0 == (columnar_presence & (1U << (MAP_Y_MAX + mvdy))))                          \
                {                                                                                  \
                    columnar_presence |= (1U << (MAP_Y_MAX + mvdy));                               \
                    pu4_node_map[MAP_X_MAX + mvdx] = columnar_presence;                            \
                }                                                                                  \
                else                                                                               \
                {                                                                                  \
                    (check_for_duplicate) = 1;                                                     \
                }                                                                                  \
            }                                                                                      \
        }                                                                                          \
    }

#define BUMP_OUTLIER_CLUSTERS(ps_cluster_blk, sdi_threshold)                                       \
    {                                                                                              \
        outlier_data_t as_outliers[MAX_NUM_CLUSTERS_64x64 + 1];                                    \
                                                                                                   \
        S32 j, k;                                                                                  \
                                                                                                   \
        S32 num_clusters_evaluated = 0;                                                            \
        S32 num_clusters = ps_cluster_blk->num_clusters;                                           \
        S32 num_outliers_present = 0;                                                              \
                                                                                                   \
        for(j = 0; num_clusters_evaluated < num_clusters; j++)                                     \
        {                                                                                          \
            cluster_data_t *ps_data = &ps_cluster_blk->as_cluster_data[j];                         \
                                                                                                   \
            if(!ps_data->is_valid_cluster)                                                         \
            {                                                                                      \
                continue;                                                                          \
            }                                                                                      \
                                                                                                   \
            num_clusters_evaluated++;                                                              \
                                                                                                   \
            if((ps_data->num_mvs == 1) && (ps_data->as_mv[0].sdi < sdi_threshold) &&               \
               (ps_cluster_blk->au1_num_clusters[ps_data->ref_id] >                                \
                MAX_NUM_CLUSTERS_IN_ONE_REF_IDX))                                                  \
            {                                                                                      \
                as_outliers[num_outliers_present].cluster_id = j;                                  \
                as_outliers[num_outliers_present].ref_idx = ps_data->ref_id;                       \
                as_outliers[num_outliers_present].sdi = ps_data->as_mv[0].sdi;                     \
                num_outliers_present++;                                                            \
            }                                                                                      \
        }                                                                                          \
                                                                                                   \
        for(j = 0; j < (num_outliers_present - 1); j++)                                            \
        {                                                                                          \
            for(k = (j + 1); k < num_outliers_present; k++)                                        \
            {                                                                                      \
                if(as_outliers[j].sdi > as_outliers[k].sdi)                                        \
                {                                                                                  \
                    as_outliers[MAX_NUM_CLUSTERS_64x64] = as_outliers[j];                          \
                    as_outliers[j] = as_outliers[k];                                               \
                    as_outliers[k] = as_outliers[MAX_NUM_CLUSTERS_64x64];                          \
                }                                                                                  \
            }                                                                                      \
        }                                                                                          \
                                                                                                   \
        for(j = 0; j < (num_outliers_present); j++)                                                \
        {                                                                                          \
            S32 ref_idx = as_outliers[j].ref_idx;                                                  \
                                                                                                   \
            if((ps_cluster_blk->au1_num_clusters[ref_idx] > MAX_NUM_CLUSTERS_IN_ONE_REF_IDX))      \
            {                                                                                      \
                ps_cluster_blk->as_cluster_data[as_outliers[j].cluster_id].is_valid_cluster = 0;   \
                ps_cluster_blk->num_clusters--;                                                    \
                ps_cluster_blk->au1_num_clusters[ref_idx]--;                                       \
            }                                                                                      \
        }                                                                                          \
    }

#define ADD_CLUSTER_CENTROID_AS_CANDS_FOR_BLK_MERGE(                                               \
    ps_cluster_data, ps_range_prms, ps_list, ps_mv, is_ref_in_l0, ref_idx)                         \
    {                                                                                              \
        ps_list = &(ps_cluster_data)->as_mv_list[!(is_ref_in_l0)][(ref_idx)];                      \
        ps_mv = &ps_list->as_mv[ps_list->num_mvs];                                                 \
                                                                                                   \
        ps_mv->i2_mvx = (ps_centroid->i4_pos_x_q8 + (1 << 7)) >> 8;                                \
        ps_mv->i2_mvy = (ps_centroid->i4_pos_y_q8 + (1 << 7)) >> 8;                                \
                                                                                                   \
        CLIP_MV_WITHIN_RANGE(ps_mv->i2_mvx, ps_mv->i2_mvy, (ps_range_prms), 0, 0, 0);              \
                                                                                                   \
        ps_cluster_data->ai4_ref_id_valid[!(is_ref_in_l0)][(ref_idx)] = 1;                         \
                                                                                                   \
        ps_list->num_mvs++;                                                                        \
    }

#define COPY_SEARCH_CANDIDATE_DATA(node, mv, pref, refid, shift)                                   \
    {                                                                                              \
        (node)->ps_mv->i2_mvx = (mv)->i2_mv_x;                                                     \
        (node)->ps_mv->i2_mvy = (mv)->i2_mv_y;                                                     \
        (node)->i1_ref_idx = *pref;                                                                \
        (node)->u1_is_avail = 1;                                                                   \
                                                                                                   \
        /* Can set the availability flag for MV Pred purposes */                                   \
        if(((node)->i1_ref_idx < 0) || ((node)->ps_mv->i2_mvx == INTRA_MV))                        \
        {                                                                                          \
            (node)->u1_is_avail = 0;                                                               \
            (node)->i1_ref_idx = refid;                                                            \
            (node)->ps_mv->i2_mvx = 0;                                                             \
            (node)->ps_mv->i2_mvy = 0;                                                             \
        }                                                                                          \
        (node)->ps_mv->i2_mvx >>= (shift);                                                         \
        (node)->ps_mv->i2_mvy >>= (shift);                                                         \
        (node)->u1_subpel_done = (shift) ? 0 : 1;                                                  \
    }
/**
*******************************************************************************
* @macro MIN_NODE
* @brief Returns the search node with lesser cost
*******************************************************************************
 */
#define MIN_NODE(a, b) (((a)->i4_tot_cost < (b)->i4_tot_cost) ? (a) : (b))

/**
*******************************************************************************
* @macro MAX_NODE
* @brief Returns search node with higher cost
*******************************************************************************
 */
#define MAX_NODE(a, b) (((a)->i4_tot_cost >= (b)->i4_tot_cost) ? (a) : (b))

/**
******************************************************************************
 *  @macro  HME_INV_WT_PRED
 *  @brief Implements inverse of wt pred formula. Actual wt pred formula is
 *  ((input * wt) + rnd) >> shift) + offset
******************************************************************************
*/
#define HME_INV_WT_PRED(inp, wt, off, shift) (((((inp) - (off)) << (shift)) + ((wt) >> 1)) / (wt))
#define HME_INV_WT_PRED1(inp, wt, off, shift)                                                      \
    (((((inp) - (off)) << (shift)) * wt + (1 << 14)) >> 15)

/**
******************************************************************************
 *  @macro  HME_WT_PRED
 *  @brief Implements wt pred formula as per spec
******************************************************************************
*/
#define HME_WT_PRED(p0, p1, w0, w1, rnd, shift)                                                    \
    (((((S32)w0) * ((S32)p0) + ((S32)w1) * ((S32)p1)) >> shift) + rnd)

/**
******************************************************************************
 *  @macro PREFETCH_BLK
 *  @brief Prefetches a block of data into cahce before hand
******************************************************************************
*/

/**
******************************************************************************
 *  @macro INSERT_NEW_NODE
 *  @brief Inserts a new search node in a list if it is unique; helps in
           removing duplicate nodes/candidates
******************************************************************************
*/
#define PREFETCH_BLK(pu1_src, src_stride, lines, type)                                             \
    {                                                                                              \
        WORD32 ctr;                                                                                \
        for(ctr = 0; ctr < lines; ctr++)                                                           \
        {                                                                                          \
            PREFETCH((char const *)pu1_src, type);                                                 \
            pu1_src += src_stride;                                                                 \
        }                                                                                          \
    }

#define INSERT_UNIQUE_NODE(                                                                        \
    as_nodes, num_nodes, new_node, au4_map, center_x, center_y, use_hashing)                       \
    {                                                                                              \
        WORD32 k;                                                                                  \
        UWORD32 map;                                                                               \
        WORD32 delta_x, delta_y;                                                                   \
        delta_x = (new_node).ps_mv->i2_mvx - (center_x);                                           \
        delta_y = (new_node).ps_mv->i2_mvy - (center_y);                                           \
        map = 0;                                                                                   \
                                                                                                   \
        if((use_hashing) && (delta_x < MAP_X_MAX) && (delta_x >= (-MAP_X_MAX)) &&                  \
           (delta_y < MAP_Y_MAX) && (delta_y >= (-MAP_Y_MAX)))                                     \
        {                                                                                          \
            map = (au4_map)[delta_x + MAP_X_MAX];                                                  \
            if(0 == (map & (1U << (delta_y + MAP_Y_MAX))))                                         \
            {                                                                                      \
                (new_node).s_mv = (new_node).ps_mv[0];                                             \
                (as_nodes)[(num_nodes)] = (new_node);                                              \
                ((num_nodes))++;                                                                   \
                map |= 1U << (delta_y + MAP_Y_MAX);                                                \
                (au4_map)[delta_x + MAP_X_MAX] = map;                                              \
            }                                                                                      \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            for(k = 0; k < ((num_nodes)); k++)                                                     \
            {                                                                                      \
                /* Search is this node is already present in unique list */                        \
                if(((as_nodes)[k].s_mv.i2_mvx == (new_node).ps_mv->i2_mvx) &&                      \
                   ((as_nodes)[k].s_mv.i2_mvy == (new_node).ps_mv->i2_mvy) &&                      \
                   ((as_nodes)[k].i1_ref_idx == (new_node).i1_ref_idx))                            \
                {                                                                                  \
                    /* This is duplicate node; need not be inserted */                             \
                    break;                                                                         \
                }                                                                                  \
            }                                                                                      \
            if(k == ((num_nodes)))                                                                 \
            {                                                                                      \
                /* Insert new node only if it is not duplicate node */                             \
                (new_node).s_mv = (new_node).ps_mv[0];                                             \
                (as_nodes)[k] = (new_node);                                                        \
                ((num_nodes))++;                                                                   \
            }                                                                                      \
        }                                                                                          \
    }

/**
******************************************************************************
 *  @macro INSERT_NEW_NODE
 *  @brief Inserts a new search node in a list if it is unique; helps in
           removing duplicate nodes/candidates
******************************************************************************
*/
#define INSERT_NEW_NODE_NOMAP(as_nodes, num_nodes, new_node, implicit_layer)                       \
    {                                                                                              \
        WORD32 k;                                                                                  \
        if(!implicit_layer)                                                                        \
        {                                                                                          \
            for(k = 0; k < (num_nodes); k++)                                                       \
            {                                                                                      \
                /* Search is this node is already present in unique list */                        \
                if((as_nodes[k].s_mv.i2_mvx == new_node.s_mv.i2_mvx) &&                            \
                   (as_nodes[k].s_mv.i2_mvy == new_node.s_mv.i2_mvy))                              \
                {                                                                                  \
                    /* This is duplicate node; need not be inserted */                             \
                    break;                                                                         \
                }                                                                                  \
            }                                                                                      \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            for(k = 0; k < (num_nodes); k++)                                                       \
            {                                                                                      \
                /* Search is this node is already present in unique list */                        \
                if((as_nodes[k].s_mv.i2_mvx == new_node.s_mv.i2_mvx) &&                            \
                   (as_nodes[k].s_mv.i2_mvy == new_node.s_mv.i2_mvy) &&                            \
                   (as_nodes[k].i1_ref_idx == new_node.i1_ref_idx))                                \
                {                                                                                  \
                    /* This is duplicate node; need not be inserted */                             \
                    break;                                                                         \
                }                                                                                  \
            }                                                                                      \
        }                                                                                          \
                                                                                                   \
        if(k == (num_nodes))                                                                       \
        {                                                                                          \
            /* Insert new node only if it is not duplicate node */                                 \
            as_nodes[k] = new_node;                                                                \
            (num_nodes)++;                                                                         \
        }                                                                                          \
    }
/**
******************************************************************************
 *  @macro INSERT_NEW_NODE_NOMAP_ALTERNATE
 *  @brief Inserts a new search node in a list if it is unique; helps in
           removing duplicate nodes/candidates
******************************************************************************
*/
#define INSERT_NEW_NODE_NOMAP_ALTERNATE(as_nodes, num_nodes, new_node, result_num, part_id)        \
    {                                                                                              \
        WORD32 k;                                                                                  \
        WORD32 part_id_1 = (new_node->i4_num_valid_parts > 8) ? new_node->ai4_part_id[part_id]     \
                                                              : part_id;                           \
        for(k = 0; k < (num_nodes); k++)                                                           \
        {                                                                                          \
            /* Search is this node is already present in unique list */                            \
            if((as_nodes[k].s_mv.i2_mvx == new_node->i2_mv_x[result_num][part_id_1]) &&            \
               (as_nodes[k].s_mv.i2_mvy == new_node->i2_mv_y[result_num][part_id_1]) &&            \
               (as_nodes[k].i1_ref_idx == new_node->i2_ref_idx[result_num][part_id_1]))            \
            {                                                                                      \
                /* This is duplicate node; need not be inserted */                                 \
                break;                                                                             \
            }                                                                                      \
        }                                                                                          \
                                                                                                   \
        if(k == (num_nodes))                                                                       \
        {                                                                                          \
            /* Insert new node only if it is not duplicate node */                                 \
            as_nodes[k].i4_tot_cost = (WORD32)new_node->i2_tot_cost[result_num][part_id_1];        \
            as_nodes[k].i4_mv_cost = (WORD32)new_node->i2_mv_cost[result_num][part_id_1];          \
            as_nodes[k].s_mv.i2_mvx = new_node->i2_mv_x[result_num][part_id_1];                    \
            as_nodes[k].s_mv.i2_mvy = new_node->i2_mv_y[result_num][part_id_1];                    \
            as_nodes[k].i1_ref_idx = (WORD8)new_node->i2_ref_idx[result_num][part_id_1];           \
            as_nodes[k].u1_part_id = new_node->ai4_part_id[part_id];                               \
            (num_nodes)++;                                                                         \
        }                                                                                          \
    }

#define INSERT_NEW_NODE(                                                                           \
    as_nodes, num_nodes, new_node, implicit_layer, au4_map, center_x, center_y, use_hashing)       \
    {                                                                                              \
        WORD32 k;                                                                                  \
        UWORD32 map;                                                                               \
        WORD32 delta_x, delta_y;                                                                   \
        delta_x = (new_node).s_mv.i2_mvx - center_x;                                               \
        delta_y = (new_node).s_mv.i2_mvy - center_y;                                               \
        map = 0;                                                                                   \
        if((delta_x < MAP_X_MAX) && (delta_x >= (-MAP_X_MAX)) && (delta_y < MAP_Y_MAX) &&          \
           (delta_y >= (-MAP_Y_MAX)) && (use_hashing))                                             \
        {                                                                                          \
            map = (au4_map)[delta_x + MAP_X_MAX];                                                  \
            if(0 == (map & (1U << (delta_y + MAP_Y_MAX))))                                         \
            {                                                                                      \
                (as_nodes)[(num_nodes)] = (new_node);                                              \
                (num_nodes)++;                                                                     \
                map |= 1U << (delta_y + MAP_Y_MAX);                                                \
                (au4_map)[delta_x + MAP_X_MAX] = map;                                              \
            }                                                                                      \
        }                                                                                          \
        else if(!(implicit_layer))                                                                 \
        {                                                                                          \
            for(k = 0; k < (num_nodes); k++)                                                       \
            {                                                                                      \
                /* Search is this node is already present in unique list */                        \
                if(((as_nodes)[k].s_mv.i2_mvx == (new_node).s_mv.i2_mvx) &&                        \
                   ((as_nodes)[k].s_mv.i2_mvy == (new_node).s_mv.i2_mvy))                          \
                {                                                                                  \
                    /* This is duplicate node; need not be inserted */                             \
                    break;                                                                         \
                }                                                                                  \
            }                                                                                      \
            if(k == (num_nodes))                                                                   \
            {                                                                                      \
                /* Insert new node only if it is not duplicate node */                             \
                (as_nodes)[k] = (new_node);                                                        \
                (num_nodes)++;                                                                     \
            }                                                                                      \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            for(k = 0; k < (num_nodes); k++)                                                       \
            {                                                                                      \
                /* Search is this node is already present in unique list */                        \
                if(((as_nodes)[k].s_mv.i2_mvx == (new_node).s_mv.i2_mvx) &&                        \
                   ((as_nodes)[k].s_mv.i2_mvy == (new_node).s_mv.i2_mvy) &&                        \
                   ((as_nodes)[k].i1_ref_idx == (new_node).i1_ref_idx))                            \
                {                                                                                  \
                    /* This is duplicate node; need not be inserted */                             \
                    break;                                                                         \
                }                                                                                  \
            }                                                                                      \
            if(k == (num_nodes))                                                                   \
            {                                                                                      \
                /* Insert new node only if it is not duplicate node */                             \
                (as_nodes)[k] = (new_node);                                                        \
                (num_nodes)++;                                                                     \
            }                                                                                      \
        }                                                                                          \
    }

#define COMPUTE_DIFF_MV(mvdx, mvdy, inp_node, mv_p_x, mv_p_y, inp_sh, pred_sh)                     \
    {                                                                                              \
        mvdx = (inp_node)->s_mv.i2_mvx << (inp_sh);                                                \
        mvdy = (inp_node)->s_mv.i2_mvy << (inp_sh);                                                \
        mvdx -= ((mv_p_x) << (pred_sh));                                                           \
        mvdy -= ((mv_p_y) << (pred_sh));                                                           \
    }

#define COMPUTE_MV_DIFFERENCE(mvdx, mvdy, inp_node, mv_p_x, mv_p_y, inp_sh, pred_sh)               \
    {                                                                                              \
        mvdx = (inp_node)->ps_mv->i2_mvx << (inp_sh);                                              \
        mvdy = (inp_node)->ps_mv->i2_mvy << (inp_sh);                                              \
        mvdx -= ((mv_p_x) << (pred_sh));                                                           \
        mvdy -= ((mv_p_y) << (pred_sh));                                                           \
    }

/**
******************************************************************************
 *  @enum  CU_MERGE_RESULT_T
 *  @brief Describes the results of merge, whether successful or not
******************************************************************************
*/
typedef enum
{
    CU_MERGED,
    CU_SPLIT
} CU_MERGE_RESULT_T;

/**
******************************************************************************
 *  @enum  PART_ORIENT_T
 *  @brief Describes the orientation of partition (vert/horz, left/rt)
******************************************************************************
*/
typedef enum
{
    VERT_LEFT,
    VERT_RIGHT,
    HORZ_TOP,
    HORZ_BOT
} PART_ORIENT_T;

/**
******************************************************************************
 *  @enum  GRID_PT_T
 *  @brief For a  3x3 rect grid, nubers each pt as shown
*     5   2   6
*     1   0   3
*     7   4   8
******************************************************************************
*/
typedef enum
{
    PT_C = 0,
    PT_L = 1,
    PT_T = 2,
    PT_R = 3,
    PT_B = 4,
    PT_TL = 5,
    PT_TR = 6,
    PT_BL = 7,
    PT_BR = 8,
    NUM_GRID_PTS
} GRID_PT_T;

/**
******************************************************************************
 *  @macro  IS_POW
 *  @brief Returns whwehter a number is power of 2
******************************************************************************
*/
#define IS_POW_2(x) (!((x) & ((x)-1)))

/**
******************************************************************************
 *  @macro  GRID_ALL_PTS_VALID
 *  @brief For a 3x3 rect grid, this can be used to enable all pts in grid
******************************************************************************
*/
#define GRID_ALL_PTS_VALID 0x1ff

/**
******************************************************************************
 *  @macro  GRID_DIAMOND_ENABLE_ALL
 *  @brief If we search diamond, this enables all 5 pts of diamond (including centre)
******************************************************************************
*/
#define GRID_DIAMOND_ENABLE_ALL                                                                    \
    (BIT_EN(PT_C) | BIT_EN(PT_L) | BIT_EN(PT_T) | BIT_EN(PT_R) | BIT_EN(PT_B))

/**
******************************************************************************
 *  @macro  GRID_RT_3_INVALID, GRID_LT_3_INVALID,GRID_TOP_3_INVALID,GRID_BOT_3_INVALID
 *  @brief For a square grid search, depending on where the best result is
 *  we can optimise search for next iteration by invalidating some pts
******************************************************************************
*/
#define GRID_RT_3_INVALID ((GRID_ALL_PTS_VALID) ^ (BIT_EN(PT_TR) | BIT_EN(PT_R) | BIT_EN(PT_BR)))
#define GRID_LT_3_INVALID ((GRID_ALL_PTS_VALID) ^ (BIT_EN(PT_TL) | BIT_EN(PT_L) | BIT_EN(PT_BL)))
#define GRID_TOP_3_INVALID ((GRID_ALL_PTS_VALID) ^ (BIT_EN(PT_TL) | BIT_EN(PT_T) | BIT_EN(PT_TR)))
#define GRID_BOT_3_INVALID ((GRID_ALL_PTS_VALID) ^ (BIT_EN(PT_BL) | BIT_EN(PT_B) | BIT_EN(PT_BR)))

/**
******************************************************************************
 *  @enum  GMV_MVTYPE_T
 *  @brief Defines what type of GMV we need (thin lobe for a very spiky
 * distribution of mv or thick lobe for a blurred distrib of mvs
******************************************************************************
*/
typedef enum
{
    GMV_THICK_LOBE,
    GMV_THIN_LOBE,
    NUM_GMV_LOBES
} GMV_MVTYPE_T;

/**
******************************************************************************
 *  @enum  BLK_TYPE_T
 *  @brief Defines all possible inter blks possible
******************************************************************************
*/
typedef enum
{
    BLK_INVALID = -1,
    BLK_4x4 = 0,
    BLK_4x8,
    BLK_8x4,
    BLK_8x8,
    BLK_4x16,
    BLK_8x16,
    BLK_12x16,
    BLK_16x4,
    BLK_16x8,
    BLK_16x12,
    BLK_16x16,
    BLK_8x32,
    BLK_16x32,
    BLK_24x32,
    BLK_32x8,
    BLK_32x16,
    BLK_32x24,
    BLK_32x32,
    BLK_16x64,
    BLK_32x64,
    BLK_48x64,
    BLK_64x16,
    BLK_64x32,
    BLK_64x48,
    BLK_64x64,
    NUM_BLK_SIZES
} BLK_SIZE_T;

/**
******************************************************************************
 *  @enum  SEARCH_COMPLEXITY_T
 *  @brief For refinement layer, this decides the number of refinement candts
******************************************************************************
*/
typedef enum
{
    SEARCH_CX_LOW = 0,
    SEARCH_CX_MED = 1,
    SEARCH_CX_HIGH = 2
} SEARCH_COMPLEXITY_T;

/**
******************************************************************************
 *  @enum  CTB_BOUNDARY_TYPES_T
 *  @brief For pictures not a multiples of CTB horizontally or vertically, we
 *  define 4 unique cases, centre (full ctbs), bottom boundary (64x8k CTBs),
 *  right boundary (8mx64 CTBs), and bottom rt corner (8mx8k CTB)
******************************************************************************
*/
typedef enum
{
    CTB_CENTRE,
    CTB_BOT_PIC_BOUNDARY,
    CTB_RT_PIC_BOUNDARY,
    CTB_BOT_RT_PIC_BOUNDARY,
    NUM_CTB_BOUNDARY_TYPES,
} CTB_BOUNDARY_TYPES_T;

/**
******************************************************************************
 *  @enum  SEARCH_CANDIDATE_TYPE_T
 *  @brief Monikers for all sorts of search candidates used in ME
******************************************************************************
*/
typedef enum
{
    ILLUSORY_CANDIDATE = -1,
    ZERO_MV = 0,
    ZERO_MV_ALTREF,
    SPATIAL_LEFT0,
    SPATIAL_TOP0,
    SPATIAL_TOP_RIGHT0,
    SPATIAL_TOP_LEFT0,
    SPATIAL_LEFT1,
    SPATIAL_TOP1,
    SPATIAL_TOP_RIGHT1,
    SPATIAL_TOP_LEFT1,
    PROJECTED_COLOC0,
    PROJECTED_COLOC1,
    PROJECTED_COLOC2,
    PROJECTED_COLOC3,
    PROJECTED_COLOC4,
    PROJECTED_COLOC5,
    PROJECTED_COLOC6,
    PROJECTED_COLOC7,
    PROJECTED_COLOC_TR0,
    PROJECTED_COLOC_TR1,
    PROJECTED_COLOC_BL0,
    PROJECTED_COLOC_BL1,
    PROJECTED_COLOC_BR0,
    PROJECTED_COLOC_BR1,
    PROJECTED_TOP0,
    PROJECTED_TOP1,
    PROJECTED_TOP_RIGHT0,
    PROJECTED_TOP_RIGHT1,
    PROJECTED_TOP_LEFT0,
    PROJECTED_TOP_LEFT1,
    PROJECTED_RIGHT0,
    PROJECTED_RIGHT1,
    PROJECTED_BOTTOM0,
    PROJECTED_BOTTOM1,
    PROJECTED_BOTTOM_RIGHT0,
    PROJECTED_BOTTOM_RIGHT1,
    PROJECTED_BOTTOM_LEFT0,
    PROJECTED_BOTTOM_LEFT1,
    COLOCATED_GLOBAL_MV0,
    COLOCATED_GLOBAL_MV1,
    PROJECTED_TOP2,
    PROJECTED_TOP3,
    PROJECTED_TOP_RIGHT2,
    PROJECTED_TOP_RIGHT3,
    PROJECTED_TOP_LEFT2,
    PROJECTED_TOP_LEFT3,
    PROJECTED_RIGHT2,
    PROJECTED_RIGHT3,
    PROJECTED_BOTTOM2,
    PROJECTED_BOTTOM3,
    PROJECTED_BOTTOM_RIGHT2,
    PROJECTED_BOTTOM_RIGHT3,
    PROJECTED_BOTTOM_LEFT2,
    PROJECTED_BOTTOM_LEFT3,
    NUM_SEARCH_CAND_TYPES
} SEARCH_CANDIDATE_TYPE_T;

typedef enum
{
    ILLUSORY_LOCATION = -1,
    COLOCATED,
    COLOCATED_4x4_TR,
    COLOCATED_4x4_BL,
    COLOCATED_4x4_BR,
    LEFT,
    TOPLEFT,
    TOP,
    TOPRIGHT,
    RIGHT,
    BOTTOMRIGHT,
    BOTTOM,
    BOTTOMLEFT,
    NUM_SEARCH_CAND_LOCATIONS
} SEARCH_CAND_LOCATIONS_T;

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
#define ENABLE_SMP ((ENABLE_2NxN) | (ENABLE_Nx2N))
#define ENABLE_ALL_PARTS                                                                           \
    ((ENABLE_2Nx2N) | (ENABLE_NxN) | (ENABLE_2NxN) | (ENABLE_Nx2N) | (ENABLE_AMP))
#define ENABLE_SQUARE_PARTS ((ENABLE_2Nx2N) | (ENABLE_NxN))

/**
******************************************************************************
 *  @enum  MV_PEL_RES_T
 *  @brief Resolution of MV fpel/hpel/qpel units. Useful for maintaining
 *  predictors. During fpel search, candts, predictors etc are in fpel units,
 *  in subpel search, they are in subpel units
******************************************************************************
*/
typedef enum
{
    MV_RES_FPEL,
    MV_RES_HPEL,
    MV_RES_QPEL
} MV_PEL_RES_T;

/**
******************************************************************************
 *  @enum  HME_SET_MVPRED_RES
 *  @brief Sets resolution for predictor bank (fpel/qpel/hpel units)
******************************************************************************
*/
#define HME_SET_MVPRED_RES(ps_pred_ctxt, mv_pel_res) ((ps_pred_ctxt)->mv_pel = mv_pel_res)

/**
******************************************************************************
 *  @enum  HME_SET_MVPRED_DIR
 *  @brief Sets the direction, meaning L0/L1. Since L0 and L1 use separate
 *  candts, the pred ctxt for them hasto be maintained separately
******************************************************************************
*/
#define HME_SET_MVPRED_DIR(ps_pred_ctxt, pred_lx) ((ps_pred_ctxt)->pred_lx = pred_lx)

/**
******************************************************************************
 *  @brief macros to clip / check mv within specified range
******************************************************************************
 */
#define CHECK_MV_WITHIN_RANGE(x, y, range)                                                         \
    (((x) > (range)->i2_min_x) && ((x) < (range)->i2_max_x) && ((y) > (range)->i2_min_y) &&        \
     ((y) < (range)->i2_max_y))

#define CONVERT_MV_LIMIT_TO_QPEL(range)                                                            \
    {                                                                                              \
        (range)->i2_max_x <<= 2;                                                                   \
        (range)->i2_max_y <<= 2;                                                                   \
        (range)->i2_min_x <<= 2;                                                                   \
        (range)->i2_min_y <<= 2;                                                                   \
    }

#define CONVERT_MV_LIMIT_TO_FPEL(range)                                                            \
    {                                                                                              \
        (range)->i2_max_x >>= 2;                                                                   \
        (range)->i2_max_y >>= 2;                                                                   \
        (range)->i2_min_x >>= 2;                                                                   \
        (range)->i2_min_y >>= 2;                                                                   \
    }

/**
******************************************************************************
 *  @brief Swicth to debug the number of subpel search nodes
******************************************************************************
*/
#define DEBUG_SUBPEL_SEARCH_NODE_HS_COUNT 0

/**
******************************************************************************
 *  @typedef  SAD_GRID_T
 *  @brief Defines a 2D array type used to store SADs across grid and across
 * partition types
******************************************************************************
*/
typedef S32 SAD_GRID_T[9][MAX_NUM_PARTS];

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/**
******************************************************************************
 *  @struct  grid_node_t
 *  @brief stores a complete info for a candt
******************************************************************************
*/
typedef struct
{
    S16 i2_mv_x;
    S16 i2_mv_y;
    S08 i1_ref_idx;
} grid_node_t;

/**
******************************************************************************
 *  @struct  search_node_t
 *  @brief   Basic structure used for storage of search results, specification
 *  of init candidates for search etc. This structure is complete for
 *  specification of mv and cost for a given direction of search (L0/L1) but
 *  does not carry information of what type of partition it represents.
******************************************************************************
 */
typedef struct
{
    /** Motion vector */
    mv_t s_mv;

    /** Used in the hme_mv_clipper function to reduce loads and stores */
    mv_t *ps_mv;

    /** Ref id, as specified in terms of Lc, unified list */
    S08 i1_ref_idx;

    /** Flag to indicate whether mv is in fpel or QPEL units */
    U08 u1_subpel_done;

    /**
     * Indicates whether this node constitutes a valid predictor candt.
     * Since this structure also used for predictor candts, some candts may
     * not be available (anti causal or outside pic boundary). Availabilit
     * can be inferred using this flag.
     */
    U08 u1_is_avail;

    /**
     * Indicates partition Id to which this node belongs. Useful during
     * subpel / fullpel refinement search to identify partition whose
     * cost needs to be minimized
     */
    U08 u1_part_id;

    /** SAD / SATD stored here */
    S32 i4_sad;

    /**
     * Cost related to coding MV, multiplied by lambda
     * TODO : Entry may be redundant, can be removed
     */
    S32 i4_mv_cost;

    /** Total cost, (SAD + MV Cost) */
    S32 i4_tot_cost;

    /** Subpel_Dist_Improvement.
        It is the reduction in distortion (SAD or SATD) achieved
        from the full-pel stage to the sub-pel stage
    */
    S32 i4_sdi;

} search_node_t;

/**
******************************************************************************
 *  @macro  INIT_SEARCH_NODE
 *  @brief   Initializes this search_node_t structure. Can be used to zero
 *          out candts, set max costs in results etc
******************************************************************************
 */
#define INIT_SEARCH_NODE(x, a)                                                                     \
    {                                                                                              \
        (x)->s_mv.i2_mvx = 0;                                                                      \
        (x)->s_mv.i2_mvy = 0;                                                                      \
        (x)->i1_ref_idx = a;                                                                       \
        (x)->i4_tot_cost = MAX_32BIT_VAL;                                                          \
        (x)->i4_sad = MAX_32BIT_VAL;                                                               \
        (x)->u1_subpel_done = 0;                                                                   \
        (x)->u1_is_avail = 1;                                                                      \
    }

/**
******************************************************************************
 *  @struct  part_attr_t
 *  @brief   Geometric description of a partition w.r.t. CU start. Note that
 *           since this is used across various CU sizes, the inference of
 *           these members is to be done in the context of specific usage
******************************************************************************
 */
typedef struct
{
    /** Start of partition w.r.t. CU start in x dirn */
    U08 u1_x_start;
    /** Size of partitino w.r.t. CU start in x dirn */
    U08 u1_x_count;
    /** Start of partition w.r.t. CU start in y dirn */
    U08 u1_y_start;
    /** Size of partitino w.r.t. CU start in y dirn */
    U08 u1_y_count;
} part_attr_t;

/**
******************************************************************************
 *  @struct  search_candt_t
 *  @brief   Complete information for a given candt in any refinement srch
******************************************************************************
 */
typedef struct
{
    /** Points to the mv, ref id info. */
    search_node_t *ps_search_node;
    /** Number of refinemnts to be done for this candt */
    U08 u1_num_steps_refine;
} search_candt_t;

/**
******************************************************************************
 *  @struct  result_node_t
 *  @brief   Contains complete search result for a CU for a given type of
 *           partition split. Holds ptrs to results for each partition, with
 *           information of partition type.
******************************************************************************
 */
typedef struct
{
    /**
     * Type of partition that the CU is split into, for which this
     * result is relevant
     */
    PART_TYPE_T e_part_type;

    /**
     * Total cost of coding the CU (sum of costs of individual partitions
     * plus other possible CU level overheads)
     */
    S32 i4_tot_cost;

    /**
     * Pointer to results of each individual partitions. Note that max
     * number of partitions a CU can be split into is MAX_NUM_PARTS
     */
    search_node_t *ps_part_result[MAX_NUM_PARTS];

    /* TU split flag : tu_split_flag[0] represents the transform splits
     *  for CU size <= 32, for 64x64 each ai4_tu_split_flag corresponds
     *  to respective 32x32  */
    S32 ai4_tu_split_flag[4];

} result_node_t;

/**
******************************************************************************
 *  @struct  ctb_node_t
 *  @brief   Finalized information for a given CU or CTB. This is a recursive
 *           structure and can hence start at CTB level, recursing for every
 *           level of split till we hit leaf CUs in the CTB. At leaf node
 *           it contains info for coded non split CU, with child nodes being
 *           set to NULL
******************************************************************************
 */
typedef struct ctb_node_t
{
    /** x offset of this CU w.r.t. CTB start (0-63) */
    U08 u1_x_off;
    /** y offset of this C U w.r.t. CTB start (0-63) */
    U08 u1_y_off;
    /** Results of each partition in both directions L0,L1 */
    search_node_t as_part_results[MAX_NUM_PARTS][2];
    /**
     * Pointers to pred buffers. Note that the buffer may be allocated
     * at parent level or at this level
     */
    U08 *apu1_pred[2];
    /** Prediction direction for each partition: 0-L0, 1-L1, 2-BI */
    U08 u1_pred_dir[MAX_NUM_PARTS];
    /**
     * When pred direction is decided to be BI, we still store the best
     * uni pred dir (L0/L1) in this array, for RD Opt purposes
     */
    U08 u1_best_uni_dir[MAX_NUM_PARTS];
    /** Stride of pred buffer pointed to by apu1_pred member */
    S32 i4_pred_stride;
    /** Size of the CU that this node represents */
    CU_SIZE_T e_cu_size;
    /** For leaf CUs, this indicats type of partition (for e.g. PRT_2NxN) */
    PART_TYPE_T e_part_type;
    /** Below entries are for a CU level*/
    S32 i4_sad;
    S32 i4_satd;
    S32 i4_mv_cost;
    S32 i4_rate;
    S32 i4_dist;
    S32 i4_tot_cost;
    /** Best costs of each partitions, if partition is BI, then best cost across uni/bi */
    S32 ai4_part_costs[4];

    /* TU split flag : tu_split_flag[0] represents the transform splits
     *  for CU size <= 32, for 64x64 each ai4_tu_split_flag corresponds
     *  to respective 32x32  */
    /* For a 8x8 TU - 1 bit used to indicate split */
    /* For a 16x16 TU - LSB used to indicate winner between 16 and 8 TU's. 4 other bits used to indicate split in each 8x8 quadrant */
    /* For a 32x32 TU - See above */
    S32 ai4_tu_split_flag[4];

    /**
     * pointers to child nodes. If this node is split, then the below point
     * to children nodes (TL, TR, BL, BR) each of quarter size (w/2, h/2)
     * If this node not split, then below point to null
     */
    struct ctb_node_t *ps_tl;
    struct ctb_node_t *ps_tr;
    struct ctb_node_t *ps_bl;
    struct ctb_node_t *ps_br;
} ctb_node_t;

/**
******************************************************************************
 *  @struct  ctb_mem_mgr_t
 *  @brief   Memory manager structure for CTB level memory allocations of CTB
 *           nodes
******************************************************************************
 */
typedef struct
{
    /** Base memory ptr */
    U08 *pu1_mem;
    /** Amount used so far (running value) */
    S32 i4_used;
    /** Total memory available for this mem mgr */
    S32 i4_tot;

    /** Size of CTB node, and alignment requiremnts */
    S32 i4_size;
    S32 i4_align;
} ctb_mem_mgr_t;

/**
******************************************************************************
 *  @struct  buf_mgr_t
 *  @brief   Memory manager structure for CTB level buffer allocations on the
 *           fly, esp useful for pred bufs and working memory
******************************************************************************
 */
typedef struct
{
    /** base memory ptr */
    U08 *pu1_wkg_mem;
    /** total memory available */
    S32 i4_total;
    /** Memory used so far */
    S32 i4_used;
} buf_mgr_t;

/**
******************************************************************************
 *  @struct  pred_candt_nodes_t
 *  @brief   For a given partition and a given CU/blk, this has pointers to
 *           all the neighbouring and coloc pred candts. All the pred candts
 *           are stored as search_node_t structures itself.
******************************************************************************
 */
typedef struct
{
    search_node_t *ps_tl;
    search_node_t *ps_t;
    search_node_t *ps_tr;
    search_node_t *ps_bl;
    search_node_t *ps_l;
    search_node_t *ps_coloc;
    search_node_t *ps_zeromv;
    search_node_t **pps_proj_coloc;

    search_node_t *ps_mvp_node;
} pred_candt_nodes_t;

/**
******************************************************************************
 *  @struct  pred_ctxt_t
 *  @brief   For a given CU/blk, has complete prediction information for all
 *           types of partitions. Note that the pred candts are only pointed
 *           to, not actually stored here. This indirection is to avoid
 *           copies after each partition search, this way, the result of
 *           a partition is updated and the causally next partition
 *           automatically uses this result
******************************************************************************
 */
typedef struct
{
    pred_candt_nodes_t as_pred_nodes[TOT_NUM_PARTS];

    /**
     *  We use S + lambda * R to evaluate cost. Here S = SAD/SATD and lambda
     *  is the scaling of bits to S and R is bits of overhead (MV + mode).
     *  Choice of lambda depends on open loop / closed loop, Qp, temporal id
     *  and possibly CU depth. It is the caller's responsiblity to pass
     *  to this module the appropriate lambda.
     */
    S32 lambda;

    /** lambda is in Q format, so this is the downshift reqd */
    S32 lambda_q_shift;

    /** Prediction direction : PRED_L0 or PRED_L1 */
    S32 pred_lx;

    /** MV resolution: FPEL, HPEL or QPEL */
    S32 mv_pel;

    /** Points to the ref bits lookup 1 ptr for each PRED_Lx */
    U08 **ppu1_ref_bits_tlu;

    /**
     *  Points to the ref scale factor, for a given ref id k,
     *  to scale as per ref id m, we use entry k+MAX_NUM_REF*m
     */
    S16 *pi2_ref_scf;

    /**
     *  Flag that indicates whether T, TR and TL candidates used
     *  are causal or projected
     */
    U08 proj_used;

} pred_ctxt_t;

/**
******************************************************************************
 *  @struct  search_results_t
 *  @brief   For a given CU/blk, Stores all the results of ME search. Results
 *           are stored per partition, also the best results for CU are stored
 *           across partitions.
******************************************************************************
 */
typedef struct
{
    /** Size of CU for which this structure used */
    CU_SIZE_T e_cu_size;

    /**
     * X and y offsets w.r.t. CTB start in encode layers. For non encode
     * layers, these may typically be 0
     */
    U08 u1_x_off;
    U08 u1_y_off;

    /** Number of best results for this CU stored */
    U08 u1_num_best_results;

    /** Number of results stored per partition. */
    U08 u1_num_results_per_part;

    /**
     * Number of result planes active. This may be different from total
     * number of active references during search. For example, we may
     * have 4 active ref, 2 ineach dirn, but active result planes may
     * only be 2, one for L0 and 1 for L1
     */
    U08 u1_num_active_ref;
    /**
     * mask of active partitions, Totally 17 bits. For a given partition
     * id, as per PART_ID_T enum the corresponding bit position is 1/0
     * indicating that partition is active or inactive
     */
    S32 i4_part_mask;

    /** Points to partial results for each partition id
     *  Temporary hack for the bug: If +1 is not kept,
     *  it doesn't bit match with older version
     */
    search_node_t *aps_part_results[MAX_NUM_REF][TOT_NUM_PARTS];

    /**
     * Ptr to best results for the current CU post bi pred evaluation and
     * intra mode insertions
     */
    inter_cu_results_t *ps_cu_results;

    /** 2 pred ctxts, one for L0 and one for L1 */
    pred_ctxt_t as_pred_ctxt[2];

    /**
     * Pointer to a table that indicates whether the ref id
     * corresponds to past or future dirn. Input is ref id Lc form
     */

    U08 *pu1_is_past;

    /**
     * Overall best CU cost, while other entries store CU costs
     * in single direction, this is best CU cost, where each
     * partition cost is evaluated as best of uni/bi
     */
    S32 best_cu_cost;

    /**
     * Split_flag which is used for deciding if 16x16 CU is split or not
     */
    U08 u1_split_flag;
} search_results_t;

/**
******************************************************************************
 *  @struct  ctb_list_t
 *  @brief   Tree structure containing info for entire CTB. At top level
 *           it points to entire CTB results, with children nodes at each lvl
 *           being non null if split.
******************************************************************************
 */
typedef struct ctb_list_t
{
    /** Indicates whether this level split further */
    U08 u1_is_split;

    /** Number of result candts present */
    U08 u1_num_candts;

    /**
     * Whether this level valid. E.g. if we are at boundary, where only
     * left 2 32x32 are within pic boundary, then the parent is force split
     * at the children level, TR and BR are invalid.
     */
    U08 u1_is_valid;

    /**
     * IF this level is 16x16 then this mask indicates which 8x8 blks
     * are valid
     */
    U08 u1_8x8_mask;

    /** Search results of this CU */
    search_results_t *ps_search_results;

    /** Search results of this CU */
    inter_cu_results_t *ps_cu_results;

    /** Pointers to leaf nodes, if CU is split further, else null */
    struct ctb_list_t *ps_tl;
    struct ctb_list_t *ps_tr;
    struct ctb_list_t *ps_bl;
    struct ctb_list_t *ps_br;
} ctb_list_t;

/**
******************************************************************************
 *  @struct  layer_mv_t
 *  @brief   mv bank structure for a particular layer
******************************************************************************
 */
typedef struct
{
    /** Number of mvs for a given ref/pred dirn */
    S32 i4_num_mvs_per_ref;
    /** Number of reference for which results stored */
    S32 i4_num_ref;
    /** Number of mvs stored per blk. Product of above two */
    S32 i4_num_mvs_per_blk;
    /** Block size of the unit for which MVs stored */
    BLK_SIZE_T e_blk_size;
    /** Number of blocks present per row */
    S32 i4_num_blks_per_row;

    /** Number of mvs stored every row */
    S32 i4_num_mvs_per_row;

    /**
     * Max number of mvs allowed per row. The main purpose of this variable
     * is to resolve or detect discrepanceis between allocation time mem
     * and run time mem, when alloc time resolution and run time resolution
     * may be different
     */
    S32 max_num_mvs_per_row;

    /**
     * Pointer to mvs of 0, 0 blk, This is different from base since the
     * mv bank is padded all sides
    */
    hme_mv_t *ps_mv;

    /** Pointer to base of mv bank mvs */
    hme_mv_t *ps_mv_base;

    /** Pointers to ref idx.One to one correspondence between this and ps_mv*/
    S08 *pi1_ref_idx;
    /** Base of ref ids just like in case of ps_mv */
    S08 *pi1_ref_idx_base;

    /** Part mask for every blk, if stored, 1 per blk */
    U08 *pu1_part_mask;
} layer_mv_t;

/**
******************************************************************************
 *  @struct  mv_hist_t
 *  @brief   Histogram structure to calculate global mvs
******************************************************************************
 */
typedef struct
{
    S32 i4_num_rows;
    S32 i4_num_cols;
    S32 i4_shift_x;
    S32 i4_shift_y;
    S32 i4_lobe1_size;
    S32 i4_lobe2_size;
    S32 i4_min_x;
    S32 i4_min_y;
    S32 i4_num_bins;
    S32 ai4_bin_count[MAX_NUM_BINS];
} mv_hist_t;

typedef struct
{
    U08 u1_is_past;
} ref_attr_t;

/**
******************************************************************************
 *  @struct  layer_ctxt_t
 *  @brief   Complete information for the layer
******************************************************************************
 */
typedef struct
{
    /** Display Width of this layer */
    S32 i4_disp_wd;
    /** Display height of this layer */
    S32 i4_disp_ht;
    /** Width of this layer */
    S32 i4_wd;
    /** height of this layer */
    S32 i4_ht;
    /** Amount of padding of input in x dirn */
    S32 i4_pad_x_inp;
    /** Amount of padding of input in y dirn */
    S32 i4_pad_y_inp;
    /** Padding amount of recon in x dirn */
    S32 i4_pad_x_rec;
    /** padding amt of recon in y dirn */
    S32 i4_pad_y_rec;

    /**
     * Offset for recon. Since recon has padding, the 0, 0 start differs
     * from base of buffer
     */
    S32 i4_rec_offset;
    /** Offset for input, same explanation as recon */
    S32 i4_inp_offset;
    /** stride of input buffer */
    S32 i4_inp_stride;
    /** stride of recon buffer */
    S32 i4_rec_stride;
    /** Pic order count */
    S32 i4_poc;
    /** input pointer. */
    U08 *pu1_inp;
    /** Base of input. Add inp_offset to go to 0, 0 locn */
    U08 *pu1_inp_base;

    /** Pointer to 4 hpel recon planes */
    U08 *pu1_rec_fxfy;
    U08 *pu1_rec_hxfy;
    U08 *pu1_rec_fxhy;
    U08 *pu1_rec_hxhy;

    /** Global mv, one set per reference searched */
    hme_mv_t s_global_mv[MAX_NUM_REF][NUM_GMV_LOBES];

    /** Layer MV bank */
    layer_mv_t *ps_layer_mvbank;

    /** Pointer to list of recon buffers for each ref id, one ptr per plane */
    U08 **ppu1_list_rec_fxfy;
    U08 **ppu1_list_rec_hxfy;
    U08 **ppu1_list_rec_fxhy;
    U08 **ppu1_list_rec_hxhy;

    void **ppv_dep_mngr_recon;

    /** Pointer to list of input buffers for each ref id, one ptr per plane */
    U08 **ppu1_list_inp;

    /** Max MV in x and y direction supported at this layer resolution */
    S16 i2_max_mv_x;
    S16 i2_max_mv_y;

    /** Converts ref id (as per Lc list) to POC */
    S32 ai4_ref_id_to_poc_lc[MAX_NUM_REF];

    S32 ai4_ref_id_to_disp_num[MAX_NUM_REF];

    /** status of the buffer */
    S32 i4_is_free;

    /** idr gop number */
    S32 i4_idr_gop_num;

    /** is reference picture */
    S32 i4_is_reference;

    /** is non reference picture processed by me*/
    S32 i4_non_ref_free;

} layer_ctxt_t;

typedef S32 (*PF_MV_COST_FXN)(search_node_t *, pred_ctxt_t *, PART_ID_T, S32);

/**
 ******************************************************************************
 *  @struct refine_prms_t
 *  @brief  All the configurable input parameters for the refinement layer
 *
 *  @param encode: Whether this layer is encoded or not
 *  @param explicit_ref: If enabled, then the number of reference frames to
 *                       be searched is a function of coarsest layer num ref
                         frames. Else, number of references collapsed to 1/2
 *  @param i4_num_fpel_results : Number of full pel results to be allowed
 *  @param i4_num_results_per_part: Number of results stored per partition
 *  @param e_search_complexity: Decides the number of initial candts, refer
 *                               to SEARCH_COMPLEXITY_T
 *  @param i4_use_rec_in_fpel: Whether to use input buf or recon buf in fpel
 *  @param i4_enable_4x4_part : if encode is 0, we use 8x8 blks, if this param
                                enabled, then we do 4x4 partial sad update
 *  @param i4_layer_id        : id of this layer (0 = finest)
 *  @param i4_num_32x32_merge_results: number of 32x32 merged results stored
 *  @param i4_num_64x64_merge_results: number of 64x64 merged results stored
 *  @param i4_use_satd_cu_merge: Use SATD during CU merge
 *  @param i4_num_steps_hpel_refine : Number of steps during hpel refinement
 *  @param i4_num_steps_qpel_refine : Same as above but for qpel
 *  @param i4_use_satd_subpel : Use of SATD or SAD for subpel
 ******************************************************************************
*/
typedef struct
{
    /* This array is used to place upper bounds on the number of search candidates */
    /* that can be used per 'search cand location' */
    U08 au1_num_fpel_search_cands[NUM_SEARCH_CAND_LOCATIONS];

    U08 u1_max_2nx2n_tu_recur_cands;

    U08 u1_max_num_fpel_refine_centers;

    U08 u1_max_num_subpel_refine_centers;

    S32 i4_encode;
    S32 explicit_ref;
    S32 i4_num_ref_fpel;
    S32 i4_num_fpel_results;

    S32 i4_num_results_per_part;

    S32 i4_num_mvbank_results;
    SEARCH_COMPLEXITY_T e_search_complexity;
    S32 i4_use_rec_in_fpel;

    S32 i4_enable_4x4_part;
    S32 i4_layer_id;

    S32 i4_num_32x32_merge_results;
    S32 i4_num_64x64_merge_results;

    S32 i4_use_satd_cu_merge;

    S32 i4_num_steps_post_refine_fpel;
    S32 i4_num_steps_fpel_refine;
    S32 i4_num_steps_hpel_refine;
    S32 i4_num_steps_qpel_refine;
    S32 i4_use_satd_subpel;

    double *pd_intra_costs;
    S32 bidir_enabled;
    S32 lambda_inp;
    S32 lambda_recon;
    S32 lambda_q_shift;

    S32 limit_active_partitions;

    S32 sdi_threshold;

    U08 u1_use_lambda_derived_from_min_8x8_act_in_ctb;

    U08 u1_max_subpel_candts;

    U08 u1_max_subpel_candts_2Nx2N;
    U08 u1_max_subpel_candts_NxN;

    U08 u1_subpel_candt_threshold;

    /* Pointer to the array which has num best results for
        fpel refinement */
    U08 *pu1_num_best_results;

} refine_prms_t;

/**
******************************************************************************
 *  @struct  coarse_prms_t
 *  @brief   All the parameters passed to coarse layer search
******************************************************************************
 */
typedef struct
{
    /** ID of this layer, typically N-1 where N is tot layers */
    S32 i4_layer_id;

    /** Initial step size, valid if full search disabled */
    S32 i4_start_step;

    /** Maximum number of iterations to consider if full search disabled */
    S32 i4_max_iters;

    /** Number of reference frames to search */
    S32 i4_num_ref;

    /** Number of best results to maintain at this layer for projection */
    S32 num_results;

    /**
     * Enable or disable full search, if disabled then, we search around initial
     * candidates with early exit
     */
    S32 do_full_search;

    /** Values of lambda and the Q format */
    S32 lambda;
    S32 lambda_q_shift;

    /** Step size for full search 2/4 */
    S32 full_search_step;

} coarse_prms_t;

typedef struct
{
    /**
     * These pointers point to modified input, one each for one ref idx.
     * Instead of weighting the reference, we weight the input with inverse
     * wt and offset.
     * +1 for storing non weighted input
     */
    U08 *apu1_wt_inp[MAX_NUM_REF + 1];

    /* These are allocated once at the start of encoding */
    /* These are necessary only if wt_pred is switched on */
    /* Else, only a single buffer is used to store the */
    /* unweighed input */
    U08 *apu1_wt_inp_buf_array[MAX_NUM_REF + 1];

    /** Stores the weights and offsets for each ref */
    S32 a_wpred_wt[MAX_NUM_REF];
    S32 a_inv_wpred_wt[MAX_NUM_REF];
    S32 a_wpred_off[MAX_NUM_REF];
    S32 wpred_log_wdc;

    S32 ai4_shift_val[MAX_NUM_REF];
} wgt_pred_ctxt_t;

/**
******************************************************************************
 *  @struct  mv_refine_ctxt_t
 *  @brief   This structure contains important parameters used motion vector
             refinement
******************************************************************************
 */
typedef struct
{
    /* Added +7 in the array sizes below to make every array dimension
    16-byte aligned */
    /** Cost of best candidate for each partition*/
    MEM_ALIGN16 WORD16 i2_tot_cost[2][TOT_NUM_PARTS + 7];

    MEM_ALIGN16 WORD16 i2_stim_injected_cost[2][TOT_NUM_PARTS + 7];

    /** Motion vector cost for the best candidate of each partition*/
    MEM_ALIGN16 WORD16 i2_mv_cost[2][TOT_NUM_PARTS + 7];
    /** X component of the motion vector of the best candidate of each partition*/
    MEM_ALIGN16 WORD16 i2_mv_x[2][TOT_NUM_PARTS + 7];
    /** Y component of the motion vector of the best candidate of each partition*/
    MEM_ALIGN16 WORD16 i2_mv_y[2][TOT_NUM_PARTS + 7];
    /** Reference index of the best candidate of each partition*/
    MEM_ALIGN16 WORD16 i2_ref_idx[2][TOT_NUM_PARTS + 7];

    /** Partition id for the various partitions*/
    WORD32 ai4_part_id[TOT_NUM_PARTS + 1];
    /** Indicates the total number of valid partitions*/
    WORD32 i4_num_valid_parts;

    /** Number of candidates to refine through*/
    WORD32 i4_num_search_nodes;

    /** Stores the satd at the end of fullpel refinement*/
    WORD16 ai2_fullpel_satd[2][TOT_NUM_PARTS];
} mv_refine_ctxt_t;

typedef mv_refine_ctxt_t fullpel_refine_ctxt_t;
typedef mv_refine_ctxt_t subpel_refine_ctxt_t;
/**
******************************************************************************
 *  @struct  hme_search_prms_t
 *  @brief   All prms going to any fpel search
******************************************************************************
 */
typedef struct
{
    /** for explicit search, indicates which ref frm to search */
    /** for implicit search, indicates the prediction direction for search */
    S08 i1_ref_idx;

    /** Blk size used for search, and for which the search is done */
    BLK_SIZE_T e_blk_size;

    /** Number of init candts being searched */
    S32 i4_num_init_candts;

    S32 i4_num_steps_post_refine;

    /**
     * For coarser searches, bigger refinement is done around each candt
     * in these cases, this prm has start step
     */
    S32 i4_start_step;

    /** whether SATD to be used for srch */
    S32 i4_use_satd;

    /** if 1, we use recon frm for search (closed loop ) */
    S32 i4_use_rec;

    /** bitmask of active partitions */
    S32 i4_part_mask;

    /** x and y offset of blk w.r.t. pic start */
    S32 i4_x_off;
    S32 i4_y_off;

    /**
     * max number of iterations to search if early exit not hit
     * relevant only for coarser searches
     */
    S32 i4_max_iters;

    /** pointer to str holding all results for this blk */
    search_results_t *ps_search_results;

    /** pts to str having all search candt with refinement info */
    search_candt_t *ps_search_candts;
    /** pts to str having valid mv range info for this blk */
    range_prms_t *aps_mv_range[MAX_NUM_REF];
    /** cost compute fxnptr */
    PF_MV_COST_FXN pf_mv_cost_compute;

    /** when this str is set up for full search, indicates step size for same */
    S32 full_search_step;

    /** stride ofinp buffer */
    S32 i4_inp_stride;

    /** x and y offset of cu w.r.t. ctb start, set to 0 for non enc layer */
    S32 i4_cu_x_off;
    S32 i4_cu_y_off;

    /** base pointer to the de-duplicated search nodes */
    search_node_t *ps_search_nodes;

    /** number of de-duplicated nodes to be searched */
    S32 i4_num_search_nodes;

    fullpel_refine_ctxt_t *ps_fullpel_refine_ctxt;

    U32 au4_src_variance[TOT_NUM_PARTS];

    S32 i4_alpha_stim_multiplier;

    U08 u1_is_cu_noisy;

    ULWORD64 *pu8_part_src_sigmaX;
    ULWORD64 *pu8_part_src_sigmaXSquared;

} hme_search_prms_t;

/**
******************************************************************************
 *  @struct  hme_err_prms_t
 *  @brief   This is input prms struct for SAD/SATD computation
******************************************************************************
 */
typedef struct
{
    /** Ptr to input blk for which err computed */
    U08 *pu1_inp;

    U16 *pu2_inp;

    /** Ptr to ref blk after adjusting for mv and coordinates in pic */
    U08 *pu1_ref;

    U16 *pu2_ref;

    /** Stride of input buffer */
    S32 i4_inp_stride;
    /** Stride of ref buffer */
    S32 i4_ref_stride;
    /** Mask of active partitions. */
    S32 i4_part_mask;
    /** Mask of active grid pts. Refer to GRID_PT_T enum for bit posns */
    S32 i4_grid_mask;
    /**
     * Pointer to SAD Grid where SADs for each partition are stored.
     * The layout is as follows: If there are M total partitions
     * and N active pts in the grid, then the first N results contain
     * first partition, e.g. 2Nx2N. Next N results contain 2nd partitino
     * sad, e.g. 2NxN_T. Totally we have MxN results.
     * Note: The active partition count may be lesser than M, still we
     * have results for M partitions
     */
    S32 *pi4_sad_grid;

    /** Pointer to TU_SPLIT grid flags */
    S32 *pi4_tu_split_flags;

    /** Pointer to the Child's satd cost */
    S32 *pi4_child_cost;

    /** pointer to the child'd TU_split flags */
    S32 *pi4_child_tu_split_flags;

    /** pointer to the child'd TU_early_cbf flags */
    S32 *pi4_child_tu_early_cbf;

    /** Pointer to TU early CBF flags */
    S32 *pi4_tu_early_cbf;

    /** pointer to the early cbf thresholds */
    S32 *pi4_tu_early_cbf_threshold;

    /** store the DC value */
    S32 i4_dc_val;

    /** Block width and ht of the block being evaluated for SAD */
    S32 i4_blk_wd;
    S32 i4_blk_ht;

    /**
     * Array of valid partition ids. E.g. if 2 partitions active,
     * then there will be 3 entries, 3rd entry being -1
     */
    S32 *pi4_valid_part_ids;
    /** Step size of the grid */
    S32 i4_step;

    /* Number of partitions */
    S32 i4_num_partitions;

    /** Store the tu_spli_flag cost */
    S32 i4_tu_split_cost;

    /** The max_depth for inter tu_tree */
    U08 u1_max_tr_depth;

    U08 u1_max_tr_size;

    /** Scratch memory for Doing hadamard */
    U08 *pu1_wkg_mem;

    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list;

} err_prms_t;

typedef struct grid
{
    WORD32 num_grids; /* Number of grid to work with */
    WORD32 ref_buf_stride; /* Buffer stride of reference buffer */
    WORD32
        grd_sz_y_x; /* Packed 16 bits indicating grid spacing in y & x direction <--grid-size-y--><--grid-size-x--> */
    UWORD8 **ppu1_ref_ptr; /* Center point for the grid search */
    WORD32 *pi4_grd_mask; /* Mask indicating which grid points need to be evaluated */
    hme_mv_t *p_mv; /* <--MVy--><--MVx--> */
    WORD32 *p_ref_idx; /* Ref idx to which the grid is pointing */
} grid_ctxt_t;

typedef struct cand
{
    hme_mv_t mv; /* MV corresponding to the candidate <--MVy--><--MVx--> */
    WORD32 ref_idx; /* Ref idx corresponding to the candidate */
    WORD32 grid_ix; /* Grid to which this candidate belongs */
    UWORD8 *pu1_ref_ptr; /* Pointer to the candidate */
} cand_t;

/**
******************************************************************************
 *  @struct  hme_ctb_prms_t
 *  @brief   Parameters to create the CTB list, which is a tree structure
******************************************************************************
 */
typedef struct
{
    /**
     * These parameters cover number of input 16x16, 32x32 and 64x64 results
     * and the number of output results that are mix of all above CU sizes.
     * i4_num_kxk_unified_out is relevant only if we are sending multiple CU
     * sizes for same region for RD Opt.
     */
    S32 i4_num_16x16_in;
    S32 i4_num_32x32_in;
    S32 i4_num_32x32_unified_out;
    S32 i4_num_64x64_in;
    S32 i4_num_64x64_unified_out;

    /** Pointers to results at differen CU sizes */
    search_results_t *ps_search_results_16x16;
    search_results_t *ps_search_results_32x32;
    search_results_t *ps_search_results_64x64;

    S32 i4_num_part_type;

    /** Indicates whether we have split at 64x64 level */
    S32 i4_cu_64x64_split;
    /** Indicates whether each of the 32x32 CU is split */
    S32 ai4_cu_32x32_split[4];

    /** X and y offset of the CTB */
    S32 i4_ctb_x;
    S32 i4_ctb_y;

    /**
     * Memory manager for the CTB that is responsible for node allocation
     * at a CU level
     */
    ctb_mem_mgr_t *ps_ctb_mem_mgr;

    /** Buffer manager that is responsible for memory allocation (pred bufs) */
    buf_mgr_t *ps_buf_mgr;
} hme_ctb_prms_t;

/**
******************************************************************************
 *  @struct  result_upd_prms_t
 *  @brief   Updation of results
******************************************************************************
 */
typedef struct
{
    /** Cost compuatation function ponter */
    PF_MV_COST_FXN pf_mv_cost_compute;

    /** Points to the SAD grid updated during SAD compute fxn */
    S32 *pi4_sad_grid;

    /** Points to the TU_SPLIT grid updates duting the SATD TU REC fxn */
    S32 *pi4_tu_split_flags;

    /**
     * This is the central mv of the grid. For e.g. if we have a 3x3 grid,
     * this covers the central pt's mv in the grid.
     */
    const search_node_t *ps_search_node_base;

    /** Search results structure updated by the result update fxn */
    search_results_t *ps_search_results;

    /** List of active partitions, only these are processed and updated */
    S32 *pi4_valid_part_ids;

    /** Reference id for this candt and grid */
    S08 i1_ref_idx;

    /** Mask of active pts in the grid */
    S32 i4_grid_mask;

    /**
     * For early exit reasons we may want to know the id of the least candt
     * This will correspond to id of  candt with least cost for 2Nx2N part,
     * if multiple partitions enabled, or if 1 part enabled, it will be for
     * id of candt of that partition
     */
    S32 i4_min_id;

    /** Step size of the grid */
    S32 i4_step;

    /** Mask of active partitions */
    S32 i4_part_mask;

    /** Min cost corresponding to min id */
    S32 i4_min_cost;

    /** Store the motion vectors in qpel unit*/
    S16 i2_mv_x;

    S16 i2_mv_y;

    U08 u1_pred_lx;

    subpel_refine_ctxt_t *ps_subpel_refine_ctxt;

    /** Current candidate in the subpel refinement process*/
    search_node_t *ps_search_node;

} result_upd_prms_t;

/**
******************************************************************************
 *  @struct  mv_grid_t
 *  @brief   Grid of MVs storing results for a CTB and neighbours. For a CTB
 *           of size 64x64, we may store upto 16x16 mvs (one for each 4x4)
 *           along with 1 neighbour on each side. Valid only for encode layer
******************************************************************************
 */
typedef struct
{
    /** All the mvs in the grid */
    search_node_t as_node[NUM_MVS_IN_CTB_GRID];

    /** Stride of the grid */
    S32 i4_stride;

    /** Start offset of the 0,0 locn in CTB. */
    S32 i4_start_offset;
} mv_grid_t;

typedef struct
{
    /* centroid's (x, y) co-ordinates in Q8 format */
    WORD32 i4_pos_x_q8;

    WORD32 i4_pos_y_q8;
} centroid_t;

typedef struct
{
    S16 min_x;

    S16 min_y;

    S16 max_x;

    S16 max_y;

    /* The cumulative sum of partition sizes of the mvs */
    /* in this cluster */
    S16 area_in_pixels;

    S16 uni_mv_pixel_area;

    S16 bi_mv_pixel_area;

    mv_data_t as_mv[128];

    U08 num_mvs;

    /* Weighted average of all mvs in the cluster */
    centroid_t s_centroid;

    S08 ref_id;

    S32 max_dist_from_centroid;

    U08 is_valid_cluster;

} cluster_data_t;

typedef struct
{
    cluster_data_t as_cluster_data[MAX_NUM_CLUSTERS_16x16];

    U08 num_clusters;

    U08 au1_num_clusters[MAX_NUM_REF];

    S16 intra_mv_area;

    S32 best_inter_cost;

} cluster_16x16_blk_t;

typedef struct
{
    cluster_data_t as_cluster_data[MAX_NUM_CLUSTERS_32x32];

    U08 num_clusters;

    U08 au1_num_clusters[MAX_NUM_REF];

    S16 intra_mv_area;

    S08 best_uni_ref;

    S08 best_alt_ref;

    S32 best_inter_cost;

    U08 num_refs;

    U08 num_clusters_with_weak_sdi_density;

} cluster_32x32_blk_t;

typedef struct
{
    cluster_data_t as_cluster_data[MAX_NUM_CLUSTERS_64x64];

    U08 num_clusters;

    U08 au1_num_clusters[MAX_NUM_REF];

    S16 intra_mv_area;

    S08 best_uni_ref;

    S08 best_alt_ref;

    S32 best_inter_cost;

    U08 num_refs;

} cluster_64x64_blk_t;

typedef struct
{
    cluster_16x16_blk_t *ps_16x16_blk;

    cluster_32x32_blk_t *ps_32x32_blk;

    cluster_64x64_blk_t *ps_64x64_blk;

    cur_ctb_cu_tree_t *ps_cu_tree_root;
    ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb;
    S32 nodes_created_in_cu_tree;

    S32 *pi4_blk_8x8_mask;

    S32 blk_32x32_mask;

    S32 sdi_threshold;

    S32 i4_frame_qstep;

    S32 i4_frame_qstep_multiplier;

    U08 au1_is_16x16_blk_split[16];

    S32 ai4_part_mask[16];

} ctb_cluster_info_t;

/**
******************************************************************************
 *  @struct  hme_merge_prms_t
 *  @brief   All parameters related to the merge process
******************************************************************************
 */
typedef struct
{
    /**
     * MV Range prms for the merged CU, this may have to be conservative
     * in comparison to individual CUs
     */
    range_prms_t *aps_mv_range[MAX_NUM_REF];

    /** Pointers to search results of 4 children CUs to be merged */
    search_results_t *ps_results_tl;
    search_results_t *ps_results_tr;
    search_results_t *ps_results_bl;
    search_results_t *ps_results_br;

    search_results_t *ps_results_grandchild;

    /** Pointer to search results of the parent CU updated during merge */
    search_results_t *ps_results_merge;

    inter_cu_results_t *ps_8x8_cu_results;

    /** Layer related context */
    layer_ctxt_t *ps_layer_ctxt;

    inter_ctb_prms_t *ps_inter_ctb_prms;

    /**
     * Points to an array of pointers. This array in turn points to
     * the active mv grid in each direction (L0/L1)
     */
    mv_grid_t **pps_mv_grid;

    ctb_cluster_info_t *ps_cluster_info;

    S08 *pi1_past_list;

    S08 *pi1_future_list;

    /** MV cost compute function */
    PF_MV_COST_FXN pf_mv_cost_compute;

    /** If segmentation info available for the parent block */
    S32 i4_seg_info_avail;

    /** Partition mask (if segmentation info available) */
    S32 i4_part_mask;

    /** Number of input results available for the merge proc from children*/
    S32 i4_num_inp_results;

    /** Whether SATD to be used for fpel searches */
    S32 i4_use_satd;

    /**
     * Number of result planes valid for this merge process. For example,
     * for fpel search in encode layer, we may have only L0 and L1
     */
    S32 i4_num_ref;

    /** Whether to use input or recon frm for search */
    S32 i4_use_rec;

    /** optimized mv grid flag : indicates if same mvgrid is used for both fpel and qpel
     *  This helps in copying fpel and qpel mv grid in pred context mv grid
     */
    S32 i4_mv_grid_opt;

    /** ctb size, typically 32 or 64 */
    S32 log_ctb_size;

    S32 i4_ctb_x_off;

    S32 i4_ctb_y_off;

    ME_QUALITY_PRESETS_T e_quality_preset;

    S32 i4_num_pred_dir_actual;

    U08 au1_pred_dir_searched[2];

    S32 i4_alpha_stim_multiplier;

    U08 u1_is_cu_noisy;

} hme_merge_prms_t;

/**
******************************************************************************
 *  @struct  mvbank_update_prms_t
 *  @brief   Useful prms for updating the mv bank
******************************************************************************
 */
typedef struct
{
    /** Number of references for which update to be done */
    S32 i4_num_ref;

    /**
     * Search blk size that was used, if this is different from the blk
     * size used in mv bank, then some replications or reductions may
     * have to be done. E.g. if search blk size is 8x8 and result blk
     * size is 4x4, then we have to update part NxN results to be
     * used for update along with replication of 2Nx2N result in each
     * of the 4 4x4 blk.
     */
    BLK_SIZE_T e_search_blk_size;

    /**
     * Redundant prm as it reflects differences between search blk size
     * and mv blk size if any
     */
    S32 i4_shift;

    S32 i4_num_active_ref_l0;

    S32 i4_num_active_ref_l1;

    S32 i4_num_results_to_store;
} mvbank_update_prms_t;

/**
******************************************************************************
 *  @struct  hme_subpel_prms_t
 *  @brief   input and control prms for subpel refinement
******************************************************************************
 */
typedef struct
{
    /** Relevant only for the case where we mix up results of diff cu sizes */
    S32 i4_num_16x16_candts;
    S32 i4_num_32x32_candts;
    S32 i4_num_64x64_candts;

    /** X and y offset of ctb w.r.t. start of pic */
    S32 i4_ctb_x_off;
    S32 i4_ctb_y_off;

    /** Max Number of diamond steps for hpel and qpel refinement */
    S32 i4_num_steps_hpel_refine;
    S32 i4_num_steps_qpel_refine;

    /** Whether SATD to be used or SAD to be used */
    S32 i4_use_satd;

    /**
     * Input ptr. This is updated inside the subpel refinement by picking
     * up correct adress
     */
    void *pv_inp;

    /**
     * Pred buffer ptr, updated inside subpel refinement process. This
     * location passed to the leaf fxn for copying the winner pred buf
     */
    U08 *pu1_pred;

    /** Interpolation fxn sent by top layer, should exact qpel be desired */
    PF_INTERP_FXN_T pf_qpel_interp;

    /** Working mem passed to leaf fxns */
    U08 *pu1_wkg_mem;

    /** prediction buffer stride fo rleaf fxns to copy the pred winner buf */
    S32 i4_pred_stride;

    /** Type of input ; sizeof(UWORD8) => unidir refinement, else BIDIR */
    S32 i4_inp_type;

    /** Stride of input buf, updated inside subpel fxn */
    S32 i4_inp_stride;

    /**
     * Pointer to the backward input ptr. This is also updated inside
     * the subpel fxn. Needed for BIDIR refinement where modified inpu
     * is 2I - P0
     */
    S16 *pi2_inp_bck;

    /** Indicates if CU merge uses SATD / SAD */
    S32 i4_use_satd_cu_merge;

    /** valid MV range in hpel and qpel units */
    range_prms_t *aps_mv_range_hpel[MAX_NUM_REF];
    range_prms_t *aps_mv_range_qpel[MAX_NUM_REF];
    /** Relevant only for mixed CU cases */
    search_results_t *ps_search_results_16x16;
    search_results_t *ps_search_results_32x32;
    search_results_t *ps_search_results_64x64;

    /** Cost computatino fxn ptr */
    PF_MV_COST_FXN pf_mv_cost_compute;

    /** Whether BI mode is allowed for this pic (not allowed in P) */
    S32 bidir_enabled;

    /**
     * Total number of references of current picture which is enocded
     */
    U08 u1_num_ref;

    /**
     * Number of candidates used for refinement
     * If given 1 candidate, then 2Nx2N is chosen as the best candidate
     */
    U08 u1_max_subpel_candts;

    U08 u1_subpel_candt_threshold;

    ME_QUALITY_PRESETS_T e_me_quality_presets;

    U08 u1_max_subpel_candts_2Nx2N;
    U08 u1_max_subpel_candts_NxN;

    U08 u1_max_num_subpel_refine_centers;

    subpel_refine_ctxt_t *ps_subpel_refine_ctxt;

    S32 i4_num_act_ref_l0;

    S32 i4_num_act_ref_l1;

    U08 u1_is_cu_noisy;
} hme_subpel_prms_t;

/**
******************************************************************************
 *  @struct  layers_descr_t
 *  @brief   One such str exists for each ref and curr input in the me ctxt
 *           Has ctxt handles for all layers of a given POC
******************************************************************************
 */
typedef struct
{
    /** Handles for all layers. Entry 0 is finest layer */
    layer_ctxt_t *aps_layers[MAX_NUM_LAYERS];
} layers_descr_t;

/**
******************************************************************************
 *  @struct  blk_ctb_attrs_t
 *  @brief   The CTB is split into 16x16 blks. For each such blk, this str
 *           stores attributes of this blk w.r.t. ctb
******************************************************************************
 */
typedef struct
{
    /**
     * ID of the blk in the full ctb. Assuming the full ctb were coded,
     * this indicates what is the blk num of this blk (in encode order)
     * within the full ctb
     */
    U08 u1_blk_id_in_full_ctb;

    /** x and y coordinates of this blk w.r.t. ctb base */
    U08 u1_blk_x;
    U08 u1_blk_y;
    /**
     * Mask of 8x8 blks that are active. Bits 0-3 for blks 0-3 in raster order
     * within a 16x16 blk. This will be 0xf in interiors and < 0xf at rt/bot
     * boundaries or at bot rt corners, where we may not have full 16x16 blk
     */
    U08 u1_blk_8x8_mask;
} blk_ctb_attrs_t;

/**
******************************************************************************
 *  @struct  ctb_boundary_attrs_t
 *  @brief   Depending on the location of ctb (rt boundary, bot boundary,
 *           bot rt corner, elsewhere) this picks out the appropriate
 *           attributes of the ctb
******************************************************************************
 */
typedef struct
{
    /**
     * 4 bit variable, one for each of the 4 possible 32x32s in a full ctb
     * If any 32x32 is partially present / not present at boundaries, that
     * bit posn will be 0
     */
    U08 u1_merge_to_32x32_flag;

    /**
     * 1 bit flag indicating whether it is a complete ctb or not, and
     * consequently whether it can be merged to a full 64x64
     */
    U08 u1_merge_to_64x64_flag;

    /** Number of valid 16x16 blks (includes those partially/fully present*/
    U08 u1_num_blks_in_ctb;

    /** 16 bit variable indicating whether the corresponding 16x16 is valid */
    S32 cu_16x16_valid_flag;

    /**
     * For possible 16 16x16 blks in a CTB, we have one attribute str for
     * every valid blk. Tightly packed structure. For example,
     *  0  1  4  5
     *  2  3  6  7
     *  8  9 12 13
     * 10 11 14 15
     * Assuming the ctb width is only 48, blks 5,7,13,15 are invalid
     * Then We store attributes in the order: 0,1,2,3,4,6,8,9,10,11,12,14
     */
    blk_ctb_attrs_t as_blk_attrs[16];
} ctb_boundary_attrs_t;

typedef struct
{
    S32 sdi;

    S32 ref_idx;

    S32 cluster_id;
} outlier_data_t;

/**
******************************************************************************
 *  @struct  coarse_dyn_range_prms_t
 *  @brief   The parameters for Dyn. Search Range in coarse ME
******************************************************************************
 */

typedef struct
{
    /* TO DO : size can be reduced, as not getting used for L0 */

    /** Dynamical Search Range parameters per layer & ref_pic */
    dyn_range_prms_t as_dyn_range_prms[MAX_NUM_LAYERS][MAX_NUM_REF];

    /** Min y value Normalized per POC distance */
    WORD16 i2_dyn_min_y_per_poc[MAX_NUM_LAYERS];
    /** Max y value Normalized per POC distance */
    WORD16 i2_dyn_max_y_per_poc[MAX_NUM_LAYERS];

} coarse_dyn_range_prms_t;

/**
******************************************************************************
 *  @struct  coarse_me_ctxt_t
 *  @brief   Handle for Coarse ME
******************************************************************************
 */
typedef struct
{
    /** Init search candts, 2 sets, one for 4x8 and one for 8x4 */
    search_node_t s_init_search_node[MAX_INIT_CANDTS * 2];

    /** For non enc layer, we search 8x8 blks and store results here */
    search_results_t s_search_results_8x8;
    /**
     * Below arays store input planes for each ref pic.
     * These are duplications, and are present within layer ctxts, but
     * kept here together for faster indexing during search
     */
    U08 *apu1_list_inp[MAX_NUM_LAYERS][MAX_NUM_REF];

    /** Ptr to all layer context placeholder for curr pic encoded */
    layers_descr_t *ps_curr_descr;

    /** Ptr to all layer ctxt place holder for all pics */
    layers_descr_t as_ref_descr[MAX_NUM_REF + 1 + NUM_BUFS_DECOMP_HME];

    /**
     * ME uses ref id lc to search multi ref. This TLU gets POC of
     * the pic w.r.t. a given ref id
     */
    S32 ai4_ref_idx_to_poc_lc[MAX_NUM_REF];

    /** use this array to get disp num from ref_idx. Used for L1 traqo **/
    S32 ai4_ref_idx_to_disp_num[MAX_NUM_REF];

    /** POC of pic encoded just before current */
    S32 i4_prev_poc;

    /** POC of curret pic being encoded */
    S32 i4_curr_poc;

    /** Number of HME layers encode + non encode */
    S32 num_layers;

    /** Alloc time parameter, max ref frms used for this session */
    S32 max_num_ref;

    /**
     * Number of layers that use explicit search. Explicit search means
     * that each ref id is searched separately
     */
    S32 num_layers_explicit_search;

    /**
     * Maximum number of results maintained at any refinement layer
     * search. Important from mem alloc perspective
     */
    S32 max_num_results;

    /** Same as above but for coarse layer */
    S32 max_num_results_coarse;

    /** Array of flags, one per layer indicating hwether layer is encoded */
    U08 u1_encode[MAX_NUM_LAYERS];

    /** Init prms send by encoder during create time */
    hme_init_prms_t s_init_prms;

    /**
     * Array look up created each frm, maintaining the corresponding
     * layer descr look up for each ref id
     */
    S32 a_ref_to_descr_id[MAX_NUM_REF];

    /**
     * Array lookup created each frame that maps a given ref id
     * pertaining to unified list to a L0/L1 list. Encoder searches in terms
     * of LC list or in other words does not differentiate between L0
     * and L1 frames for most of search. Finally to report results to
     * encoder, the ref id has to be remapped to suitable list
     */
    S32 a_ref_idx_lc_to_l0[MAX_NUM_REF];
    S32 a_ref_idx_lc_to_l1[MAX_NUM_REF];

    /** Width and ht of each layer */
    S32 a_wd[MAX_NUM_LAYERS];
    S32 a_ht[MAX_NUM_LAYERS];

    /** Histogram, one for each ref, allocated during craete time */
    mv_hist_t *aps_mv_hist[MAX_NUM_REF];

    /** Whether a given ref id in Lc list is past frm or future frm */
    U08 au1_is_past[MAX_NUM_REF];

    /** These are L0 and L1 lists, storing ref id Lc in them */
    S08 ai1_past_list[MAX_NUM_REF];
    S08 ai1_future_list[MAX_NUM_REF];

    /** Number of past and future ref pics sent this frm */
    S32 num_ref_past;
    S32 num_ref_future;

    void *pv_ext_frm_prms;

    hme_frm_prms_t *ps_hme_frm_prms;

    hme_ref_map_t *ps_hme_ref_map;
    /**
     *  Scale factor of any given ref lc to another ref in Q8
     *  First MAX_NUM_REF entries are to scale an mv of ref id k
     *  w.r.t. ref id 0 (approx 256 * POC delta(0) / POC delta(k))
     *  Next MAX_NUM_REF entreis are to scale mv of ref id 1 w.r.t. 0
     *  And so on
     */
    S16 ai2_ref_scf[MAX_NUM_REF * MAX_NUM_REF];

    /** bits for a given ref id, in either list L0/L1 */
    U08 au1_ref_bits_tlu_lc[2][MAX_NUM_REF];

    /** Points to above: 1 ptr for each list */
    U08 *apu1_ref_bits_tlu_lc[2];

    /** number of b fraems between P, depends on number of hierarchy layers */
    S32 num_b_frms;

    /** Frame level qp passed every frame by ME's caller */
    S32 frm_qstep;

    /** Backup of frame parameters */
    hme_frm_prms_t s_frm_prms;

    /** Weighted prediction parameters for all references are stored
     *  Scratch buffers for populated widgted inputs are also stored in this
     */
    wgt_pred_ctxt_t s_wt_pred;

    /** Weighted pred enable flag */
    S32 i4_wt_pred_enable_flag;

    /* Pointer to hold 5 rows of best search node information */
    search_node_t *aps_best_search_nodes_4x8_n_rows[MAX_NUM_REF];

    search_node_t *aps_best_search_nodes_8x4_n_rows[MAX_NUM_REF];

    /* Pointer to hold 5 rows of best search node information */
    S16 *api2_sads_4x4_n_rows[MAX_NUM_REF];

    /*  Number of row buffers to store SADs and best search nodes */
    S32 i4_num_row_bufs;

    /* (HEVCE_MAX_HEIGHT>>1) assuming layer 1 is coarse layer and >>2 assuming block size is 4x4*/
    S32 ai4_row_index[(HEVCE_MAX_HEIGHT >> 1) >> 2];

    /* store L1 cost required for rate control for enc decision*/
    S32 i4_L1_hme_best_cost;

    /* store L1 cost required for modulation index calc*/
    //S32 i4_L1_hme_best_cost_for_ref;

    /* store L1 satd */
    S32 i4_L1_hme_sad;
    /* EIID: layer1 buffer to store the early inter intra costs and decisions */
    /* pic_level pointer stored here */
    ihevce_ed_blk_t *ps_ed_blk;
    /* EIID: layer1 buffer to store the sad/cost information for rate control
    or cu level qp modulation*/
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1;
    /** Dynamical Search Range parameters */
    coarse_dyn_range_prms_t s_coarse_dyn_range_prms;

    /** Dependency manager for Row level sync in HME pass */
    void *apv_dep_mngr_hme_sync[MAX_NUM_HME_LAYERS - 1];

    /* pointer buffers for memory mapping */
    UWORD8 *pu1_me_reverse_map_info;

    /*blk count which has higher SAD*/
    S32 i4_num_blks_high_sad;

    /*num of 8x8 blocks in nearest poc*/
    S32 i4_num_blks;

    /* thread id of the current context */
    WORD32 thrd_id;

    /* Should be typecast to a struct of type 'ihevce_me_optimised_function_list_t' */
    void *pv_me_optimised_function_list;

    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list;

} coarse_me_ctxt_t;

/**
******************************************************************************
 *  @struct  coarse_dyn_range_prms_t
 *  @brief   The parameters for Dyn. Search Range in coarse ME
******************************************************************************
 */
typedef struct
{
    /** Dynamical Search Range parameters per ref_pic */
    dyn_range_prms_t as_dyn_range_prms[MAX_NUM_REF];

    /** Min y value Normalized per POC distance */
    WORD16 i2_dyn_min_y_per_poc;
    /** Max y value Normalized per POC distance */
    WORD16 i2_dyn_max_y_per_poc;

    /* The number of ref. pic. actually used in L0. Used to communicate */
    /* to ihevce_l0_me_frame_end and frame process                      */
    WORD32 i4_num_act_ref_in_l0;

    /*display number*/
    WORD32 i4_display_num;

} l0_dyn_range_prms_t;

/**
******************************************************************************
 *  @brief inter prediction (MC) context for me loop
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

} inter_pred_me_ctxt_t;

typedef void FT_CALC_SATD_AND_RESULT(err_prms_t *ps_prms, result_upd_prms_t *ps_result_prms);

typedef struct
{
    FT_CALC_SATD_AND_RESULT *pf_evalsatd_update_1_best_result_pt_pu_16x16_num_part_eq_1;
    FT_CALC_SATD_AND_RESULT *pf_evalsatd_update_1_best_result_pt_pu_16x16_num_part_lt_9;
    FT_CALC_SATD_AND_RESULT *pf_evalsatd_update_1_best_result_pt_pu_16x16_num_part_lt_17;
    FT_CALC_SATD_AND_RESULT *pf_evalsatd_update_2_best_results_pt_pu_16x16_num_part_eq_1;
    FT_CALC_SATD_AND_RESULT *pf_evalsatd_update_2_best_results_pt_pu_16x16_num_part_lt_9;
    FT_CALC_SATD_AND_RESULT *pf_evalsatd_update_2_best_results_pt_pu_16x16_num_part_lt_17;
    FT_HAD_8X8_USING_4_4X4_R *pf_had_8x8_using_4_4x4_r;
    FT_HAD_16X16_R *pf_had_16x16_r;
    FT_HAD_32X32_USING_16X16 *pf_compute_32x32HAD_using_16x16;
} me_func_selector_t;

/**
******************************************************************************
 *  @struct  me_frm_ctxt_t
 *  @brief   Handle for ME
******************************************************************************
 */
typedef struct
{
    /** Init search candts, 2 sets, one for 4x8 and one for 8x4 */
    search_node_t s_init_search_node[MAX_INIT_CANDTS];

    /** Motion Vectors array */
    mv_t as_search_cand_mv[MAX_INIT_CANDTS];

    /** Results of 16 16x16 blks within a CTB used in enc layer */
    search_results_t as_search_results_16x16[16];

    /** Results of 4 32x32 blks in a ctb for enc layer merge stage */
    search_results_t as_search_results_32x32[4];

    /** Same as above but fo 64x64 blk */
    search_results_t s_search_results_64x64;

    /**
     * Below arays store input, 4 recon planes for each ref pic.
     * These are duplications, and are present within layer ctxts, but
     * kept here together for faster indexing during search
     */

    U08 *apu1_list_rec_fxfy[MAX_NUM_LAYERS][MAX_NUM_REF];
    U08 *apu1_list_rec_hxfy[MAX_NUM_LAYERS][MAX_NUM_REF];
    U08 *apu1_list_rec_fxhy[MAX_NUM_LAYERS][MAX_NUM_REF];
    U08 *apu1_list_rec_hxhy[MAX_NUM_LAYERS][MAX_NUM_REF];
    U08 *apu1_list_inp[MAX_NUM_LAYERS][MAX_NUM_REF];

    void *apv_list_dep_mngr[MAX_NUM_LAYERS][MAX_NUM_REF];

    /** Ptr to all layer context placeholder for curr pic encoded */
    layers_descr_t *ps_curr_descr;

    /**
     * ME uses ref id lc to search multi ref. This TLU gets POC of
     * the pic w.r.t. a given ref id
     */
    S32 ai4_ref_idx_to_poc_lc[MAX_NUM_REF];

    /** POC of pic encoded just before current */
    S32 i4_prev_poc;

    /** POC of curret pic being encoded */
    S32 i4_curr_poc;

    /** Buf mgr for memory allocation */
    buf_mgr_t s_buf_mgr;

    /** MV Grid for L0 and L1, this is active one used */
    mv_grid_t as_mv_grid[2];

    /**
     * MV grid for FPEL and QPEL maintained separately. Depending on the
     * correct prediction res. being used, copy appropriate results to
     * the as_mv_Grid structure
     */
    mv_grid_t as_mv_grid_fpel[2];
    mv_grid_t as_mv_grid_qpel[2];

    /** Number of HME layers encode + non encode */
    S32 num_layers;

    /** Alloc time parameter, max ref frms used for this session */
    S32 max_num_ref;

    /**
     * Number of layers that use explicit search. Explicit search means
     * that each ref id is searched separately
     */
    S32 num_layers_explicit_search;

    /**
     * Maximum number of results maintained at any refinement layer
     * search. Important from mem alloc perspective
     */
    S32 max_num_results;

    /** Same as above but for coarse layer */
    S32 max_num_results_coarse;

    /** Array of flags, one per layer indicating hwether layer is encoded */
    U08 u1_encode[MAX_NUM_LAYERS];

    /* Parameters used for lambda computation */
    frm_lambda_ctxt_t s_frm_lambda_ctxt;

    /**
     * Array look up created each frm, maintaining the corresponding
     * layer descr look up for each ref id
     */
    S32 a_ref_to_descr_id[MAX_NUM_REF];

    /**
     * Array lookup created each frame that maps a given ref id
     * pertaining to unified list to a L0/L1 list. Encoder searches in terms
     * of LC list or in other words does not differentiate between L0
     * and L1 frames for most of search. Finally to report results to
     * encoder, the ref id has to be remapped to suitable list
     */
    S32 a_ref_idx_lc_to_l0[MAX_NUM_REF];
    S32 a_ref_idx_lc_to_l1[MAX_NUM_REF];

    /** Width and ht of each layer */
    S32 i4_wd;
    S32 i4_ht;

    /** Histogram, one for each ref, allocated during craete time */
    mv_hist_t *aps_mv_hist[MAX_NUM_REF];

    /**
     * Back input requiring > 8  bit precision, allocated during
     * create time, storing 2I-P0 for Bidir refinement
     */
    S16 *pi2_inp_bck;
    ctb_boundary_attrs_t as_ctb_bound_attrs[NUM_CTB_BOUNDARY_TYPES];

    /** Whether a given ref id in Lc list is past frm or future frm */
    U08 au1_is_past[MAX_NUM_REF];

    /** These are L0 and L1 lists, storing ref id Lc in them */
    S08 ai1_past_list[MAX_NUM_REF];
    S08 ai1_future_list[MAX_NUM_REF];

    /** Number of past and future ref pics sent this frm */
    S32 num_ref_past;
    S32 num_ref_future;

    /**
     * Passed by encoder, stored as void to avoid header file inclusion
     * of encoder wks into ME, these are frm prms passed by encoder,
     * pointers to ctbanalyse_t and cu_analyse_t structures and the
     * corresponding running ptrs
     */

    ctb_analyse_t *ps_ctb_analyse_base;
    cur_ctb_cu_tree_t *ps_cu_tree_base;
    me_ctb_data_t *ps_me_ctb_data_base;

    ctb_analyse_t *ps_ctb_analyse_curr_row;
    cu_analyse_t *ps_cu_analyse_curr_row;
    cur_ctb_cu_tree_t *ps_cu_tree_curr_row;
    me_ctb_data_t *ps_me_ctb_data_curr_row;

    /** Log2 of ctb size e.g. for 64 size, it will be 6 */
    S32 log_ctb_size;

    hme_frm_prms_t *ps_hme_frm_prms;

    hme_ref_map_t *ps_hme_ref_map;

    /**
     *  Scale factor of any given ref lc to another ref in Q8
     *  First MAX_NUM_REF entries are to scale an mv of ref id k
     *  w.r.t. ref id 0 (approx 256 * POC delta(0) / POC delta(k))
     *  Next MAX_NUM_REF entreis are to scale mv of ref id 1 w.r.t. 0
     *  And so on
     */
    S16 ai2_ref_scf[MAX_NUM_REF * MAX_NUM_REF];

    /** bits for a given ref id, in either list L0/L1 */
    U08 au1_ref_bits_tlu_lc[2][MAX_NUM_REF];

    /** Points to above: 1 ptr for each list */
    U08 *apu1_ref_bits_tlu_lc[2];

    /**
     *  Frame level base pointer to L0 IPE ctb analyze structures.
     *  This strucutres include the following
     *
     *  1. Best costs and modes at all levels of CTB (CU=8,16,32,64)
     *  2. Recommended IPE intra CU sizes for this CTB size
     *  3. Early intra/inter decision structures for all 8x8 blocks of CTB
     *     populated by L1-ME and L1-IPE
     *
     */
    ipe_l0_ctb_analyse_for_me_t *ps_ipe_l0_ctb_frm_base;

    /** array of ptrs to intra cost per layer encoded, stored at 8x8 */
    double *apd_intra_cost[MAX_NUM_LAYERS];

    /** number of b fraems between P, depends on number of hierarchy layers */
    S32 num_b_frms;

    /** Frame level qp passed every frame by ME's caller */
    S32 frm_qstep;

    /** Frame level qp with higher precision : left shifted by 8 */
    S32 qstep_ls8;

    /** Backup of frame parameters */
    hme_frm_prms_t s_frm_prms;

    /** Weighted prediction parameters for all references are stored
     *  Scratch buffers for populated widgted inputs are also stored in this
     */
    wgt_pred_ctxt_t s_wt_pred;

    /** Weighted pred enable flag */
    S32 i4_wt_pred_enable_flag;

    /** Results of 16 16x16 blks within a CTB used in enc layer */
    inter_cu_results_t as_cu16x16_results[16];

    /** Results of 4 32x32 blks in a ctb for enc layer merge stage */
    inter_cu_results_t as_cu32x32_results[4];

    /** Same as above but fo 64x64 blk */
    inter_cu_results_t s_cu64x64_results;

    /** Results of 64 8x8 blks within a CTB used in enc layer */
    inter_cu_results_t as_cu8x8_results[64];

    WORD32 i4_is_prev_frame_reference;

    rc_quant_t *ps_rc_quant_ctxt;

    /** Dynamical Search Range parameters */
    l0_dyn_range_prms_t as_l0_dyn_range_prms[NUM_SG_INTERLEAVED];

    /** Dependency manager for Row level sync in L0 ME pass */
    void *pv_dep_mngr_l0_me_sync;

    /** Pointer to structure containing function pointers of encoder*/
    me_func_selector_t *ps_func_selector;

    cluster_16x16_blk_t *ps_blk_16x16;

    cluster_32x32_blk_t *ps_blk_32x32;

    cluster_64x64_blk_t *ps_blk_64x64;

    ctb_cluster_info_t *ps_ctb_cluster_info;

    fullpel_refine_ctxt_t *ps_fullpel_refine_ctxt;

    /* thread id of the current context */
    WORD32 thrd_id;

    /* dependency manager for froward ME sync */
    void *pv_dep_mngr_encloop_dep_me;
    WORD32 i4_l0me_qp_mod;

    /*mc ctxt to reuse lume inter pred fucntion
    for the purpose of TRAQO*/
    inter_pred_me_ctxt_t s_mc_ctxt;

    WORD32 i4_rc_pass;
    /*pic type*/
    WORD32 i4_pic_type;

    WORD32 i4_temporal_layer;

    WORD32 i4_count;

    WORD32 i4_use_const_lamda_modifier;

    double f_i_pic_lamda_modifier;

    UWORD8 u1_is_curFrame_a_refFrame;

    /* src_var related variables */
    U32 au4_4x4_src_sigmaX[MAX_NUM_SIGMAS_4x4];
    U32 au4_4x4_src_sigmaXSquared[MAX_NUM_SIGMAS_4x4];
} me_frm_ctxt_t;

/**
******************************************************************************
 *  @struct  me_ctxt_t
 *  @brief   Handle for ME
******************************************************************************
 */
typedef struct
{
    /** Init prms send by encoder during create time */
    hme_init_prms_t s_init_prms;

    /** Not used in encoder, relevant to test bench */
    U08 *pu1_debug_out;

    void *pv_ext_frm_prms;

    /* Frame level ME ctxt */
    me_frm_ctxt_t *aps_me_frm_prms[MAX_NUM_ME_PARALLEL];

    /** Ptr to all layer ctxt place holder for all pics */
    /** number of reference descriptors should be equal to max number of active references **/
    layers_descr_t as_ref_descr[((DEFAULT_MAX_REFERENCE_PICS << 1) * MAX_NUM_ME_PARALLEL) + 1];

    /* Should be typecast to a struct of type 'ihevce_me_optimised_function_list_t' */
    void *pv_me_optimised_function_list;

    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list;

    /* Pointer to Tile params base */
    void *pv_tile_params_base;

} me_ctxt_t;

typedef struct
{
    /** array of context for each thread */
    coarse_me_ctxt_t *aps_me_ctxt[MAX_NUM_FRM_PROC_THRDS_PRE_ENC];

    /** memtabs storage memory */
    hme_memtab_t as_memtabs[HME_COARSE_TOT_MEMTABS];

    /** Frame level parameters for ME */
    hme_frm_prms_t s_frm_prms;

    /** Holds all reference mapping */
    hme_ref_map_t s_ref_map;

    /** number of threads created run time */
    WORD32 i4_num_proc_thrds;

    /** Dependency manager for Row level sync in HME pass */
    /* Note : Indexing should be like layer_id - 1        */
    void *apv_dep_mngr_hme_sync[MAX_NUM_HME_LAYERS - 1];
    /* Should be typecast to a struct of type 'ihevce_me_optimised_function_list_t' */
    void *pv_me_optimised_function_list;

    ihevce_cmn_opt_func_t s_cmn_opt_func;
} coarse_me_master_ctxt_t;

typedef struct
{
    /** array of context for each thread */
    me_ctxt_t *aps_me_ctxt[MAX_NUM_FRM_PROC_THRDS_ENC];

    /** memtabs storage memory */
    hme_memtab_t as_memtabs[MAX_HME_ENC_TOT_MEMTABS];

    /** Frame level parameters for ME */
    hme_frm_prms_t as_frm_prms[MAX_NUM_ME_PARALLEL];

    /** Holds all reference mapping */
    hme_ref_map_t as_ref_map[MAX_NUM_ME_PARALLEL];

    /** number of threads created run time */
    WORD32 i4_num_proc_thrds;

    /** number of me frames running in parallel */
    WORD32 i4_num_me_frm_pllel;

    /** Pointer to structure containing function pointers of encoder*/
    me_func_selector_t s_func_selector;
    /* Should be typecast to a struct of type 'ihevce_me_optimised_function_list_t' */
    void *pv_me_optimised_function_list;

    ihevce_cmn_opt_func_t s_cmn_opt_func;

    /* Pointer to Tile params base */
    void *pv_tile_params_base;

} me_master_ctxt_t;

typedef struct
{
    S16 i2_mv_x;

    S16 i2_mv_y;

    U08 u1_ref_idx;

    U32 au4_node_map[2 * MAP_Y_MAX];

} subpel_dedup_enabler_t;

typedef subpel_dedup_enabler_t hme_dedup_enabler_t;

typedef struct
{
    layer_ctxt_t *ps_curr_layer;

    layer_ctxt_t *ps_coarse_layer;

    U08 *pu1_num_fpel_search_cands;

    S32 *pi4_ref_id_lc_to_l0_map;

    S32 *pi4_ref_id_lc_to_l1_map;

    S32 i4_pos_x;

    S32 i4_pos_y;

    S32 i4_num_act_ref_l0;

    S32 i4_num_act_ref_l1;

    search_candt_t *ps_search_cands;

    U08 u1_search_candidate_list_index;

    S32 i4_max_num_init_cands;

    U08 u1_pred_dir;

    /* Indicates the position of the current predDir in the processing order of predDir */
    U08 u1_pred_dir_ctr;

    /* The following 4 flags apply exclusively to spatial candidates */
    U08 u1_is_topRight_available;

    U08 u1_is_topLeft_available;

    U08 u1_is_top_available;

    U08 u1_is_left_available;

    S08 i1_default_ref_id;

    S08 i1_alt_default_ref_id;

    U08 u1_num_results_in_mvbank;

    BLK_SIZE_T e_search_blk_size;

} fpel_srch_cand_init_data_t;

typedef struct
{
    U08 *pu1_pred;

    S32 i4_pred_stride;

    U08 u1_pred_buf_array_id;

} hme_pred_buf_info_t;

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/
typedef void (*PF_SAD_FXN_T)(err_prms_t *);

typedef void (*PF_SAD_RESULT_FXN_T)(err_prms_t *, result_upd_prms_t *ps_result_prms);

typedef WORD32 (*PF_SAD_FXN_TU_REC)(
    err_prms_t *,
    WORD32 lambda,
    WORD32 lamda_q_shift,
    WORD32 i4_frm_qstep,
    me_func_selector_t *ps_func_selector);

typedef void (*PF_RESULT_FXN_T)(result_upd_prms_t *);

typedef void (*PF_CALC_SAD_AND_RESULT)(
    hme_search_prms_t *, wgt_pred_ctxt_t *, err_prms_t *, result_upd_prms_t *, U08 **, S32);

#endif /* _HME_DEFS_H_ */
