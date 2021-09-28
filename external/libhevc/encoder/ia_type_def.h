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

/*****************************************************************************/
/*                                                                           */
/*  File Name        : ia_type_def.h                                         */
/*                                                                           */
/*  Description      : Type definations file                                 */
/*                                                                           */
/*  List of Functions: None                                                  */
/*                                                                           */
/*  Issues / Problems: None                                                  */
/*                                                                           */
/*  Revision History :                                                       */
/*                                                                           */
/*        DD MM YYYY       Author                Changes                     */
/*        29 07 2005       ittiam                Created                     */
/*                                                                           */
/*****************************************************************************/

#ifndef __TYPEDEFTEST_H__
#define __TYPEDEFTEST_H__

#include <stdint.h>

/****************************************************************************/
/*     types               type define    prefix        examples      bytes */
/************************  ***********    ******    ****************  ***** */
typedef char CHAR8; /* c       CHAR8    c_name     1   */
typedef char *pCHAR8; /* pc      pCHAR8   pc_nmae    1   */
typedef int8_t WORD8; /* b       WORD8    b_name     1   */
typedef int8_t *pWORD8; /* pb      pWORD8   pb_nmae    1   */
typedef uint8_t UWORD8; /* ub      UWORD8   ub_count   1   */
typedef uint8_t *pUWORD8; /* pub     pUWORD8  pub_count  1   */

typedef int16_t WORD16; /* s       WORD16   s_count    2   */
typedef int16_t *pWORD16; /* ps      pWORD16  ps_count   2   */
typedef uint16_t UWORD16; /* us      UWORD16  us_count   2   */
typedef uint16_t *pUWORD16; /* pus     pUWORD16 pus_count  2   */

typedef int32_t WORD24; /* k       WORD24   k_count    3   */
typedef int32_t *pWORD24; /* pk      pWORD24  pk_count   3   */
typedef uint32_t UWORD24; /* uk      UWORD24  uk_count   3   */
typedef uint32_t *pUWORD24; /* puk     pUWORD24 puk_count  3   */

typedef int32_t WORD32; /* i       WORD32   i_count    4   */
typedef int32_t *pWORD32; /* pi      pWORD32  pi_count   4   */
typedef uint32_t UWORD32; /* ui      UWORD32  ui_count   4   */
typedef uint32_t *pUWORD32; /* pui     pUWORD32 pui_count  4   */

/* These typedefs remain same across C64xP and ARM */
typedef int64_t WORD40; /* m       WORD40   m_count    5   */
typedef int64_t *pWORD40; /* pm      pWORD40  pm_count   5   */
typedef uint64_t UWORD40; /* um      UWORD40  um_count   5   */
typedef uint64_t *pUWORD40; /* pum     pUWORD40 pum_count  5   */

typedef int64_t LWORD64; /* h       LWORD64   h_count    8   */
typedef int64_t *pWORD64; /* ph      pWORD64  ph_count   8   */
typedef uint64_t ULWORD64; /* uh      ULWORD64  uh_count   8   */
typedef uint64_t *pUWORD64; /* puh     pUWORD64 puh_count  8   */

typedef float FLOAT32; /* f       FLOAT32  f_count    4   */
typedef float *pFLOAT32; /* pf      pFLOAT32 pf_count   4   */
typedef double FLOAT64; /* d       UFLOAT64 d_count    8   */
typedef double *pFlOAT64; /* pd      pFLOAT64 pd_count   8   */

typedef void VOID; /* v       VOID     v_flag     4   */
typedef void *pVOID; /* pv      pVOID    pv_flag    4   */

/* variable size types: platform optimized implementation */
typedef int32_t BOOL; /* bool    BOOL     bool_true      */
typedef uint32_t UBOOL; /* ubool   BOOL     ubool_true     */
typedef int32_t FLAG; /* flag    FLAG     flag_false     */
typedef uint32_t UFLAG; /* uflag   FLAG     uflag_false    */
typedef int32_t LOOPIDX; /* lp      LOOPIDX  lp_index       */
typedef uint32_t ULOOPIDX; /* ulp     SLOOPIDX ulp_index      */
typedef int32_t WORD; /* lp      LOOPIDX  lp_index       */
typedef uint32_t UWORD; /* ulp     SLOOPIDX ulp_index      */

typedef LOOPIDX LOOPINDEX; /* lp    LOOPIDX  lp_index       */
typedef ULOOPIDX ULOOPINDEX; /* ulp   SLOOPIDX ulp_index      */

#define PLATFORM_INLINE __inline

#endif /* __TYPEDEFTEST_H__ */
