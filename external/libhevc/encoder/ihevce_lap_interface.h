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
* @file
*  ihevce_lap_interface.h
*
* @brief
*  This file contains structure definitions related to look-ahead processing
*
* @author
*  ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_LAP_INTERFACE_H_
#define _IHEVCE_LAP_INTERFACE_H_

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief  lap interface ctxt
******************************************************************************
 */
typedef struct
{
    ihevce_sys_api_t *ps_sys_api;
    void *pv_hle_ctxt;
    void *pv_lap_module_ctxt;

    /**
    * Control Input buffer Queue id
    */

    WORD32 i4_ctrl_in_que_id;
    /**
    *
    *EnC and application owned command buffer size
    */
    WORD32 i4_ctrl_cmd_buf_size;

    /**
    * Control Input buffer blocking mode
    */
    WORD32 i4_ctrl_in_que_blocking_mode;

    /**
    * Control output buffer Queue id
    */
    WORD32 i4_ctrl_out_que_id;

    /**
    * Dynamic bitrate change Callback function
    */
    void (*ihevce_dyn_bitrate_cb)(void *pv_hle_ctxt, void *pv_dyn_bitrate_prms);

} lap_intface_t;

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/

WORD32 ihevce_lap_get_num_mem_recs(void);

WORD32 ihevce_lap_get_mem_recs(iv_mem_rec_t *ps_mem_tab, WORD32 i4_mem_space);

WORD32 ihevce_lap_get_num_ip_bufs(ihevce_lap_static_params_t *ps_lap_stat_prms);

void *ihevce_lap_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_lap_static_params_t *ps_lap_params,
    ihevce_static_cfg_params_t *ps_static_cfg_prms);

ihevce_lap_enc_buf_t *
    ihevce_lap_process(void *pv_interface_ctxt, ihevce_lap_enc_buf_t *ps_curr_inp);

WORD32 ihevce_check_last_inp_buf(WORD32 *pi4_cmd_buf);

#endif /* _IHEVCE_LAP_INTERFACE_H_ */
