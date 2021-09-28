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
* \file ihevce_memory_init.h
*
* \brief
*    This file contains interface defiation of encoder memory manager
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_MEMORY_INIT_H_
#define _IHEVCE_MEMORY_INIT_H_

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
void ihevce_mem_manager_init(enc_ctxt_t *ps_enc_ctxt, ihevce_hle_ctxt_t *ps_intrf_ctxt);

void ihevce_mem_manager_que_init(
    enc_ctxt_t *ps_enc_ctxt,
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    iv_input_data_ctrl_buffs_desc_t *ps_input_data_ctrl_buffs_desc,
    iv_input_asynch_ctrl_buffs_desc_t *ps_input_asynch_ctrl_buffs_desc,
    iv_output_data_buffs_desc_t *ps_output_data_buffs_desc,
    iv_recon_data_buffs_desc_t *ps_recon_data_buffs_desc);

void ihevce_mem_manager_free(enc_ctxt_t *ps_enc_ctxt, ihevce_hle_ctxt_t *ps_intrf_ctxt);

#endif /* _IHEVCE_MEMORY_INIT_H_ */
