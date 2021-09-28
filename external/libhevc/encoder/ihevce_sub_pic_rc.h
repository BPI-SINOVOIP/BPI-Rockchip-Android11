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
* \file ihevce_sub_pic_rc.h
*
* \brief
*    This file contains interface defination of SubPic level RC
*
* \date
*    11/09/2015
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_SUB_PIC_RC_H_
#define _IHEVCE_SUB_PIC_RC_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define MIN_QP_NO_CLIP_DEV 4

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

void ihevce_sub_pic_rc_in_data(
    void *pv_multi_thrd_ctxt, void *pv_ctxt, void *pv_ctb_ipe_analyse, void *pv_frm_ctb_prms);

void ihevce_sub_pic_rc_scale_query(void *pv_multi_thrd_ctxt, void *pv_ctxt);

#endif /* _IHEVCE_SUB_PIC_RC_H_ */
