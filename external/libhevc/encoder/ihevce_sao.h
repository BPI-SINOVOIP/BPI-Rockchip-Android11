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
* \file ihevce_sao.h
*
* \brief
*    This file contains interface defination of sao ctb function
*
* \date
*    30/01/2014
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_SAO_H_
#define _IHEVCE_SAO_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define BYTES_PER_PIXEL 2

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

void ihevce_sao_ctb(sao_ctxt_t *ps_sao_ctxt, ihevce_tile_params_t *ps_tile_params);

void ihevce_sao_analyse(
    sao_ctxt_t *ps_sao_ctxt,
    ctb_enc_loop_out_t *ps_ctb_enc_loop_out,
    UWORD32 *pu4_frame_rdopt_header_bits,
    ihevce_tile_params_t *ps_tile_params);

#endif /* _IHEVCE_SAO_H_ */
