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
* \file convert_float_to_fix.c
*
* \brief
*    This file contain float to fix  and fix to float conversion function
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
#include <math.h>
#include <time.h>
#include <string.h>

/* User include files */
#include "ia_type_def.h"
/* #include "num_struct.h" */
#include "var_q_operator.h"

#define ABS(x) (((x) > 0) ? (x) : (-(x)))

void convert_float_to_fix(float a_f, number_t *a)
{
    double log_a_f;
    if(a_f != 0)
    {
        log_a_f = log(ABS(a_f)) / log(2);

        a->e = 30 - (WORD32)ceil(log_a_f);
        a->sm = (WORD32)(a_f * pow(2, a->e));
    }
    else
    {
        a->e = 0;
        a->sm = 0;
    }
}

void convert_fix_to_float(number_t a, float *a_f)
{
    *a_f = (float)(a.sm / pow(2, a.e));
}

#ifdef ITT_C6678
#pragma CODE_SECTION(convert_fix_to_float, "itt_varq_l1pram");
#pragma CODE_SECTION(convert_float_to_fix, "itt_varq_l1pram");
#endif
