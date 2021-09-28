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
* \file ihevce_plugin_priv.h
*
* \brief
*    This file contains sample application definations and structures
*
* \date
*    15/04/2014
*
* \author
*    Ittiam
*
*
* List of Functions
*
*
******************************************************************************
*/
#ifndef _IHEVCE_PLUGIN_PRIV_H_
#define _IHEVCE_PLUGIN_PRIV_H_

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

#define XTRA_INP_DATA_BUFS 0
#define MAX_NUM_INP_DATA_BUFS MAX_SUB_GOP_SIZE + NUM_LAP2_LOOK_AHEAD
#define MAX_NUM_INP_CTRL_SYNC_BUFS MAX_NUM_INP_DATA_BUFS
#define MAX_NUM_INP_CTRL_ASYNC_BUFS 5

#define XTRA_OUT_DATA_BUFS 0
#define MAX_NUM_OUT_DATA_BUFS (16 + XTRA_OUT_DATA_BUFS)
#define MAX_NUM_OUT_CTRL_ASYNC_BUFS 16

#define MAX_NUM_RECON_DATA_BUFS 64

/** Queue from Master to Slave for MBR/MRES cases **/
#define MBR_M2S_QUEUE 200

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/**
*  @brief  Store output buffer parameters
*/
typedef struct
{
    UWORD8 *pu1_bs_buffer;
    WORD32 i4_bytes_generated;
    WORD32 i4_is_key_frame;
    ULWORD64 u8_pts;
    LWORD64 i8_dts;
} bitstream_buf_t;

/**
*  @brief  Store buffer queue parameters
*/
typedef struct
{
    /******* Buffer q indexes *******/
    WORD32 i4_q_rd_idx;
    WORD32 i4_q_wr_idx;
    WORD32 i4_quit;
    WORD32 i4_q_size;

    /******* Semaphore Handles ******/
    void *pv_q_deq_sem_handle;
    void *pv_rel_free_sem_handle;
} queue_t;

/**
*  @brief  Datatype of global pointer used for data sharing
*          across encoder nodes.
*/

typedef struct
{
    queue_t s_queue_params;
    bitstream_buf_t bs_buf_nodes[MBR_M2S_QUEUE];
    WORD32 i4_slave_inst_done;

} ihevce_mbr_mres_handle_t;

typedef struct
{
    WORD32 i4_idx;

    UWORD8 *pu1_buf;

    WORD32 i4_is_free;

    WORD32 i4_is_prod;

    WORD32 i4_timestamp_low;

    WORD32 i4_timestamp_high;

    WORD32 i4_bytes_gen;

    WORD32 i4_is_key_frame;

    WORD32 i4_buf_size;

    WORD32 i4_end_flag;

} out_buf_ctxt_t;

typedef struct
{
    ULWORD64 u8_total_bits;

    UWORD32 u4_num_frms_enc;

    /* mutex controlling the out strm buf b/w appln and encoder */
    void *pv_app_out_strm_buf_mutex_hdl;

    void *pv_app_out_strm_buf_cond_var_hdl;

} out_strm_prms_t;

typedef struct
{
    void *pv_mem_mngr_handle; /*!< memory manager handle */

    WORD32 ai4_out_strm_end_flag[IHEVCE_MAX_NUM_RESOLUTIONS]
                                [IHEVCE_MAX_NUM_BITRATES]; /*!< end of strm processing */

    out_strm_prms_t as_out_strm_prms[IHEVCE_MAX_NUM_RESOLUTIONS]
                                    [IHEVCE_MAX_NUM_BITRATES]; /*!< to store out strm related prms */

} app_ctxt_t;

typedef struct
{
    /*!< Static paramters same memory pointer will be passed to
         processing interface layer */
    ihevce_static_cfg_params_t *ps_static_cfg_prms;

    /*!< Osal Handle */
    void *pv_osal_handle;

    /*!< Call back API for freeing */
    void (*ihevce_mem_free)(void *pv_handle, void *pv_mem);

    /*!< Call back API to be called during allocation */
    void *(*ihevce_mem_alloc)(void *pv_handle, UWORD32 u4_size);

    /** App context memory */
    app_ctxt_t s_app_ctxt;

    /** semaphore handle for Input data proc thread */
    void *pv_app_inp_ctrl_sem_hdl;

    /** semaphore handle for Output data proc thread */
    void *pv_app_out_sts_sem_hdl;

    /** Pointer to HLE interface ctxt */
    void *pv_hle_interface_ctxt;

    /** Memtab of input buffers */
    iv_mem_rec_t s_memtab_inp_data_buf;

    /** Memtab of input command buffers */
    iv_mem_rec_t s_memtab_inp_sync_ctrl_buf;

    /** Array of memtabs of outptu buffers */
    iv_mem_rec_t as_memtab_out_data_buf[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES];

    /* pointer to async command input buffer */
    UWORD8 *pu1_inp_async_ctrl_buf;

    /* pointer to async command output buffer*/
    UWORD8 *pu1_out_ctrl_buf;

    /* HLE thread handle */
    void *pv_hle_thread_hdl;

    /* flag to indicate that flush mode is ON */
    WORD32 i4_flush_mode_on;

    /* field id for interlaced case */
    WORD32 i4_field_id;

    /* frame stride of input buffers */
    WORD32 i4_frm_stride;

    /* flag to indicate Output end status */
    WORD32 ai4_out_end_flag[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES];

    /* output buffer context */
    out_buf_ctxt_t aaas_out_bufs[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES]
                                [MAX_NUM_OUT_DATA_BUFS + 1];

    /* Num Output buffers */
    WORD32 i4_num_out_bufs;

    /* Free outbuf idx */
    WORD32 ai4_free_out_buf_idx[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES];

    /* Out produced idx */
    WORD32 i4_prod_out_buf_idx;

    /* DTS for output population */
    LWORD64 i8_dts;

    /* Flag used for flushing in case of EVAL version */
    WORD32 i4_internal_flush;

    ULWORD64 u8_num_frames_encoded;

    /* Count no of frames queued */
    ULWORD64 u8_num_frames_queued;

    /** Structure which contains params to be shared across different FFMPEG instances **/
    ihevce_mbr_mres_handle_t
        *ps_mbr_mres_handle[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES];

    /* Dynamic change in bitrate detecting mechnaism related vaiables */
    WORD32 ai4_old_bitrate[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES];

} plugin_ctxt_t;

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

#endif /* _IHEVCE_PLUGIN_PRIV_H_ */
