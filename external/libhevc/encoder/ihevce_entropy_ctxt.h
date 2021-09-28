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
* @file ihevce_entropy_ctxt.h
*
* @brief
*  This file contains structures and interface prototypes for header encoding
*
* @author
*  Ittiam
******************************************************************************
*/

#ifndef _IHEVCE_ENTROPY_CTXT_H_
#define _IHEVCE_ENTROPY_CTXT_H_

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief      Data for the trace functionality
******************************************************************************
 */
typedef struct
{
    /**
     *  pointer to vps_t struct
                                     */
    vps_t *ps_vps;

    /**
     *  pointer to sps_t struct
                                     */
    sps_t *ps_sps;

    /**
     *  pointer to pps_t struct
                                     */
    pps_t *ps_pps;

    /**
     *  pointer to slice_header_t struct
                                     */
    slice_header_t *ps_slice_hdr;

    /**
     *  pointer to ihevce_src_params_t struct
                                             */
    ihevce_src_params_t *ps_src_params;

    /**
     *  pointer to pps_t ihevce_out_strm_params_t
                                                 */
    ihevce_out_strm_params_t *ps_out_atrm_params;

    /**
     *  pointer to ihevce_coding_params_t struct
                                                */
    ihevce_coding_params_t *ps_coding_params;

    /**
     *  pointer to ihevce_config_prms_t struct
                                              */
    ihevce_config_prms_t *ps_config_prms;

} entropy_ctxt_t;

#endif  //_IHEVCE_ENTROPY_CTXT_H_
