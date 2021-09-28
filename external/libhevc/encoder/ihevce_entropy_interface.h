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
* \file ihevce_entropy_interface.h
*
* \brief
*    This file contains interface defination of HEVC entropy function
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_ENTROPY_INTERFACE_H_
#define _IHEVCE_ENTROPY_INTERFACE_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/**  @brief  Enable/Disable NAL size populatation in output buffer */
#define POPULATE_NAL_SIZE 1

/**  @brief  Enable/Disable NAL offset populatation in output buffer */
#define POPULATE_NAL_OFFSET 0

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
WORD32 ihevce_entropy_get_num_mem_recs(void);

WORD32 ihevce_entropy_size_of_out_buffer(frm_proc_ent_cod_ctxt_t *ps_curr_inp);

WORD32 ihevce_entropy_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_mem_space,
    WORD32 i4_resolution_id);

void *ihevce_entropy_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    void *pv_tile_params_base,
    WORD32 i4_res_id);

WORD32 ihevce_entropy_encode_frame(
    void *pv_entropy_hdl,
    iv_output_data_buffs_t *ps_curr_out,
    frm_proc_ent_cod_ctxt_t *ps_curr_inp,
    WORD32 i4_out_buf_size);

#endif /* _IHEVCE_ENTROPY_INTERFACE_H_ */
