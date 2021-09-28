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
* \file rc_common.h
*
* \brief
*    This file contains common macro used by rate control module
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/
#ifndef _RC_COMMON_H_
#define _RC_COMMON_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
/****************************************************************************
   NOTE : Put only those things into this file which are common across many
          files, say I_TO_P_BIT_RATIO macro is used across bit_allocation.c
          and rate_control_api.c.If anything is exclusive only to one file,
          define it in the same file

          This file is an RC private file. It should not be exported to Codec
 ****************************************************************************/

/* Defines the maximum and the minimum quantizer allowed in the stream.*/
#define MAX_MPEG2_QP (4095) /*255*/ /* 127*/
#define ERR_2PASS_DISTR_GOP 5

#define ENABLE_SSD_CALC_RC 0

#ifdef ARM9
/* Mem tab alignment to 4 bytes  */
#define MEM_TAB_ALIGNMENT 32 /*ALIGN_WORD32*/
#else /* ARM968 */
/* Mem tab alignment to 128 bytes  */
#define MEM_TAB_ALIGNMENT ALIGN_128_BYTE
#endif /* ARM968 */

#define COMP_TO_BITS_MAP(X, factor)                                                                \
    ((-1.7561f * (X * X * X * X) + (2.5547f * X * X * X) - 0.3408f * (X * X) + (0.5343f * X) -     \
      0.003f) *                                                                                    \
     factor)

#define COMP_TO_BITS_MAP_2_PASS(X, factor)                                                         \
    ((-1.7561f * (X * X * X * X) + (2.5547f * X * X * X) - 0.3408f * (X * X) + (0.5343f * X) -     \
      0.003f) *                                                                                    \
     factor)
/* Calculates P = (X*Y/Z) (Assuming all the four are in integers)*/
#define X_PROD_Y_DIV_Z(X1, Y1, Z1, P1)                                                             \
    {                                                                                              \
        number_t vq_a, vq_b, vq_c;                                                                 \
        SET_VAR_Q(vq_a, (X1), 0);                                                                  \
        SET_VAR_Q(vq_b, (Y1), 0);                                                                  \
        SET_VAR_Q(vq_c, (Z1), 0);                                                                  \
        mult32_var_q(vq_a, vq_b, &vq_a);                                                           \
        div32_var_q(vq_a, vq_c, &vq_a);                                                            \
        number_t_to_word32(vq_a, &(P1));                                                           \
    }

/* Maximum number of drain-rates supported. Currently a maximum of only 2
   drain-rates supported. One for
   I pictures and the other for P & B pictures */
#define MAX_NUM_DRAIN_RATES 2

/* The ratios between I to P and P to B Qp is specified here */
#define K_Q 4
#define I_TO_P_RATIO (18) /*(16)*/ /* In K_Q Q factor */
#define P_TO_B_RATIO (18) /* In K_Q Q factor */
#define B_TO_B1_RATIO (18)
#define B1_TO_B2_RATIO (18)
#define P_TO_I_RATIO (14)
#define I_TO_B_RATIO ((P_TO_B_RATIO * I_TO_P_RATIO) >> K_Q)
#define I_TO_B1_RATIO ((B_TO_B1_RATIO * P_TO_B_RATIO * I_TO_P_RATIO) >> (K_Q + K_Q))
#define I_TO_B2_RATIO                                                                              \
    ((B1_TO_B2_RATIO * B_TO_B1_RATIO * P_TO_B_RATIO * I_TO_P_RATIO) >> (K_Q + K_Q + K_Q))

#define P_TO_B_RATIO_HBR (16)
#define I_TO_P_RATIO_LOW_MOTION (20)
#define I_TO_P_RATIO_VLOW_MOTION (23)
#define I_TO_P_RATIO_VVLOW_MOTION (26)

/* #define I_TO_P_RATIO (16)  In K_Q Q factor */
/* #define P_TO_B_RATIO (16)  In K_Q Q factor */

/*Ratio of I frame bit consumptin vs average bit consumption for rest of the GOP
 * This is based on experimaental runs over different seqience(same resolution scaled)*/
#define I_TO_AVG_REST_GOP_BIT (8)
#define MINIMUM_VISIBILITY_B4_STATIC_I                                                             \
    (18)  //assumes this minimum lap window for bit allocation of static frame
#define MINIMUM_FRM_I_TO_REST_LAP_ENABLED (8)
#define I_TO_AVG_REST_GOP_BIT_MIN (1)
#define I_TO_AVG_REST_GOP_BIT_MAX (20)
#define I_TO_AVG_REST_GOP_BIT_MAX_INFINITE (80)
#define I_TO_AVG_REST_GOP_BIT_MAX_2_PASS (40)
#define I_TO_AVG_REST_GOP_BIT_MIN_2_PASS (0.5f)

#define UPPER_THRESHOLD_EBF_Q4 (15)
#define STATIC_I_TO_REST_MULTIPLIER (6)

/*also present in encoder herader file with same name*/
#define MAX_LAP_COMPLEXITY_Q7 (90)
#define DEFAULT_TEX_PERCENTAGE_Q5 24

#ifdef DISABLE_NON_STEADY_STATE_CODE
#define NON_STEADSTATE_CODE (0)
#else
#define NON_STEADSTATE_CODE (1)
#endif

/*HEVC_hierarchy*/
#define I_TO_P_BIT_RATIO (6)
#define P_TO_B_BIT_RATIO (2)
#define B_TO_B1_BIT_RATO0 (2)
#define B1_TO_B2_BIT_RATIO (2)

/*define static I_TO_P ratio for all pic types*/
/* Trying to detect a static content and fixing the quality for that content. The trigger for such a
content is if the ratio between the estimated I frame to that of P is more than 18 times. If such
a simple content is detected then the bit ditribution is fixed to a ration of 36:2:1 (I:P:B) */
#define STATIC_I_TO_B2_RATIO (100)  //(24)
#define STATIC_P_TO_B2_RATIO (2)
#define STATIC_B_TO_B2_RATIO (1)
#define STATIC_B1_TO_B2_RATIO (1)

/*Fsim limits*/
#define RC_FSIM_LOW_THR_SCD 64

#define RC_FSIM_HIGH_THR_STATIC 115

#endif /* _RC_COMMON_H_ */
