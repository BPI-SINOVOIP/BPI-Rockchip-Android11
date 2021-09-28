/******************************************************************************
 *
 * Copyright (C) 2015 The Android Open Source Project
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
*******************************************************************************
* @file
*  ih264e_sei.c
*
* @brief
*  This file contains function definitions related to SEI NAL's header encoding
*
* @author
*  ittiam
*
* @par List of Functions:
*  - ih264e_put_sei_mdcv_params
*  - ih264e_put_sei_cll_params
*  - ih264e_put_sei_ave_params
*  - ih264e_put_sei_ccv_params
*  - ih264e_put_sei_msg
*
* @remarks
*  None
*
*******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* User include files */
#include "ih264_typedefs.h"
#include "iv2.h"
#include "ive2.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264e_trace.h"
#include "ih264e_error.h"
#include "ih264e_bitstream.h"
#include "ih264_defs.h"
#include "ih264_structs.h"
#include "ih264_cabac_tables.h"
#include "ih264e_cabac_structs.h"
#include "irc_cntrl_param.h"
#include "ime_distortion_metrics.h"
#include "ime_defs.h"
#include "ime_structs.h"
#include "ih264e_defs.h"
#include "irc_cntrl_param.h"
#include "irc_frame_info_collector.h"
#include "ih264_trans_quant_itrans_iquant.h"
#include "ih264_inter_pred_filters.h"
#include "ih264_deblk_edge_filters.h"
#include "ih264e_structs.h"
#include "ih264e_encode_header.h"
#include "ih264e_sei.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
******************************************************************************
*
*  @brief Generates Mastering Display Color Volume (Supplemental Enhancement Information )
*
*  @par   Description
*  Parse Supplemental Enhancement Information
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_sei_mdcv
*  pointer to structure containing mdcv SEI data
*
*  @return      success or failure error code
*
******************************************************************************
*/
IH264E_ERROR_T ih264e_put_sei_mdcv_params(sei_mdcv_params_t *ps_sei_mdcv,
                                 bitstrm_t *ps_bitstrm)
{
    WORD32 return_status = IH264E_SUCCESS;
    UWORD8 u1_payload_size = 0;
    UWORD32 u4_count;

    if(ps_sei_mdcv == NULL)
    {
        return IH264E_FAIL;
    }

    u1_payload_size += (NUM_SEI_MDCV_PRIMARIES * 2); /* display primaries x */
    u1_payload_size += (NUM_SEI_MDCV_PRIMARIES * 2); /* display primaries y */
    u1_payload_size += 2; /* white point x */
    u1_payload_size += 2; /* white point y */
    u1_payload_size += 4; /* max display mastering luminance */
    u1_payload_size += 4; /* min display mastering luminance */

    /************************************************************************/
    /* PayloadSize : This is the size of the payload in bytes               */
    /************************************************************************/
    PUT_BITS(ps_bitstrm, u1_payload_size, 8, return_status, "u1_payload_size");

    /*******************************************************************************/
    /* Put the mastering display color volume SEI parameters into the bitstream.   */
    /*******************************************************************************/

    /* display primaries x */
    for(u4_count = 0; u4_count < NUM_SEI_MDCV_PRIMARIES; u4_count++)
    {
        PUT_BITS(ps_bitstrm, ps_sei_mdcv->au2_display_primaries_x[u4_count], 16,
                                return_status, "u2_display_primaries_x");

        PUT_BITS(ps_bitstrm, ps_sei_mdcv->au2_display_primaries_y[u4_count], 16,
                                return_status, "u2_display_primaries_y");
    }

    /* white point x */
    PUT_BITS(ps_bitstrm, ps_sei_mdcv->u2_white_point_x, 16, return_status, "u2_white point x");

    /* white point y */
    PUT_BITS(ps_bitstrm, ps_sei_mdcv->u2_white_point_y, 16, return_status, "u2_white point y");

    /* max display mastering luminance */
    PUT_BITS(ps_bitstrm, ps_sei_mdcv->u4_max_display_mastering_luminance, 32,
                                return_status, "u4_max_display_mastering_luminance");

    /* min display mastering luminance */
    PUT_BITS(ps_bitstrm, ps_sei_mdcv->u4_min_display_mastering_luminance, 32,
                                return_status, "u4_max_display_mastering_luminance");

    return (return_status);
}

/**
******************************************************************************
*
*  @brief Stores content light level info in bitstream
*
*  @par   Description
*  Parse Supplemental Enhancement Information
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_sei_cll
*  Pinter to structure containing cll sei params
*
*  @return      success or failure error code
*
******************************************************************************
*/
IH264E_ERROR_T ih264e_put_sei_cll_params(sei_cll_params_t *ps_sei_cll,
                                bitstrm_t *ps_bitstrm)
{
    WORD32 return_status = IH264E_SUCCESS;
    UWORD8 u1_payload_size = 0;

    if(ps_sei_cll == NULL)
    {
        return IH264E_FAIL;
    }

    u1_payload_size += 2; /* max pic average light level */
    u1_payload_size += 2; /* max content light level */

    /************************************************************************/
    /* PayloadSize : This is the size of the payload in bytes               */
    /************************************************************************/
    PUT_BITS(ps_bitstrm, u1_payload_size, 8, return_status, "u1_payload_size");

    PUT_BITS(ps_bitstrm, ps_sei_cll->u2_max_content_light_level, 16,
                                return_status, "u2_max_content_light_level");
    PUT_BITS(ps_bitstrm, ps_sei_cll->u2_max_pic_average_light_level, 16,
                                return_status, "u2_max_pic_average_light_level");

    return (return_status);
}

/**
******************************************************************************
*
*  @brief Stores ambient viewing environment info in bitstream
*
*  @par   Description
*  Parse Supplemental Enhancement Information
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_sei_ave
*  pointer to ambient viewing environment info
*
*  @return      success or failure error code
*
******************************************************************************
*/
IH264E_ERROR_T ih264e_put_sei_ave_params(sei_ave_params_t *ps_sei_ave,
                                bitstrm_t *ps_bitstrm)
{
    WORD32 return_status = IH264E_SUCCESS;
    UWORD8 u1_payload_size = 0;

    if(ps_sei_ave == NULL)
    {
        return IH264E_FAIL;
    }

    u1_payload_size += 4; /* ambient illuminance */
    u1_payload_size += 2; /* ambient light x */
    u1_payload_size += 2; /* ambient light y */

    /************************************************************************/
    /* PayloadSize : This is the size of the payload in bytes               */
    /************************************************************************/
    PUT_BITS(ps_bitstrm, u1_payload_size, 8, return_status, "u1_payload_size");

    PUT_BITS(ps_bitstrm, ps_sei_ave->u4_ambient_illuminance, 32,
                                return_status, "u4_ambient_illuminance");
    PUT_BITS(ps_bitstrm, ps_sei_ave->u2_ambient_light_x, 16,
                                return_status, "u2_ambient_light_x");
    PUT_BITS(ps_bitstrm, ps_sei_ave->u2_ambient_light_y, 16,
                                return_status, "u2_ambient_light_y");

    return (return_status);
}

/**
******************************************************************************
*
*  @brief Generates Content Color Volume info (Supplemental Enhancement Information )
*
*  @par   Description
*  Parse Supplemental Enhancement Information
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_sei_ccv
*  pointer to structure containing CCV SEI data
*
*  @return      success or failure error code
*
******************************************************************************
*/
IH264E_ERROR_T ih264e_put_sei_ccv_params(sei_ccv_params_t *ps_sei_ccv,
                                bitstrm_t *ps_bitstrm)
{
    WORD32 return_status = IH264E_SUCCESS;
    UWORD16 u2_payload_bits = 0;
    UWORD8  u1_payload_bytes = 0;
    UWORD32 u4_count;

    if(ps_sei_ccv == NULL)
    {
        return IH264E_FAIL;
    }

    u2_payload_bits += 1;   /* ccv cancel flag */
    if(0 == (UWORD32)ps_sei_ccv->u1_ccv_cancel_flag)
    {
        u2_payload_bits += 1;   /* ccv persistence flag */
        u2_payload_bits += 1;   /* ccv primaries present flag */
        u2_payload_bits += 1;   /* ccv min luminance value present flag */
        u2_payload_bits += 1;   /* ccv max luminance value present flag */
        u2_payload_bits += 1;   /* ccv avg luminance value present flag */
        u2_payload_bits += 2;   /* ccv reserved zero 2bits */
        if(1 == ps_sei_ccv->u1_ccv_primaries_present_flag)
        {
            u2_payload_bits += (NUM_SEI_CCV_PRIMARIES * 32);  /* ccv primaries x[ c ] */
            u2_payload_bits += (NUM_SEI_CCV_PRIMARIES * 32);  /* ccv primaries y[ c ] */
        }
        if(1 == ps_sei_ccv->u1_ccv_min_luminance_value_present_flag)
        {
            u2_payload_bits += 32;  /* ccv min luminance value */
        }
        if(1 == ps_sei_ccv->u1_ccv_max_luminance_value_present_flag)
        {
            u2_payload_bits += 32;  /* ccv max luminance value */
        }
        if(1 == ps_sei_ccv->u1_ccv_avg_luminance_value_present_flag)
        {
            u2_payload_bits += 32;  /* ccv avg luminance value */
        }
    }

    u1_payload_bytes = (UWORD8)((u2_payload_bits + 7) >> 3);
    /************************************************************************/
    /* PayloadSize : This is the size of the payload in bytes               */
    /************************************************************************/
    PUT_BITS(ps_bitstrm, u1_payload_bytes, 8, return_status, "u1_payload_bytes");

    /*******************************************************************************/
    /* Put the Content Color Volume SEI parameters into the bitstream.  */
    /*******************************************************************************/

    PUT_BITS(ps_bitstrm, ps_sei_ccv->u1_ccv_cancel_flag, 1,
                            return_status, "u1_ccv_cancel_flag");

    if(0 == ps_sei_ccv->u1_ccv_cancel_flag)
    {
        PUT_BITS(ps_bitstrm, ps_sei_ccv->u1_ccv_persistence_flag, 1,
                                return_status, "u1_ccv_persistence_flag");
        PUT_BITS(ps_bitstrm, ps_sei_ccv->u1_ccv_primaries_present_flag, 1,
                                return_status, "u1_ccv_primaries_present_flag");
        PUT_BITS(ps_bitstrm, ps_sei_ccv->u1_ccv_min_luminance_value_present_flag, 1,
                                return_status, "u1_ccv_min_luminance_value_present_flag");
        PUT_BITS(ps_bitstrm, ps_sei_ccv->u1_ccv_max_luminance_value_present_flag, 1,
                                return_status, "u1_ccv_max_luminance_value_present_flag");
        PUT_BITS(ps_bitstrm, ps_sei_ccv->u1_ccv_avg_luminance_value_present_flag, 1,
                                return_status, "u1_ccv_avg_luminance_value_present_flag");
        PUT_BITS(ps_bitstrm, ps_sei_ccv->u1_ccv_reserved_zero_2bits, 2,
                                return_status, "u1_ccv_reserved_zero_2bits");

        /* ccv primaries */
        if(1 == ps_sei_ccv->u1_ccv_primaries_present_flag)
        {
            for(u4_count = 0; u4_count < NUM_SEI_CCV_PRIMARIES; u4_count++)
            {
                PUT_BITS(ps_bitstrm, ps_sei_ccv->ai4_ccv_primaries_x[u4_count], 32,
                                return_status, "i4_ccv_primaries_x");
                PUT_BITS(ps_bitstrm, ps_sei_ccv->ai4_ccv_primaries_y[u4_count], 32,
                                return_status, "i4_ccv_primaries_y");
            }
        }

        if(1 == ps_sei_ccv->u1_ccv_min_luminance_value_present_flag)
        {
            PUT_BITS(ps_bitstrm, ps_sei_ccv->u4_ccv_min_luminance_value, 32,
                                return_status, "u4_ccv_min_luminance_value");
        }
        if(1 == ps_sei_ccv->u1_ccv_max_luminance_value_present_flag)
        {
            PUT_BITS(ps_bitstrm, ps_sei_ccv->u4_ccv_max_luminance_value, 32,
                                return_status, "u4_ccv_max_luminance_value");
        }
        if(1 == ps_sei_ccv->u1_ccv_avg_luminance_value_present_flag)
        {
            PUT_BITS(ps_bitstrm, ps_sei_ccv->u4_ccv_avg_luminance_value, 32,
                                return_status, "u4_ccv_avg_luminance_value");
        }
    }

    return (return_status);
}

/**
******************************************************************************
*
*  @brief Generates SEI (Supplemental Enhancement Information )
*
*  @par   Description
*  Parse Supplemental Enhancement Information
*
*  @param[in]   e_payload_type
*  Determines the type of SEI msg
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_sei_params
*  pointer to structure containing SEI data
*               buffer period, recovery point, picture timing
*
*  @return      success or failure error code
*
******************************************************************************
*/
IH264E_ERROR_T ih264e_put_sei_msg(IH264_SEI_TYPE e_payload_type,
                         sei_params_t *ps_sei_params,
                         bitstrm_t *ps_bitstrm)
{
    WORD32 return_status = IH264E_SUCCESS;

    /************************************************************************/
    /* PayloadType : Send in the SEI type in the stream                     */
    /************************************************************************/

    UWORD32 u4_payload_type = e_payload_type;

    while(u4_payload_type > 0xFF)
    {
        PUT_BITS(ps_bitstrm, 0xFF, 8, return_status, "payload");
        u4_payload_type -= 0xFF;
    }

    PUT_BITS(ps_bitstrm, (UWORD32)u4_payload_type, 8, return_status, "e_payload_type");

    switch(e_payload_type)
    {
    case IH264_SEI_MASTERING_DISP_COL_VOL :
        return_status = ih264e_put_sei_mdcv_params(&(ps_sei_params->s_sei_mdcv_params),
                                                    ps_bitstrm);
        break;

    case IH264_SEI_CONTENT_LIGHT_LEVEL_DATA :
        return_status = ih264e_put_sei_cll_params(&(ps_sei_params->s_sei_cll_params),
                                                    ps_bitstrm);
        break;

    case IH264_SEI_AMBIENT_VIEWING_ENVIRONMENT :
        return_status = ih264e_put_sei_ave_params(&(ps_sei_params->s_sei_ave_params),
                                                    ps_bitstrm);
        break;

    case IH264_SEI_CONTENT_COLOR_VOLUME :
         return_status = ih264e_put_sei_ccv_params(&(ps_sei_params->s_sei_ccv_params),
                                                    ps_bitstrm);
         break;

    default :
        return_status = IH264E_FAIL;
    }

    /* rbsp trailing bits */
    if((IH264E_SUCCESS == return_status) && (ps_bitstrm->i4_bits_left_in_cw & 0x7))
        return_status = ih264e_put_rbsp_trailing_bits(ps_bitstrm);

    return(return_status);
}


