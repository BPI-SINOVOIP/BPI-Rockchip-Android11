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
#ifndef VAR_Q_OPERATOR_H
#define VAR_Q_OPERATOR_H

typedef struct
{
    WORD32 sm; /* MSB 1 bit sign & rest magnitude */
    WORD32 e; /* Q-format */
} number_t;

void mult32_var_q(number_t a, number_t b, number_t *c);

void div32_var_q(number_t a, number_t b, number_t *c);

void add32_var_q(number_t a, number_t b, number_t *c);

void sub32_var_q(number_t a, number_t b, number_t *c);

void sqrt32_var_q(number_t a, number_t *c);

void number_t_to_word32(number_t num_a, WORD32 *a);

void convert_float_to_fix(float a_f, number_t *a);

void convert_fix_to_float(number_t a, float *a_f);

#define SET_VAR_Q(a, b, c)                                                                         \
    {                                                                                              \
        (a).sm = (b);                                                                              \
        (a).e = (c);                                                                               \
    }

/*right shift greated than 32 bit for word32 variable is undefined*/
#define convert_varq_to_fixq(varq, a, q_fact)                                                      \
    {                                                                                              \
        if((varq).e > q_fact)                                                                      \
        {                                                                                          \
            if(((varq).e - q_fact) >= (WORD32)(sizeof(WORD32) * 8))                                \
            {                                                                                      \
                *a = 0;                                                                            \
            }                                                                                      \
            else                                                                                   \
            {                                                                                      \
                *a = (varq).sm >> ((varq).e - q_fact);                                             \
            }                                                                                      \
        }                                                                                          \
        else                                                                                       \
            *a = (varq).sm << (q_fact - (varq).e);                                                 \
    }

#define SET_VARQ_FRM_FIXQ(fixq, var_q, q_fact)                                                     \
    {                                                                                              \
        (var_q).sm = fixq;                                                                         \
        (var_q).e = q_fact;                                                                        \
    }
#endif
