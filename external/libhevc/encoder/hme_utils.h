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
* \file hme_utils.h
*
* \brief
*    Prototypes for various utilities used by coarse/refinement/subpel fxns
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _HME_UTILS_H_
#define _HME_UTILS_H_

/*****************************************************************************/
/* Functions                                                                 */
/*****************************************************************************/

/**
********************************************************************************
*  @fn     hme_init_histogram(
*
*  @brief  Top level entry point for Coarse ME. Runs across blocks and does the
*          needful by calling other low level routines.
*
*  @param[in,out]  ps_hist : the histogram structure
*
*  @param[in]  i4_max_mv_x : Maximum mv allowed in x direction (fpel units)
*
*  @param[in]  i4_max_mv_y : Maximum mv allowed in y direction (fpel units)
*
*  @return None
********************************************************************************
*/
void hme_init_histogram(mv_hist_t *ps_hist, S32 i4_max_mv_x, S32 i4_max_mv_y);

/**
********************************************************************************
*  @fn     hme_update_histogram(
*
*  @brief  Updates the histogram given an mv entry
*
*  @param[in,out]  ps_hist : the histogram structure
*
*  @param[in]  i4_mv_x : x component of the mv (fpel units)
*
*  @param[in]  i4_mv_y : y component of the mv (fpel units)
*
*  @return None
********************************************************************************
*/
void hme_update_histogram(mv_hist_t *ps_hist, S32 i4_mv_x, S32 i4_mv_y);

/**
********************************************************************************
*  @fn     hme_get_global_mv(
*
*  @brief  returns the global mv of a previous picture. Accounts for the fact
*          that the delta poc of the previous picture may have been different
*          from delta poc of current picture. Delta poc is POC difference
*          between a picture and its reference.
*
*  @param[out]  ps_mv: mv_t structure where the motion vector is returned
*
*  @param[in]  i4_delta_poc: the delta poc for the current pic w.r.t. reference
*
*  @return None
********************************************************************************
*/
void hme_get_global_mv(layer_ctxt_t *ps_prev_layer, hme_mv_t *ps_mv, S32 i4_delta_poc);

/**
********************************************************************************
*  @fn     hme_calculate_global_mv(
*
*  @brief  Calculates global mv for a given histogram
*
*  @param[in]  ps_hist : the histogram structure
*
*  @param[in]  ps_mv : used to return the global mv
*
*  @param[in]  e_lobe_type : refer to GMV_MVTYPE_T
*
*  @return None
********************************************************************************
*/
void hme_calculate_global_mv(mv_hist_t *ps_hist, hme_mv_t *ps_mv, GMV_MVTYPE_T e_lobe_type);

/**
********************************************************************************
*  @fn     hme_collate_fpel_results(search_results_t *ps_search_results,
*           S32 i1_ref_idx, S32 i1_idx_to_merge)
*
*  @brief  After full pel search and result seeding in every search iteration
*          results, this function called to collapse a given search iteration
*          results into another.
*
*  @param[in,out] ps_search_results : Search results data structure
*  @param[in]     i1_ref_idx: id of the search iteration where the results
                              will be collapsed
*  @param[in]     i1_idx_to_merge : id of the search iteration from which the
*                   results are picked up.

*
*  @return None
********************************************************************************
*/
void hme_collate_fpel_results(
    search_results_t *ps_search_results, S08 i1_ref_idx, S08 i1_idx_to_merge);

/**
********************************************************************************
*  @fn     hme_map_mvs_to_grid(mv_grid_t **pps_mv_grid,
            search_results_t *ps_search_results, S32 i4_num_ref)
*
*  @brief  For a given CU whose results are in ps_search_results, the 17x17
*          mv grid is updated for future use within the CTB
*
*  @param[in] ps_search_results : Search results data structure
*
*  @param[out] pps_mv_grid: The mv grid (as many as num ref)
*
*  @param[in]  i4_num_ref: nuber of search iterations to update
*
*  @param[in]  mv_res_shift: Shift for resolution of mv (fpel/qpel)
*
*  @return None
********************************************************************************
*/
void hme_map_mvs_to_grid(
    mv_grid_t **pps_mv_grid,
    search_results_t *ps_search_results,
    U08 *pu1_pred_dir_searched,
    S32 i4_num_pred_dir);

/**
********************************************************************************
*  @fn     hme_create_valid_part_ids(S32 i4_part_mask, S32 *pi4_valid_part_ids)
*
*  @brief  Expands the part mask to a list of valid part ids terminated by -1
*
*  @param[in] i4_part_mask : bit mask of active partitino ids
*
*  @param[out] pi4_valid_part_ids : array, each entry has one valid part id
*               Terminated by -1 to signal end.
*
*  @return number of partitions
********************************************************************************
*/
S32 hme_create_valid_part_ids(S32 i4_part_mask, S32 *pi4_valid_part_ids);

/**
********************************************************************************
*  @fn     get_num_blks_in_ctb(S32 i4_ctb_x,
                        S32 i4_ctb_y,
                        S32 i4_pic_wd,
                        S32 i4_pic_ht,
                        S32 i4_blk_size)
*
*  @brief  returns the number of blks in the ctb (64x64 ctb)
*
*  @param[in] i4_ctb_x : pixel x offset of the top left corner of ctb in pic
*
*  @param[in] i4_ctb_y : pixel y offset of the top left corner of ctb in pic
*
*  @param[in] i4_ctb_x : width of the picture in pixels
*
*  @param[in] i4_pic_ht : height of hte picture in pixels
*
*  @param[in] i4_blk_size : Size of the blk in pixels
*
*  @return number of blks in the ctb
********************************************************************************
*/
S32 get_num_blks_in_ctb(S32 i4_ctb_x, S32 i4_ctb_y, S32 i4_pic_wd, S32 i4_pic_ht, S32 i4_blk_size);

/**
********************************************************************************
*  @fn     hevc_avg_2d(U08 *pu1_src1,
*                   U08 *pu1_src2,
*                   S32 i4_src1_stride,
*                   S32 i4_src2_stride,
*                   S32 i4_blk_wd,
*                   S32 i4_blk_ht,
*                   U08 *pu1_dst,
*                   S32 i4_dst_stride)
*
*
*  @brief  point wise average of two buffers into a third buffer
*
*  @param[in] pu1_src1 : first source buffer
*
*  @param[in] pu1_src2 : 2nd source buffer
*
*  @param[in] i4_src1_stride : stride of source 1 buffer
*
*  @param[in] i4_src2_stride : stride of source 2 buffer
*
*  @param[in] i4_blk_wd : block width
*
*  @param[in] i4_blk_ht : block height
*
*  @param[out] pu1_dst : destination buffer
*
*  @param[in] i4_dst_stride : stride of the destination buffer
*
*  @return void
********************************************************************************
*/
void hevc_avg_2d(
    U08 *pu1_src1,
    U08 *pu1_src2,
    S32 i4_src1_stride,
    S32 i4_src2_stride,
    S32 i4_blk_wd,
    S32 i4_blk_ht,
    U08 *pu1_dst,
    S32 i4_dst_stride);

/**
********************************************************************************
*  @fn     hme_pick_back_search_node(search_results_t *ps_search_results,
*                                   search_node_t *ps_search_node_fwd,
*                                   S32 i4_part_idx,
*                                   layer_ctxt_t *ps_curr_layer)
*
*
*  @brief  returns the search node corresponding to a ref idx in same or
*          opp direction. Preference is given to opp direction, but if that
*          does not yield results, same direction is attempted.
*
*  @param[in] ps_search_results: search results overall
*
*  @param[in] ps_search_node_fwd: search node corresponding to "fwd" direction
*
*  @param[in] i4_part_idx : partition id
*
*  @param[in] ps_curr_layer : layer context for current layer.
*
*  @return search node corresponding to hte "other direction"
********************************************************************************
*/
search_node_t *hme_pick_back_search_node(
    search_results_t *ps_search_results,
    search_node_t *ps_search_node_fwd,
    S32 i4_part_idx,
    layer_ctxt_t *ps_curr_layer);

/**
********************************************************************************
*  @fn     hme_study_input_segmentation(U08 *pu1_inp,
*                                       S32 i4_inp_stride,
*                                       S32 limit_active_partitions)
*
*
*  @brief  Examines input 16x16 for possible edges and orientations of those,
*          and returns a bit mask of partitions that should be searched for
*
*  @param[in] pu1_inp : input buffer
*
*  @param[in] i4_inp_stride: input stride
*
*  @param[in] limit_active_partitions : 1: Edge algo done and partitions are
*               limited, 0 : Brute force, all partitions considered
*
*  @return part mask (bit mask of active partitions to search)
********************************************************************************
*/
S32 hme_study_input_segmentation(U08 *pu1_inp, S32 i4_inp_stride, S32 limit_active_partitions);

/**
********************************************************************************
*  @fn     hme_init_search_results(search_results_t *ps_search_results,
*                           S32 i4_num_ref,
*                           S32 i4_num_best_results,
*                           S32 i4_num_results_per_part,
*                           BLK_SIZE_T e_blk_size,
*                           S32 i4_x_off,
*                           S32 i4_y_off)
*
*  @brief  Initializes the search results structure with some key attributes
*
*  @param[out] ps_search_results : search results structure to initialise
*
*  @param[in] i4_num_Ref: corresponds to the number of ref ids searched
*
*  @param[in] i4_num_best_results: Number of best results for the CU to
*               be maintained in the result structure
*
*  @param[in] i4_num_results_per_part: Per active partition the number of best
*               results to be maintained
*
*  @param[in] e_blk_size: blk size of the CU for which this structure used
*
*  @param[in] i4_x_off: x offset of the top left of CU from CTB top left
*
*  @param[in] i4_y_off: y offset of the top left of CU from CTB top left
*
*  @return void
********************************************************************************
*/
void hme_init_search_results(
    search_results_t *ps_search_results,
    S32 i4_num_ref,
    S32 i4_num_best_results,
    S32 i4_num_results_per_part,
    BLK_SIZE_T e_blk_size,
    S32 i4_x_off,
    S32 i4_y_off,
    U08 *pu1_is_past);

/**
********************************************************************************
*  @fn     hme_reset_search_results((search_results_t *ps_search_results,
*                               S32 i4_part_mask)
*
*
*  @brief  Resets the best results to maximum values, so as to allow search
*          for the new CU's partitions. The existing results may be from an
*          older CU using same structure.
*
*  @param[in] ps_search_results: search results structure
*
*  @param[in] i4_part_mask : bit mask of active partitions
*
*  @param[in] mv_res : Resolution of the mv predictors (fpel/qpel)
*
*  @return void
********************************************************************************
*/
void hme_reset_search_results(search_results_t *ps_search_results, S32 i4_part_mask, S32 mv_res);

/**
********************************************************************************
*  @fn     hme_clamp_grid_by_mvrange(search_node_t *ps_search_node,
*                               S32 i4_step,
*                               range_prms_t *ps_mvrange)
*
*  @brief  Given a central pt within mv range, and a grid of points surrounding
*           this pt, this function returns a grid mask of pts within search rng
*
*  @param[in] ps_search_node: the centre pt of the grid
*
*  @param[in] i4_step: step size of grid
*
*  @param[in] ps_mvrange: structure containing the current mv range
*
*  @return bitmask of the  pts in grid within search range
********************************************************************************
*/
S32 hme_clamp_grid_by_mvrange(search_node_t *ps_search_node, S32 i4_step, range_prms_t *ps_mvrange);

/**
********************************************************************************
*  @fn    layer_ctxt_t *hme_get_past_layer_ctxt(me_ctxt_t *ps_ctxt,
                                    S32 i4_layer_id)
*
*  @brief  returns the layer ctxt of the layer with given id from the temporally
*          previous frame
*
*  @param[in] ps_ctxt : ME context
*
*  @param[in] i4_layer_id : id of layer required
*
*  @return layer ctxt of given layer id in temporally previous frame
********************************************************************************
*/
layer_ctxt_t *hme_get_past_layer_ctxt(
    me_ctxt_t *ps_ctxt, me_frm_ctxt_t *ps_frm_ctxt, S32 i4_layer_id, S32 i4_num_me_frm_pllel);

layer_ctxt_t *hme_coarse_get_past_layer_ctxt(coarse_me_ctxt_t *ps_ctxt, S32 i4_layer_id);

/**
********************************************************************************
*  @fn    void hme_init_mv_bank(layer_ctxt_t *ps_layer_ctxt,
                        BLK_SIZE_T e_blk_size,
                        S32 i4_num_ref,
                        S32 i4_num_results_per_part)
*
*  @brief  Given a blk size to be used for this layer, this function initialize
*          the mv bank to make it ready to store and return results.
*
*  @param[in, out] ps_layer_ctxt: pointer to layer ctxt
*
*  @param[in] e_blk_size : resolution at which mvs are stored
*
*  @param[in] i4_num_ref: number of reference frames corresponding to which
*              results are stored.
*
*  @param[in] e_blk_size : resolution at which mvs are stored
*
*  @param[in] i4_num_results_per_part : Number of results to be stored per
*               ref idx. So these many best results stored
*
*  @return void
********************************************************************************
*/
void hme_init_mv_bank(
    layer_ctxt_t *ps_layer_ctxt,
    BLK_SIZE_T e_blk_size,
    S32 i4_num_ref,
    S32 i4_num_results_per_part,
    U08 u1_enc);

/**
********************************************************************************
*  @fn    void hme_derive_search_range(range_prms_t *ps_range,
*                                   range_prms_t *ps_pic_limit,
*                                   range_prms_t *ps_mv_limit,
*                                   S32 i4_x,
*                                   S32 i4_y,
*                                   S32 blk_wd,
*                                   S32 blk_ht)
*
*  @brief  given picture limits and blk dimensions and mv search limits, obtains
*          teh valid search range such that the blk stays within pic boundaries,
*          where picture boundaries include padded portions of picture
*
*  @param[out] ps_range: updated with actual search range
*
*  @param[in] ps_pic_limit : picture boundaries
*
*  @param[in] ps_mv_limit: Search range limits for the mvs
*
*  @param[in] i4_x : x coordinate of the blk
*
*  @param[in] i4_y : y coordinate of the blk
*
*  @param[in] blk_wd : blk width
*
*  @param[in] blk_ht : blk height
*
*  @return void
********************************************************************************
*/
void hme_derive_search_range(
    range_prms_t *ps_range,
    range_prms_t *ps_pic_limit,
    range_prms_t *ps_mv_limit,
    S32 i4_x,
    S32 i4_y,
    S32 blk_wd,
    S32 blk_ht);

/**
********************************************************************************
*  @fn    void hme_get_spatial_candt(layer_ctxt_t *ps_curr_layer,
*                                   BLK_SIZE_T e_search_blk_size,
*                                   S32 blk_x,
*                                   S32 blk_y,
*                                   S08 i1_ref_idx,
*                                   search_node_t *ps_top_neighbours,
*                                   search_node_t *ps_left_neighbours,
*                                   S32 i4_result_id);
*
*  @brief  Obtains top, top left, top right and left adn bottom left candts
*
*  @param[in] ps_curr_layer: layer ctxt, has the mv bank structure pointer
*
*  @param[in] e_search_blk_size : search blk size of current layer
*
*  @param[in] i4_blk_x : x coordinate of the block in mv bank
*
*  @param[in] i4_blk_y : y coordinate of the block in mv bank
*
*  @param[in] i1_ref_idx : Corresponds to ref idx from which to pick up mv
*              results, useful if multiple ref idx candts maintained separately.
*
*  @param[out] ps_top_neighbours : T, TL, TR candts are output here
*
*  @param[out] ps_left_neighbours : L BL candts outptu here
*
*  @param[in] i4_result_id : If multiple results stored per ref idx, this
*              pts to the id of the result
*
*  @return void
********************************************************************************
*/
void hme_get_spatial_candt(
    layer_ctxt_t *ps_curr_layer,
    BLK_SIZE_T e_search_blk_size,
    S32 blk_x,
    S32 blk_y,
    S08 i1_ref_idx,
    search_node_t *ps_top_neighbours,
    search_node_t *ps_left_neighbours,
    S32 i4_result_id,
    S32 i4_tr_avail,
    S32 i4_bl_avail,
    S32 encode);

void hme_get_spatial_candt_in_l1_me(
    layer_ctxt_t *ps_curr_layer,
    BLK_SIZE_T e_search_blk_size,
    S32 i4_blk_x,
    S32 i4_blk_y,
    S08 i1_ref_idx,
    U08 u1_pred_dir,
    search_node_t *ps_top_neighbours,
    search_node_t *ps_left_neighbours,
    S32 i4_result_id,
    S32 tr_avail,
    S32 bl_avail,
    S32 i4_num_act_ref_l0,
    S32 i4_num_act_ref_l1);

/**
********************************************************************************
*  @fn    void hme_fill_ctb_neighbour_mvs(layer_ctxt_t *ps_curr_layer,
*                                   S32 i4_blk_x,
*                                   S32 i4_blk_y,
*                                   mvgrid_t *ps_mv_grid ,
*                                   S32 i1_ref_id)
*
*  @brief  The 18x18 MV grid for a ctb, is filled in first row and 1st col
*          this corresponds to neighbours (TL, T, TR, L, BL)
*
*  @param[in] ps_curr_layer: layer ctxt, has the mv bank structure pointer
*
*  @param[in] blk_x : x coordinate of the block in mv bank
*
*  @param[in] blk_y : y coordinate of the block in mv bank
*
*  @param[in] ps_mv_grid : Grid (18x18 mvs at 4x4 level)
*
*  @param[in] u1_pred_lx : Corresponds to pred dir from which to pick up mv
*              results
*
*  @return void
********************************************************************************
*/
void hme_fill_ctb_neighbour_mvs(
    layer_ctxt_t *ps_curr_layer,
    S32 blk_x,
    S32 blk_y,
    mv_grid_t *ps_mv_grid,
    U08 u1_pred_dir_ctr,
    U08 u1_default_ref_id,
    S32 i4_num_act_ref_l0);

/**
********************************************************************************
*  @fn     void *hme_get_wkg_mem(buf_mgr_t *ps_buf_mgr, S32 i4_size)
*
*  @brief  Allocates a block of size = i4_size from working memory and returns
*
*  @param[in,out] ps_buf_mgr: Buffer manager for wkg memory
*
*  @param[in]  i4_size : size required
*
*  @return void pointer to allocated memory, NULL if failure
********************************************************************************
*/
void *hme_get_wkg_mem(buf_mgr_t *ps_buf_mgr, S32 i4_size);

void hme_reset_wkg_mem(buf_mgr_t *ps_buf_mgr);

void hme_init_wkg_mem(buf_mgr_t *ps_buf_mgr, U08 *pu1_mem, S32 size);

void hme_reset_ctb_mem_mgr(ctb_mem_mgr_t *ps_ctb_mem_mgr);

void hme_init_ctb_mem_mgr(ctb_mem_mgr_t *ps_ctb_mem_mgr, U08 *pu1_mem, S32 size);

void hme_fill_mvbank_intra(layer_ctxt_t *ps_layer_ctxt);

void hme_scale_mv_grid(mv_grid_t *ps_mv_grid);

void hme_downscale_mv_grid(mv_grid_t *ps_mv_grid);

void hme_create_parent_ctb(
    ctb_node_t *ps_ctb_node_parent,
    ctb_node_t *ps_ctb_child_tl,
    ctb_node_t *ps_ctb_child_tr,
    ctb_node_t *ps_ctb_child_bl,
    ctb_node_t *ps_ctb_child_br,
    CU_SIZE_T e_cu_size_parent,
    buf_mgr_t *ps_buf_mgr);

void hme_create_merged_ctbs(
    search_results_t *ps_results_merged,
    ctb_mem_mgr_t *ps_ctb_mem_mgr,
    buf_mgr_t *ps_buf_mgr,
    ctb_node_t **pps_ctb_list_unified,
    S32 num_candts);

void hme_init_mv_grid(mv_grid_t *ps_mv_grid);

typedef void (*pf_get_wt_inp)(
    layer_ctxt_t *ps_curr_layer,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    S32 dst_stride,
    S32 pos_x,
    S32 pos_y,
    S32 size,
    S32 num_ref,
    U08 u1_is_wt_pred_on);

/**
********************************************************************************
*  @fn    void hme_pad_left(U08 *pu1_dst, S32 stride, S32 pad_wd, S32 pad_ht)
*
*  @brief  Pads horizontally to left side. Each pixel replicated across a line
*
*  @param[in] pu1_dst : destination pointer. Points to the pixel to be repeated
*
*  @param[in] stride : stride of destination buffer
*
*  @param[in] pad_wd : Amt of horizontal padding to be done
*
*  @param[in] pad_ht : Number of lines for which horizontal padding to be done
*
*  @return void
********************************************************************************
*/
void hme_pad_left(U08 *pu1_dst, S32 stride, S32 pad_wd, S32 pad_ht);

/**
********************************************************************************
*  @fn    void hme_pad_right(U08 *pu1_dst, S32 stride, S32 pad_wd, S32 pad_ht)
*
*  @brief  Pads horizontally to rt side. Each pixel replicated across a line
*
*  @param[in] pu1_dst : destination pointer. Points to the pixel to be repeated
*
*  @param[in] stride : stride of destination buffer
*
*  @param[in] pad_wd : Amt of horizontal padding to be done
*
*  @param[in] pad_ht : Number of lines for which horizontal padding to be done
*
*  @return void
********************************************************************************
*/
void hme_pad_right(U08 *pu1_dst, S32 stride, S32 pad_wd, S32 pad_ht);

/**
********************************************************************************
*  @fn    void hme_pad_top(U08 *pu1_dst, S32 stride, S32 pad_ht, S32 pad_wd)
*
*  @brief  Pads vertically on the top. Repeats the top line for top padding
*
*  @param[in] pu1_dst : destination pointer. Points to the line to be repeated
*
*  @param[in] stride : stride of destination buffer
*
*  @param[in] pad_ht : Amt of vertical padding to be done
*
*  @param[in] pad_wd : Number of columns for which vertical padding to be done
*
*  @return void
********************************************************************************
*/
void hme_pad_top(U08 *pu1_dst, S32 stride, S32 pad_ht, S32 pad_wd);

/**
********************************************************************************
*  @fn    void hme_pad_bot(U08 *pu1_dst, S32 stride, S32 pad_ht, S32 pad_wd)
*
*  @brief  Pads vertically on the bot. Repeats the top line for top padding
*
*  @param[in] pu1_dst : destination pointer. Points to the line to be repeated
*
*  @param[in] stride : stride of destination buffer
*
*  @param[in] pad_ht : Amt of vertical padding to be done
*
*  @param[in] pad_wd : Number of columns for which vertical padding to be done
*
*  @return void
********************************************************************************
*/
void hme_pad_bot(U08 *pu1_dst, S32 stride, S32 pad_ht, S32 pad_wd);

/**
**************************************************************************************************
*  @fn     hme_populate_pus(search_results_t *ps_search_results, inter_cu_results_t *ps_cu_results)
*
*  @brief  Population the pu_results structure with the results after the subpel refinement
*
*          This is called post subpel refinmenent for 16x16s, 8x8s and
*          for post merge evaluation for 32x32,64x64 CUs
*
*  @param[in,out] ps_search_results : Search results data structure
*                 - ps_cu_results : cu_results data structure
*                   ps_pu_result  : Pointer to the memory for storing PU's
*
****************************************************************************************************
*/
void hme_populate_pus(
    me_ctxt_t *ps_thrd_ctxt,
    me_frm_ctxt_t *ps_ctxt,
    hme_subpel_prms_t *ps_subpel_prms,
    search_results_t *ps_search_results,
    inter_cu_results_t *ps_cu_results,
    inter_pu_results_t *ps_pu_results,
    pu_result_t *ps_pu_result,
    inter_ctb_prms_t *ps_inter_ctb_prms,
    wgt_pred_ctxt_t *ps_wt_prms,
    layer_ctxt_t *ps_curr_layer,
    U08 *pu1_pred_dir_searched,
    WORD32 i4_num_active_ref);

void hme_populate_pus_8x8_cu(
    me_ctxt_t *ps_thrd_ctxt,
    me_frm_ctxt_t *ps_ctxt,
    hme_subpel_prms_t *ps_subpel_prms,
    search_results_t *ps_search_results,
    inter_cu_results_t *ps_cu_results,
    inter_pu_results_t *ps_pu_results,
    pu_result_t *ps_pu_result,
    inter_ctb_prms_t *ps_inter_ctb_prms,
    U08 *pu1_pred_dir_searched,
    WORD32 i4_num_active_ref,
    U08 u1_blk_8x8_mask);

S32 hme_recompute_lambda_from_min_8x8_act_in_ctb(
    me_frm_ctxt_t *ps_ctxt, ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb);

/**
********************************************************************************
*  @fn     hme_update_dynamic_search_params
*
*  @brief  Update the Dynamic search params based on the current MVs
*
*  @param[in,out]  ps_dyn_range_prms    [inout] : Dyn. Range Param str.
*                  i2_mvy               [in]    : current MV y comp.
*
*  @return None
********************************************************************************
*/
void hme_update_dynamic_search_params(dyn_range_prms_t *ps_dyn_range_prms, WORD16 i2_mvy);

S32 hme_create_child_nodes_cu_tree(
    cur_ctb_cu_tree_t *ps_cu_tree_root,
    cur_ctb_cu_tree_t *ps_cu_tree_cur_node,
    S32 nodes_already_created);

void hme_add_new_node_to_a_sorted_array(
    search_node_t *ps_result_node,
    search_node_t **pps_sorted_array,
    U08 *pu1_shifts,
    U32 u4_num_results_updated,
    U08 u1_shift);

S32 hme_find_pos_of_implicitly_stored_ref_id(
    S08 *pi1_ref_idx, S08 i1_ref_idx, S32 i4_result_id, S32 i4_num_results);

S32 hme_populate_search_candidates(fpel_srch_cand_init_data_t *ps_ctxt);

void hme_init_pred_buf_info(
    hme_pred_buf_info_t (*ps_info)[MAX_NUM_INTER_PARTS],
    hme_pred_buf_mngr_t *ps_buf_mngr,
    U08 u1_pu1_wd,
    U08 u1_pu1_ht,
    PART_TYPE_T e_part_type);

void hme_debrief_bipred_eval(
    part_type_results_t *ps_part_type_result,
    hme_pred_buf_info_t (*ps_pred_buf_info)[MAX_NUM_INTER_PARTS],
    hme_pred_buf_mngr_t *ps_pred_buf_mngr,
    U08 *pu1_allocated_pred_buf_array_indixes,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list);

U08 hme_decide_search_candidate_priority_in_l1_and_l2_me(
    SEARCH_CANDIDATE_TYPE_T e_cand_type, ME_QUALITY_PRESETS_T e_quality_preset);

U08 hme_decide_search_candidate_priority_in_l0_me(SEARCH_CANDIDATE_TYPE_T e_cand_type, U08 u1_index);

void hme_search_cand_data_init(
    S32 *pi4_id_Z,
    S32 *pi4_id_coloc,
    S32 *pi4_num_coloc_cands,
    U08 *pu1_search_candidate_list_index,
    S32 i4_num_act_ref_l0,
    S32 i4_num_act_ref_l1,
    U08 u1_is_bidir_enabled,
    U08 u1_4x4_blk_in_l1me);

void hme_compute_variance_for_all_parts(
    U08 *pu1_data,
    S32 i4_data_stride,
    S32 *pi4_valid_part_array,
    U32 *pu4_variance,
    S32 i4_num_valid_parts,
    U08 u1_cu_size);

void hme_compute_sigmaX_and_sigmaXSquared(
    U08 *pu1_data,
    S32 i4_buf_stride,
    void *pv_sigmaX,
    void *pv_sigmaXSquared,
    U08 u1_base_blk_wd,
    U08 u1_base_blk_ht,
    U08 u1_blk_wd,
    U08 u1_blk_ht,
    U08 u1_is_sigma_pointer_size_32_bit,
    U08 u1_array_stride);

void hme_compute_final_sigma_of_pu_from_base_blocks(
    U32 *pu4_SigmaX,
    U32 *pu4_SigmaXSquared,
    ULWORD64 *pu8_final_sigmaX,
    ULWORD64 *pu8_final_sigmaX_Squared,
    U08 u1_cu_size,
    U08 u1_base_block_size,
    S32 i4_part_id,
    U08 u1_base_blk_array_stride);

void hme_compute_stim_injected_distortion_for_all_parts(
    U08 *pu1_pred,
    S32 i4_pred_stride,
    S32 *pi4_valid_part_array,
    ULWORD64 *pu8_src_sigmaX,
    ULWORD64 *pu8_src_sigmaXSquared,
    S32 *pi4_sad_array,
    S32 i4_alpha_stim_multiplier,
    S32 i4_inv_wt,
    S32 i4_inv_wt_shift_val,
    S32 i4_num_valid_parts,
    S32 i4_wpred_log_wdc,
    U08 u1_cu_size);

void sigma_for_cusize_16_and_baseblock_size_16(
    U08 *pu1_data, S32 i4_data_stride, U32 *pu4_sigmaX, U32 *pu4_sigmaXSquared);

void sigma_for_cusize_16_and_baseblock_size_8(
    U08 *pu1_data, S32 i4_data_stride, U32 *pu4_sigmaX, U32 *pu4_sigmaXSquared, U08 diff_cu_size);

void sigma_for_cusize_16_and_baseblock_size_4(
    U08 *pu1_data, S32 i4_data_stride, U32 *pu4_sigmaX, U32 *pu4_sigmaXSquared);

void sigma_for_cusize_32_and_baseblock_size_32(
    U08 *pu1_data, S32 i4_data_stride, U32 *pu4_sigmaX, U32 *pu4_sigmaXSquared);

void sigma_for_cusize_64_and_baseblock_size_64(
    U08 *pu1_data, S32 i4_data_stride, U32 *pu4_sigmaX, U32 *pu4_sigmaXSquared);

void hme_choose_best_noise_preserver_amongst_fpel_and_subpel_winners(
    fullpel_refine_ctxt_t *ps_fullpel_winner_data,
    search_node_t **pps_part_results,
    layer_ctxt_t *ps_curr_layer,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    U32 *pu4_src_variance,
    S32 i4_cu_x_off_in_ctb,
    S32 i4_cu_y_off_in_ctb,
    S32 i4_ctb_x_off,
    S32 i4_ctb_y_off,
    S32 i4_inp_stride,
    S32 i4_alpha_stim_multiplier,
    U08 u1_subpel_uses_satd);

#if TEMPORAL_NOISE_DETECT
WORD32 ihevce_16x16block_temporal_noise_detect(
    WORD32 had_block_size,
    WORD32 ctb_width,
    WORD32 ctb_height,
    ihevce_ctb_noise_params *ps_ctb_noise_params,
    fpel_srch_cand_init_data_t *s_proj_srch_cand_init_data,
    hme_search_prms_t *s_search_prms_blk,
    me_frm_ctxt_t *ps_ctxt,
    WORD32 num_pred_dir,
    WORD32 i4_num_act_ref_l0,
    WORD32 i4_num_act_ref_l1,
    WORD32 i4_cu_x_off,
    WORD32 i4_cu_y_off,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    WORD32 input_stride,
    WORD32 index_8x8_block,
    WORD32 num_horz_blocks,
    WORD32 num_8x8_in_ctb_row,
    WORD32 i4_index_variance);
#endif

/**
********************************************************************************
*  @fn     hme_decide_part_types(search_results_t *ps_search_results)
*
*  @brief  Does uni/bi evaluation accross various partition types,
*          decides best inter partition types for the CU, compares
*          intra cost and decides the best K results for the CU
*
*          This is called post subpel refinmenent for 16x16s, 8x8s and
*          for post merge evaluation for 32x32,64x64 CUs
*
*  @param[in,out] ps_search_results : Search results data structure
*                 - In : 2 lists of upto 2mvs & refids, active partition mask
*                 - Out: Best results for final rdo evaluation of the cu
*
*  @param[in]     ps_subpel_prms : Sub pel params data structure

*
*  @par Description
*    --------------------------------------------------------------------------------
*     Flow:
*            for each category (SMP,AMP,2Nx2N based on part mask)
*            {
*                for each part_type
*                {
*                    for each part
*                        pick best candidate from each list
*                    combine uni part type
*                    update best results for part type
*                }
*                pick the best part type for given category (for SMP & AMP)
*            }
*                    ||
*                    ||
*                    \/
*            for upto 3 best part types
*            {
*                for each part
*                {
*                    compute fixed size had for all uni and remember coeffs
*                    compute bisatd
*                    uni vs bi and gives upto two results
*                    also gives the pt level pred buffer
*                }
*             }
*                    ||
*                    ||
*                    \/
*            select X candidates for tu recursion as per the Note below
*               tu_rec_on_part_type (reuse transform coeffs)
*                    ||
*                    ||
*                    \/
*            insert intra nodes at appropriate result id
*                    ||
*                    ||
*                    \/
*            populate y best resuls for rdo based on preset
*
*     Note :
*     number of TU rec for P pics : 2 2nx2n + 1 smp + 1 amp for ms or 9 for hq
*     number of TU rec for B pics : 1 2nx2n + 1 smp + 1 amp for ms or 2 uni 2nx2n + 1 smp + 1 amp for ms or 9 for hq
*     --------------------------------------------------------------------------------
*
*  @return None
********************************************************************************
*/
void hme_decide_part_types(
    inter_cu_results_t *ps_cu_results,
    inter_pu_results_t *ps_pu_results,
    inter_ctb_prms_t *ps_inter_ctb_prms,
    me_frm_ctxt_t *ps_ctxt,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list);

void hme_compute_pred_and_evaluate_bi(
    inter_cu_results_t *ps_cu_results,
    inter_pu_results_t *ps_pu_results,
    inter_ctb_prms_t *ps_inter_ctb_prms,
    part_type_results_t *ps_part_type_result,
    ULWORD64 *pu8_winning_pred_sigmaXSquare,
    ULWORD64 *pu8_winning_pred_sigmaX,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list);

/**
********************************************************************************
*  @fn     hme_insert_intra_nodes_post_bipred
*
*  @brief  Compares intra costs (populated by IPE) with the best inter costs
*          (populated after evaluating bi-pred) and updates the best results
*          if intra cost is better
*
*  @param[in,out]  ps_cu_results    [inout] : Best results structure of CU
*                  ps_cur_ipe_ctb   [in]    : intra results for the current CTB
*                  i4_frm_qstep     [in]    : current frame quantizer(qscale)*
*
*  @return None
********************************************************************************
*/
void hme_insert_intra_nodes_post_bipred(
    inter_cu_results_t *ps_cu_results,
    ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb,
    WORD32 i4_frm_qstep);

void hme_set_mv_limit_using_dvsr_data(
    me_frm_ctxt_t *ps_ctxt,
    layer_ctxt_t *ps_curr_layer,
    range_prms_t *ps_mv_limit,
    S16 *pi2_prev_enc_frm_max_mv_y,
    U08 u1_num_act_ref_pics);

S32 hme_part_mask_populator(
    U08 *pu1_inp,
    S32 i4_inp_stride,
    U08 u1_limit_active_partitions,
    U08 u1_is_bPic,
    U08 u1_is_refPic,
    U08 u1_blk_8x8_mask,
    ME_QUALITY_PRESETS_T e_me_quality_preset);

#endif /* #ifndef _HME_UTILS_H_ */
