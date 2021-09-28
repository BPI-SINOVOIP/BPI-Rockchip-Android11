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
* \file ihevce_hle_interface.h
*
* \brief
*    This file contains infertace prototypes of High level enocder interafce
*    structure and interface functions.
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_HLE_INTERFACE_H_
#define _IHEVCE_HLE_INTERFACE_H_

#include "ihevce_profile.h"

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define IHEVCE_DLL

#define DUMP_MBR_MULTI_RES_INFO 0

#define DUMP_RC_2_PASS_DATA_BINARY_APP 1
/*print attributes */

/*print everything on console */
#define PRINTF(v, x, y, ...) ps_sys_api->ihevce_printf(v, __VA_ARGS__)

#define FPRINTF(v, fp, x, y, ...)                                                                  \
    if(NULL != fp)                                                                                 \
    {                                                                                              \
        ps_sys_api->s_file_io_api.ihevce_fprintf(v, fp, __VA_ARGS__);                              \
    }

/* Semaphore attribute */
#define SEM_START_VALUE 1
#define THREAD_STACK_SIZE 0x80000

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
typedef enum
{
    BUFF_QUE_NON_BLOCKING_MODE = 0,

    BUFF_QUE_BLOCKING_MODE

} BUFF_QUE_MODES_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
/**
 *  @brief  Structure to describe Process interface parameters of Encoder
 */
typedef struct
{
    /**
     * Size for version tracking purpose
     */
    WORD32 i4_size;

    /**
     * Flag to communicate that HLE thread int are done
     */
    WORD32 i4_hle_init_done;

    /**
     * Error code communciate any error during create stage
     */
    WORD32 i4_error_code;

    /**
    * GPU memory accumalator
    */
    WORD32 i4_gpu_mem_size;

    /**
     *  OSAL handle
     */
    void *pv_osal_handle;

    /**
     * Encoder Handle
     */
    void *apv_enc_hdl[IHEVCE_MAX_NUM_RESOLUTIONS];

    /**
     *  Static parameters structure
     */
    ihevce_static_cfg_params_t *ps_static_cfg_prms;

    /**
     * Memory Manager handle
     */
    void *pv_mem_mgr_hdl;

    /**
     *  Input Buffer callback handle
     */
    void *pv_inp_cb_handle;

    /**
     *  Ouput Buffer callback handle
     */
    void *pv_out_cb_handle;

    /**
     *  Ouput Recon Buffer callback handle
     */
    void *pv_recon_cb_handle;

    /**
     * Call back API to be called while the buffer for bitstream filling is done
     */
    IV_API_CALL_STATUS_T (*ihevce_output_strm_fill_done)
    (void *pv_out_cb_handle, void *pv_curr_out, WORD32 i4_bitrate_instance, WORD32 i4_res_instance);

    /**
     * Call back API to be called while the buffer for recon filling is done
     */
    IV_API_CALL_STATUS_T (*ihevce_output_recon_fill_done)
    (void *pv_recon_cb_handle,
     void *pv_curr_out,
     WORD32 i4_bitrate_instance,
     WORD32 i4_res_instance);

    /**
     * Call back API to be called while freeing the input buffer
     */
    IV_API_CALL_STATUS_T (*ihevce_set_free_input_buff)
    (void *pv_inp_cb_handle, iv_input_data_ctrl_buffs_t *ps_input_buf);

    /**
     * Call back API to be called during allocation using memory manager
     */
    void (*ihevce_mem_alloc)(
        void *pv_mem_mgr_hdl, ihevce_sys_api_t *ps_sys_api, iv_mem_rec_t *ps_memtab);

    /**
     * Call back API for freeing using memory manager
     */
    void (*ihevce_mem_free)(void *pv_mem_mgr_hdl, iv_mem_rec_t *ps_memtab);

    /* create or run time input buffer allocation, 1: create time 0: run time*/
    WORD32 i4_create_time_input_allocation;

    /* create or run time output buffer allocation, 1: create time 0: run time*/
    WORD32 i4_create_time_output_allocation;

    /*Cores per resolution*/
    WORD32 ai4_num_core_per_res[IHEVCE_MAX_NUM_RESOLUTIONS];

    /**
    *  Error Handling callback handle
    */
    void *pv_cmd_err_cb_handle;

    /**
    * Call back API to be called when errors need to be reported
    */
    IV_API_CALL_STATUS_T (*ihevce_cmds_error_report)
    (void *pv_cmd_err_cb_handle, WORD32 i4_error_code, WORD32 i4_cmd_type, WORD32 i4_buf_id);

    /**
    * Flag to indicate if ECU is enabled/disabled
    */
    WORD32 i4_p6_opt_enabled;

    /**
     * profile stats
     */
    profile_database_t profile_hle;
    profile_database_t profile_pre_enc_l1l2[IHEVCE_MAX_NUM_RESOLUTIONS];
    profile_database_t profile_pre_enc_l0ipe[IHEVCE_MAX_NUM_RESOLUTIONS];
    profile_database_t profile_enc_me[IHEVCE_MAX_NUM_RESOLUTIONS];
    profile_database_t profile_enc[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES];
    profile_database_t profile_entropy[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES];

} ihevce_hle_ctxt_t;

/**
******************************************************************************
 *  @brief  Indivisual Thread context structure
******************************************************************************
 */
typedef struct
{
    /**  Unique Id associated with every frame processing thread */
    WORD32 i4_thrd_id;

    /** pointer to encoder context structure */
    void *pv_enc_ctxt;

    /** pointer to the hle context structure */
    ihevce_hle_ctxt_t *ps_hle_ctxt;

} frm_proc_thrd_ctxt_t;

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

/** Create API
 *
 * ps_hle_ctxt : Pointer to high level encoder context.
 *    It is assumed that application before calling this API
 *    has initialized with correct pointers for following
 *      - pv_osal_handle
 *      - pv_app_sem_handle
 *      - ps_static_cfg_prms
 *      - ihevce_mem_alloc
 *      - ihevce_mem_free
 *
 * Encoder after initilaization would store the encoder handle in
 *      - pv_enc_hdl
 *
 * Create Return status (success or fail) is returned
 */
IHEVCE_DLL IV_API_CALL_STATUS_T ihevce_hle_interface_create(ihevce_hle_ctxt_t *ps_hle_ctxt);

/** Query IO buffers requirements API
 *
 *  ps_hle_ctxt : Pointer to high level encoder context.
 *  ps_input_bufs_req : memory to store input buffer requirements
 *  ps_output_bufs_req : memory to store output buffer requirements
 *
 * Should be called only after a sucessfull create of codec instance
 *
 * Return status (success or fail) is returned
 */
IHEVCE_DLL IV_API_CALL_STATUS_T ihevce_query_io_buf_req(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    iv_input_bufs_req_t *ps_input_bufs_req,
    iv_res_layer_output_bufs_req_t *ps_res_layer_output_bufs_req,
    iv_res_layer_recon_bufs_req_t *ps_res_layer_recon_bufs_req);

/** Create buffer ports for procesing API
 *
 *  ps_hle_ctxt : Pointer to high level encoder context.
 *  ps_input_data_ctrl_buffs_desc :
 *       Pointer to Input (data/control) buffers details memory
 *  ps_input_asynch_ctrl_buffs_desc :
 *       Pointer to Input async control buffers details memory
 *  ps_output_data_buffs_desc :
 *      Pointer to output data buffers details memory
 *  ps_output_status_buffs_desc:
 *      Pointer to outtput async control buffers details memory
 *
 * Return status (success or fail) is returned
 */
IHEVCE_DLL IV_API_CALL_STATUS_T ihevce_create_ports(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    iv_input_data_ctrl_buffs_desc_t *ps_input_data_ctrl_buffs_desc,
    iv_input_asynch_ctrl_buffs_desc_t *ps_input_asynch_ctrl_buffs_desc,
    iv_res_layer_output_data_buffs_desc_t *ps_mres_output_data_buffs_desc,
    iv_res_layer_recon_data_buffs_desc_t *ps_mres_recon_data_buffs_desc);

/** Processing interface layer thread API
 *
 *  This is the entry point for this thread
 *  pointer to ihevce_hle_ctxt_t has to be passed
 *  to this function as the argument
 *
 *  return should be a exit code (0)
 */
IHEVCE_DLL WORD32 ihevce_hle_interface_thrd(void *pv_proc_intf_ctxt);

/** Get version API
 *
 *  This is API to return the version number of the encoder
 *
 *  returns the version number string
 */
IHEVCE_DLL const char *ihevce_get_encoder_version(void);

/** Validate Encoder parameters
 *
 *  This is API to return the version number of the encoder
 *
 *  returns the version number string
 */
IHEVCE_DLL WORD32 ihevce_validate_encoder_parameters(ihevce_static_cfg_params_t *ps_static_cfg_prms);

/** Get free input frame data buffer API
 *
 * ps_hle_ctxt : Pointer to high level encoder context.
 * pi4_buff_id : pointer to store the buffer id of the buffer returned.
 * i4_blocking_mode : Blocking mode to control if the the API should wait
 *                    for a free buffer to be available and then
 *                    return with a valid buffer @sa BUFF_QUE_MODES_T
 * returns NULL if no free buffer is present in queue (if non blocking mode)
 */
IHEVCE_DLL void *ihevce_q_get_free_inp_data_buff(
    ihevce_hle_ctxt_t *ps_hle_ctxt, WORD32 *pi4_buff_id, WORD32 i4_blocking_mode);

/** Get free input control data buffer API
 *
 * ps_hle_ctxt : Pointer to high level encoder context.
 * pi4_buff_id : pointer to store the buffer id of the buffer returned.
 * i4_blocking_mode : Blocking mode to control if the the API should wait
 *                    for a free buffer to be available and then
 *                    return with a valid buffer @sa BUFF_QUE_MODES_T
 * returns NULL if no free buffer is present in queue (if non blocking mode)
 */
IHEVCE_DLL void *ihevce_q_get_free_inp_ctrl_buff(
    ihevce_hle_ctxt_t *ps_hle_ctxt, WORD32 *pi4_buff_id, WORD32 i4_blocking_mode);

IHEVCE_DLL void *ihevce_q_get_free_out_strm_buff(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    WORD32 *pi4_buff_id,
    WORD32 i4_blocking_mode,
    WORD32 i4_bitrate_instance,
    WORD32 i4_res_instance);

IHEVCE_DLL void *ihevce_q_get_free_out_recon_buff(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    WORD32 *pi4_buff_id,
    WORD32 i4_blocking_mode,
    WORD32 i4_bitrate_instance,
    WORD32 i4_res_instance);

/** Set Input frame data buffer as produced API
 *
 * ps_hle_ctxt : Pointer to high level encoder context.
 * i4_buff_id : buffer id of the buffer returned during get free buf.
 */
IHEVCE_DLL IV_API_CALL_STATUS_T
    ihevce_q_set_inp_data_buff_prod(ihevce_hle_ctxt_t *ps_hle_ctxt, WORD32 i4_buff_id);

/** Set Input control data buffer as produced API
 *
 * ps_hle_ctxt : Pointer to high level encoder context.
 * i4_buff_id : buffer id of the buffer returned during get free buf.
 */
IHEVCE_DLL IV_API_CALL_STATUS_T
    ihevce_q_set_inp_ctrl_buff_prod(ihevce_hle_ctxt_t *ps_hle_ctxt, WORD32 i4_buff_id);

IHEVCE_DLL IV_API_CALL_STATUS_T ihevce_q_set_out_strm_buff_prod(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    WORD32 i4_buff_id,
    WORD32 i4_bitrate_instance_id,
    WORD32 i4_resolution_id);

IHEVCE_DLL IV_API_CALL_STATUS_T ihevce_q_set_out_recon_buff_prod(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    WORD32 i4_buff_id,
    WORD32 i4_bitrate_instance_id,
    WORD32 i4_resolution_id);

/** Get next filled recon data buffer API
 *
 * ps_hle_ctxt : Pointer to high level encoder context.
 * pi4_buff_id : pointer to store the buffer id of the buffer returned.
 * i4_blocking_mode : Blocking mode to control if the the API should wait
 *                    for a produced buffer to be available and then
 *                    return with a valid buffer @sa BUFF_QUE_MODES_T
 * returns NULL if no produced buffer is present in queue (if non blocking mode)
 */
IHEVCE_DLL void *ihevce_q_get_filled_recon_buff(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    WORD32 *pi4_buff_id,
    WORD32 i4_blocking_mode,
    WORD32 i4_bitrate_instance_id,
    WORD32 i4_resolution_id);

/** Release/ Free recon buffer buffer API
 *
 * ps_hle_ctxt : Pointer to high level encoder context.
 * i4_buff_id : buffer id of the buffer returned during get next buf.
 */
IHEVCE_DLL IV_API_CALL_STATUS_T ihevce_q_rel_recon_buf(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    WORD32 i4_buff_id,
    WORD32 i4_bitrate_instance_id,
    WORD32 i4_resolution_id);

/** Delete API
 *
 * Should be called only after the high level encoder thread exits or returns
 */
IHEVCE_DLL IV_API_CALL_STATUS_T ihevce_hle_interface_delete(ihevce_hle_ctxt_t *ps_hle_ctxt);

/** Trace API
 *
 * Open and Close trace file pointer.
 */
IHEVCE_DLL WORD32 ihevce_trace_init(UWORD8 *pu1_file_name);

IHEVCE_DLL WORD32 ihevce_trace_deinit(void);

/** Header API
 *
 *  Get sequence headers asynchronously
 */
WORD32 ihevce_entropy_encode_header(
    ihevce_hle_ctxt_t *ps_hle_ctxt, WORD32 i4_bitrate_instance_id, WORD32 i4_resolution_id);

#endif /* _IHEVCE_HLE_INTERFACE_H_ */
