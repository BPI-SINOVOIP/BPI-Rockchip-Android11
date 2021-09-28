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
* \file ihevce_me_pass.h
*
* \brief
*    Interfaces to create, control and run the ME module
*
* \date
*    22/10/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_ME_PASS_H_
#define _IHEVCE_ME_PASS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
/*!
******************************************************************************
* \if Function name : ihevce_me_get_num_mem_recs \endif
*
* \brief
*    Number of memory records are returned for ME module
*
*
* \return
*    Number of memory records
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_me_get_num_mem_recs(WORD32 i4_num_me_frm_pllel);

/*!
******************************************************************************
* \if Function name : ihevce_me_get_mem_recs \endif
*
* \brief
*    Memory requirements are returned for ME.
*
* \param[in,out]  ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_init_prms : Create time static parameters
*
* \return
*    Number of records
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_me_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    WORD32 i4_mem_space,
    WORD32 i4_resolution_id,
    WORD32 i4_num_me_frm_pllel);

/*!
******************************************************************************
* \if Function name : ihevce_me_init \endif
*
* \brief
*    Intialization for ME context state structure .
*
* \param[in] ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_init_prms : Create time static parameters
* \param[in] pv_osal_handle : Osal handle
*
* \return
*    Handle to the ME context
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_me_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    void *pv_osal_handle,
    rc_quant_t *ps_rc_quant_ctxt,
    void *pv_tile_params_base,
    WORD32 i4_resolution_id,
    WORD32 i4_num_me_frm_pllel,
    UWORD8 u1_is_popcnt_available);

/**
*******************************************************************************
* \if Function name : ihevce_me_set_resolution \endif
*
* \brief
*    Sets the resolution for ME state
*
* \par Description:
*    ME requires information of resolution to prime up its layer descriptors
*    and contexts. This API is called whenever a control call from application
*    causes a change of resolution. Has to be called once initially before
*    processing any frame. Again this is just a glue function and calls the
*    actual ME API for the same.
*
* \param[in,out] pv_me_ctxt: Handle to the ME context
* \param[in] n_enc_layers: Number of layers getting encoded
* \param[in] p_wd : Pointer containing widths of each layer getting encoded.
* \param[in] p_ht : Pointer containing heights of each layer getting encoded.
*
* \returns
*  none
*
* \author
*  Ittiam
*
*******************************************************************************
*/
void ihevce_me_set_resolution(void *pv_me_ctxt, WORD32 n_enc_layers, WORD32 *p_wd, WORD32 *p_ht);

/*!
******************************************************************************
* \if Function name : ihevce_me_frame_init \endif
*
* \brief
*    Frame level ME initialisation function
*
* \par Description:
*    The following pre-conditions exist for this function: a. We have the input
*    pic ready for encode, b. We have the reference list with POC, L0/L1 IDs
*    and ref ptrs ready for this picture and c. ihevce_me_set_resolution has
*    been called atleast once. Once these are supplied, the following are
*    done here: a. Input pyramid creation, b. Updation of ME's internal DPB
*    based on available ref list information
*
* \param[in] pv_ctxt : pointer to ME module
* \param[in] ps_frm_ctb_prms : CTB characteristics parameters
* \param[in] ps_frm_lamda : Frame level Lambda params
* \param[in] num_ref_l0 : Number of reference pics in L0 list
* \param[in] num_ref_l1 : Number of reference pics in L1 list
* \param[in] num_ref_l0_active : Active reference pics in L0 dir for current frame (shall be <= num_ref_l0)
* \param[in] num_ref_l1_active : Active reference pics in L1 dir for current frame (shall be <= num_ref_l1)
* \param[in] pps_rec_list_l0 : List of recon pics in L0 list
* \param[in] pps_rec_list_l1 : List of recon pics in L1 list
* \param[in] ps_enc_lap_inp  : pointer to input yuv buffer (frame buffer)
* \param[in] i4_frm_qp       : current picture QP
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_me_frame_init(
    void *pv_me_ctxt,
    me_enc_rdopt_ctxt_t *ps_cur_out_me_prms,
    ihevce_static_cfg_params_t *ps_stat_prms,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    frm_lambda_ctxt_t *ps_frm_lamda,
    WORD32 num_ref_l0,
    WORD32 num_ref_l1,
    WORD32 num_ref_l0_active,
    WORD32 num_ref_l1_active,
    recon_pic_buf_t **pps_rec_list_l0,
    recon_pic_buf_t **pps_rec_list_l1,
    recon_pic_buf_t *(*aps_ref_list)[HEVCE_MAX_REF_PICS * 2],
    func_selector_t *ps_func_selector,
    ihevce_lap_enc_buf_t *ps_enc_lap_inp,
    void *pv_coarse_layer,
    WORD32 i4_me_frm_id,
    WORD32 i4_thrd_id,
    WORD32 i4_frm_qp,
    WORD32 i4_temporal_layer_id,
    WORD8 i1_cu_qp_delta_enabled_flag,
    void *pv_dep_mngr_encloop_dep_me);

/*!
******************************************************************************
* \if Function name : ihevce_me_process \endif
*
* \brief
*    Frame level ME function
*
* \par Description:
*    Processing of all layers starting from coarse and going
*    to the refinement layers, all layers
*    that are encoded go CTB by CTB. Outputs of this function are populated
*    ctb_analyse_t structures, one per CTB.
*
* \param[in] pv_ctxt : pointer to ME module
* \param[in] ps_enc_lap_inp  : pointer to input yuv buffer (frame buffer)
* \param[in,out] ps_ctb_out : pointer to CTB analyse output structure (frame buffer)
* \param[out] ps_cu_out : pointer to CU analyse output structure (frame buffer)
* \param[in]  pd_intra_costs : pointerto intra cost buffer
* \param[in]  ps_multi_thrd_ctxt : pointer to multi thread ctxt
* \param[in]  thrd_id : Thread id of the current thrd in which function is executed
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_me_process(
    void *pv_me_ctxt,
    ihevce_lap_enc_buf_t *ps_enc_lap_inp,
    ctb_analyse_t *ps_ctb_out,
    me_enc_rdopt_ctxt_t *ps_cur_out_me_prms,
    double *pd_intra_costs,
    ipe_l0_ctb_analyse_for_me_t *ps_ipe_analyse_ctb,
    pre_enc_L0_ipe_encloop_ctxt_t *ps_l0_ipe_input,
    void *pv_coarse_layer,
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt,
    WORD32 i4_frame_parallelism_level,
    WORD32 thrd_id,
    WORD32 i4_me_frm_id);

/*!
******************************************************************************
* \if Function name : ihevce_me_frame_dpb_update \endif
*
* \brief
*    Frame level ME initialisation function
*
* \par Description:
*   Updation of ME's internal DPB
*    based on available ref list information
*
* \param[in] pv_ctxt : pointer to ME module
* \param[in] num_ref_l0 : Number of reference pics in L0 list
* \param[in] num_ref_l1 : Number of reference pics in L1 list
* \param[in] pps_rec_list_l0 : List of recon pics in L0 list
* \param[in] pps_rec_list_l1 : List of recon pics in L1 list
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_me_frame_dpb_update(
    void *pv_me_ctxt,
    WORD32 num_ref_l0,
    WORD32 num_ref_l1,
    recon_pic_buf_t **pps_rec_list_l0,
    recon_pic_buf_t **pps_rec_list_l1,
    WORD32 i4_thrd_id);

void ihevce_derive_me_init_prms(
    ihevce_static_cfg_params_t *ps_init_prms,
    hme_init_prms_t *ps_hme_init_prms,
    S32 i4_num_proc_thrds,
    WORD32 i4_resolution_id);

void ihevc_me_update_ref_desc(
    hme_ref_desc_t *ps_ref_desc,
    recon_pic_buf_t *ps_recon_pic,
    WORD32 ref_id_l0,
    WORD32 ref_id_l1,
    WORD32 ref_id_lc,
    WORD32 is_fwd);

WORD32 ihevce_me_find_poc_in_list(
    recon_pic_buf_t **pps_rec_list, WORD32 poc, WORD32 i4_idr_gop_num, WORD32 num_ref);

void ihevce_me_create_ref_map(
    recon_pic_buf_t **pps_rec_list_l0,
    recon_pic_buf_t **pps_rec_list_l1,
    WORD32 num_ref_l0_active,
    WORD32 num_ref_l1_active,
    WORD32 num_ref,
    hme_ref_map_t *ps_ref_map);

/*!
******************************************************************************
* \if Function name : ihevce_l0_me_frame_end \endif
*
* \brief
*    End of frame update function performs
*       - Dynamic Search Range collation
*
* \param[in] pv_ctxt : pointer to ME module
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_l0_me_frame_end(
    void *pv_me_ctxt, WORD32 i4_idx_dvsr_p, WORD32 i4_display_num, WORD32 i4_me_frm_id);

#endif /* _IHEVCE_ME_PASS_H_ */
