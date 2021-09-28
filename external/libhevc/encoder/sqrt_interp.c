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
* \file squrt_interp.c
*
* \brief
*    This file contain square root interpolate function
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/
/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/
/* System include files */
#include <stdio.h>
#include <stdlib.h>

/* User include files */
#include "ia_type_def.h"
#include "defs.h"
/* #include "constants.h" */
#include "ia_basic_ops32.h"
/* #include "ia_basic_ops16.h" */
#include "ia_basic_ops40.h"
#include "sqrt_interp.h"
/* #include "ia_enhaacplus_enc_basicops.h" */
#include "common_rom.h"

WORD32 sqrtFix_interpolate(WORD32 num, WORD *q, const WORD32 *sqrt_tab)
{
    WORD32 index, answer, temp, delta, step_size;
    WORD q_temp = *q;
    WORD k;

    if(num == 0)
        return 0;

    k = norm32(num);
    temp = shr32(shl32(num, k), 21);
    q_temp += k;
    index = temp & 0x1FF; /* Leave leading 1 */
    {
        delta = shl32((shl32(num, k) - shl32(temp, 21)), 10);
        step_size = sub32(sqrt_tab[index + 1], sqrt_tab[index]);
        step_size = mult32_shl_sat(step_size, delta); /* Q:Q_SQRT_TAB + 21 + 10 - 31 */
        answer = add32(sqrt_tab[index], step_size);
    }
    if(q_temp & 1)
    {
        q_temp -= 1;
        answer = mult32_shl_sat(answer, INV_SQRT_2_Q31);
    }

    q_temp = q_temp >> 1;
    q_temp += (Q_SQRT_TAB);
    *q = q_temp;

    answer >>= 1;
    *q -= 1;

    return answer;
}

WORD32 sqrtFix(WORD32 num, WORD *q, const WORD32 *sqrt_tab)
{
    WORD32 index, answer, temp;
    WORD k;
    WORD q_temp = *q;

    if(num == 0)
        return 0;

    k = norm32(num);
    temp = shr32(shl32(num, k), 21);
    q_temp += k;
    index = temp & 0x1FF; /* Leave leading 1 */
    answer = sqrt_tab[index];
    if(q_temp & 1)
    {
        q_temp -= 1;
        answer = mult32x16in32_shl(answer, INV_SQRT_2_Q15);
    }
    q_temp = q_temp >> 1;
    q_temp += Q_SQRT_TAB;
    *q = q_temp;
    return answer;
}

#ifdef ITT_C6678
#pragma CODE_SECTION(sqrtFix_interpolate, "itt_varq_l1pram");
#pragma CODE_SECTION(sqrtFix, "itt_varq_l1pram");
#endif
