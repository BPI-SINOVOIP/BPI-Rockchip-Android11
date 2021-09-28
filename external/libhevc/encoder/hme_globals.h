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
* \file hme_globals.h
*
* \brief
*    Contains all the global declarations used by HME
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _HME_GLOBALS_H_

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief Converts an encode order to raster order x coord. Meant for 16x16
 *         CU within 64x64 or within 32x32
******************************************************************************
 */
extern U08 gau1_encode_to_raster_x[16];

/**
******************************************************************************
 *  @brief Converts an encode order to raster order y coord. Meant for 16x16
 *         CU within 64x64 or within 32x32
******************************************************************************
 */
extern U08 gau1_encode_to_raster_y[16];

/**
******************************************************************************
 *  @brief Given a CU id within the bigger CU (0..3), and the partition type
 *         currently within the small CU, we can figure out candidate
 *         partition types for bigger CU. E.g. IF CU id is 0, and is AMP of
 *         nLx2N, candidate partitions for bigger CU are nLx2N and 2Nx2N
******************************************************************************
 */
extern PART_TYPE_T ge_part_type_to_merge_part[4][MAX_PART_TYPES][3];

/**
******************************************************************************
 *  @brief A given partition type has 1,2 or 4 partitions, each corresponding
 *  to a unique partition id PART_ID_T enum type. So, this global converts
 *  partition type to a bitmask of corresponding partition ids.
******************************************************************************
 */
extern S32 gai4_part_type_to_part_mask[MAX_PART_TYPES];

/**
******************************************************************************
 *  @brief Reads out the index of function pointer to a sad_compute function
 *         of blk given a blk size enumeration
******************************************************************************
 */
extern U08 gau1_blk_size_to_fp[NUM_BLK_SIZES];

/**
******************************************************************************
 *  @brief Reads out the width of blk given a blk size enumeration
******************************************************************************
 */
extern U08 gau1_blk_size_to_wd[NUM_BLK_SIZES];

extern U08 gau1_blk_size_to_wd_shift[NUM_BLK_SIZES];

/**
******************************************************************************
 *  @brief Reads out the height of blk given a blk size enumeration
******************************************************************************
 */
extern U08 gau1_blk_size_to_ht[NUM_BLK_SIZES];

/**
******************************************************************************
 *  @brief Given a minimum pt enum in a 3x3 grid, reads out the list of active
 *  search pts in next iteration as a bit-mask, eliminating need to search
 *  pts that have already been searched in this iteration.
******************************************************************************
 */
extern S32 gai4_opt_grid_mask[NUM_GRID_PTS];

/**
******************************************************************************
 *  @brief Given a minimum pt enum in a 3x3 grid, reads out the x offset of
 *  the min pt relative to center assuming step size of 1
******************************************************************************
 */
extern S08 gai1_grid_id_to_x[NUM_GRID_PTS];

/**
******************************************************************************
 *  @brief Given a minimum pt enum in a 3x3 grid, reads out the y offset of
 *  the min pt relative to center assuming step size of 1
******************************************************************************
*/
extern S08 gai1_grid_id_to_y[NUM_GRID_PTS];

/**
******************************************************************************
 *  @brief Lookup of the blk size enum, given a specific partition and cu size
******************************************************************************
*/
extern BLK_SIZE_T ge_part_id_to_blk_size[NUM_CU_SIZES][TOT_NUM_PARTS];

/**
******************************************************************************
 *  @brief For a given partition split, find number of partitions
******************************************************************************
*/
extern U08 gau1_num_parts_in_part_type[MAX_PART_TYPES];

/**
******************************************************************************
 *  @brief For a given partition split, returns the enumerations of specific
 *  partitions in raster order. E.g. for PART_2NxN, part id 0 is
 *  PART_ID_2NxN_T and part id 1 is PART_ID_2NxN_B
******************************************************************************
*/
extern PART_ID_T ge_part_type_to_part_id[MAX_PART_TYPES][MAX_NUM_PARTS];

/**
******************************************************************************
 *  @brief For a given partition id, returs the rectangular position and size
 *  of partition within cu relative ot cu start.
******************************************************************************
*/
extern part_attr_t gas_part_attr_in_cu[TOT_NUM_PARTS];

/**
******************************************************************************
 *  @brief Gives the CU type enumeration given a  blk size.
******************************************************************************
*/
extern CU_SIZE_T ge_blk_size_to_cu_size[NUM_BLK_SIZES];

/**
******************************************************************************

 *  @brief Given a minimum pt enum in a diamond grid, reads out the list
 *  of active search pts in next iteration as a bit-mask, eliminating need
 *  to search pts that have already been searched in this iteration.
******************************************************************************
 */
extern S32 gai4_opt_grid_mask_diamond[5];

/**
******************************************************************************
 *  @brief Given a minimum pt enum in a 9 point grid, reads out the list
 *  of active search pts in next iteration as a bit-mask, eliminating need
 *  to search pts that have already been searched in this iteration.
******************************************************************************
 */

extern S32 gai4_opt_grid_mask_conventional[9];

/**
******************************************************************************
 *  @brief Given a raster coord x, y, this aray returns the CU id in encoding
 *         order. Indexed as [y][x]
******************************************************************************
 */
extern U08 gau1_cu_id_raster_to_enc[4][4];
/**
******************************************************************************
 *  @brief Given a CU size, this array returns blk size enum
******************************************************************************
 */
extern BLK_SIZE_T ge_cu_size_to_blk_size[NUM_CU_SIZES];

/**
******************************************************************************
 *  @brief Given a part type, returns whether the part type is vertically
 *          oriented.
******************************************************************************
 */
extern U08 gau1_is_vert_part[MAX_PART_TYPES];

/**
******************************************************************************
 *  @brief Given a partition, returns the number of best results to consider
 *          for full pell refinement.
******************************************************************************
 */
extern U08 gau1_num_best_results_PQ[TOT_NUM_PARTS];
extern U08 gau1_num_best_results_HQ[TOT_NUM_PARTS];
extern U08 gau1_num_best_results_MS[TOT_NUM_PARTS];
extern U08 gau1_num_best_results_HS[TOT_NUM_PARTS];
extern U08 gau1_num_best_results_XS[TOT_NUM_PARTS];
extern U08 gau1_num_best_results_XS25[TOT_NUM_PARTS];

/**
******************************************************************************
 *  @brief gau1_cu_tr_valid[y][x] returns the validity of a top rt candt for
 *         CU with raster id x, y within CTB. Valid for 16x16 CUs and above
******************************************************************************
 */
extern U08 gau1_cu_tr_valid[4][4];
/**
******************************************************************************
 *  @brief gau1_cu_tr_valid[y][x] returns the validity of a bot lt candt for
 *         CU with raster id x, y within CTB. Valid for 16x16 CUs and above
******************************************************************************
 */
extern U08 gau1_cu_bl_valid[4][4];

/**
******************************************************************************
 *  @brief Returns the validity of top rt candt for a given part id, will not
 *          be valid if tr of a part pts to a non causal neighbour like 16x8B
******************************************************************************
 */
extern U08 gau1_partid_tr_valid[TOT_NUM_PARTS];
/**
******************************************************************************
 *  @brief Returns the validity of bottom left cant for given part id, will
 *  not be valid, if bl of a part pts to a non causal neighbour like 8x16R
******************************************************************************
 */
extern U08 gau1_partid_bl_valid[TOT_NUM_PARTS];

/**
******************************************************************************
 *  @brief The number of partition id in the CU, e.g. PART_ID_16x8_B is 2nd
******************************************************************************
 */
extern U08 gau1_part_id_to_part_num[TOT_NUM_PARTS];

/**
******************************************************************************
 *  @brief Returns partition type for a given partition id, e.g.
 *  PART_ID_16x8_B returns PRT_TYPE_16x8
******************************************************************************
 */
extern PART_TYPE_T ge_part_id_to_part_type[TOT_NUM_PARTS];

/**
******************************************************************************
 *  @brief given raster id x, y of 8x8 blk in 64x64 CTB, return the enc order
******************************************************************************
 */
extern U08 gau1_8x8_cu_id_raster_to_enc[8][8];

/**
******************************************************************************
 *  @brief Return the bits for a given partition id which gets added to the
 *  cost. Although the bits are for a given partition type, we add off the
 *  bits per partition while computing mv cost. For example, if the bits for
 *  2NxN part type is 3, we add 1.5 bits for 2NxN_T and 1.5 for 2NxN_B.
 *  Hence this is stored in Q1 format
******************************************************************************
*/
extern U08 gau1_bits_for_part_id_q1[TOT_NUM_PARTS];

/**
******************************************************************************
 *  @brief Returns 1 if there are qpel points to the top and bottom of the
 *         current point
******************************************************************************
*/
extern S32 gai4_2pt_qpel_interpol_possible_vert[4][4];

/**
******************************************************************************
 *  @brief Returns 1 if there are qpel points to the left and right of the
 *         current point
******************************************************************************
*/
extern S32 gai4_2pt_qpel_interpol_possible_horz[4][4];

/**
******************************************************************************
 *  @brief For a given partition split, num bits to encode the partition type
 *         and split cu,tu bits; assuming one bin equal to one bit for now
******************************************************************************
*/
extern U08 gau1_num_bits_for_part_type[MAX_PART_TYPES];

/**
******************************************************************************
 * @brief Used exclusively in the Intrinsics version of the function
 *       'hme_combine_4x4_sads_and_compute_cost_high_speed' instead
 *       of calling get_range()
******************************************************************************
 */
extern S16 gi2_mvy_range[MAX_MVY_SUPPORTED_IN_COARSE_LAYER + 1][8];

/**
******************************************************************************
 * @brief Used exclusively in the Intrinsics version of the function
 *       'hme_combine_4x4_sads_and_compute_cost_high_speed' instead
 *       of calling get_range()
******************************************************************************
 */
extern S16 gi2_mvx_range[MAX_MVX_SUPPORTED_IN_COARSE_LAYER * 2 + 1][8];

extern S32 gai4_select_qpel_function_vert[4][16];

extern S32 gai4_select_qpel_function_horz[4][16];

extern S32 gai4_partition_area[TOT_NUM_PARTS];

extern const U08 gau1_search_cand_priority_in_l1_and_l2_me[2][NUM_SEARCH_CAND_TYPES];

/* 12 cases are -  */
/* case  0 - P picture, num_refs=1, 4x4 in L1ME = 0 */
/* case  1 - P picture, num_refs=1, 4x4 in L1ME = 1 */
/* case  2 - P picture, num_refs=2, 4x4 in L1ME = 0 */
/* case  3 - P picture, num_refs=2, 4x4 in L1ME = 1 */
/* case  4 - P picture, num_refs=3, 4x4 in L1ME = 0 */
/* case  5 - P picture, num_refs=3, 4x4 in L1ME = 1 */
/* case  6 - P picture, num_refs=3, 4x4 in L1ME = 0 */
/* case  7 - P picture, num_refs=3, 4x4 in L1ME = 1 */
/* case  8 - B picture, num_refs=1, 4x4 in L1ME = 0 */
/* case  9 - B picture, num_refs=1, 4x4 in L1ME = 1 */
/* case 10 - B picture, num_refs=2, 4x4 in L1ME = 0 */
/* case 11 - B picture, num_refs=2, 4x4 in L1ME = 1 */
extern const U08 gau1_search_cand_priority_in_l0_me[12][NUM_SEARCH_CAND_TYPES];

extern const SEARCH_CANDIDATE_TYPE_T
    gae_search_cand_priority_to_search_cand_type_map_in_l0_me[12][NUM_SEARCH_CAND_TYPES];

extern const U08 gau1_max_num_search_cands_in_l0_me[12];

extern const SEARCH_CAND_LOCATIONS_T gae_search_cand_type_to_location_map[NUM_SEARCH_CAND_TYPES];

extern const S08 gai1_search_cand_type_to_result_id_map[NUM_SEARCH_CAND_TYPES];

extern const U08 gau1_search_cand_type_to_spatiality_map[NUM_SEARCH_CAND_TYPES];

extern const S32 gai4_search_cand_location_to_x_offset_map[NUM_SEARCH_CAND_LOCATIONS];

extern const S32 gai4_search_cand_location_to_y_offset_map[NUM_SEARCH_CAND_LOCATIONS];

/**
******************************************************************************
 * @brief Used exclusively in the Intrinsics version of the function
 *       'hme_combine_4x4_sads_and_compute_cost_high_quality' instead
 *       of calling get_range()
******************************************************************************
 */
extern S16 gi2_mvx_range_high_quality[MAX_MVX_SUPPORTED_IN_COARSE_LAYER * 2 + 1][8];

extern const S16 gai2_mvx_range_mapping[8193][8];

extern const S16 gai2_mvy_range_mapping[4097][8];

extern const S16 gai2_set_best_cost_max[8][8];

extern const S08 gai1_mv_adjust[8][2];

extern const S08 gai1_mv_offsets_from_center_in_rect_grid[NUM_POINTS_IN_RECTANGULAR_GRID][2];

#endif /* #ifndef _HME_GLOBALS_H_*/
