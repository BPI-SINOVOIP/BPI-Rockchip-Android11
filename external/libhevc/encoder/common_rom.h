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
* \file common_rom.h
*
* \brief
*    This file contain square root table declarations
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/
#ifndef SQRT_TAB_H
#define SQRT_TAB_H

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

#define INV_SQRT_2_Q31 1518500250 /* 1/sqrt(2) in Q31 */
#define INV_SQRT_2_Q15 23170

#define Q_SQRT_TAB 15

extern const WORD32 gi4_sqrt_tab[513]; /*sqrt_tab in Q15*/

#endif
