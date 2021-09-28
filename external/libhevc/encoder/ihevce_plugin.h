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
* \file ihevce_plugin.h
*
* \brief
*    This file contains plugin interface definations and structures
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
#ifndef _IHEVCE_PLUGIN_H_
#define _IHEVCE_PLUGIN_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define MAX_INP_PLANES 3

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
typedef enum
{
    IHEVCE_EFAIL = 0xFFFFFFFF,
    IHEVCE_EOK = 0
} IHEVCE_PLUGIN_STATUS_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
typedef struct
{
    /* input buffer pointers  */
    void *apv_inp_planes[MAX_INP_PLANES];

    /* input buffer strides */
    WORD32 ai4_inp_strd[MAX_INP_PLANES];

    /* input buffer size */
    WORD32 ai4_inp_size[MAX_INP_PLANES];

    /* PTS of the input */
    ULWORD64 u8_pts;

    /* Current bitrate*/
    WORD32 i4_curr_bitrate;

    /* Current peak bitrate*/
    WORD32 i4_curr_peak_bitrate;

    /* Unused variable retained for backward compatibility*/
    WORD32 i4_curr_rate_factor;

    /* force idr flag */
    WORD32 i4_force_idr_flag;
} ihevce_inp_buf_t;

typedef struct
{
    /* Output buffer pointer (if set to NULL then no output is sent out from encoder) */
    UWORD8 *pu1_output_buf;

    /* Number of bytes generated in the buffer */
    WORD32 i4_bytes_generated;

    /* Key frame flag */
    WORD32 i4_is_key_frame;

    /* PTS of the output */
    ULWORD64 u8_pts;

    /* DTS of the output */
    LWORD64 i8_dts;

    /* Flag to check if last output buffer sent from encoder */
    WORD32 i4_end_flag;

} ihevce_out_buf_t;

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

IHEVCE_PLUGIN_STATUS_T ihevce_set_def_params(ihevce_static_cfg_params_t *ps_params);

IHEVCE_PLUGIN_STATUS_T ihevce_init(ihevce_static_cfg_params_t *ps_params, void **ppv_ihevce_hdl);

IHEVCE_PLUGIN_STATUS_T ihevce_encode_header(void *pv_ihevce_hdl, ihevce_out_buf_t *ps_out);

IHEVCE_PLUGIN_STATUS_T
    ihevce_encode(void *pv_ihevce_hdl, ihevce_inp_buf_t *ps_inp, ihevce_out_buf_t *ps_out);

IHEVCE_PLUGIN_STATUS_T ihevce_close(void *pv_ihevce_hdl);

void ihevce_init_sys_api(void *pv_cb_handle, ihevce_sys_api_t *ps_sys_api);

#ifdef __cplusplus
}
#endif

#endif /* _IHEVCE_PLUGIN_H_ */
