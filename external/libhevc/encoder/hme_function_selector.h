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
* \file hme_function_selector.h
*
* \brief
*    function selector prototypes
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _HME_FUNCTION_SELECTOR_H_
#define _HME_FUNCTION_SELECTOR_H_

#include "ihevce_defs.h"

/*****************************************************************************/
/* Functions                                                                 */
/*****************************************************************************/
void hme_init_function_ptr(void *pv_enc_ctxt, IV_ARCH_T e_processor_arch);

#endif /* _HME_FUNCTION_SELECTOR_H_ */
