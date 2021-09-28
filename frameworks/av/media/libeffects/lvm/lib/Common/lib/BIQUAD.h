/*
 * Copyright (C) 2004-2010 NXP Software
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _BIQUAD_H_
#define _BIQUAD_H_

#include "LVM_Types.h"
/**********************************************************************************
   INSTANCE MEMORY TYPE DEFINITION
***********************************************************************************/
typedef struct
{
#ifdef SUPPORT_MC
    /* The memory region created by this structure instance is typecast
     * into another structure containing a pointer and an array of filter
     * coefficients. In one case this memory region is used for storing
     * DC component of channels
     */
    LVM_FLOAT *pStorage;
    LVM_FLOAT Storage[LVM_MAX_CHANNELS];
#else
    LVM_FLOAT Storage[6];
#endif
} Biquad_FLOAT_Instance_t;
/**********************************************************************************
   COEFFICIENT TYPE DEFINITIONS
***********************************************************************************/

/*** Biquad coefficients **********************************************************/
typedef struct
{
    LVM_FLOAT  A2;   /*  a2  */
    LVM_FLOAT  A1;   /*  a1  */
    LVM_FLOAT  A0;   /*  a0  */
    LVM_FLOAT  B2;   /* -b2! */
    LVM_FLOAT  B1;   /* -b1! */
} BQ_FLOAT_Coefs_t;

/*** First order coefficients *****************************************************/
typedef struct
{
    LVM_FLOAT A1;   /*  a1  */
    LVM_FLOAT A0;   /*  a0  */
    LVM_FLOAT B1;   /* -b1! */
} FO_FLOAT_Coefs_t;

/*** First order coefficients with Shift*****************************************************/
typedef struct
{
    LVM_FLOAT A1;    /*  a1  */
    LVM_FLOAT A0;    /*  a0  */
    LVM_FLOAT B1;    /* -b1! */
} FO_FLOAT_LShx_Coefs_t;
/*** Band pass coefficients *******************************************************/
typedef struct
{
    LVM_FLOAT  A0;   /*  a0  */
    LVM_FLOAT  B2;   /* -b2! */
    LVM_FLOAT  B1;   /* -b1! */
} BP_FLOAT_Coefs_t;

/*** Peaking coefficients *********************************************************/
typedef struct
{
    LVM_FLOAT A0;   /*  a0  */
    LVM_FLOAT B2;   /* -b2! */
    LVM_FLOAT B1;   /* -b1! */
    LVM_FLOAT  G;   /* Gain */
} PK_FLOAT_Coefs_t;

/**********************************************************************************
   TAPS TYPE DEFINITIONS
***********************************************************************************/

/*** Types used for first order and shelving filter *******************************/
typedef struct
{
    LVM_FLOAT Storage[ (1 * 2) ];  /* One channel, two taps of size LVM_INT32 */
} Biquad_1I_Order1_FLOAT_Taps_t;

typedef struct
{
#ifdef SUPPORT_MC
    /* LVM_MAX_CHANNELS channels, two taps of size LVM_FLOAT */
    LVM_FLOAT Storage[ (LVM_MAX_CHANNELS * 2) ];
#else
    LVM_FLOAT Storage[ (2 * 2) ];  /* Two channels, two taps of size LVM_FLOAT */
#endif
} Biquad_2I_Order1_FLOAT_Taps_t;

/*** Types used for biquad, band pass and peaking filter **************************/
typedef struct
{
    LVM_FLOAT Storage[ (1 * 4) ];  /* One channel, four taps of size LVM_FLOAT */
} Biquad_1I_Order2_FLOAT_Taps_t;

typedef struct
{
#ifdef SUPPORT_MC
    /* LVM_MAX_CHANNELS, four taps of size LVM_FLOAT */
    LVM_FLOAT Storage[ (LVM_MAX_CHANNELS * 4) ];
#else
    LVM_FLOAT Storage[ (2 * 4) ];  /* Two channels, four taps of size LVM_FLOAT */
#endif
} Biquad_2I_Order2_FLOAT_Taps_t;
/* The names of the functions are changed to satisfy QAC rules: Name should be Unique withing 16 characters*/
#define BQ_2I_D32F32Cll_TRC_WRA_01_Init  Init_BQ_2I_D32F32Cll_TRC_WRA_01
#define BP_1I_D32F32C30_TRC_WRA_02       TWO_BP_1I_D32F32C30_TRC_WRA_02

/**********************************************************************************
   FUNCTION PROTOTYPES: BIQUAD FILTERS
***********************************************************************************/

/*** 16 bit data path *************************************************************/

void BQ_2I_D16F32Css_TRC_WRA_01_Init (   Biquad_FLOAT_Instance_t         *pInstance,
                                         Biquad_2I_Order2_FLOAT_Taps_t   *pTaps,
                                         BQ_FLOAT_Coefs_t            *pCoef);

void BQ_2I_D16F32C15_TRC_WRA_01 (           Biquad_FLOAT_Instance_t       *pInstance,
                                            LVM_FLOAT                    *pDataIn,
                                            LVM_FLOAT                    *pDataOut,
                                            LVM_INT16                    NrSamples);

void BQ_2I_D16F32C14_TRC_WRA_01 (           Biquad_FLOAT_Instance_t       *pInstance,
                                            LVM_FLOAT                    *pDataIn,
                                            LVM_FLOAT                    *pDataOut,
                                            LVM_INT16                    NrSamples);

void BQ_2I_D16F32C13_TRC_WRA_01 (           Biquad_FLOAT_Instance_t       *pInstance,
                                            LVM_FLOAT                    *pDataIn,
                                            LVM_FLOAT                    *pDataOut,
                                            LVM_INT16                    NrSamples);

void BQ_2I_D16F16Css_TRC_WRA_01_Init (   Biquad_FLOAT_Instance_t         *pInstance,
                                         Biquad_2I_Order2_FLOAT_Taps_t   *pTaps,
                                         BQ_FLOAT_Coefs_t            *pCoef);

void BQ_2I_D16F16C15_TRC_WRA_01( Biquad_FLOAT_Instance_t       *pInstance,
                                 LVM_FLOAT               *pDataIn,
                                 LVM_FLOAT               *pDataOut,
                                 LVM_INT16               NrSamples);

void BQ_2I_D16F16C14_TRC_WRA_01( Biquad_FLOAT_Instance_t       *pInstance,
                                 LVM_FLOAT               *pDataIn,
                                 LVM_FLOAT               *pDataOut,
                                 LVM_INT16               NrSamples);

void BQ_1I_D16F16Css_TRC_WRA_01_Init (   Biquad_FLOAT_Instance_t         *pInstance,
                                         Biquad_1I_Order2_FLOAT_Taps_t   *pTaps,
                                         BQ_FLOAT_Coefs_t            *pCoef);

void BQ_1I_D16F16C15_TRC_WRA_01 ( Biquad_FLOAT_Instance_t       *pInstance,
                                  LVM_FLOAT               *pDataIn,
                                  LVM_FLOAT               *pDataOut,
                                  LVM_INT16               NrSamples);

void BQ_1I_D16F32Css_TRC_WRA_01_Init (   Biquad_FLOAT_Instance_t         *pInstance,
                                         Biquad_1I_Order2_FLOAT_Taps_t   *pTaps,
                                         BQ_FLOAT_Coefs_t            *pCoef);

void BQ_1I_D16F32C14_TRC_WRA_01 ( Biquad_FLOAT_Instance_t       *pInstance,
                                  LVM_FLOAT              *pDataIn,
                                  LVM_FLOAT               *pDataOut,
                                  LVM_INT16               NrSamples);
/*** 32 bit data path *************************************************************/
void BQ_2I_D32F32Cll_TRC_WRA_01_Init (      Biquad_FLOAT_Instance_t       *pInstance,
                                            Biquad_2I_Order2_FLOAT_Taps_t *pTaps,
                                            BQ_FLOAT_Coefs_t          *pCoef);
void BQ_2I_D32F32C30_TRC_WRA_01 (           Biquad_FLOAT_Instance_t  *pInstance,
                                            LVM_FLOAT                    *pDataIn,
                                            LVM_FLOAT                    *pDataOut,
                                            LVM_INT16                 NrSamples);
#ifdef SUPPORT_MC
void BQ_MC_D32F32C30_TRC_WRA_01 (           Biquad_FLOAT_Instance_t      *pInstance,
                                            LVM_FLOAT                    *pDataIn,
                                            LVM_FLOAT                    *pDataOut,
                                            LVM_INT16                    NrFrames,
                                            LVM_INT16                    NrChannels);
#endif

/**********************************************************************************
   FUNCTION PROTOTYPES: FIRST ORDER FILTERS
***********************************************************************************/

/*** 16 bit data path *************************************************************/
void FO_1I_D16F16Css_TRC_WRA_01_Init(    Biquad_FLOAT_Instance_t         *pInstance,
                                         Biquad_1I_Order1_FLOAT_Taps_t   *pTaps,
                                         FO_FLOAT_Coefs_t            *pCoef);

void FO_1I_D16F16C15_TRC_WRA_01( Biquad_FLOAT_Instance_t       *pInstance,
                                 LVM_FLOAT               *pDataIn,
                                 LVM_FLOAT               *pDataOut,
                                 LVM_INT16               NrSamples);

void FO_2I_D16F32Css_LShx_TRC_WRA_01_Init(Biquad_FLOAT_Instance_t       *pInstance,
                                          Biquad_2I_Order1_FLOAT_Taps_t *pTaps,
                                          FO_FLOAT_LShx_Coefs_t     *pCoef);

void FO_2I_D16F32C15_LShx_TRC_WRA_01(Biquad_FLOAT_Instance_t       *pInstance,
                                     LVM_FLOAT               *pDataIn,
                                     LVM_FLOAT               *pDataOut,
                                     LVM_INT16               NrSamples);
/*** 32 bit data path *************************************************************/
void FO_1I_D32F32Cll_TRC_WRA_01_Init( Biquad_FLOAT_Instance_t       *pInstance,
                                      Biquad_1I_Order1_FLOAT_Taps_t *pTaps,
                                      FO_FLOAT_Coefs_t          *pCoef);
void FO_1I_D32F32C31_TRC_WRA_01( Biquad_FLOAT_Instance_t       *pInstance,
                                 LVM_FLOAT                     *pDataIn,
                                 LVM_FLOAT                     *pDataOut,
                                 LVM_INT16                     NrSamples);
#ifdef SUPPORT_MC
void FO_Mc_D16F32C15_LShx_TRC_WRA_01(Biquad_FLOAT_Instance_t  *pInstance,
                                     LVM_FLOAT                *pDataIn,
                                     LVM_FLOAT                *pDataOut,
                                     LVM_INT16                NrFrames,
                                     LVM_INT16                NrChannels);
#endif
/**********************************************************************************
   FUNCTION PROTOTYPES: BAND PASS FILTERS
***********************************************************************************/

/*** 16 bit data path *************************************************************/
void BP_1I_D16F16Css_TRC_WRA_01_Init( Biquad_FLOAT_Instance_t       *pInstance,
                                      Biquad_1I_Order2_FLOAT_Taps_t *pTaps,
                                      BP_FLOAT_Coefs_t              *pCoef);
void BP_1I_D16F16C14_TRC_WRA_01 (     Biquad_FLOAT_Instance_t       *pInstance,
                                      LVM_FLOAT                     *pDataIn,
                                      LVM_FLOAT                     *pDataOut,
                                      LVM_INT16                     NrSamples);
void BP_1I_D16F32Cll_TRC_WRA_01_Init (Biquad_FLOAT_Instance_t       *pInstance,
                                      Biquad_1I_Order2_FLOAT_Taps_t *pTaps,
                                      BP_FLOAT_Coefs_t              *pCoef);
void BP_1I_D16F32C30_TRC_WRA_01 (           Biquad_FLOAT_Instance_t       *pInstance,
                                            LVM_FLOAT                    *pDataIn,
                                            LVM_FLOAT                    *pDataOut,
                                            LVM_INT16                    NrSamples);
/*** 32 bit data path *************************************************************/
void BP_1I_D32F32Cll_TRC_WRA_02_Init (      Biquad_FLOAT_Instance_t       *pInstance,
                                            Biquad_1I_Order2_FLOAT_Taps_t *pTaps,
                                            BP_FLOAT_Coefs_t          *pCoef);
void BP_1I_D32F32C30_TRC_WRA_02(            Biquad_FLOAT_Instance_t       *pInstance,
                                            LVM_FLOAT                    *pDataIn,
                                            LVM_FLOAT                    *pDataOut,
                                            LVM_INT16                    NrSamples);

/*** 32 bit data path STEREO ******************************************************/
void PK_2I_D32F32CssGss_TRC_WRA_01_Init (   Biquad_FLOAT_Instance_t       *pInstance,
                                            Biquad_2I_Order2_FLOAT_Taps_t *pTaps,
                                            PK_FLOAT_Coefs_t          *pCoef);
void PK_2I_D32F32C14G11_TRC_WRA_01( Biquad_FLOAT_Instance_t       *pInstance,
                                    LVM_FLOAT               *pDataIn,
                                    LVM_FLOAT               *pDataOut,
                                    LVM_INT16               NrSamples);
#ifdef SUPPORT_MC
void PK_Mc_D32F32C14G11_TRC_WRA_01(Biquad_FLOAT_Instance_t       *pInstance,
                                   LVM_FLOAT               *pDataIn,
                                   LVM_FLOAT               *pDataOut,
                                   LVM_INT16               NrFrames,
                                   LVM_INT16               NrChannels);
#endif

/**********************************************************************************
   FUNCTION PROTOTYPES: DC REMOVAL FILTERS
***********************************************************************************/

/*** 16 bit data path STEREO ******************************************************/
#ifdef SUPPORT_MC
void DC_Mc_D16_TRC_WRA_01_Init     (        Biquad_FLOAT_Instance_t       *pInstance);

void DC_Mc_D16_TRC_WRA_01          (        Biquad_FLOAT_Instance_t       *pInstance,
                                            LVM_FLOAT               *pDataIn,
                                            LVM_FLOAT               *pDataOut,
                                            LVM_INT16               NrFrames,
                                            LVM_INT16               NrChannels);
#else
void DC_2I_D16_TRC_WRA_01_Init     (        Biquad_FLOAT_Instance_t       *pInstance);

void DC_2I_D16_TRC_WRA_01          (        Biquad_FLOAT_Instance_t       *pInstance,
                                            LVM_FLOAT               *pDataIn,
                                            LVM_FLOAT               *pDataOut,
                                            LVM_INT16               NrSamples);
#endif

/**********************************************************************************/

#endif  /** _BIQUAD_H_ **/

