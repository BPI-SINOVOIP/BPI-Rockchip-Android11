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
* \file var_q_operator.c
*
* \brief
*    This files to be used for basic fixed Q-point functions
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
/* User include files */
#include "ia_type_def.h"
#include "defs.h"
#include "ia_basic_ops32.h"
#include "ia_basic_ops40.h"
/* #include "num_struct.h" */
#include "var_q_operator.h"
#include "sqrt_interp.h"
#include "common_rom.h"

#define NUM_BITS_MAG 32

/************************************************************************************/
/* The files to be used for basic fixed Q-point functions :                         */
/*                                                                                  */
/* audio/ia_standards/c64x/include/ia_basic_ops32.h        cvs_version : FULL_V1_16 */
/* audio/ia_standards/c64x/include/ia_basic_ops40.h     cvs_version : FULL_V1_16    */
/*                                                                                  */
/************************************************************************************/

/* Multiply */

void mult32_var_q(number_t a, number_t b, number_t *c)
{
    WORD32 Q_a;
    WORD32 Q_b;
    /* WORD32 final_Q; */
    WORD32 norm_a;
    WORD32 norm_b;

    norm_a = norm32(a.sm); /* norm32 defined in ia_basic_ops32.h */
    norm_b = norm32(b.sm);

    Q_a = norm_a + a.e;
    Q_b = norm_b + b.e;

    a.sm = shl32_sat(a.sm, norm_a);
    b.sm = shl32_sat(b.sm, norm_b);

    c->sm = mult32(a.sm, b.sm); /* mult32 defined in ia_basic_ops40.h */
    c->e = a.e + b.e + norm_a + norm_b - 32; /* mult32 decreases the Q-format by 32 */
}

/* Division */

void div32_var_q(number_t a, number_t b, number_t *c)
{
    WORD32 qoutient_q_format;

    c->sm = div32(a.sm, b.sm, &qoutient_q_format); /* div32 defined in ia_basic_ops32.h */
    c->e = (a.e - b.e) + qoutient_q_format;
}

/* Addition */

void add32_var_q(number_t a, number_t b, number_t *c)
{
    WORD32 Q_a;
    WORD32 Q_b;
    WORD32 final_Q;
    WORD32 norm_a;
    WORD32 norm_b;

    norm_a = norm32(a.sm) - 1; /* norm32 defined in ia_basic_ops32.h */
    norm_b = norm32(b.sm) - 1; /* we normalise a & b only to 30t bit
                                        instead of to 31st bit
                                        */

    Q_a = norm_a + a.e;
    Q_b = norm_b + b.e;

    if(Q_b < Q_a)
    {
        b.sm = shl32_dir_sat(b.sm, norm_b);
        a.sm = shr32_dir_sat(a.sm, ((a.e - b.e) - norm_b));
        final_Q = Q_b;
    }
    else if(Q_a < Q_b)
    {
        a.sm = shl32_dir_sat(a.sm, norm_a);
        b.sm = shr32_dir_sat(b.sm, ((b.e - a.e) - norm_a));
        final_Q = Q_a;
    }
    else
    {
        a.sm = shl32_dir_sat(a.sm, norm_a);
        b.sm = shl32_dir_sat(b.sm, norm_b);
        final_Q = Q_a;
    }

    c->sm = add32(a.sm, b.sm); /* add32_shr defined in ia_basic_ops32.h */
    c->e = final_Q; /* because add32_shr does right shift
                                            by 1 before adding */
}

/* Subtraction */

void sub32_var_q(number_t a, number_t b, number_t *c)
{
    WORD32 Q_a;
    WORD32 Q_b;
    WORD32 final_Q;
    WORD32 norm_a;
    WORD32 norm_b;

    norm_a = norm32(a.sm) - 1; /* norm32 defined in ia_basic_ops32.h */
    norm_b = norm32(b.sm) - 1; /* we normalise a & b only to 30t bit
                                        instead of to 31st bit
                                        */

    Q_a = norm_a + a.e;
    Q_b = norm_b + b.e;

    if(Q_b < Q_a)
    {
        b.sm = shl32_dir_sat(b.sm, norm_b);
        a.sm = shr32_dir_sat(a.sm, ((a.e - b.e) - norm_b));
        final_Q = Q_b;
    }
    else if(Q_a < Q_b)
    {
        a.sm = shl32_dir_sat(a.sm, norm_a);
        b.sm = shr32_dir_sat(b.sm, ((b.e - a.e) - norm_a));
        final_Q = Q_a;
    }
    else
    {
        a.sm = shl32_dir_sat(a.sm, norm_a);
        b.sm = shl32_dir_sat(b.sm, norm_b);
        final_Q = Q_a;
    }

    c->sm = sub32(a.sm, b.sm); /* add32_shr defined in ia_basic_ops32.h */
    c->e = final_Q; /* because add32_shr does right shift
                                            by 1 before adding */
}

/* square root */

void sqrt32_var_q(number_t a, number_t *c)
{
    WORD32 q_temp;
    q_temp = a.e;
    c->sm = sqrtFix_interpolate(a.sm, &q_temp, gi4_sqrt_tab);
    /* c->sm = sqrtFix(a.sm, &q_temp, gi4_sqrt_tab); */
    c->e = q_temp;
}

void number_t_to_word32(number_t num_a, WORD32 *a)
{
    *a = shr32_dir_sat(num_a.sm, num_a.e);
}

/*
convert_float_to_fix(float a_f,
                     number_t *a)
{
            double log_a_f;
            log_a_f = log(ABS(a_f))/log(2);

            a->e = 30 - (WORD32)ceil(log_a_f);
            a->sm = (WORD32) (a_f * pow(2, a->e));
}
*/

#ifdef ITT_C6678
#pragma CODE_SECTION(number_t_to_word32, "itt_varq_l1pram");
#pragma CODE_SECTION(sqrt32_var_q, "itt_varq_l1pram");
#pragma CODE_SECTION(sub32_var_q, "itt_varq_l1pram");
#pragma CODE_SECTION(add32_var_q, "itt_varq_l1pram");
#pragma CODE_SECTION(div32_var_q, "itt_varq_l1pram");
#pragma CODE_SECTION(mult32_var_q, "itt_varq_l1pram");
#endif
