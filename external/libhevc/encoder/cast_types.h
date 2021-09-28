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
/*  File Name         : cast_types.h                                         */
/*                                                                           */
/*  Description       : This file contains all the necessary constants and   */
/*                      type definitions according to CAST specifications.   */
/*                                                                           */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         28 09 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

#ifndef CAST_TYPES_H
#define CAST_TYPES_H

#include "ihevc_typedefs.h"

/*****************************************************************************/
/* Constants                                                                 */
/*****************************************************************************/

/* The following definitions indicates the input parameter / argument state  */

/* Parameter declared with IN, will be used to hold INput value */
#define IN

/* Parameter declared with OUT, will be used to hold OUTput value */
#define OUT

/* Parameter declared with INOUT, will have INput value and will hold        */
/* OUTput value                                                              */
#define INOUT

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/* Typedef's for built-in  datatypes */

typedef float FLOAT;

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/* Defined to hold the unsigned 64 bit data */
typedef struct
{
    UWORD32 lsw; /* Holds lower 32 bits */
    UWORD32 msw; /* Holds upper 32 bits */
} UWORD64;

/* Defined to hold the signed 64 bit data */
typedef struct
{
    UWORD32 lsw; /* Holds lower 32 bits */
    WORD32 msw; /* Holds upper 32 bits */
} WORD64;

#endif /* CAST_TYPES_H */
