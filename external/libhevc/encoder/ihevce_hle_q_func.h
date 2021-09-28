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
* \file ihevce_hle_q_func.h
*
* \brief
*    This file contains interface defination Que related functions
*    of high level encoder
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_HEL_Q_FUNC_H_
#define _IHEVCE_HEL_Q_FUNC_H_

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
void *ihevce_q_get_free_buff(
    void *pv_enc_ctxt, WORD32 i4_q_id, WORD32 *pi4_buff_id, WORD32 i4_blocking_mode);

IV_API_CALL_STATUS_T ihevce_q_set_buff_prod(void *pv_enc_ctxt, WORD32 i4_q_id, WORD32 i4_buff_id);

void *ihevce_q_get_filled_buff(
    void *pv_enc_ctxt, WORD32 i4_q_id, WORD32 *pi4_buff_id, WORD32 i4_blocking_mode);

IV_API_CALL_STATUS_T ihevce_q_rel_buf(void *pv_enc_ctxt, WORD32 i4_q_id, WORD32 i4_buff_id);

void ihevce_force_end(ihevce_hle_ctxt_t *ps_hle_ctxt);

#endif /* _IHEVCE_HEL_Q_FUNC_H_ */
