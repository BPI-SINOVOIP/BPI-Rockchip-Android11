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
*  ih264e_sei.h
*
* @brief
*  This file contains function declarations for sei message encoding
*
* @author
*
*
* @remarks
*  None
*
*******************************************************************************
*/

#ifndef ENCODER_IH264E_SEI_H_
#define ENCODER_IH264E_SEI_H_


/*****************************************************************************/
/* INTERFACE STRUCTURE DEFINITIONS                                           */
/*****************************************************************************/
typedef enum
{
    /* SEI PREFIX */

    IH264_SEI_MASTERING_DISP_COL_VOL       = 137,
    IH264_SEI_CONTENT_LIGHT_LEVEL_DATA     = 144,
    IH264_SEI_AMBIENT_VIEWING_ENVIRONMENT  = 148,
    IH264_SEI_CONTENT_COLOR_VOLUME         = 149

}IH264_SEI_TYPE;

/*****************************************************************************/
/* Function Declarations                                              */
/*****************************************************************************/

/**
******************************************************************************
*
*  @brief Generates Mastering Display Color Volume (Supplemental Enhancement Information )
*
*  @par   Description
*  Parse Supplemental Enhancement Information
*
*  @param[in]   ps_sei_mdcl
*  pointer to structure containing mdcv SEI data
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @return      success or failure error code
*
******************************************************************************
*/
IH264E_ERROR_T ih264e_put_sei_mdcv_params(sei_mdcv_params_t *ps_sei_mdcv,
                                 bitstrm_t *ps_bitstrm);

/**
******************************************************************************
*
*  @brief Stores content light level info in bitstream
*
*  @par   Description
*  Parse Supplemental Enhancement Information
*
*  @param[in]   ps_sei_cll
*  pointer to structure containing cll sei data
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @return      success or failure error code
*
******************************************************************************
*/
IH264E_ERROR_T ih264e_put_sei_cll_params(sei_cll_params_t *ps_sei_cll,
                                bitstrm_t *ps_bitstrm);

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
                                bitstrm_t *ps_bitstrm);

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
*  pointer to structure containing ccv SEI data
*
*  @return      success or failure error code
*
******************************************************************************
*/
IH264E_ERROR_T ih264e_put_sei_ccv_params(sei_ccv_params_t *ps_sei_ccv,
                                bitstrm_t *ps_bitstrm);

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
*  @param[in]   ps_sei
*  pointer to structure containing SEI data
*
*  @return      success or failure error code
*
******************************************************************************
*/
IH264E_ERROR_T ih264e_put_sei_msg(IH264_SEI_TYPE e_payload_type,
                         sei_params_t *ps_sei_params,
                         bitstrm_t *ps_bitstrm);


#endif /* ENCODER_IH264E_SEI_H_ */
