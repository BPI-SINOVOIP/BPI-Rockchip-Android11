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
* \file ihevce_coarse_me_pass.h
*
* \brief
*    Interfaces to create, control and run the Coarse ME module
*
* \date
*    22/10/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_COARSE_ME_PASS_H_
#define _IHEVCE_COARSE_ME_PASS_H_

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
WORD32 ihevce_coarse_me_get_num_mem_recs();

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_get_mem_recs \endif
*
* \brief
*    Memory requirements are returned for coarse ME.
*
* \param[in,out]  ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_init_prms : Create time static parameters
* \param[in] i4_num_proc_thrds : Number of processing threads for this module
* \param[in] i4_mem_space : memspace in whihc memory request should be done
*
* \return
*    Number of records
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_coarse_me_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    WORD32 i4_mem_space,
    WORD32 i4_resolution_id);

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_init \endif
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
void *ihevce_coarse_me_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    void *pv_osal_handle,
    WORD32 i4_resolution_id,
    UWORD8 u1_is_popcnt_available);

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_reg_thrds_sem \endif
*
* \brief
*    Intialization for ME context state structure with semaphores .
*
* \param[in] pv_me_ctxt : pointer to Coarse ME ctxt
* \param[in] ppv_sem_hdls : Arry of semaphore handles
* \param[in] i4_num_proc_thrds : Number of processing threads
*
* \return
*   none
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_coarse_me_reg_thrds_sem(void *pv_me_ctxt, void **ppv_sem_hdls, WORD32 i4_num_proc_thrds);

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_delete \endif
*
* \brief
*    Destroy Coarse ME module
* Note : Only Destroys the resources allocated in the module like
*   semaphore,etc. Memory free is done Separately using memtabs
*
* \param[in] pv_me_ctxt : pointer to Coarse ME ctxt
* \param[in] ps_init_prms : Create time static parameters
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_coarse_me_delete(
    void *pv_me_ctxt, ihevce_static_cfg_params_t *ps_init_prms, WORD32 i4_resolution_id);

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
void ihevce_coarse_me_set_resolution(
    void *pv_me_ctxt, WORD32 n_enc_layers, WORD32 *p_wd, WORD32 *p_ht);

void ihevce_coarse_me_get_rc_param(
    void *pv_me_ctxt,
    LWORD64 *i8_acc_frame_hme_cost,
    LWORD64 *i8_acc_frame_hme_sad,
    LWORD64 *i8_acc_num_blks_higher_sad,
    LWORD64 *i8_total_blks,
    WORD32 i4_is_prev_pic_same_scene);
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
void ihevce_coarse_me_frame_init(
    void *pv_me_ctxt,
    ihevce_static_cfg_params_t *ps_stat_prms,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    frm_lambda_ctxt_t *ps_frm_lamda,
    WORD32 num_ref_l0,
    WORD32 num_ref_l1,
    WORD32 num_ref_l0_active,
    WORD32 num_ref_l1_active,
    recon_pic_buf_t **pps_rec_list_l0,
    recon_pic_buf_t **pps_rec_list_l1,
    ihevce_lap_enc_buf_t *ps_enc_lap_inp,
    WORD32 i4_frm_qp,
    ihevce_ed_blk_t *ps_layer1_buf,  //EIID
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1,
    UWORD8 *pu1_me_reverse_map_info,
    WORD32 i4_temporal_layer_id);

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
void ihevce_coarse_me_process(
    void *pv_me_ctxt,
    ihevce_lap_enc_buf_t *ps_enc_lap_inp,
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt,
    WORD32 thrd_id,
    WORD32 i4_ping_pong);

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
void ihevce_coarse_me_frame_dpb_update(
    void *pv_me_ctxt,
    WORD32 num_ref_l0,
    WORD32 num_ref_l1,
    recon_pic_buf_t **pps_rec_list_l0,
    recon_pic_buf_t **pps_rec_list_l1);

/*!
******************************************************************************
* \if Function name : ihevce_me_get_lyr_prms_job_que \endif
*
* \brief Returns to the caller key attributes related to dependency between layers
*          for multi-thread execution
*
*
* \par Description:
*    This function requires the precondition that the width and ht of encode
*    layer is known, and ME API ihevce_me_set_resolution() API called with
*    this info. Based on this, ME populates useful information for the encoder
*    to execute the multi-thread (concurrent across layers) in this API.
*    The number of layers, number of vertical units in each layer, and for
*    each vertial unit in each layer, its dependency on previous layer's units
*    From ME's perspective, a vertical unit is one which is smallest min size
*    vertically (and spans the entire row horizontally). This is CTB for encode
*    layer, and 8x8 / 4x4 for non encode layers.
*
* \param[in] pv_ctxt : ME handle
* \param[in] ps_curr_inp : Input buffer descriptor
* \param[out] pi4_num_hme_lyrs : Num of HME layers (ME updates)
* \param[out] pi4_num_vert_units_in_lyr : Array of size N (num layers), each
*                     entry has num vertical units in that particular layer
* \param[in] ps_me_job_q_prms : Array of job queue prms, one for each unit in a
*                 layer. Note that this is contiguous in order of processing
*                 All k units of layer N-1 from top to bottom, followed by
*                 all m units of layer N-2 .... ends with X units of layer 0
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_coarse_me_get_lyr_prms_job_que(
    void *pv_me_ctxt,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    WORD32 *pi4_num_hme_lyrs,
    WORD32 *pi4_num_vert_units_in_lyr,
    multi_thrd_me_job_q_prms_t *ps_me_job_q_prms);

/*!
******************************************************************************
* \if Function name : ihevce_me_frame_end \endif
*
* \brief
*    End of frame update function performs GMV collation
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
void ihevce_coarse_me_frame_end(void *pv_me_ctxt);

void ihevce_coarse_me_get_lyr1_ctxt(
    void *pv_me_ctxt, void *pv_layer_ctxt, void *pv_layer_mv_bank_ctxt);

void ihevce_coarse_me_set_lyr1_mv_bank(
    void *pv_me_ctxt,
    ihevce_lap_enc_buf_t *ps_enc_lap_inp,
    void *pv_mv_bank,
    void *pv_ref_idx_bank,
    WORD32 i4_curr_idx);

WORD32 ihevce_coarse_me_get_lyr_buf_desc(
    void *pv_me_ctxt, UWORD8 **ppu1_decomp_lyr_bufs, WORD32 *pi4_lyr_buf_stride);

#endif /* _IHEVCE_COARSE_ME_PASS_H_ */
