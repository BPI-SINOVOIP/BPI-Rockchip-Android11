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

#ifndef __LVC_MIXER_PRIVATE_H__
#define __LVC_MIXER_PRIVATE_H__

/**********************************************************************************
   INCLUDE FILES
***********************************************************************************/

#include "LVC_Mixer.h"
#include "VectorArithmetic.h"

/* Instance parameter structure */
typedef struct
{
    /* General */
    LVM_FLOAT                       Target;           /*number specifying value of Target Gain */
    LVM_FLOAT                       Current;          /*number specifying value of Current Gain */
    LVM_FLOAT                       Delta;            /*number specifying value of Delta Gain */
} Mix_Private_FLOAT_st;

/**********************************************************************************
   DEFINITIONS
***********************************************************************************/
#define LVCore_MixInSoft_D32C31_SAT    LVCore_InSoft_D32C31_SAT
#define LVCore_MixSoft_1St_D32C31_WRA  LVCore_Soft_1St_D32C31_WRA
#define LVCore_MixHard_2St_D32C31_SAT  LVCore_Hard_2St_D32C31_SAT

/**********************************************************************************
   FUNCTION PROTOTYPES (LOW LEVEL SUBFUNCTIONS)
***********************************************************************************/

/*** 16 bit functions *************************************************************/
void LVC_Core_MixInSoft_D16C31_SAT( LVMixer3_FLOAT_st *ptrInstance,
                                    const LVM_FLOAT     *src,
                                    LVM_FLOAT     *dst,
                                    LVM_INT16     n);
#ifdef SUPPORT_MC
void LVC_Core_MixInSoft_Mc_D16C31_SAT(LVMixer3_FLOAT_st *ptrInstance,
                                    const LVM_FLOAT     *src,
                                          LVM_FLOAT     *dst,
                                          LVM_INT16     NrFrames,
                                          LVM_INT16     NrChannels);
#endif
void LVC_Core_MixSoft_1St_D16C31_WRA( LVMixer3_FLOAT_st *ptrInstance,
                                      const LVM_FLOAT     *src,
                                      LVM_FLOAT     *dst,
                                      LVM_INT16     n);
#ifdef SUPPORT_MC
void LVC_Core_MixSoft_Mc_D16C31_WRA(LVMixer3_FLOAT_st *ptrInstance,
                                    const LVM_FLOAT     *src,
                                          LVM_FLOAT     *dst,
                                          LVM_INT16     NrFrames,
                                          LVM_INT16     NrChannels);
#endif
void LVC_Core_MixHard_2St_D16C31_SAT( LVMixer3_FLOAT_st *pInstance1,
                                      LVMixer3_FLOAT_st         *pInstance2,
                                      const LVM_FLOAT     *src1,
                                      const LVM_FLOAT     *src2,
                                      LVM_FLOAT     *dst,
                                      LVM_INT16     n);

/**********************************************************************************/
/* For applying different gains to Left and right chennals                        */
/* ptrInstance1 applies to Left channel                                           */
/* ptrInstance2 applies to Right channel                                          */
/* Gain values should not be more that 1.0                                        */
/**********************************************************************************/
#ifdef SUPPORT_MC
void LVC_Core_MixSoft_1St_MC_float_WRA(Mix_Private_FLOAT_st **ptrInstance,
                                         const LVM_FLOAT      *src,
                                         LVM_FLOAT            *dst,
                                         LVM_INT16            NrFrames,
                                         LVM_INT16            NrChannels);
#endif
void LVC_Core_MixSoft_1St_2i_D16C31_WRA( LVMixer3_FLOAT_st        *ptrInstance1,
                                         LVMixer3_FLOAT_st        *ptrInstance2,
                                         const LVM_FLOAT    *src,
                                         LVM_FLOAT          *dst,
                                         LVM_INT16          n);

/**********************************************************************************/
/* For applying different gains to Left and right chennals                        */
/* ptrInstance1 applies to Left channel                                           */
/* ptrInstance2 applies to Right channel                                          */
/* Gain values should not be more that 1.0                                        */
/**********************************************************************************/
#ifdef SUPPORT_MC
void LVC_Core_MixHard_1St_MC_float_SAT(Mix_Private_FLOAT_st **ptrInstance,
                                         const LVM_FLOAT      *src,
                                         LVM_FLOAT            *dst,
                                         LVM_INT16            NrFrames,
                                         LVM_INT16            NrChannels);
#endif
void LVC_Core_MixHard_1St_2i_D16C31_SAT( LVMixer3_FLOAT_st        *ptrInstance1,
                                         LVMixer3_FLOAT_st        *ptrInstance2,
                                         const LVM_FLOAT    *src,
                                         LVM_FLOAT          *dst,
                                         LVM_INT16          n);

/*** 32 bit functions *************************************************************/
/**********************************************************************************/

#endif //#ifndef __LVC_MIXER_PRIVATE_H__

