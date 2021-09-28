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
/****************************************************************************/
/*                                                                          */
/*  File Name         :  trace_support.h                                    */
/*                                                                          */
/*  Description       :  Defines the functions for trace_support.c          */
/*                                                                          */
/*  List of Functions :                                                     */
/*                                                                          */
/*  Issues / Problems :                                                     */
/*                                                                          */
/*  Revision History  :                                                     */
/*                                                                          */
/*       DD MM YYYY      Author(s)        Changes (Describe the changes)    */
/*       24 03 2008      DPKA               Creation                        */
/****************************************************************************/
#ifndef TRACE_SUPPORT_H
#define TRACE_SUPPORT_H

#define TRACE_SUPPORT 0

#define RC_DEBUG_LEVEL_1 0

#define RC_2PASS_GOP_DEBUG 0

#define HEVC_RC 1

typedef struct
{
    WORD8 *pu1_buf;
    WORD32 i4_offset;
    WORD32 i4_max_size;
} trace_support_t;

void init_trace_support(WORD8 *pu1_buf, WORD32 i4_size);

#if TRACE_SUPPORT
#define trace_printf(...) printf(__VA_ARGS__)
#else
#define trace_printf(...)
#endif

#define ASSERT(x) assert((x))
//#define ASSERT(x) ihevcd_debug_assert((x))

#endif
